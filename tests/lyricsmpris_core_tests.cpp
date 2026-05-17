#include "LyricsCore.h"

#include <QtTest/QtTest>

using namespace lyricsmpris;

class LyricsMprisCoreTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesSyncedLrc();
    void parsesMultiTimestampLines();
    void keepsPlainLyricsFallback();
    void normalizesNoisyTitles();
    void scoresLikelyMatches();
    void parsesProviderFixtures();
    void releasesDocumentStorage();
};

void LyricsMprisCoreTests::parsesSyncedLrc() {
    const LyricDocument document = parseLyrics("[ar:Artist]\n[00:01.50] First line\n[00:03.250]Second line\n", "test");
    QVERIFY(document.hasSyncedLines());
    QCOMPARE(document.syncedLines.size(), 2);
    QCOMPARE(document.syncedLines.at(0).timeMs, 1500);
    QCOMPARE(document.syncedLines.at(1).timeMs, 3250);
    QCOMPARE(selectLineAt(document, 1400), QString());
    QCOMPARE(selectLineAt(document, 2000), QString("First line"));
    QCOMPARE(selectLineAt(document, 4000), QString("Second line"));
}

void LyricsMprisCoreTests::parsesMultiTimestampLines() {
    const LyricDocument document = parseLyrics("[00:10.00][00:20.00]Chorus\n[00:30.0]\n", "test");
    QCOMPARE(document.syncedLines.size(), 3);
    QCOMPARE(document.syncedLines.at(0).timeMs, 10000);
    QCOMPARE(document.syncedLines.at(1).timeMs, 20000);
    QCOMPARE(document.syncedLines.at(2).timeMs, 30000);
    QCOMPARE(selectLineAt(document, 21000), QString("Chorus"));
    QCOMPARE(selectLineAt(document, 31000), QString());
}

void LyricsMprisCoreTests::keepsPlainLyricsFallback() {
    const LyricDocument document = parseLyrics("Line one\n\nLine two\n", "plain");
    QVERIFY(!document.hasSyncedLines());
    QCOMPARE(document.plainLines.size(), 2);
    QCOMPARE(selectLineAt(document, 0), QString("Line one"));
}

void LyricsMprisCoreTests::normalizesNoisyTitles() {
    QCOMPARE(normalizedTitle("Song Title (feat. Someone) - Remastered 2011"), QString("song title"));
    QCOMPARE(normalizedTitle("HELLO!!! [Official Lyric Video]"), QString("hello"));
    QCOMPARE(normalizedArtist("Artist feat. Guest"), QString("artist"));
}

void LyricsMprisCoreTests::scoresLikelyMatches() {
    TrackQuery query;
    query.title = "Song Title (Remastered)";
    query.artist = "Main Artist";
    query.album = "Album";
    query.durationMs = 180000;

    ProviderCandidate good;
    good.title = "Song Title";
    good.artist = "Main Artist";
    good.album = "Album";
    good.durationMs = 181000;
    good.syncedLyrics = "[00:01.00]Hello";

    ProviderCandidate bad = good;
    bad.title = "Different Song";
    bad.artist = "Other Artist";
    bad.durationMs = 240000;

    QVERIFY(scoreCandidate(query, good) >= 100);
    QVERIFY(scoreCandidate(query, bad) < scoreCandidate(query, good));
}

void LyricsMprisCoreTests::parsesProviderFixtures() {
    const QByteArray lrclib = R"([
        {"trackName":"Song","artistName":"Artist","albumName":"Album","duration":120,"syncedLyrics":"[00:01.00]Hi","plainLyrics":"Hi"}
    ])";
    QList<ProviderCandidate> candidates = parseLrclibJson(lrclib);
    QCOMPARE(candidates.size(), 1);
    QCOMPARE(candidates.first().provider, QString("lrclib"));
    QCOMPARE(candidates.first().durationMs, 120000);
    QVERIFY(documentFromCandidate(candidates.first()).hasSyncedLines());

    const QByteArray netease = R"({"lrc":{"lyric":"[00:02.00]Hello"}})";
    ProviderCandidate neteaseLyric = parseNeteaseLyricJson(netease);
    QCOMPARE(neteaseLyric.provider, QString("netease"));
    QVERIFY(documentFromCandidate(neteaseLyric).hasSyncedLines());

    const QByteArray kugou = R"({"content":"WzAwOjAxLjAwXUhvbGE="})";
    ProviderCandidate kugouLyric = parseKugouDownloadJson(kugou);
    QVERIFY(documentFromCandidate(kugouLyric).hasSyncedLines());
}

void LyricsMprisCoreTests::releasesDocumentStorage() {
    LyricDocument document = parseLyrics("[00:01.00]Hello\n[00:02.00]World\n", "test");
    QVERIFY(document.syncedLines.capacity() > 0);
    document.clearAndFree();
    QVERIFY(document.isEmpty());
    QCOMPARE(document.syncedLines.capacity(), qsizetype(0));
}

QTEST_MAIN(LyricsMprisCoreTests)
#include "lyricsmpris_core_tests.moc"
