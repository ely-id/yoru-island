#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

namespace lyricsmpris {

struct TrackQuery {
    QString title;
    QString artist;
    QString album;
    int durationMs = 0;
};

struct LyricLine {
    qint64 timeMs = 0;
    QString text;
};

struct LyricDocument {
    QVector<LyricLine> syncedLines;
    QStringList plainLines;
    QString provider;
    bool instrumental = false;

    bool hasSyncedLines() const;
    bool hasPlainLines() const;
    bool isEmpty() const;
    void clearAndFree();
};

struct ProviderCandidate {
    QString provider;
    QString title;
    QString artist;
    QString album;
    int durationMs = 0;
    QString syncedLyrics;
    QString plainLyrics;
    bool instrumental = false;
};

QString cleanLyricText(QString text);
QString normalizedTitle(QString title);
QString normalizedArtist(QString artist);
QStringList significantTokens(const QString &value);
LyricDocument parseLyrics(const QString &lyrics, const QString &provider = QString());
LyricDocument documentFromCandidate(const ProviderCandidate &candidate);
QString selectLineAt(const LyricDocument &document, qint64 positionMs);
int scoreCandidate(const TrackQuery &query, const ProviderCandidate &candidate);

QList<ProviderCandidate> parseLrclibJson(const QByteArray &payload);
QList<ProviderCandidate> parseLrcxJson(const QByteArray &payload);
QList<ProviderCandidate> parseNeteaseSearchJson(const QByteArray &payload);
ProviderCandidate parseNeteaseLyricJson(const QByteArray &payload);
QList<ProviderCandidate> parseQqSearchJson(const QByteArray &payload);
ProviderCandidate parseQqLyricJson(const QByteArray &payload);
QList<ProviderCandidate> parseKugouSongSearchJson(const QByteArray &payload);
QList<QJsonObject> parseKugouLyricSearchJson(const QByteArray &payload);
ProviderCandidate parseKugouDownloadJson(const QByteArray &payload);
QList<ProviderCandidate> parseMusixmatchJson(const QByteArray &payload, const QString &provider);

} // namespace lyricsmpris
