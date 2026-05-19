#include "LyricsLookupEngine.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTextStream>
#include <QUrlQuery>
#include <algorithm>

namespace lyricsmpris {
namespace {

constexpr int kPrimaryFallbackDelayMs = 1800;
constexpr int kNetworkTimeoutMs = 7000;
constexpr int kSearchCandidateLimit = 5;
constexpr int kDownloadCandidateLimit = 2;

bool lyricsDebugEnabled() {
    return !qEnvironmentVariableIsEmpty("LYRICSMPRIS_DEBUG");
}

bool isPrimaryProvider(const QString &provider) {
    return provider == QLatin1String("lrclib")
        || provider == QLatin1String("lrcx")
        || provider == QLatin1String("musixmatch");
}

bool isFallbackProvider(const QString &provider) {
    return provider == QLatin1String("lyricsovh")
        || provider == QLatin1String("netease")
        || provider == QLatin1String("qq")
        || provider == QLatin1String("kugou");
}

QUrl withQuery(const QString &base, const QUrlQuery &query) {
    QUrl url(base);
    url.setQuery(query);
    return url;
}

QString makeSearchQuery(const TrackQuery &query) {
    return (query.title.trimmed() + QLatin1Char(' ') + query.artist.trimmed()).trimmed();
}

QByteArray safeBody(QNetworkReply *reply) {
    const QByteArray body = reply->readAll();
    return body.left(2 * 1024 * 1024);
}

} // namespace

QStringList supportedLyricProviders() {
    return {
        QStringLiteral("lrclib"),
        QStringLiteral("lrcx"),
        QStringLiteral("musixmatch"),
        QStringLiteral("lyricsovh"),
        QStringLiteral("netease"),
        QStringLiteral("qq"),
        QStringLiteral("kugou")
    };
}

QStringList defaultLyricProviders() {
    QStringList providers = {
        QStringLiteral("lrclib"),
        QStringLiteral("lrcx"),
        QStringLiteral("lyricsovh"),
        QStringLiteral("netease"),
        QStringLiteral("qq"),
        QStringLiteral("kugou")
    };
    if (!qEnvironmentVariable("MUSIXMATCH_API_KEY").isEmpty())
        providers.insert(2, QStringLiteral("musixmatch"));
    return providers;
}

QStringList normalizeLyricProviders(QStringList providers) {
    if (providers.isEmpty()) providers = defaultLyricProviders();

    const QStringList supported = supportedLyricProviders();
    QStringList normalized;
    for (const QString &providerList : std::as_const(providers)) {
        const QStringList parts = providerList.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString part : parts) {
            part = part.trimmed().toLower();
            if (!part.isEmpty() && supported.contains(part) && !normalized.contains(part))
                normalized.append(part);
        }
    }
    return normalized;
}

LyricsLookupEngine::LyricsLookupEngine(QObject *parent)
    : QObject(parent) {
    m_fallbackTimer.setInterval(kPrimaryFallbackDelayMs);
    m_fallbackTimer.setSingleShot(true);
    connect(&m_fallbackTimer, &QTimer::timeout, this, &LyricsLookupEngine::startFallbackProviders);
}

void LyricsLookupEngine::start(const TrackQuery &query, const QStringList &providers) {
    stop();

    m_generation++;
    m_running = true;
    m_query = query;
    m_providers = normalizeLyricProviders(providers);

    bool startedPrimary = false;
    for (const QString &provider : std::as_const(m_providers)) {
        if (!isPrimaryProvider(provider)) continue;
        if (startProvider(provider)) startedPrimary = true;
    }

    if (startedPrimary) m_fallbackTimer.start();
    else startFallbackProviders();
}

void LyricsLookupEngine::stop() {
    m_generation++;
    m_fallbackTimer.stop();
    abortNetwork();
    clearState();
}

void LyricsLookupEngine::clearState() {
    m_query = TrackQuery();
    m_providers.clear();
    m_fallbackProviders.clear();
    m_startedProviders.clear();
    m_bestSyncedDocument.clearAndFree();
    m_bestPlainDocument.clearAndFree();
    m_bestSyncedScore = 0;
    m_bestPlainScore = 0;
    m_nextFallbackProviderIndex = 0;
    m_pendingReplies = 0;
    m_running = false;
    m_fallbackStarted = false;
}

void LyricsLookupEngine::abortNetwork() {
    for (QNetworkReply *reply : std::as_const(m_replies)) {
        if (!reply) continue;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    m_replies.clear();
    m_pendingReplies = 0;
}

void LyricsLookupEngine::startFallbackProviders() {
    if (!m_running || m_fallbackStarted) return;
    if (m_bestSyncedDocument.hasSyncedLines()) {
        finishWithDocument(std::move(m_bestSyncedDocument), QStringLiteral("synced"), true);
        return;
    }

    m_fallbackStarted = true;
    m_nextFallbackProviderIndex = 0;
    m_fallbackProviders.clear();
    for (const QString &provider : std::as_const(m_providers)) {
        if (isFallbackProvider(provider) && !m_fallbackProviders.contains(provider))
            m_fallbackProviders.append(provider);
    }

    if (!startNextFallbackProvider()) maybeAdvance();
}

bool LyricsLookupEngine::startNextFallbackProvider() {
    if (!m_running) return false;

    while (m_nextFallbackProviderIndex < m_fallbackProviders.size()) {
        const QString provider = m_fallbackProviders.at(m_nextFallbackProviderIndex++);
        if (startProvider(provider)) return true;
    }
    return false;
}

bool LyricsLookupEngine::startProvider(const QString &provider) {
    if (m_startedProviders.contains(provider)) return false;
    m_startedProviders.insert(provider);

    if (provider == QLatin1String("lrclib")) return startLrclib();
    if (provider == QLatin1String("lrcx")) return startLrcx();
    if (provider == QLatin1String("musixmatch")) return startMusixmatch();
    if (provider == QLatin1String("lyricsovh")) return startLyricsOvh();
    if (provider == QLatin1String("netease")) return startNetease();
    if (provider == QLatin1String("qq")) return startQq();
    if (provider == QLatin1String("kugou")) return startKugou();
    return false;
}

bool LyricsLookupEngine::startLrclib() {
    QUrlQuery getQuery;
    getQuery.addQueryItem(QStringLiteral("track_name"), m_query.title);
    getQuery.addQueryItem(QStringLiteral("artist_name"), m_query.artist);
    if (!m_query.album.isEmpty()) getQuery.addQueryItem(QStringLiteral("album_name"), m_query.album);
    if (m_query.durationMs > 0) getQuery.addQueryItem(QStringLiteral("duration"), QString::number(qMax(1, m_query.durationMs / 1000)));
    get(withQuery(QStringLiteral("https://lrclib.net/api/get"), getQuery), QStringLiteral("lrclib"), QStringLiteral("lrclib-get"));

    QUrlQuery searchQuery;
    searchQuery.addQueryItem(QStringLiteral("track_name"), m_query.title);
    searchQuery.addQueryItem(QStringLiteral("artist_name"), m_query.artist);
    get(withQuery(QStringLiteral("https://lrclib.net/api/search"), searchQuery), QStringLiteral("lrclib"), QStringLiteral("lrclib-search"));
    return true;
}

bool LyricsLookupEngine::startLrcx() {
    QUrlQuery advanced;
    advanced.addQueryItem(QStringLiteral("title"), m_query.title);
    advanced.addQueryItem(QStringLiteral("artist"), m_query.artist);
    if (!m_query.album.isEmpty()) advanced.addQueryItem(QStringLiteral("album"), m_query.album);
    if (m_query.durationMs > 0) advanced.addQueryItem(QStringLiteral("duration"), QString::number(qMax(1, m_query.durationMs / 1000)));
    get(withQuery(QStringLiteral("https://api.lrc.cx/api/v1/lyrics/advance"), advanced), QStringLiteral("lrcx"), QStringLiteral("lrcx-json"));

    QUrlQuery legacy;
    legacy.addQueryItem(QStringLiteral("title"), m_query.title);
    legacy.addQueryItem(QStringLiteral("artist"), m_query.artist);
    get(withQuery(QStringLiteral("https://api.lrc.cx/lyrics"), legacy), QStringLiteral("lrcx"), QStringLiteral("lrcx-text"));
    return true;
}

bool LyricsLookupEngine::startLyricsOvh() {
    if (m_query.title.trimmed().isEmpty() || m_query.artist.trimmed().isEmpty()) return false;

    ProviderCandidate requestMetadata;
    requestMetadata.provider = QStringLiteral("lyricsovh");
    requestMetadata.title = m_query.title;
    requestMetadata.artist = m_query.artist;
    requestMetadata.album = m_query.album;
    requestMetadata.durationMs = m_query.durationMs;
    requestMetadata.metadataTrusted = true;

    const QByteArray url = QByteArrayLiteral("https://api.lyrics.ovh/v1/")
        + QUrl::toPercentEncoding(m_query.artist)
        + QByteArrayLiteral("/")
        + QUrl::toPercentEncoding(m_query.title);
    QNetworkReply *reply = get(QUrl::fromEncoded(url), QStringLiteral("lyricsovh"), QStringLiteral("lyricsovh-lyrics"));
    copyCandidateMetadata(reply, requestMetadata);
    return true;
}

bool LyricsLookupEngine::startNetease() {
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("s"), makeSearchQuery(m_query));
    urlQuery.addQueryItem(QStringLiteral("type"), QStringLiteral("1"));
    urlQuery.addQueryItem(QStringLiteral("limit"), QString::number(kSearchCandidateLimit));
    urlQuery.addQueryItem(QStringLiteral("offset"), QStringLiteral("0"));
    get(withQuery(QStringLiteral("https://music.163.com/api/search/get"), urlQuery), QStringLiteral("netease"), QStringLiteral("netease-search"));
    return true;
}

bool LyricsLookupEngine::startQq() {
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    urlQuery.addQueryItem(QStringLiteral("p"), QStringLiteral("1"));
    urlQuery.addQueryItem(QStringLiteral("n"), QString::number(kSearchCandidateLimit));
    urlQuery.addQueryItem(QStringLiteral("w"), makeSearchQuery(m_query));
    get(withQuery(QStringLiteral("https://c.y.qq.com/soso/fcgi-bin/client_search_cp"), urlQuery), QStringLiteral("qq"), QStringLiteral("qq-search"));
    return true;
}

bool LyricsLookupEngine::startKugou() {
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("keyword"), makeSearchQuery(m_query));
    urlQuery.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
    urlQuery.addQueryItem(QStringLiteral("pagesize"), QString::number(kSearchCandidateLimit));
    get(withQuery(QStringLiteral("https://songsearch.kugou.com/song_search_v2"), urlQuery), QStringLiteral("kugou"), QStringLiteral("kugou-song-search"));
    return true;
}

bool LyricsLookupEngine::startMusixmatch() {
    const QString apiKey = qEnvironmentVariable("MUSIXMATCH_API_KEY");
    if (apiKey.isEmpty()) return false;

    ProviderCandidate requestMetadata;
    requestMetadata.provider = QStringLiteral("musixmatch");
    requestMetadata.title = m_query.title;
    requestMetadata.artist = m_query.artist;
    requestMetadata.album = m_query.album;
    requestMetadata.durationMs = m_query.durationMs;
    requestMetadata.metadataTrusted = true;

    QUrlQuery subtitleQuery;
    subtitleQuery.addQueryItem(QStringLiteral("q_track"), m_query.title);
    subtitleQuery.addQueryItem(QStringLiteral("q_artist"), m_query.artist);
    subtitleQuery.addQueryItem(QStringLiteral("apikey"), apiKey);
    QNetworkReply *subtitleReply = get(withQuery(QStringLiteral("https://api.musixmatch.com/ws/1.1/matcher.subtitle.get"), subtitleQuery), QStringLiteral("musixmatch"), QStringLiteral("musixmatch-subtitle"));
    copyCandidateMetadata(subtitleReply, requestMetadata);

    QUrlQuery lyricsQuery = subtitleQuery;
    QNetworkReply *lyricsReply = get(withQuery(QStringLiteral("https://api.musixmatch.com/ws/1.1/matcher.lyrics.get"), lyricsQuery), QStringLiteral("musixmatch"), QStringLiteral("musixmatch-lyrics"));
    copyCandidateMetadata(lyricsReply, requestMetadata);
    return true;
}

QNetworkReply *LyricsLookupEngine::get(const QUrl &url, const QString &provider, const QString &stage) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("lyricsmpris-cpp/2.2 TideIsland"));
    request.setRawHeader("Accept", "application/json,text/plain,*/*");
    if (provider == QLatin1String("netease"))
        request.setRawHeader("Referer", "https://music.163.com/");
    else if (provider == QLatin1String("qq"))
        request.setRawHeader("Referer", "https://y.qq.com/");
    else if (provider == QLatin1String("kugou"))
        request.setRawHeader("Referer", "https://www.kugou.com/");
    request.setTransferTimeout(kNetworkTimeoutMs);

    QNetworkReply *reply = m_network.get(request);
    reply->setProperty("generation", m_generation);
    reply->setProperty("provider", provider);
    reply->setProperty("stage", stage);
    m_replies.append(reply);
    m_pendingReplies++;
    connect(reply, &QNetworkReply::finished, this, &LyricsLookupEngine::handleNetworkFinished);
    return reply;
}

void LyricsLookupEngine::copyCandidateMetadata(QNetworkReply *reply, const ProviderCandidate &candidate) {
    reply->setProperty("candidateTitle", candidate.title);
    reply->setProperty("candidateArtist", candidate.artist);
    reply->setProperty("candidateAlbum", candidate.album);
    reply->setProperty("candidateDurationMs", candidate.durationMs);
    reply->setProperty("candidateMetadataTrusted", candidate.metadataTrusted);
}

ProviderCandidate LyricsLookupEngine::candidateFromReply(QNetworkReply *reply, ProviderCandidate candidate) const {
    if (candidate.title.isEmpty()) candidate.title = reply->property("candidateTitle").toString();
    if (candidate.artist.isEmpty()) candidate.artist = reply->property("candidateArtist").toString();
    if (candidate.album.isEmpty()) candidate.album = reply->property("candidateAlbum").toString();
    if (candidate.durationMs <= 0) candidate.durationMs = reply->property("candidateDurationMs").toInt();
    if (reply->property("candidateMetadataTrusted").isValid())
        candidate.metadataTrusted = reply->property("candidateMetadataTrusted").toBool();
    return candidate;
}

void LyricsLookupEngine::handleNetworkFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    m_replies.removeOne(reply);
    m_pendingReplies = qMax(0, m_pendingReplies - 1);

    const int generation = reply->property("generation").toInt();
    const QString provider = reply->property("provider").toString();
    const QString stage = reply->property("stage").toString();
    const bool current = m_running && generation == m_generation;
    const QByteArray body = current && reply->error() == QNetworkReply::NoError ? safeBody(reply) : QByteArray();

    if (current && !body.isEmpty()) {
        if (stage == QLatin1String("lrclib-get") || stage == QLatin1String("lrclib-search")) {
            for (ProviderCandidate candidate : parseLrclibJson(body)) considerCandidate(candidate);
        } else if (stage == QLatin1String("lrcx-json")) {
            for (ProviderCandidate candidate : parseLrcxJson(body)) considerCandidate(candidate);
        } else if (stage == QLatin1String("lrcx-text")) {
            ProviderCandidate candidate;
            candidate.provider = provider;
            candidate.metadataTrusted = false;
            candidate.syncedLyrics = QString::fromUtf8(body);
            considerCandidate(candidate);
        } else if (stage == QLatin1String("lyricsovh-lyrics")) {
            considerCandidate(candidateFromReply(reply, parseLyricsOvhJson(body)));
        } else if (stage == QLatin1String("netease-search")) {
            QList<ProviderCandidate> candidates = parseNeteaseSearchJson(body);
            std::sort(candidates.begin(), candidates.end(), [this](const ProviderCandidate &left, const ProviderCandidate &right) {
                return scoreCandidate(m_query, left) > scoreCandidate(m_query, right);
            });
            int requested = 0;
            for (int index = 0; index < candidates.size() && requested < kDownloadCandidateLimit; ++index) {
                const ProviderCandidate &candidate = candidates.at(index);
                const CandidateEvaluation evaluation = evaluateCandidate(m_query, candidate);
                debugCandidate(candidate, evaluation, evaluation.accepted ? QStringLiteral("download") : QStringLiteral("skip"));
                if (!evaluation.accepted) continue;
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("os"), QStringLiteral("pc"));
                query.addQueryItem(QStringLiteral("id"), candidate.syncedLyrics);
                query.addQueryItem(QStringLiteral("lv"), QStringLiteral("-1"));
                query.addQueryItem(QStringLiteral("tv"), QStringLiteral("-1"));
                QNetworkReply *next = get(withQuery(QStringLiteral("https://music.163.com/api/song/lyric"), query), provider, QStringLiteral("netease-lyric"));
                copyCandidateMetadata(next, candidate);
                requested++;
            }
        } else if (stage == QLatin1String("netease-lyric")) {
            considerCandidate(candidateFromReply(reply, parseNeteaseLyricJson(body)));
        } else if (stage == QLatin1String("qq-search")) {
            QList<ProviderCandidate> candidates = parseQqSearchJson(body);
            std::sort(candidates.begin(), candidates.end(), [this](const ProviderCandidate &left, const ProviderCandidate &right) {
                return scoreCandidate(m_query, left) > scoreCandidate(m_query, right);
            });
            int requested = 0;
            for (int index = 0; index < candidates.size() && requested < kDownloadCandidateLimit; ++index) {
                const ProviderCandidate &candidate = candidates.at(index);
                const CandidateEvaluation evaluation = evaluateCandidate(m_query, candidate);
                debugCandidate(candidate, evaluation, evaluation.accepted ? QStringLiteral("download") : QStringLiteral("skip"));
                if (!evaluation.accepted) continue;
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("songmid"), candidate.syncedLyrics);
                query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
                query.addQueryItem(QStringLiteral("nobase64"), QStringLiteral("1"));
                QNetworkReply *next = get(withQuery(QStringLiteral("https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg"), query), provider, QStringLiteral("qq-lyric"));
                copyCandidateMetadata(next, candidate);
                requested++;
            }
        } else if (stage == QLatin1String("qq-lyric")) {
            considerCandidate(candidateFromReply(reply, parseQqLyricJson(body)));
        } else if (stage == QLatin1String("kugou-song-search")) {
            QList<ProviderCandidate> candidates = parseKugouSongSearchJson(body);
            std::sort(candidates.begin(), candidates.end(), [this](const ProviderCandidate &left, const ProviderCandidate &right) {
                return scoreCandidate(m_query, left) > scoreCandidate(m_query, right);
            });
            int requested = 0;
            for (int index = 0; index < candidates.size() && requested < kDownloadCandidateLimit; ++index) {
                const ProviderCandidate &candidate = candidates.at(index);
                const CandidateEvaluation evaluation = evaluateCandidate(m_query, candidate);
                debugCandidate(candidate, evaluation, evaluation.accepted ? QStringLiteral("download") : QStringLiteral("skip"));
                if (!evaluation.accepted) continue;
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("ver"), QStringLiteral("1"));
                query.addQueryItem(QStringLiteral("man"), QStringLiteral("yes"));
                query.addQueryItem(QStringLiteral("client"), QStringLiteral("pc"));
                query.addQueryItem(QStringLiteral("keyword"), makeSearchQuery({candidate.title, candidate.artist, QString(), 0}));
                query.addQueryItem(QStringLiteral("duration"), QString::number(qMax(1, candidate.durationMs)));
                query.addQueryItem(QStringLiteral("hash"), candidate.syncedLyrics);
                QNetworkReply *next = get(withQuery(QStringLiteral("https://lyrics.kugou.com/search"), query), provider, QStringLiteral("kugou-lyric-search"));
                copyCandidateMetadata(next, candidate);
                requested++;
            }
        } else if (stage == QLatin1String("kugou-lyric-search")) {
            const QList<QJsonObject> candidates = parseKugouLyricSearchJson(body);
            if (!candidates.isEmpty()) {
                const QJsonObject first = candidates.first();
                QUrlQuery query;
                query.addQueryItem(QStringLiteral("ver"), QStringLiteral("1"));
                query.addQueryItem(QStringLiteral("client"), QStringLiteral("pc"));
                query.addQueryItem(QStringLiteral("id"), QString::number(static_cast<qint64>(first.value(QStringLiteral("id")).toDouble())));
                query.addQueryItem(QStringLiteral("accesskey"), first.value(QStringLiteral("accesskey")).toString());
                query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("lrc"));
                query.addQueryItem(QStringLiteral("charset"), QStringLiteral("utf8"));
                QNetworkReply *next = get(withQuery(QStringLiteral("https://lyrics.kugou.com/download"), query), provider, QStringLiteral("kugou-download"));
                next->setProperty("candidateTitle", reply->property("candidateTitle"));
                next->setProperty("candidateArtist", reply->property("candidateArtist"));
                next->setProperty("candidateAlbum", reply->property("candidateAlbum"));
                next->setProperty("candidateDurationMs", reply->property("candidateDurationMs"));
                next->setProperty("candidateMetadataTrusted", reply->property("candidateMetadataTrusted"));
            }
        } else if (stage == QLatin1String("kugou-download")) {
            considerCandidate(candidateFromReply(reply, parseKugouDownloadJson(body)));
        } else if (stage.startsWith(QStringLiteral("musixmatch"))) {
            for (ProviderCandidate candidate : parseMusixmatchJson(body, provider)) {
                candidate = candidateFromReply(reply, std::move(candidate));
                considerCandidate(candidate);
            }
        }
    }

    reply->deleteLater();
    maybeAdvance();
}

void LyricsLookupEngine::considerCandidate(ProviderCandidate candidate) {
    if (!m_running) return;

    const CandidateEvaluation evaluation = evaluateCandidate(m_query, candidate);
    debugCandidate(candidate, evaluation, evaluation.accepted ? QStringLiteral("candidate") : QStringLiteral("reject"));
    if (!evaluation.accepted) return;

    LyricDocument document = documentFromCandidate(candidate);
    if (document.isEmpty()) return;

    if (document.hasSyncedLines()) {
        if (evaluation.highConfidence) {
            finishWithDocument(std::move(document), QStringLiteral("synced"), true);
            return;
        }
        rememberSyncedDocument(std::move(document), evaluation.score);
        return;
    }

    if (document.hasPlainLines())
        rememberPlainDocument(std::move(document), evaluation.score);
}

void LyricsLookupEngine::rememberSyncedDocument(LyricDocument document, int score) {
    if (score <= m_bestSyncedScore) return;
    m_bestSyncedScore = score;
    m_bestSyncedDocument.clearAndFree();
    m_bestSyncedDocument = std::move(document);
}

void LyricsLookupEngine::rememberPlainDocument(LyricDocument document, int score) {
    if (score <= m_bestPlainScore) return;
    m_bestPlainScore = score;
    m_bestPlainDocument.clearAndFree();
    m_bestPlainDocument = std::move(document);
}

void LyricsLookupEngine::maybeAdvance() {
    if (!m_running || m_pendingReplies > 0) return;

    if (m_bestSyncedDocument.hasSyncedLines()) {
        finishWithDocument(std::move(m_bestSyncedDocument), QStringLiteral("synced"), true);
        return;
    }

    if (!m_fallbackStarted) {
        startFallbackProviders();
        return;
    }

    if (startNextFallbackProvider()) return;

    if (m_bestPlainDocument.hasPlainLines()) {
        finishWithDocument(std::move(m_bestPlainDocument), QStringLiteral("plain"), false);
        return;
    }

    finishNotFound();
}

void LyricsLookupEngine::finishWithDocument(LyricDocument document, const QString &status, bool finalSynced) {
    m_fallbackTimer.stop();
    abortNetwork();
    m_query = TrackQuery();
    m_providers.clear();
    m_bestSyncedDocument.clearAndFree();
    m_bestPlainDocument.clearAndFree();
    m_bestSyncedScore = 0;
    m_bestPlainScore = 0;
    m_nextFallbackProviderIndex = 0;
    m_running = false;
    m_fallbackStarted = false;
    m_fallbackProviders.clear();
    m_startedProviders.clear();
    emit documentReady(std::move(document), status, finalSynced);
}

void LyricsLookupEngine::finishNotFound() {
    m_fallbackTimer.stop();
    abortNetwork();
    clearState();
    emit notFound();
}

void LyricsLookupEngine::debugCandidate(const ProviderCandidate &candidate, const CandidateEvaluation &evaluation, const QString &action) const {
    if (!lyricsDebugEnabled()) return;

    QTextStream stream(stderr);
    stream << "[LyricsMatch] " << action
           << " provider=" << candidate.provider
           << " title=\"" << candidate.title << "\""
           << " artist=\"" << candidate.artist << "\""
           << " album=\"" << candidate.album << "\""
           << " durationMs=" << candidate.durationMs
           << " trusted=" << (candidate.metadataTrusted ? "true" : "false")
           << " score=" << evaluation.score
           << " accepted=" << (evaluation.accepted ? "true" : "false")
           << " high=" << (evaluation.highConfidence ? "true" : "false")
           << " reason=" << evaluation.reason;
    if (!evaluation.lyricMetadata.isEmpty()) {
        stream << " lrcTitle=\"" << evaluation.lyricMetadata.title << "\""
               << " lrcArtist=\"" << evaluation.lyricMetadata.artist << "\"";
    }
    stream << Qt::endl;
}

} // namespace lyricsmpris
