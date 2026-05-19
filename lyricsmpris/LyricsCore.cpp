#include "LyricsCore.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <algorithm>

namespace lyricsmpris {
namespace {

constexpr double kArtistSimilarityThreshold = 0.60;
constexpr int kAcceptedScore = 72;
constexpr int kAcceptedPlainScore = 68;
constexpr int kHighConfidenceScore = 92;
constexpr double kStrictTitleSimilarityThreshold = 0.90;

QString stringValue(const QJsonObject &object, const QString &key) {
    const QJsonValue value = object.value(key);
    if (value.isString()) return value.toString();
    if (value.isDouble()) return QString::number(static_cast<qint64>(value.toDouble()));
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

bool isPlaceholderLyricText(QString text) {
    text = text.toLower();
    text.remove(QRegularExpression(QStringLiteral(R"(\s+)")));
    return text == QStringLiteral("暂无歌词")
        || text == QStringLiteral("暂无")
        || text.contains(QStringLiteral("纯音乐"))
        || text.contains(QStringLiteral("没有填词"))
        || text.contains(QStringLiteral("instrumental"));
}

bool isCjk(QChar character) {
    const uint code = character.unicode();
    return (code >= 0x3400 && code <= 0x4dbf)
        || (code >= 0x4e00 && code <= 0x9fff)
        || (code >= 0xf900 && code <= 0xfaff);
}

bool hasCjk(const QString &value) {
    for (QChar character : value) {
        if (isCjk(character)) return true;
    }
    return false;
}

bool hasLatin(const QString &value) {
    for (QChar character : value) {
        if (character.script() == QChar::Script_Latin) return true;
    }
    return false;
}

QStringList comparisonUnits(const QString &normalizedValue) {
    QStringList units;
    const QStringList words = normalizedValue.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &word : words) {
        if (word == QLatin1String("the")
            || word == QLatin1String("and")
            || word == QLatin1String("a")
            || word == QLatin1String("an")) {
            continue;
        }

        QString latinRun;
        auto flushLatinRun = [&units, &latinRun]() {
            if (!latinRun.isEmpty()) {
                units.append(latinRun);
                latinRun.clear();
            }
        };

        for (QChar character : word) {
            if (isCjk(character)) {
                flushLatinRun();
                units.append(QString(character));
            } else if (character.isLetterOrNumber()) {
                latinRun.append(character);
            } else {
                flushLatinRun();
            }
        }
        flushLatinRun();
    }

    units.removeDuplicates();
    return units;
}

double unitSimilarity(const QString &left, const QString &right, bool titleMode) {
    const QString normalizedLeft = titleMode ? normalizedTitle(left) : normalizedArtist(left);
    const QString normalizedRight = titleMode ? normalizedTitle(right) : normalizedArtist(right);
    if (normalizedLeft.isEmpty() || normalizedRight.isEmpty()) return 0.0;
    if (normalizedLeft == normalizedRight) return 1.0;

    const QStringList leftUnits = comparisonUnits(normalizedLeft);
    const QStringList rightUnits = comparisonUnits(normalizedRight);
    if (leftUnits.isEmpty() || rightUnits.isEmpty()) return 0.0;

    int common = 0;
    for (const QString &unit : leftUnits) {
        if (rightUnits.contains(unit)) common++;
    }

    return (2.0 * common) / double(leftUnits.size() + rightUnits.size());
}

bool unitsContainAll(const QString &needle, const QString &haystack, bool titleMode) {
    const QString normalizedNeedle = titleMode ? normalizedTitle(needle) : normalizedArtist(needle);
    const QString normalizedHaystack = titleMode ? normalizedTitle(haystack) : normalizedArtist(haystack);
    const QStringList needleUnits = comparisonUnits(normalizedNeedle);
    const QStringList haystackUnits = comparisonUnits(normalizedHaystack);
    if (needleUnits.isEmpty() || haystackUnits.isEmpty()) return false;

    for (const QString &unit : needleUnits) {
        if (!haystackUnits.contains(unit)) return false;
    }
    return true;
}

bool isShortLatinArtistAlias(const QString &artist) {
    const QStringList units = comparisonUnits(normalizedArtist(artist));
    if (units.size() != 1) return false;
    const QString unit = units.first();
    if (unit.size() > 4) return false;
    for (QChar character : unit) {
        if (character.script() != QChar::Script_Latin) return false;
    }
    return true;
}

bool artistContainsShortLatinAlias(const QString &alias, const QString &artist) {
    const QStringList aliasUnits = comparisonUnits(normalizedArtist(alias));
    if (aliasUnits.size() != 1) return false;

    const QString aliasUnit = aliasUnits.first();
    if (comparisonUnits(normalizedArtist(artist)).contains(aliasUnit)) return true;

    QString compactArtist;
    for (QChar character : normalizedArtist(artist)) {
        if (character.script() == QChar::Script_Latin || character.isDigit())
            compactArtist.append(character);
    }
    return !compactArtist.isEmpty() && compactArtist.contains(aliasUnit);
}

bool sameComparableScript(const QString &left, const QString &right) {
    const QString normalizedLeft = left.toLower();
    const QString normalizedRight = right.toLower();
    return (hasCjk(normalizedLeft) && hasCjk(normalizedRight))
        || (hasLatin(normalizedLeft) && hasLatin(normalizedRight));
}

QString lyricTextForMetadata(const ProviderCandidate &candidate) {
    return (candidate.syncedLyrics + QLatin1Char('\n') + candidate.plainLyrics).trimmed();
}

int pointsForSimilarity(double similarity, int maxPoints) {
    return int(similarity * maxPoints + 0.5);
}

QString reasonWithScore(const QString &reason, int score) {
    return reason + QStringLiteral(":") + QString::number(score);
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

    if (titleMode)
        value.remove(QRegularExpression(QStringLiteral(R"(^\s*\d{1,3}\s*[\.\-_\)]\s*)")));

    const QRegularExpression featuringBrackets(
        QStringLiteral(R"((\(|\[|\{)[^\)\]\}]*\b(feat|ft\.?|featuring)\b[^\)\]\}]*(\)|\]|\}))"),
        QRegularExpression::CaseInsensitiveOption);
    value.remove(featuringBrackets);

    const QRegularExpression featuring(
        QStringLiteral(R"(\b(feat\.?|featuring|ft\.?)\b.*$)"),
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

bool LyricMetadata::isEmpty() const {
    return title.trimmed().isEmpty() && artist.trimmed().isEmpty() && album.trimmed().isEmpty();
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
    QStringList tokens = comparisonUnits(value);
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const QString &token) {
        return token == QLatin1String("the")
            || token == QLatin1String("and")
            || token == QLatin1String("a")
            || token == QLatin1String("an");
    }), tokens.end());
    tokens.removeDuplicates();
    return tokens;
}

LyricMetadata lyricMetadataFromText(const QString &lyrics) {
    LyricMetadata metadata;
    QString source = lyrics;
    source.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    source.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const QRegularExpression metadataExpression(
        QStringLiteral(R"(^\s*\[([a-zA-Z][a-zA-Z0-9_+\-]*)\s*:\s*([^\]]*)\]\s*$)"));
    const QStringList rows = source.split(QLatin1Char('\n'));
    for (const QString &rawRow : rows) {
        const QRegularExpressionMatch match = metadataExpression.match(rawRow.trimmed());
        if (!match.hasMatch()) continue;

        const QString key = match.captured(1).toLower();
        const QString value = cleanLyricText(match.captured(2));
        if (value.isEmpty()) continue;

        if (key == QLatin1String("ti") && metadata.title.isEmpty()) metadata.title = value;
        else if (key == QLatin1String("ar") && metadata.artist.isEmpty()) metadata.artist = value;
        else if (key == QLatin1String("al") && metadata.album.isEmpty()) metadata.album = value;
    }
    return metadata;
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
            if (isPlaceholderLyricText(text)) continue;
            for (qint64 timestamp : timestamps) document.syncedLines.append({timestamp, text});
            continue;
        }

        const QString plain = cleanLyricText(row);
        if (!plain.isEmpty() && !isPlaceholderLyricText(plain)) document.plainLines.append(plain);
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

CandidateEvaluation evaluateCandidate(const TrackQuery &query, const ProviderCandidate &candidate) {
    CandidateEvaluation evaluation;
    evaluation.lyricMetadata = lyricMetadataFromText(lyricTextForMetadata(candidate));

    if (normalizedTitle(query.title).isEmpty()) {
        evaluation.reason = QStringLiteral("missing_query_title");
        return evaluation;
    }

    double bestTitleSimilarity = 0.0;
    bool hasTitleEvidence = false;
    if (candidate.metadataTrusted && !candidate.title.trimmed().isEmpty()) {
        const double similarity = unitSimilarity(query.title, candidate.title, true);
        if (similarity < kStrictTitleSimilarityThreshold) {
            evaluation.reason = QStringLiteral("title_mismatch");
            return evaluation;
        }
        bestTitleSimilarity = std::max(bestTitleSimilarity, similarity);
        hasTitleEvidence = true;
    }
    if (!evaluation.lyricMetadata.title.trimmed().isEmpty()) {
        const double similarity = unitSimilarity(query.title, evaluation.lyricMetadata.title, true);
        if (similarity < kStrictTitleSimilarityThreshold) {
            evaluation.reason = QStringLiteral("lyric_title_mismatch");
            return evaluation;
        }
        bestTitleSimilarity = std::max(bestTitleSimilarity, similarity);
        hasTitleEvidence = true;
    }
    if (!hasTitleEvidence) {
        evaluation.reason = candidate.metadataTrusted
            ? QStringLiteral("missing_candidate_title")
            : QStringLiteral("untrusted_metadata");
        return evaluation;
    }

    double bestArtistSimilarity = 0.0;
    bool hasArtistEvidence = false;
    if (!query.artist.trimmed().isEmpty()) {
        const bool queryIsShortLatinAlias = isShortLatinArtistAlias(query.artist);
        bool shortLatinAliasSeen = false;
        if (candidate.metadataTrusted && !candidate.artist.trimmed().isEmpty()) {
            double similarity = unitSimilarity(query.artist, candidate.artist, false);
            const bool listedArtist = unitsContainAll(query.artist, candidate.artist, false);
            shortLatinAliasSeen = shortLatinAliasSeen || artistContainsShortLatinAlias(query.artist, candidate.artist);
            if (similarity < kArtistSimilarityThreshold
                && !listedArtist
                && (sameComparableScript(query.artist, candidate.artist) || queryIsShortLatinAlias)) {
                evaluation.reason = QStringLiteral("artist_mismatch");
                return evaluation;
            }
            if (listedArtist) similarity = std::max(similarity, 0.92);
            bestArtistSimilarity = std::max(bestArtistSimilarity, similarity);
            hasArtistEvidence = true;
        }
        if (!evaluation.lyricMetadata.artist.trimmed().isEmpty()) {
            double similarity = unitSimilarity(query.artist, evaluation.lyricMetadata.artist, false);
            const bool listedArtist = unitsContainAll(query.artist, evaluation.lyricMetadata.artist, false);
            shortLatinAliasSeen = shortLatinAliasSeen || artistContainsShortLatinAlias(query.artist, evaluation.lyricMetadata.artist);
            if (similarity < kArtistSimilarityThreshold
                && !listedArtist
                && (sameComparableScript(query.artist, evaluation.lyricMetadata.artist) || queryIsShortLatinAlias)) {
                evaluation.reason = QStringLiteral("lyric_artist_mismatch");
                return evaluation;
            }
            if (listedArtist) similarity = std::max(similarity, 0.92);
            bestArtistSimilarity = std::max(bestArtistSimilarity, similarity);
            hasArtistEvidence = true;
        }
        if (queryIsShortLatinAlias && !shortLatinAliasSeen) {
            evaluation.reason = QStringLiteral("missing_artist_alias");
            return evaluation;
        }
    }

    evaluation.score += pointsForSimilarity(bestTitleSimilarity, 60);
    if (hasArtistEvidence) evaluation.score += pointsForSimilarity(bestArtistSimilarity, 25);

    double bestAlbumSimilarity = 0.0;
    if (!query.album.trimmed().isEmpty()) {
        if (candidate.metadataTrusted && !candidate.album.trimmed().isEmpty())
            bestAlbumSimilarity = std::max(bestAlbumSimilarity, unitSimilarity(query.album, candidate.album, true));
        if (!evaluation.lyricMetadata.album.trimmed().isEmpty())
            bestAlbumSimilarity = std::max(bestAlbumSimilarity, unitSimilarity(query.album, evaluation.lyricMetadata.album, true));
        if (bestAlbumSimilarity >= 0.60) evaluation.score += pointsForSimilarity(bestAlbumSimilarity, 8);
    }

    if (query.durationMs > 0) {
        if (candidate.durationMs > 0) {
            const int delta = std::abs(query.durationMs - candidate.durationMs);
            const int tolerance = std::max(15000, int(query.durationMs * 0.08));
            if (delta > tolerance) {
                evaluation.reason = QStringLiteral("duration_mismatch");
                return evaluation;
            }

            if (delta <= 2000) evaluation.score += 20;
            else if (delta <= 5000) evaluation.score += 15;
            else if (delta <= 10000) evaluation.score += 10;
            else evaluation.score += 5;
        } else if (query.durationMs < 90000) {
            evaluation.reason = QStringLiteral("missing_candidate_duration");
            return evaluation;
        }
    }

    if (!candidate.syncedLyrics.isEmpty()) evaluation.score += 8;
    else if (!candidate.plainLyrics.isEmpty()) evaluation.score += 2;
    if (candidate.instrumental) evaluation.score += 4;

    const int acceptScore = !candidate.syncedLyrics.isEmpty() || candidate.instrumental
        ? kAcceptedScore
        : kAcceptedPlainScore;
    evaluation.accepted = evaluation.score >= acceptScore;
    evaluation.highConfidence = evaluation.accepted
        && evaluation.score >= kHighConfidenceScore
        && bestTitleSimilarity >= 0.92
        && (query.artist.trimmed().isEmpty() || bestArtistSimilarity >= 0.90)
        && (query.durationMs <= 0 || candidate.durationMs > 0);
    evaluation.reason = evaluation.accepted
        ? QStringLiteral("accepted")
        : reasonWithScore(QStringLiteral("score_below_threshold"), evaluation.score);
    return evaluation;
}

int scoreCandidate(const TrackQuery &query, const ProviderCandidate &candidate) {
    return evaluateCandidate(query, candidate).score;
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
        if (candidate.artist.isEmpty()) candidate.artist = artistsFromArray(song.value(QStringLiteral("ar")).toArray());
        candidate.album = stringValue(song.value(QStringLiteral("album")).toObject(), QStringLiteral("name"));
        if (candidate.album.isEmpty()) candidate.album = stringValue(song.value(QStringLiteral("al")).toObject(), QStringLiteral("name"));
        candidate.durationMs = intValue(song, QStringLiteral("duration"));
        if (candidate.durationMs <= 0) candidate.durationMs = intValue(song, QStringLiteral("dt"));
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

ProviderCandidate parseLyricsOvhJson(const QByteArray &payload) {
    ProviderCandidate candidate;
    candidate.provider = QStringLiteral("lyricsovh");
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return candidate;

    candidate.plainLyrics = stringValue(document.object(), QStringLiteral("lyrics"));
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
    candidate.metadataTrusted = false;

    const QJsonObject subtitle = body.value(QStringLiteral("subtitle")).toObject();
    const QJsonObject lyrics = body.value(QStringLiteral("lyrics")).toObject();
    candidate.syncedLyrics = stringValue(subtitle, QStringLiteral("subtitle_body"));
    candidate.plainLyrics = stringValue(lyrics, QStringLiteral("lyrics_body"));
    return candidate.syncedLyrics.isEmpty() && candidate.plainLyrics.isEmpty()
        ? QList<ProviderCandidate>()
        : QList<ProviderCandidate>({candidate});
}

} // namespace lyricsmpris
