#include "LyricsCore.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <algorithm>

namespace lyricsmpris {
namespace {

QString stringValue(const QJsonObject &object, const QString &key) {
    const QJsonValue value = object.value(key);
    if (value.isString()) return value.toString();
    if (value.isDouble()) return QString::number(value.toInt());
    return QString();
}

int intValue(const QJsonObject &object, const QString &key) {
    const QJsonValue value = object.value(key);
    if (value.isDouble()) return value.toInt();
    if (value.isString()) return value.toString().toInt();
    return 0;
}

QString artistsFromArray(const QJsonArray &array) {
    QStringList artists;
    for (const QJsonValue &value : array) {
        if (value.isString()) {
            artists.append(value.toString());
        } else if (value.isObject()) {
            const QJsonObject object = value.toObject();
            const QString name = stringValue(object, QStringLiteral("name"));
            if (!name.isEmpty()) artists.append(name);
        }
    }
    return artists.join(QStringLiteral(", "));
}

QString firstNonEmpty(const QStringList &values) {
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty()) return value;
    }
    return QString();
}

QList<ProviderCandidate> parseLrclibObject(const QJsonObject &object) {
    ProviderCandidate candidate;
    candidate.provider = QStringLiteral("lrclib");
    candidate.title = firstNonEmpty({
        stringValue(object, QStringLiteral("trackName")),
        stringValue(object, QStringLiteral("name"))
    });
    candidate.artist = firstNonEmpty({
        stringValue(object, QStringLiteral("artistName")),
        stringValue(object, QStringLiteral("artist"))
    });
    candidate.album = firstNonEmpty({
        stringValue(object, QStringLiteral("albumName")),
        stringValue(object, QStringLiteral("album"))
    });
    const int durationSeconds = intValue(object, QStringLiteral("duration"));
    candidate.durationMs = durationSeconds > 0 ? durationSeconds * 1000 : 0;
    candidate.syncedLyrics = stringValue(object, QStringLiteral("syncedLyrics"));
    candidate.plainLyrics = stringValue(object, QStringLiteral("plainLyrics"));
    candidate.instrumental = object.value(QStringLiteral("instrumental")).toBool(false);
    return candidate.syncedLyrics.isEmpty() && candidate.plainLyrics.isEmpty() && !candidate.instrumental
        ? QList<ProviderCandidate>()
        : QList<ProviderCandidate>({candidate});
}

QList<ProviderCandidate> parseLrcxObject(const QJsonObject &object) {
    ProviderCandidate candidate;
    candidate.provider = QStringLiteral("lrcx");
    candidate.title = firstNonEmpty({
        stringValue(object, QStringLiteral("title")),
        stringValue(object, QStringLiteral("trackName")),
        stringValue(object, QStringLiteral("name"))
    });
    candidate.artist = firstNonEmpty({
        stringValue(object, QStringLiteral("artist")),
        stringValue(object, QStringLiteral("artistName"))
    });
    candidate.album = firstNonEmpty({
        stringValue(object, QStringLiteral("album")),
        stringValue(object, QStringLiteral("albumName"))
    });
    const int duration = intValue(object, QStringLiteral("duration"));
    candidate.durationMs = duration > 0 && duration < 10000 ? duration * 1000 : duration;
    candidate.syncedLyrics = firstNonEmpty({
        stringValue(object, QStringLiteral("lyrics")),
        stringValue(object, QStringLiteral("lyric")),
        stringValue(object, QStringLiteral("lrc")),
        stringValue(object, QStringLiteral("syncedLyrics"))
    });
    candidate.plainLyrics = stringValue(object, QStringLiteral("plainLyrics"));
    return candidate.syncedLyrics.isEmpty() && candidate.plainLyrics.isEmpty()
        ? QList<ProviderCandidate>()
        : QList<ProviderCandidate>({candidate});
}

QString normalizeCommon(QString value, bool titleMode) {
    value = value.toLower();
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    value.replace(QStringLiteral("&#39;"), QStringLiteral("'"));

    const QRegularExpression noisyBrackets(
        QStringLiteral(R"((\(|\[|\{)[^\)\]\}]*\b(feat|ft\.?|featuring|with|remaster(?:ed)?|live|mono|stereo|explicit|clean|radio|edit|version|official|audio|video|lyrics?|mv|hd)\b[^\)\]\}]*(\)|\]|\}))"),
        QRegularExpression::CaseInsensitiveOption);
    value.remove(noisyBrackets);

    if (titleMode) {
        const QRegularExpression suffixNoise(
            QStringLiteral(R"(\s+-\s+.*\b(remaster(?:ed)?|live|radio|edit|version|official|audio|video|lyrics?|mv|hd)\b.*$)"),
            QRegularExpression::CaseInsensitiveOption);
        value.remove(suffixNoise);
    }

    const QRegularExpression featuring(
        QStringLiteral(R"(\b(feat\.?|featuring|ft\.?|with)\b.*$)"),
        QRegularExpression::CaseInsensitiveOption);
    value.remove(featuring);
    value.replace(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N}]+)")), QStringLiteral(" "));
    value.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return value.trimmed();
}

QString jsonLyricValue(const QJsonObject &object) {
    return firstNonEmpty({
        stringValue(object, QStringLiteral("lyric")),
        stringValue(object, QStringLiteral("lyrics")),
        stringValue(object, QStringLiteral("content")),
        stringValue(object, QStringLiteral("lrc")),
        stringValue(object, QStringLiteral("qrc"))
    });
}

} // namespace

bool LyricDocument::hasSyncedLines() const {
    return !syncedLines.isEmpty();
}

bool LyricDocument::hasPlainLines() const {
    return !plainLines.isEmpty();
}

bool LyricDocument::isEmpty() const {
    return syncedLines.isEmpty() && plainLines.isEmpty() && !instrumental;
}

void LyricDocument::clearAndFree() {
    QVector<LyricLine>().swap(syncedLines);
    QStringList().swap(plainLines);
    provider.clear();
    provider.squeeze();
    instrumental = false;
}

QString cleanLyricText(QString text) {
    text.remove(QRegularExpression(QStringLiteral(R"(<\d+,\d+(?:,\d+)?>)")));
    text.remove(QRegularExpression(QStringLiteral(R"(\[\d+,\d+(?:,\d+)?\])")));
    text.remove(QRegularExpression(QStringLiteral(R"(<\/?\w+[^>]*>)")));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return text.trimmed();
}

QString normalizedTitle(QString title) {
    return normalizeCommon(std::move(title), true);
}

QString normalizedArtist(QString artist) {
    return normalizeCommon(std::move(artist), false);
}

QStringList significantTokens(const QString &value) {
    QStringList tokens = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const QString &token) {
        return token.size() < 2
            || token == QLatin1String("the")
            || token == QLatin1String("and")
            || token == QLatin1String("a")
            || token == QLatin1String("an");
    }), tokens.end());
    tokens.removeDuplicates();
    return tokens;
}

LyricDocument parseLyrics(const QString &lyrics, const QString &provider) {
    LyricDocument document;
    document.provider = provider;

    QString source = lyrics;
    source.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    source.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const QRegularExpression timestampExpression(
        QStringLiteral(R"(\[(\d{1,3}):(\d{2})(?:[\.:](\d{1,3}))?\])"));
    const QRegularExpression metadataExpression(
        QStringLiteral(R"(^\[[a-zA-Z][a-zA-Z0-9_+\-]*:.*\]$)"));

    const QStringList rows = source.split(QLatin1Char('\n'));
    for (const QString &rawRow : rows) {
        const QString row = rawRow.trimmed();
        if (row.isEmpty()) continue;
        if (metadataExpression.match(row).hasMatch()) continue;

        QVector<qint64> timestamps;
        QRegularExpressionMatchIterator iterator = timestampExpression.globalMatch(row);
        while (iterator.hasNext()) {
            const QRegularExpressionMatch match = iterator.next();
            const int minutes = match.captured(1).toInt();
            const int seconds = match.captured(2).toInt();
            QString fractionText = match.captured(3);
            int milliseconds = 0;
            if (!fractionText.isEmpty()) {
                if (fractionText.size() == 1) milliseconds = fractionText.toInt() * 100;
                else if (fractionText.size() == 2) milliseconds = fractionText.toInt() * 10;
                else milliseconds = fractionText.left(3).toInt();
            }
            timestamps.append((minutes * 60 + seconds) * 1000LL + milliseconds);
        }

        if (!timestamps.isEmpty()) {
            QString text = row;
            text.remove(timestampExpression);
            text = cleanLyricText(text);
            for (qint64 timestamp : timestamps) document.syncedLines.append({timestamp, text});
            continue;
        }

        const QString plain = cleanLyricText(row);
        if (!plain.isEmpty()) document.plainLines.append(plain);
    }

    std::stable_sort(document.syncedLines.begin(), document.syncedLines.end(), [](const LyricLine &left, const LyricLine &right) {
        return left.timeMs < right.timeMs;
    });
    return document;
}

LyricDocument documentFromCandidate(const ProviderCandidate &candidate) {
    LyricDocument document = parseLyrics(
        !candidate.syncedLyrics.isEmpty() ? candidate.syncedLyrics : candidate.plainLyrics,
        candidate.provider);
    document.instrumental = candidate.instrumental;
    if (!candidate.plainLyrics.isEmpty() && document.plainLines.isEmpty() && document.syncedLines.isEmpty()) {
        document = parseLyrics(candidate.plainLyrics, candidate.provider);
        document.instrumental = candidate.instrumental;
    }
    return document;
}

QString selectLineAt(const LyricDocument &document, qint64 positionMs) {
    if (!document.syncedLines.isEmpty()) {
        if (positionMs < document.syncedLines.first().timeMs) return QString();

        int low = 0;
        int high = document.syncedLines.size() - 1;
        while (low <= high) {
            const int mid = low + (high - low) / 2;
            if (document.syncedLines.at(mid).timeMs <= positionMs) low = mid + 1;
            else high = mid - 1;
        }
        return high >= 0 ? document.syncedLines.at(high).text : QString();
    }

    return document.plainLines.isEmpty() ? QString() : document.plainLines.first();
}

int scoreCandidate(const TrackQuery &query, const ProviderCandidate &candidate) {
    const QString queryTitle = normalizedTitle(query.title);
    const QString candidateTitle = normalizedTitle(candidate.title);
    if (queryTitle.isEmpty()) return 0;

    int score = 0;
    if (!candidateTitle.isEmpty()) {
        if (candidateTitle == queryTitle) score += 60;
        else if (candidateTitle.contains(queryTitle) || queryTitle.contains(candidateTitle)) score += 44;
        else {
            const QStringList queryTokens = significantTokens(queryTitle);
            const QStringList candidateTokens = significantTokens(candidateTitle);
            int common = 0;
            for (const QString &token : queryTokens) {
                if (candidateTokens.contains(token)) common++;
            }
            if (!queryTokens.isEmpty()) score += (common * 38) / queryTokens.size();
        }
    } else {
        score += 35;
    }

    const QString queryArtist = normalizedArtist(query.artist);
    const QString candidateArtist = normalizedArtist(candidate.artist);
    if (!queryArtist.isEmpty() && !candidateArtist.isEmpty()) {
        if (candidateArtist == queryArtist) score += 25;
        else if (candidateArtist.contains(queryArtist) || queryArtist.contains(candidateArtist)) score += 20;
        else {
            const QStringList queryTokens = significantTokens(queryArtist);
            const QStringList candidateTokens = significantTokens(candidateArtist);
            int common = 0;
            for (const QString &token : queryTokens) {
                if (candidateTokens.contains(token)) common++;
            }
            if (!queryTokens.isEmpty()) score += (common * 18) / queryTokens.size();
        }
    } else if (queryArtist.isEmpty() || candidateArtist.isEmpty()) {
        score += 6;
    }

    const QString queryAlbum = normalizedTitle(query.album);
    const QString candidateAlbum = normalizedTitle(candidate.album);
    if (!queryAlbum.isEmpty() && !candidateAlbum.isEmpty()) {
        if (candidateAlbum == queryAlbum) score += 8;
        else if (candidateAlbum.contains(queryAlbum) || queryAlbum.contains(candidateAlbum)) score += 5;
    }

    if (query.durationMs > 0 && candidate.durationMs > 0) {
        const int delta = std::abs(query.durationMs - candidate.durationMs);
        if (delta <= 2000) score += 20;
        else if (delta <= 5000) score += 14;
        else if (delta <= 10000) score += 8;
        else if (delta <= 20000) score += 3;
    } else {
        score += 3;
    }

    if (!candidate.syncedLyrics.isEmpty()) score += 10;
    else if (!candidate.plainLyrics.isEmpty()) score += 3;
    if (candidate.instrumental) score += 5;
    return score;
}

QList<ProviderCandidate> parseLrclibJson(const QByteArray &payload) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError) return {};

    QList<ProviderCandidate> candidates;
    if (document.isObject()) return parseLrclibObject(document.object());
    if (!document.isArray()) return {};

    const QJsonArray array = document.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;
        candidates.append(parseLrclibObject(value.toObject()));
    }
    return candidates;
}

QList<ProviderCandidate> parseLrcxJson(const QByteArray &payload) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError) return {};

    QList<ProviderCandidate> candidates;
    if (document.isObject()) return parseLrcxObject(document.object());
    if (!document.isArray()) return {};

    const QJsonArray array = document.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;
        candidates.append(parseLrcxObject(value.toObject()));
    }
    return candidates;
}

QList<ProviderCandidate> parseNeteaseSearchJson(const QByteArray &payload) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};

    const QJsonObject result = document.object().value(QStringLiteral("result")).toObject();
    const QJsonArray songs = result.value(QStringLiteral("songs")).toArray();
    QList<ProviderCandidate> candidates;
    for (const QJsonValue &value : songs) {
        const QJsonObject song = value.toObject();
        ProviderCandidate candidate;
        candidate.provider = QStringLiteral("netease");
        candidate.title = stringValue(song, QStringLiteral("name"));
        candidate.artist = artistsFromArray(song.value(QStringLiteral("artists")).toArray());
        candidate.album = stringValue(song.value(QStringLiteral("album")).toObject(), QStringLiteral("name"));
        candidate.durationMs = intValue(song, QStringLiteral("duration"));
        candidate.syncedLyrics = stringValue(song, QStringLiteral("id"));
        if (!candidate.title.isEmpty() && !candidate.syncedLyrics.isEmpty()) candidates.append(candidate);
    }
    return candidates;
}

ProviderCandidate parseNeteaseLyricJson(const QByteArray &payload) {
    ProviderCandidate candidate;
    candidate.provider = QStringLiteral("netease");
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return candidate;

    const QJsonObject object = document.object();
    candidate.syncedLyrics = stringValue(object.value(QStringLiteral("lrc")).toObject(), QStringLiteral("lyric"));
    if (candidate.syncedLyrics.isEmpty()) {
        candidate.plainLyrics = stringValue(object.value(QStringLiteral("tlyric")).toObject(), QStringLiteral("lyric"));
    }
    return candidate;
}

QList<ProviderCandidate> parseQqSearchJson(const QByteArray &payload) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};

    const QJsonArray songs = document.object()
        .value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("song")).toObject()
        .value(QStringLiteral("list")).toArray();

    QList<ProviderCandidate> candidates;
    for (const QJsonValue &value : songs) {
        const QJsonObject song = value.toObject();
        ProviderCandidate candidate;
        candidate.provider = QStringLiteral("qq");
        candidate.title = firstNonEmpty({
            stringValue(song, QStringLiteral("songname")),
            stringValue(song, QStringLiteral("title"))
        });
        candidate.artist = artistsFromArray(song.value(QStringLiteral("singer")).toArray());
        candidate.album = stringValue(song, QStringLiteral("albumname"));
        const int interval = intValue(song, QStringLiteral("interval"));
        candidate.durationMs = interval > 0 ? interval * 1000 : 0;
        candidate.syncedLyrics = firstNonEmpty({
            stringValue(song, QStringLiteral("songmid")),
            stringValue(song, QStringLiteral("mid"))
        });
        if (!candidate.title.isEmpty() && !candidate.syncedLyrics.isEmpty()) candidates.append(candidate);
    }
    return candidates;
}

ProviderCandidate parseQqLyricJson(const QByteArray &payload) {
    ProviderCandidate candidate;
    candidate.provider = QStringLiteral("qq");
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return candidate;

    const QJsonObject object = document.object();
    candidate.syncedLyrics = jsonLyricValue(object);
    if (candidate.syncedLyrics.isEmpty()) {
        const QString encoded = stringValue(object, QStringLiteral("lyric"));
        const QByteArray decoded = QByteArray::fromBase64(encoded.toUtf8());
        if (!decoded.isEmpty()) candidate.syncedLyrics = QString::fromUtf8(decoded);
    }
    return candidate;
}

QList<ProviderCandidate> parseKugouSongSearchJson(const QByteArray &payload) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};

    const QJsonArray songs = document.object()
        .value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("lists")).toArray();

    QList<ProviderCandidate> candidates;
    for (const QJsonValue &value : songs) {
        const QJsonObject song = value.toObject();
        ProviderCandidate candidate;
        candidate.provider = QStringLiteral("kugou");
        candidate.title = cleanLyricText(firstNonEmpty({
            stringValue(song, QStringLiteral("SongName")),
            stringValue(song, QStringLiteral("FileName"))
        }));
        candidate.artist = cleanLyricText(stringValue(song, QStringLiteral("SingerName")));
        candidate.album = cleanLyricText(stringValue(song, QStringLiteral("AlbumName")));
        const int durationSeconds = intValue(song, QStringLiteral("Duration"));
        candidate.durationMs = durationSeconds > 0 ? durationSeconds * 1000 : 0;
        candidate.syncedLyrics = firstNonEmpty({
            stringValue(song, QStringLiteral("FileHash")),
            stringValue(song, QStringLiteral("Hash"))
        });
        candidate.plainLyrics = stringValue(song, QStringLiteral("AlbumID"));
        if (!candidate.title.isEmpty() && !candidate.syncedLyrics.isEmpty()) candidates.append(candidate);
    }
    return candidates;
}

QList<QJsonObject> parseKugouLyricSearchJson(const QByteArray &payload) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const QJsonArray candidates = document.object().value(QStringLiteral("candidates")).toArray();
    QList<QJsonObject> objects;
    for (const QJsonValue &value : candidates) {
        if (value.isObject()) objects.append(value.toObject());
    }
    return objects;
}

ProviderCandidate parseKugouDownloadJson(const QByteArray &payload) {
    ProviderCandidate candidate;
    candidate.provider = QStringLiteral("kugou");
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return candidate;

    const QString content = stringValue(document.object(), QStringLiteral("content"));
    const QByteArray decoded = QByteArray::fromBase64(content.toUtf8());
    candidate.syncedLyrics = decoded.isEmpty() ? content : QString::fromUtf8(decoded);
    return candidate;
}

QList<ProviderCandidate> parseMusixmatchJson(const QByteArray &payload, const QString &provider) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};

    const QJsonObject body = document.object()
        .value(QStringLiteral("message")).toObject()
        .value(QStringLiteral("body")).toObject();
    ProviderCandidate candidate;
    candidate.provider = provider;

    const QJsonObject subtitle = body.value(QStringLiteral("subtitle")).toObject();
    const QJsonObject lyrics = body.value(QStringLiteral("lyrics")).toObject();
    candidate.syncedLyrics = stringValue(subtitle, QStringLiteral("subtitle_body"));
    candidate.plainLyrics = stringValue(lyrics, QStringLiteral("lyrics_body"));
    return candidate.syncedLyrics.isEmpty() && candidate.plainLyrics.isEmpty()
        ? QList<ProviderCandidate>()
        : QList<ProviderCandidate>({candidate});
}

} // namespace lyricsmpris
