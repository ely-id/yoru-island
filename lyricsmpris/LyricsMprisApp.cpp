#include "LyricsMprisApp.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <cstdio>

namespace lyricsmpris {
namespace {

constexpr int kRefreshIntervalMs = 1500;
constexpr int kPositionIntervalMs = 350;
constexpr qsizetype kMaxLocalLyricsSize = 1024 * 1024;

bool isMprisService(const QString &service) {
    return service.startsWith(QStringLiteral("org.mpris.MediaPlayer2."));
}

} // namespace

LyricsMprisApp::LyricsMprisApp(AppOptions options, QObject *parent)
    : QObject(parent),
      m_options(std::move(options)) {
    m_options.providers = normalizeLyricProviders(m_options.providers);

    m_refreshTimer.setInterval(kRefreshIntervalMs);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &LyricsMprisApp::refreshPlayers);

    m_positionTimer.setInterval(kPositionIntervalMs);
    m_positionTimer.setSingleShot(false);
    connect(&m_positionTimer, &QTimer::timeout, this, &LyricsMprisApp::updatePosition);

    connect(&m_lookup, &LyricsLookupEngine::documentReady, this, &LyricsMprisApp::handleLookupDocument);
    connect(&m_lookup, &LyricsLookupEngine::notFound, this, &LyricsMprisApp::handleLookupNotFound);
}

void LyricsMprisApp::start() {
    if (m_options.lookupMode) {
        startLookup();
        return;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(handleNameOwnerChanged(QString,QString,QString)));
    bus.connect(
        QString(),
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(handlePropertiesChanged(QString,QVariantMap,QStringList)));

    refreshPlayers();
    m_refreshTimer.start();
}

void LyricsMprisApp::startLookup() {
    PlayerInfo player;
    player.playbackStatus = QStringLiteral("Playing");
    player.title = m_options.lookupTitle;
    player.artist = m_options.lookupArtist;
    player.album = m_options.lookupAlbum;
    player.lengthMs = m_options.lookupDurationMs;
    player.trackId = QStringLiteral("lookup");
    player.valid = true;
    startTrack(player);
}

void LyricsMprisApp::refreshPlayers() {
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    if (!interface) {
        emitStatus(QStringLiteral("error"));
        return;
    }

    QDBusReply<QStringList> namesReply = interface->registeredServiceNames();
    if (!namesReply.isValid()) {
        emitStatus(QStringLiteral("error"));
        return;
    }

    const QStringList services = namesReply.value();
    QSet<QString> seen;
    for (const QString &service : services) {
        if (!isMprisService(service) || serviceBlocked(service)) continue;
        seen.insert(service);
        updatePlayer(service);
    }

    const QList<QString> knownServices = m_players.keys();
    for (const QString &service : knownServices) {
        if (!seen.contains(service)) m_players.remove(service);
    }

    chooseActivePlayer();
}

void LyricsMprisApp::handleNameOwnerChanged(const QString &name, const QString &, const QString &) {
    if (isMprisService(name)) QTimer::singleShot(0, this, &LyricsMprisApp::refreshPlayers);
}

void LyricsMprisApp::handlePropertiesChanged(const QString &interfaceName, const QVariantMap &, const QStringList &) {
    if (interfaceName == QLatin1String("org.mpris.MediaPlayer2.Player"))
        QTimer::singleShot(0, this, &LyricsMprisApp::refreshPlayers);
}

void LyricsMprisApp::emitStatus(const QString &status) {
    if (status == m_lastStatus) return;
    m_lastStatus = status;

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("status"));
    object.insert(QStringLiteral("status"), status);
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact);
    std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void LyricsMprisApp::emitLine(const QString &line, bool synced) {
    if (line == m_lastLine && synced == m_lastLineSynced) return;
    m_lastLine = line;
    m_lastLineSynced = synced;

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("line"));
    object.insert(QStringLiteral("text"), line);
    object.insert(QStringLiteral("synced"), synced && !line.isEmpty());
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    std::fwrite(payload.constData(), 1, size_t(payload.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void LyricsMprisApp::updatePlayer(const QString &service) {
    QDBusInterface properties(
        service,
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QDBusConnection::sessionBus());
    if (!properties.isValid()) return;

    QDBusMessage reply = properties.call(
        QStringLiteral("GetAll"),
        QStringLiteral("org.mpris.MediaPlayer2.Player"));
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) return;

    QVariantMap propertyMap = qdbus_cast<QVariantMap>(reply.arguments().first());
    PlayerInfo player = m_players.value(service);
    player.service = service;
    player.valid = true;
    player.playbackStatus = variantToString(propertyMap.value(QStringLiteral("PlaybackStatus")));
    if (player.playbackStatus == QLatin1String("Playing")) player.lastActive = QDateTime::currentDateTimeUtc();

    QVariantMap metadata = qdbus_cast<QVariantMap>(unwrapDbusVariant(propertyMap.value(QStringLiteral("Metadata"))));
    player.title = variantToString(metadata.value(QStringLiteral("xesam:title")));
    player.artist = variantToStringListText(metadata.value(QStringLiteral("xesam:artist")));
    player.album = variantToString(metadata.value(QStringLiteral("xesam:album")));
    player.url = variantToString(metadata.value(QStringLiteral("xesam:url")));
    player.trackId = variantToString(metadata.value(QStringLiteral("mpris:trackid")));
    player.lengthMs = variantToLongLong(metadata.value(QStringLiteral("mpris:length"))) / 1000;
    player.inlineLyrics = variantToString(metadata.value(QStringLiteral("xesam:asText")));
    if (player.inlineLyrics.isEmpty()) player.inlineLyrics = variantToString(metadata.value(QStringLiteral("xesam:comment")));

    QDBusMessage positionReply = properties.call(
        QStringLiteral("Get"),
        QStringLiteral("org.mpris.MediaPlayer2.Player"),
        QStringLiteral("Position"));
    if (positionReply.type() != QDBusMessage::ErrorMessage && !positionReply.arguments().isEmpty()) {
        player.positionMs = variantToLongLong(positionReply.arguments().first()) / 1000;
    }

    m_players.insert(service, player);
}

void LyricsMprisApp::chooseActivePlayer() {
    QString chosen;

    if (!m_options.preferredService.isEmpty() && m_players.contains(m_options.preferredService))
        chosen = m_options.preferredService;

    if (chosen.isEmpty()) {
        for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
            if (it->playbackStatus == QLatin1String("Playing") && (!it->title.isEmpty() || !it->trackId.isEmpty())) {
                chosen = it.key();
                break;
            }
        }
    }

    if (chosen.isEmpty()) {
        QDateTime latest;
        for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
            if (!it->lastActive.isValid()) continue;
            if (!latest.isValid() || it->lastActive > latest) {
                latest = it->lastActive;
                chosen = it.key();
            }
        }
    }

    if (chosen.isEmpty()) {
        for (auto it = m_players.cbegin(); it != m_players.cend(); ++it) {
            if (it->playbackStatus == QLatin1String("Paused") && (!it->title.isEmpty() || !it->trackId.isEmpty())) {
                chosen = it.key();
                break;
            }
        }
    }

    if (chosen.isEmpty() && !m_players.isEmpty()) chosen = m_players.cbegin().key();

    if (chosen.isEmpty()) {
        clearCurrentTrack();
        return;
    }

    const PlayerInfo player = m_players.value(chosen);
    if (player.playbackStatus == QLatin1String("Stopped") || (player.title.isEmpty() && player.trackId.isEmpty())) {
        clearCurrentTrack();
        return;
    }

    m_activeService = chosen;
    const QString nextTrackKey = trackKeyFor(player);
    if (nextTrackKey != m_currentTrackKey) startTrack(player);
    else {
        m_currentPlayer = player;
        updatePosition();
    }
}

QString LyricsMprisApp::trackKeyFor(const PlayerInfo &player) const {
    return QStringList({
        player.service,
        player.trackId,
        player.title,
        player.artist,
        player.album,
        QString::number(player.lengthMs)
    }).join(QLatin1Char('|'));
}

TrackQuery LyricsMprisApp::queryFor(const PlayerInfo &player) const {
    TrackQuery query;
    query.title = player.title;
    query.artist = player.artist;
    query.album = player.album;
    query.durationMs = int(player.lengthMs);
    return query;
}

void LyricsMprisApp::startTrack(const PlayerInfo &player) {
    clearCurrentTrack();
    m_activeService = player.service;
    m_currentPlayer = player;
    m_currentTrackKey = trackKeyFor(player);
    m_hasAcceptedDocument = false;
    emitLine(QString(), false);
    emitStatus(QStringLiteral("searching"));

    if (player.title.trimmed().isEmpty()) {
        emitStatus(QStringLiteral("not_found"));
        maybeQuitLookup();
        return;
    }

    bool hasSyncedDocument = false;
    tryInlineLyrics(player, &hasSyncedDocument);
    if (!hasSyncedDocument) tryLocalLyrics(player, &hasSyncedDocument);
    if (hasSyncedDocument) return;

    m_lookup.start(queryFor(player), m_options.providers);
}

void LyricsMprisApp::tryInlineLyrics(const PlayerInfo &player, bool *hasSyncedDocument) {
    if (player.inlineLyrics.trimmed().isEmpty()) return;

    ProviderCandidate candidate;
    candidate.provider = QStringLiteral("mpris");
    candidate.title = player.title;
    candidate.artist = player.artist;
    candidate.album = player.album;
    candidate.durationMs = int(player.lengthMs);
    candidate.syncedLyrics = player.inlineLyrics;

    LyricDocument document = documentFromCandidate(candidate);
    if (document.hasSyncedLines()) {
        acceptDocument(std::move(document), QStringLiteral("synced"), true);
        *hasSyncedDocument = true;
        return;
    }

    if (document.hasPlainLines())
        acceptDocument(std::move(document), QStringLiteral("plain"), false);
}

void LyricsMprisApp::tryLocalLyrics(const PlayerInfo &player, bool *hasSyncedDocument) {
    if (player.url.isEmpty()) return;

    QUrl url(player.url);
    QString localPath;
    if (url.isLocalFile()) localPath = url.toLocalFile();
    else if (QFileInfo::exists(player.url)) localPath = player.url;
    if (localPath.isEmpty()) return;

    const QFileInfo mediaInfo(localPath);
    const QString base = mediaInfo.completeBaseName();
    const QString dir = mediaInfo.absolutePath();
    const QStringList candidates = {
        dir + QLatin1Char('/') + base + QStringLiteral(".lrc"),
        dir + QLatin1Char('/') + base + QStringLiteral(".LRC"),
        localPath + QStringLiteral(".lrc"),
        dir + QStringLiteral("/lyrics/") + base + QStringLiteral(".lrc")
    };

    for (const QString &path : candidates) {
        QFile file(path);
        if (!file.exists() || file.size() <= 0 || file.size() > kMaxLocalLyricsSize) continue;
        if (!file.open(QIODevice::ReadOnly)) continue;

        ProviderCandidate candidate;
        candidate.provider = QStringLiteral("local");
        candidate.title = player.title;
        candidate.artist = player.artist;
        candidate.album = player.album;
        candidate.durationMs = int(player.lengthMs);
        candidate.syncedLyrics = QString::fromUtf8(file.readAll());

        LyricDocument document = documentFromCandidate(candidate);
        if (document.hasSyncedLines()) {
            acceptDocument(std::move(document), QStringLiteral("synced"), true);
            *hasSyncedDocument = true;
            return;
        }
        if (document.hasPlainLines())
            acceptDocument(std::move(document), QStringLiteral("plain"), false);
    }
}

void LyricsMprisApp::handleLookupDocument(LyricDocument document, const QString &status, bool finalSynced) {
    acceptDocument(std::move(document), status, finalSynced);
}

void LyricsMprisApp::handleLookupNotFound() {
    if (!m_hasAcceptedDocument) {
        emitLine(QString(), false);
        emitStatus(QStringLiteral("not_found"));
    }
    maybeQuitLookup();
}

void LyricsMprisApp::acceptDocument(LyricDocument document, const QString &status, bool finalSynced) {
    releaseCurrentDocument();
    m_currentDocument = std::move(document);
    m_hasAcceptedDocument = true;
    emitStatus(status);
    updatePosition();

    if (finalSynced) m_lookup.stop();
    maybeQuitLookup();
}

void LyricsMprisApp::updatePosition() {
    if (m_currentTrackKey.isEmpty() || m_currentDocument.isEmpty()) {
        m_positionTimer.stop();
        return;
    }

    PlayerInfo player = m_players.value(m_activeService, m_currentPlayer);
    if (player.service.isEmpty()) player = m_currentPlayer;

    if (!player.service.isEmpty()) {
        QDBusInterface properties(
            player.service,
            QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QDBusConnection::sessionBus());
        QDBusMessage positionReply = properties.call(
            QStringLiteral("Get"),
            QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QStringLiteral("Position"));
        if (positionReply.type() != QDBusMessage::ErrorMessage && !positionReply.arguments().isEmpty())
            player.positionMs = variantToLongLong(positionReply.arguments().first()) / 1000;
    }

    const QString line = selectLineAt(m_currentDocument, player.positionMs);
    emitLine(line, m_currentDocument.hasSyncedLines());

    const bool shouldPoll = m_currentDocument.hasSyncedLines()
        && player.playbackStatus != QLatin1String("Stopped")
        && !m_currentTrackKey.isEmpty();
    if (shouldPoll && !m_positionTimer.isActive()) m_positionTimer.start();
    else if (!shouldPoll && m_positionTimer.isActive()) m_positionTimer.stop();
}

void LyricsMprisApp::clearCurrentTrack() {
    m_lookup.stop();
    m_positionTimer.stop();
    releaseCurrentDocument();
    m_currentTrackKey.clear();
    m_currentTrackKey.squeeze();
    m_currentPlayer = PlayerInfo();
    m_activeService.clear();
    m_activeService.squeeze();
    m_hasAcceptedDocument = false;
    emitLine(QString(), false);
    emitStatus(QStringLiteral("idle"));
}

void LyricsMprisApp::releaseCurrentDocument() {
    m_currentDocument.clearAndFree();
}

void LyricsMprisApp::maybeQuitLookup() {
    if (m_options.lookupMode)
        QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
}

bool LyricsMprisApp::serviceBlocked(const QString &service) const {
    const QString lower = service.toLower();
    for (const QString &blocked : m_options.blockedServices) {
        if (!blocked.isEmpty() && lower.contains(blocked)) return true;
    }
    return false;
}

QVariant LyricsMprisApp::unwrapDbusVariant(const QVariant &value) {
    if (value.metaType().id() == qMetaTypeId<QDBusVariant>())
        return qvariant_cast<QDBusVariant>(value).variant();
    return value;
}

QString LyricsMprisApp::variantToString(const QVariant &value) {
    const QVariant unwrapped = unwrapDbusVariant(value);
    if (unwrapped.metaType().id() == qMetaTypeId<QDBusObjectPath>())
        return qvariant_cast<QDBusObjectPath>(unwrapped).path();
    if (unwrapped.metaType().id() == qMetaTypeId<QDBusArgument>())
        return qdbus_cast<QString>(unwrapped);
    if (unwrapped.canConvert<QString>()) return unwrapped.toString();
    return QString();
}

QString LyricsMprisApp::variantToStringListText(const QVariant &value) {
    const QVariant unwrapped = unwrapDbusVariant(value);
    if (unwrapped.metaType().id() == qMetaTypeId<QDBusArgument>())
        return qdbus_cast<QStringList>(unwrapped).join(QStringLiteral(", "));
    if (unwrapped.canConvert<QStringList>())
        return unwrapped.toStringList().join(QStringLiteral(", "));
    if (unwrapped.metaType().id() == QMetaType::QVariantList) {
        QStringList values;
        for (const QVariant &item : unwrapped.toList()) values.append(variantToString(item));
        return values.join(QStringLiteral(", "));
    }
    return variantToString(unwrapped);
}

qint64 LyricsMprisApp::variantToLongLong(const QVariant &value) {
    const QVariant unwrapped = unwrapDbusVariant(value);
    if (unwrapped.metaType().id() == qMetaTypeId<QDBusArgument>())
        return qdbus_cast<qlonglong>(unwrapped);
    return unwrapped.toLongLong();
}

} // namespace lyricsmpris
