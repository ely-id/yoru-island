#include "LyricsMprisApp.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

using namespace lyricsmpris;

namespace {

QStringList splitCommaList(const QString &value) {
    QStringList result;
    for (QString item : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        item = item.trimmed().toLower();
        if (!item.isEmpty()) result.append(item);
    }
    return result;
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("lyricsmpris"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.1.0-cpp"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lightweight MPRIS lyrics helper for Tide Island"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("[PLAYER_SERVICE]"), QStringLiteral("Optional preferred MPRIS player service"));

    const QCommandLineOption pipeOption(QStringLiteral("pipe"), QStringLiteral("Write current lyric/status updates as JSONL to stdout"));
    const QCommandLineOption blockOption(QStringLiteral("block"), QStringLiteral("Comma-separated blocked MPRIS service name fragments"), QStringLiteral("SERVICE1,SERVICE2"));
    const QCommandLineOption providersOption(QStringLiteral("providers"), QStringLiteral("Comma-separated lyric providers in preferred order"), QStringLiteral("PROVIDERS"));
    const QCommandLineOption databaseOption(QStringLiteral("database"), QStringLiteral("Accepted for compatibility; ignored because lyrics are not cached on disk"), QStringLiteral("DATABASE"));
    const QCommandLineOption noKaraokeOption(QStringLiteral("no-karaoke"), QStringLiteral("Accepted for compatibility; per-word karaoke rendering is not used in pipe mode"));
    const QCommandLineOption visibleLinesOption(QStringLiteral("visible-lines"), QStringLiteral("Accepted for compatibility; pipe mode emits one current line"), QStringLiteral("COUNT"));
    const QCommandLineOption listProvidersOption(QStringLiteral("list-providers"), QStringLiteral("Print supported provider names and exit"));

    parser.addOption(pipeOption);
    parser.addOption(blockOption);
    parser.addOption(providersOption);
    parser.addOption(databaseOption);
    parser.addOption(noKaraokeOption);
    parser.addOption(visibleLinesOption);
    parser.addOption(listProvidersOption);
    parser.process(app);

    if (parser.isSet(listProvidersOption)) {
        QTextStream(stdout) << "lrclib,lrcx,netease,qq,kugou,musixmatch\n";
        return 0;
    }

    AppOptions options;
    options.pipe = true;
    options.providers = parser.isSet(providersOption)
        ? splitCommaList(parser.value(providersOption))
        : QStringList();
    for (const QString &blocked : splitCommaList(parser.value(blockOption)))
        options.blockedServices.insert(blocked);

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) options.preferredService = positional.first();

    LyricsMprisApp controller(std::move(options));
    QTimer::singleShot(0, &controller, &LyricsMprisApp::start);
    return app.exec();
}
