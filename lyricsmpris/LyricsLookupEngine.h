#pragma once

#include "LyricsCore.h"

#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace lyricsmpris {

QStringList defaultLyricProviders();
QStringList supportedLyricProviders();
QStringList normalizeLyricProviders(QStringList providers);

class LyricsLookupEngine final : public QObject {
    Q_OBJECT

public:
    explicit LyricsLookupEngine(QObject *parent = nullptr);

    void start(const TrackQuery &query, const QStringList &providers);
    void stop();

signals:
    void documentReady(lyricsmpris::LyricDocument document, QString status, bool finalSynced);
    void notFound();

private slots:
    void startFallbackProviders();
    void handleNetworkFinished();

private:
    void clearState();
    void abortNetwork();
    bool startProvider(const QString &provider);
    bool startNextFallbackProvider();
    bool startLrclib();
    bool startLrcx();
    bool startLyricsOvh();
    bool startNetease();
    bool startQq();
    bool startKugou();
    bool startMusixmatch();
    QNetworkReply *get(const QUrl &url, const QString &provider, const QString &stage);
    void copyCandidateMetadata(QNetworkReply *reply, const ProviderCandidate &candidate);
    ProviderCandidate candidateFromReply(QNetworkReply *reply, ProviderCandidate candidate) const;
    void considerCandidate(ProviderCandidate candidate);
    void rememberSyncedDocument(LyricDocument document, int score);
    void rememberPlainDocument(LyricDocument document, int score);
    void maybeAdvance();
    void finishWithDocument(LyricDocument document, const QString &status, bool finalSynced);
    void finishNotFound();
    void debugCandidate(const ProviderCandidate &candidate, const CandidateEvaluation &evaluation, const QString &action) const;

    QNetworkAccessManager m_network;
    QTimer m_fallbackTimer;
    TrackQuery m_query;
    QStringList m_providers;
    QStringList m_fallbackProviders;
    QSet<QString> m_startedProviders;
    QList<QNetworkReply *> m_replies;
    LyricDocument m_bestSyncedDocument;
    LyricDocument m_bestPlainDocument;
    int m_bestSyncedScore = 0;
    int m_bestPlainScore = 0;
    int m_nextFallbackProviderIndex = 0;
    int m_generation = 0;
    int m_pendingReplies = 0;
    bool m_running = false;
    bool m_fallbackStarted = false;
};

} // namespace lyricsmpris
