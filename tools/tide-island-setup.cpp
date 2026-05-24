#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <termios.h>
#include <unistd.h>

namespace {
constexpr auto configDirName = "tide-island";
constexpr auto configFileName = "userconfig.json";
constexpr auto setupLockFileName = "setup-wizard.lock";
constexpr auto overviewBindCommand = "qs ipc -p /usr/share/tide-island call overview toggle";
constexpr auto overviewBindLine = "bind = SUPER, TAB, exec, qs ipc -p /usr/share/tide-island call overview toggle";
constexpr qint64 setupLockMaxAgeSeconds = 60 * 60;

QString glyph(char32_t codepoint)
{
    return QString::fromUcs4(&codepoint, 1);
}

QString envString(const char *name)
{
    return QString::fromLocal8Bit(qgetenv(name));
}

QString homePath()
{
    const QString home = envString("HOME");
    if (!home.isEmpty())
        return home;

    return QDir::homePath();
}

QString expandUserPath(QString path)
{
    if (path == "~")
        return homePath();
    if (path.startsWith("~/"))
        return homePath() + path.sliced(1);
    return path;
}

QString configHome()
{
    const QString override = envString("TIDE_ISLAND_CONFIG_HOME");
    if (!override.isEmpty())
        return expandUserPath(override);

    const QString xdgConfigHome = envString("XDG_CONFIG_HOME");
    if (!xdgConfigHome.isEmpty())
        return expandUserPath(xdgConfigHome);

    return homePath() + QStringLiteral("/.config");
}

QString userConfigPath()
{
    const QString override = envString("TIDE_ISLAND_USER_CONFIG");
    if (!override.isEmpty())
        return expandUserPath(override);

    return configHome() + QStringLiteral("/") + configDirName + QStringLiteral("/") + configFileName;
}

QString setupLockPath()
{
    return configHome() + QStringLiteral("/") + configDirName + QStringLiteral("/") + setupLockFileName;
}

QString hyprlandConfigPath()
{
    const QString override = envString("TIDE_ISLAND_HYPRLAND_CONFIG");
    if (!override.isEmpty())
        return expandUserPath(override);

    return configHome() + QStringLiteral("/hypr/hyprland.conf");
}

QJsonArray stringArray(std::initializer_list<QString> values)
{
    QJsonArray result;
    for (const QString &value : values)
        result.append(value);
    return result;
}

QJsonObject controlCenterIconsDefaults()
{
    return {
        {QStringLiteral("charging"), glyph(0xf0e7)},
        {QStringLiteral("brightness"), glyph(0xf00df)},
        {QStringLiteral("volume"), glyph(0xf057e)},
    };
}

QJsonObject statusIconsDefaults()
{
    return {
        {QStringLiteral("default"), glyph(0x1f3a7)},
        {QStringLiteral("notification"), glyph(0xf0f3)},
        {QStringLiteral("volume"), glyph(0xf057e)},
        {QStringLiteral("mute"), glyph(0xf075f)},
        {QStringLiteral("brightnessLow"), glyph(0xf00de)},
        {QStringLiteral("brightnessMedium"), glyph(0xf00df)},
        {QStringLiteral("brightnessHigh"), glyph(0xf00e0)},
        {QStringLiteral("charging"), glyph(0xf0e7)},
        {QStringLiteral("discharging"), glyph(0xf244)},
        {QStringLiteral("cpu"), glyph(0xf035b)},
        {QStringLiteral("ram"), glyph(0xf061a)},
        {QStringLiteral("bluetooth"), glyph(0xf02cb)},
    };
}

QJsonObject defaultUserConfig()
{
    return {
        {QStringLiteral("wallpaperPath"), QString()},
        {QStringLiteral("workspaceOverviewWindowRadius"), 12.0},
        {QStringLiteral("iconFontFamily"), QStringLiteral("JetBrainsMono Nerd Font")},
        {QStringLiteral("textFontFamily"), QStringLiteral("Inter Display")},
        {QStringLiteral("heroFontFamily"), QStringLiteral("Inter Display")},
        {QStringLiteral("timeFontFamily"), QStringLiteral("Inter Display")},
        {QStringLiteral("tlpSudoPassword"), QString()},
        {QStringLiteral("overviewCloseKey"), 16777216},
        {QStringLiteral("overviewPreviousWorkspaceKey"), 16777234},
        {QStringLiteral("overviewNextWorkspaceKey"), 16777236},
        {QStringLiteral("overviewGlobalShortcutAppid"), QStringLiteral("quickshell")},
        {QStringLiteral("overviewGlobalShortcutName"), QStringLiteral("dynamic-island-overview")},
        {QStringLiteral("workspaceOverviewWorkspaceActivateButton"), 1},
        {QStringLiteral("workspaceOverviewWindowDragButton"), 1},
        {QStringLiteral("workspaceOverviewWindowFocusButton"), 1},
        {QStringLiteral("workspaceOverviewWindowCloseButton"), 3},
        {QStringLiteral("dynamicIslandSwipeButton"), 1},
        {QStringLiteral("dynamicIslandPrimaryButton"), 1},
        {QStringLiteral("dynamicIslandPrimaryAction"), QStringLiteral("toggleExpandedPlayer")},
        {QStringLiteral("dynamicIslandSecondaryButton"), 3},
        {QStringLiteral("dynamicIslandSecondaryAction"), QStringLiteral("toggleControlCenter")},
        {QStringLiteral("dynamicIslandLeftSwipeItems"), stringArray({QStringLiteral("cava"), QStringLiteral("battery")})},
        {QStringLiteral("controlCenterIcons"), controlCenterIconsDefaults()},
        {QStringLiteral("statusIcons"), statusIconsDefaults()},
    };
}

bool ensurePrivateConfigDir(const QString &filePath)
{
    const QFileInfo info(filePath);
    QDir dir(info.absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;

    QFile::setPermissions(info.absolutePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return true;
}

QJsonObject loadUserConfig()
{
    QFile file(userConfigPath());
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QByteArray bytes = file.readAll();
    if (bytes.trimmed().isEmpty())
        return {};

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {};

    return document.object();
}

bool saveUserConfig(const QJsonObject &data)
{
    const QString path = userConfigPath();
    if (!ensurePrivateConfigDir(path))
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return false;

    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool jsonValueHasExpectedType(const QJsonValue &value, const QJsonValue &fallback)
{
    if (fallback.isString())
        return value.isString();
    if (fallback.isDouble())
        return value.isDouble();
    if (fallback.isArray())
        return value.isArray();
    if (fallback.isObject())
        return value.isObject();
    if (fallback.isBool())
        return value.isBool();

    return !value.isUndefined() && !value.isNull();
}

void mergeMissingObjectKeys(QJsonObject *target, const QJsonObject &fallback, const QString &prefix, QStringList *missing)
{
    for (auto it = fallback.constBegin(); it != fallback.constEnd(); ++it) {
        const QJsonValue value = target->value(it.key());
        if (!value.isUndefined() && !value.isNull() && jsonValueHasExpectedType(value, it.value()))
            continue;

        target->insert(it.key(), it.value());
        missing->append(prefix + QStringLiteral(".") + it.key());
    }
}

bool mergeMissingUserConfig(QJsonObject *data, QStringList *missing)
{
    bool changed = false;
    const QJsonObject defaults = defaultUserConfig();
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        const QString key = it.key();
        const QJsonValue fallback = it.value();
        QJsonValue value = data->value(key);

        if (fallback.isObject() && value.isObject()) {
            QJsonObject object = value.toObject();
            const int before = missing->size();
            mergeMissingObjectKeys(&object, fallback.toObject(), QStringLiteral("userconfig.") + key, missing);
            if (missing->size() != before) {
                data->insert(key, object);
                changed = true;
            }
            continue;
        }

        if (!value.isUndefined() && !value.isNull() && jsonValueHasExpectedType(value, fallback))
            continue;

        data->insert(key, fallback);
        missing->append(QStringLiteral("userconfig.") + key);
        changed = true;
    }

    return changed;
}

QString configString(const QJsonObject &data, const QString &key)
{
    const QJsonValue value = data.value(key);
    return value.isString() ? value.toString() : QString();
}

bool readableFilePath(const QString &path)
{
    if (path.trimmed().isEmpty())
        return false;

    const QFileInfo info(expandUserPath(path.trimmed()));
    return info.isFile() && info.isReadable();
}

QString stripComment(const QString &line)
{
    bool quoted = false;
    bool escaped = false;
    QString result;
    result.reserve(line.size());

    for (const QChar ch : line) {
        if (escaped) {
            result.append(ch);
            escaped = false;
            continue;
        }

        if (ch == u'\\') {
            result.append(ch);
            escaped = true;
            continue;
        }

        if (ch == u'"') {
            quoted = !quoted;
            result.append(ch);
            continue;
        }

        if (ch == u'#' && !quoted)
            break;

        result.append(ch);
    }

    return result.trimmed();
}

QString normalizeSpace(const QString &value)
{
    return value.simplified();
}

QStringList readHyprlandLines()
{
    QFile file(hyprlandConfigPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QString text = QString::fromUtf8(file.readAll());
    return text.split(u'\n');
}

QHash<QString, QString> hyprlandVariables(const QStringList &lines)
{
    QHash<QString, QString> variables;
    for (const QString &line : lines) {
        const QString clean = stripComment(line);
        if (!clean.startsWith(u'$') || !clean.contains(u'='))
            continue;

        const int equals = clean.indexOf(u'=');
        const QString name = clean.left(equals).trimmed().sliced(1);
        const QString value = clean.mid(equals + 1).trimmed();
        if (!name.isEmpty())
            variables.insert(name, value);
    }

    return variables;
}

QSet<QString> resolveHyprlandMods(QString rawMods, const QHash<QString, QString> &variables)
{
    for (auto it = variables.constBegin(); it != variables.constEnd(); ++it)
        rawMods.replace(QStringLiteral("$") + it.key(), it.value());

    rawMods.replace(u'+', u' ');
    rawMods.replace(u'|', u' ');

    QSet<QString> mods;
    for (const QString &part : rawMods.split(u' ', Qt::SkipEmptyParts))
        mods.insert(part.toUpper());
    return mods;
}

bool overviewBindPresent()
{
    const QStringList lines = readHyprlandLines();
    const QHash<QString, QString> variables = hyprlandVariables(lines);
    const QString targetCommand = normalizeSpace(QString::fromLatin1(overviewBindCommand));

    for (const QString &line : lines) {
        const QString clean = stripComment(line);
        if (!clean.toLower().startsWith(QStringLiteral("bind")) || !clean.contains(u'='))
            continue;

        const int equals = clean.indexOf(u'=');
        const QString binding = clean.mid(equals + 1);
        const QStringList parts = binding.split(u',');
        if (parts.size() < 4)
            continue;

        const QString rawMods = parts.at(0).trimmed();
        const QString key = parts.at(1).trimmed();
        const QString dispatcher = parts.at(2).trimmed();
        const QString command = parts.mid(3).join(u',').trimmed();

        if (key.toUpper() != QStringLiteral("TAB") || dispatcher.toLower() != QStringLiteral("exec"))
            continue;
        if (resolveHyprlandMods(rawMods, variables) != QSet<QString>{QStringLiteral("SUPER")})
            continue;
        if (normalizeSpace(command).remove(u'\'').remove(u'"') == targetCommand)
            return true;
    }

    return false;
}

QStringList missingItems(QJsonObject *normalizedConfig = nullptr)
{
    QJsonObject data = loadUserConfig();
    QStringList missing;
    mergeMissingUserConfig(&data, &missing);

    if (!readableFilePath(configString(data, QStringLiteral("wallpaperPath"))))
        missing.append(QStringLiteral("userconfig.wallpaperPath"));
    if (configString(data, QStringLiteral("tlpSudoPassword")).isEmpty())
        missing.append(QStringLiteral("userconfig.tlpSudoPassword"));
    if (!overviewBindPresent())
        missing.append(QStringLiteral("hyprlandBind"));

    missing.removeDuplicates();
    if (normalizedConfig)
        *normalizedConfig = data;
    return missing;
}

void printCheck(const QStringList &missing)
{
    QTextStream out(stdout);
    if (missing.isEmpty()) {
        out << "ok\n";
        return;
    }

    out << "missing\n";
    for (const QString &item : missing)
        out << item << '\n';
}

bool processExists(qint64 pid)
{
    if (pid <= 0)
        return false;

    if (::kill(static_cast<pid_t>(pid), 0) == 0)
        return true;
    return errno == EPERM;
}

void clearSetupLock()
{
    QFile::remove(setupLockPath());
}

QJsonObject readSetupLock()
{
    QFile file(setupLockPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

bool setupLockActive()
{
    const QJsonObject data = readSetupLock();
    const qint64 pid = static_cast<qint64>(data.value(QStringLiteral("pid")).toDouble(-1));
    if (!processExists(pid)) {
        clearSetupLock();
        return false;
    }

    const qint64 createdAt = static_cast<qint64>(data.value(QStringLiteral("createdAt")).toDouble(0));
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (createdAt > 0 && (now - createdAt) > setupLockMaxAgeSeconds) {
        clearSetupLock();
        return false;
    }

    return true;
}

bool writeSetupLock(qint64 pid)
{
    const QString path = setupLockPath();
    if (!ensurePrivateConfigDir(path))
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    const QJsonObject data{
        {QStringLiteral("pid"), double(pid)},
        {QStringLiteral("createdAt"), double(QDateTime::currentSecsSinceEpoch())},
    };
    file.write(QJsonDocument(data).toJson(QJsonDocument::Compact));
    file.write("\n");
    if (!file.commit())
        return false;

    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

QString readLine(const QString &prompt)
{
    QTextStream out(stdout);
    out << prompt;
    out.flush();

    char buffer[4096];
    if (!std::fgets(buffer, sizeof(buffer), stdin))
        return {};

    return QString::fromLocal8Bit(buffer).trimmed();
}

QString readHiddenLine(const QString &prompt)
{
    QTextStream out(stdout);
    out << prompt;
    out.flush();

    termios oldTermios;
    if (tcgetattr(STDIN_FILENO, &oldTermios) != 0)
        return readLine(QString());

    termios newTermios = oldTermios;
    newTermios.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &newTermios);

    QTextStream in(stdin);
    char buffer[4096];
    const QString value = std::fgets(buffer, sizeof(buffer), stdin)
        ? QString::fromLocal8Bit(buffer).trimmed()
        : QString();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTermios);
    out << '\n';
    return value;
}

void promptWallpaper(QJsonObject *data)
{
    QTextStream out(stdout);
    out << "\nWallpaper path\n";
    out << "This image is used as the workspace overview background and preview wallpaper.\n";

    const QString current = configString(*data, QStringLiteral("wallpaperPath"));
    if (!current.isEmpty())
        out << "Current value is not usable: " << current << '\n';
    out.flush();

    while (true) {
        const QString value = readLine(QStringLiteral("Enter a readable wallpaper image path: ")).remove(u'\'').remove(u'"');
        if (value.isEmpty() && std::feof(stdin))
            return;

        const QFileInfo path(expandUserPath(value));
        if (!value.isEmpty() && path.isFile() && path.isReadable()) {
            data->insert(QStringLiteral("wallpaperPath"), path.absoluteFilePath());
            saveUserConfig(*data);
            out << "Saved wallpaperPath.\n";
            return;
        }

        out << "That path does not exist or is not readable. Please try again.\n";
    }
}

void promptTlpPassword(QJsonObject *data)
{
    QTextStream out(stdout);
    out << "\nTLP sudo password\n";
    out << "This password is only used by the control center to run sudo -S tlp <mode> when switching TLP modes.\n";
    out << "Tide Island will not use it to install files, change system configuration, run other sudo commands, or do anything unrelated to TLP mode switching.\n";
    out.flush();

    while (true) {
        const QString first = readHiddenLine(QStringLiteral("Enter your sudo password (input is hidden): "));
        if (first.isEmpty() && std::feof(stdin))
            return;

        if (first.isEmpty()) {
            out << "The password cannot be empty, or this setup will run again next time.\n";
            continue;
        }

        const QString second = readHiddenLine(QStringLiteral("Enter it again to confirm: "));
        if (first == second) {
            data->insert(QStringLiteral("tlpSudoPassword"), first);
            saveUserConfig(*data);
            out << "Saved tlpSudoPassword.\n";
            return;
        }

        out << "The two entries did not match. Please try again.\n";
    }
}

bool appendHyprlandBind(QString *errorMessage)
{
    if (overviewBindPresent())
        return false;

    const QString path = hyprlandConfigPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QString existing;
    QFile input(path);
    if (input.exists()) {
        if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage)
                *errorMessage = input.errorString();
            return false;
        }
        existing = QString::fromUtf8(input.readAll());
    }

    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = output.errorString();
        return false;
    }

    const QString separator = existing.isEmpty() || existing.endsWith(u'\n') ? QString() : QStringLiteral("\n");
    const QString addition = QStringLiteral("\n# Tide Island workspace overview\n") + QString::fromLatin1(overviewBindLine) + QStringLiteral("\n");
    output.write((existing + separator + addition).toUtf8());
    return true;
}

void configureHyprlandBind()
{
    QTextStream out(stdout);
    out << "\nHyprland SUPER+TAB overview binding\n";
    out << "This binding lets SUPER+TAB call Tide Island's overview toggle to open or close the workspace overview.\n";

    QString errorMessage;
    const bool changed = appendHyprlandBind(&errorMessage);
    if (!changed && !errorMessage.isEmpty()) {
        out << "Could not write to " << hyprlandConfigPath() << ": " << errorMessage << '\n';
        return;
    }

    if (!changed) {
        out << "The Hyprland binding already exists.\n";
        return;
    }

    out << "Appended to " << hyprlandConfigPath() << ":\n";
    out << overviewBindLine << '\n';

    QProcess reloadProcess;
    reloadProcess.setProgram(QStringLiteral("hyprctl"));
    reloadProcess.setArguments({QStringLiteral("reload")});
    reloadProcess.setStandardOutputFile(QProcess::nullDevice());
    reloadProcess.setStandardErrorFile(QProcess::nullDevice());
    reloadProcess.start();
    const bool reloadSucceeded = reloadProcess.waitForFinished(5000)
        && reloadProcess.exitStatus() == QProcess::NormalExit
        && reloadProcess.exitCode() == 0;
    if (reloadSucceeded)
        out << "Ran hyprctl reload.\n";
    else
        out << "Could not reload Hyprland automatically. Run hyprctl reload manually, or log out and back in.\n";
}

QString executablePath()
{
    const QString path = QCoreApplication::applicationFilePath();
    return path.isEmpty() ? QStringLiteral("tide-island-setup") : path;
}

QString findExecutable(const QString &name)
{
    const QStringList paths = envString("PATH").split(u':', Qt::SkipEmptyParts);
    for (const QString &dir : paths) {
        const QString candidate = dir + QStringLiteral("/") + name;
        const QFileInfo info(candidate);
        if (info.isFile() && info.isExecutable())
            return candidate;
    }
    return {};
}

QStringList terminalCommand(QStringList base)
{
    const QString name = QFileInfo(base.value(0)).fileName();
    const QString script = executablePath();
    const QString title = QStringLiteral("Tide Island Setup");

    if (name == QStringLiteral("kitty") || name == QStringLiteral("foot"))
        return base << QStringLiteral("--title") << title << script << QStringLiteral("--wizard");
    if (name == QStringLiteral("alacritty"))
        return base << QStringLiteral("--title") << title << QStringLiteral("-e") << script << QStringLiteral("--wizard");
    if (name == QStringLiteral("wezterm"))
        return base << QStringLiteral("start") << QStringLiteral("--") << script << QStringLiteral("--wizard");
    if (name == QStringLiteral("konsole"))
        return base << QStringLiteral("--new-tab") << QStringLiteral("-p") << QStringLiteral("tabtitle=Tide Island Setup") << QStringLiteral("-e") << script << QStringLiteral("--wizard");
    if (name == QStringLiteral("gnome-terminal"))
        return base << QStringLiteral("--title=Tide Island Setup") << QStringLiteral("--") << script << QStringLiteral("--wizard");
    if (name == QStringLiteral("xterm"))
        return base << QStringLiteral("-T") << title << QStringLiteral("-e") << script << QStringLiteral("--wizard");

    return base << QStringLiteral("-e") << script << QStringLiteral("--wizard");
}

int launchWizard()
{
    QJsonObject normalized;
    const QStringList missing = missingItems(&normalized);
    if (missing.isEmpty()) {
        clearSetupLock();
        return 0;
    }

    saveUserConfig(normalized);

    if (setupLockActive())
        return 0;

    QList<QStringList> candidates;
    const QString terminal = envString("TERMINAL");
    if (!terminal.isEmpty())
        candidates.append(QProcess::splitCommand(terminal));

    for (const QString &name : {QStringLiteral("kitty"), QStringLiteral("foot"), QStringLiteral("alacritty"),
         QStringLiteral("wezterm"), QStringLiteral("ghostty"), QStringLiteral("konsole"),
         QStringLiteral("gnome-terminal"), QStringLiteral("xterm")}) {
        const QString path = findExecutable(name);
        if (!path.isEmpty())
            candidates.append({path});
    }

    for (const QStringList &candidate : candidates) {
        if (candidate.isEmpty())
            continue;

        qint64 pid = -1;
        const QStringList command = terminalCommand(candidate);
        if (!QProcess::startDetached(command.first(), command.mid(1), QString(), &pid))
            continue;

        writeSetupLock(pid);
        return 0;
    }

    QTextStream err(stderr);
    err << "Tide Island setup is missing configuration, but no terminal emulator was found.\n";
    err << "Run this manually: " << executablePath() << " --wizard\n";
    return 1;
}

int runWizard()
{
    writeSetupLock(QCoreApplication::applicationPid());
    QJsonObject data;
    QStringList missing = missingItems(&data);
    if (missing.isEmpty()) {
        clearSetupLock();
        QTextStream(stdout) << "Tide Island setup is already complete.\n";
        return 0;
    }

    saveUserConfig(data);

    QTextStream out(stdout);
    out << "Tide Island setup\n";
    out << "Missing config keys are filled with defaults. Interactive prompts only appear for values that need local input.\n";
    out.flush();

    if (missing.contains(QStringLiteral("userconfig.wallpaperPath"))) {
        promptWallpaper(&data);
        missing = missingItems(&data);
    }

    if (missing.contains(QStringLiteral("userconfig.tlpSudoPassword"))) {
        promptTlpPassword(&data);
        missing = missingItems(&data);
    }

    if (missing.contains(QStringLiteral("hyprlandBind")))
        configureHyprlandBind();

    missing = missingItems(&data);
    saveUserConfig(data);
    clearSetupLock();

    if (!missing.isEmpty()) {
        out << "\nStill missing:\n";
        for (const QString &item : missing)
            out << item << '\n';
        return 1;
    }

    out << "\nSetup complete. This wizard will not appear on the next Tide Island start.\n";
    return 0;
}

void printUsage()
{
    QTextStream err(stderr);
    err << "Usage: tide-island-setup --check | --launch | --wizard\n";
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments().mid(1);
    if (args.size() != 1) {
        printUsage();
        return 2;
    }

    const QString arg = args.first();
    if (arg == QStringLiteral("--check")) {
        QJsonObject normalized;
        const QStringList missing = missingItems(&normalized);
        saveUserConfig(normalized);
        printCheck(missing);
        return missing.isEmpty() ? 0 : 1;
    }

    if (arg == QStringLiteral("--launch"))
        return launchWizard();

    if (arg == QStringLiteral("--wizard"))
        return runWizard();

    printUsage();
    return 2;
}
