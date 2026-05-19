#pragma once

#include "LyricsCore.h"
#include "LyricsLookupEngine.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariant>

namespace lyricsmpris {

struct AppOptions {
    bool pipe = true;
    bool lookupMode = false;
    QString preferredService;
    QString lookupTitle;
    QString lookupArtist;
    QString lookupAlbum;
    int lookupDurationMs = 0;
    QStringList providers;
    QSet<QString> blockedServices;
};

class LyricsMprisApp final : public QObject {
    Q_OBJECT

public:
    explicit LyricsMprisApp(AppOptions options, QObject *parent = nullptr);

public slots:
    void start();

private slots:
    void refreshPlayers();
    void handleNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void handlePropertiesChanged(const QString &interfaceName, const QVariantMap &changedProperties, const QStringList &invalidatedProperties);
    void updatePosition();
    void handleLookupDocument(LyricDocument document, const QString &status, bool finalSynced);
    void handleLookupNotFound();

private:
    struct PlayerInfo {
        QString service;
        QString playbackStatus;
        QString title;
        QString artist;
        QString album;
        QString url;
        QString trackId;
        QString inlineLyrics;
        qint64 lengthMs = 0;
        qint64 positionMs = 0;
        QDateTime lastActive;
        bool valid = false;
    };

    void emitStatus(const QString &status);
    void emitLine(const QString &line, bool synced);
    void clearCurrentTrack();
    void releaseCurrentDocument();
    void startLookup();
    void chooseActivePlayer();
    void updatePlayer(const QString &service);
    void startTrack(const PlayerInfo &player);
    QString trackKeyFor(const PlayerInfo &player) const;
    TrackQuery queryFor(const PlayerInfo &player) const;
    void tryInlineLyrics(const PlayerInfo &player, bool *hasSyncedDocument);
    void tryLocalLyrics(const PlayerInfo &player, bool *hasSyncedDocument);
    void acceptDocument(LyricDocument document, const QString &status, bool finalSynced);
    void maybeQuitLookup();
    bool serviceBlocked(const QString &service) const;
    static QVariant unwrapDbusVariant(const QVariant &value);
    static QString variantToString(const QVariant &value);
    static QString variantToStringListText(const QVariant &value);
    static qint64 variantToLongLong(const QVariant &value);

    AppOptions m_options;
    LyricsLookupEngine m_lookup;
    QTimer m_refreshTimer;
    QTimer m_positionTimer;
    QHash<QString, PlayerInfo> m_players;
    QString m_activeService;
    QString m_currentTrackKey;
    PlayerInfo m_currentPlayer;
    LyricDocument m_currentDocument;
    bool m_hasAcceptedDocument = false;
    QString m_lastStatus;
    QString m_lastLine;
    bool m_lastLineSynced = false;
};

} // namespace lyricsmpris
