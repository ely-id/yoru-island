#include "backend.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QVariant>

namespace {
QByteArray stripJsonComments(const QByteArray &input){
    QByteArray output;
    output.reserve(input.size());

    enum class State {
        Normal,
        String,
        LineComment,
        BlockComment,
    };

    State state = State::Normal;
    bool escaped = false;

    for (qsizetype i = 0; i < input.size(); ++i) {
        const char ch = input.at(i);
        const char next = i + 1 < input.size() ? input.at(i + 1) : '\0';

        switch (state) {
        case State::Normal:
            if (ch == '"') {
                output.append(ch);
                state = State::String;
            } else if (ch == '/' && next == '/') {
                output.append(' ');
                output.append(' ');
                ++i;
                state = State::LineComment;
            } else if (ch == '/' && next == '*') {
                output.append(' ');
                output.append(' ');
                ++i;
                state = State::BlockComment;
            } else {
                output.append(ch);
            }
            break;
        case State::String:
            output.append(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                state = State::Normal;
            }
            break;
        case State::LineComment:
            if (ch == '\n' || ch == '\r') {
                output.append(ch);
                state = State::Normal;
            } else {
                output.append(' ');
            }
            break;
        case State::BlockComment:
            if (ch == '*' && next == '/') {
                output.append(' ');
                output.append(' ');
                ++i;
                state = State::Normal;
            } else if (ch == '\n' || ch == '\r') {
                output.append(ch);
            } else {
                output.append(' ');
            }
            break;
        }
    }

    return output;
}

UserConfigMap toUserConfigMap(const QVariantMap &userConfig){
    UserConfigMap result;
    result.reserve(static_cast<std::size_t>(userConfig.size()));

    for (auto it = userConfig.cbegin(); it != userConfig.cend(); ++it)
        result.emplace(it.key(), it.value());

    return result;
}
}

std::size_t QStringHash::operator()(const QString &key) const noexcept{
    return static_cast<std::size_t>(qHash(key));
}

Backend::Backend(QObject *parent) : QObject(parent), m_userConfigPath(QDir::homePath() + QStringLiteral("/.config/tide-island/userconfig.json")){
    load();
}

QString Backend::userConfigPath() const{
    return m_userConfigPath;
}

QString Backend::errorString() const{
    return m_errorString;
}

QVariantMap Backend::userConfig() const{
    return toVariantMap();
}

bool Backend::save(const QVariantMap &userConfig){
    const QFileInfo configInfo(m_userConfigPath);
    QDir directory(configInfo.absolutePath());
    if (!directory.exists() && !QDir().mkpath(configInfo.absolutePath())) {
        setErrorString(QStringLiteral("Could not create %1").arg(configInfo.absolutePath()));
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromVariant(userConfig);
    if (!document.isObject()) {
        setErrorString(QStringLiteral("User config must be a JSON object."));
        return false;
    }

    QSaveFile file(m_userConfigPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setErrorString(QStringLiteral("Could not write %1: %2").arg(m_userConfigPath, file.errorString()));
        return false;
    }

    file.write(document.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setErrorString(QStringLiteral("Could not save %1: %2").arg(m_userConfigPath, file.errorString()));
        return false;
    }

    setUserConfig(userConfig);
    setErrorString(QString());
    return true;
}

void Backend::load(){
    QFile file(m_userConfigPath);
    if (!file.exists()) {
        setUserConfig({});
        setErrorString(QString());
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setUserConfig({});
        setErrorString(QStringLiteral("Could not read %1: %2").arg(m_userConfigPath, file.errorString()));
        return;
    }

    const QByteArray contents = file.readAll();
    if (contents.trimmed().isEmpty()) {
        setUserConfig({});
        setErrorString(QString());
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stripJsonComments(contents), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setUserConfig({});
        setErrorString(QStringLiteral("Invalid JSON in %1 at offset %2: %3")
            .arg(m_userConfigPath)
            .arg(parseError.offset)
            .arg(parseError.errorString()));
        return;
    }

    if (!document.isObject()) {
        setUserConfig({});
        setErrorString(QStringLiteral("Invalid JSON in %1: root value must be an object.").arg(m_userConfigPath));
        return;
    }

    setUserConfig(document.object().toVariantMap());
    setErrorString(QString());
}

void Backend::setErrorString(const QString &errorString){
    if (m_errorString == errorString)
        return;

    m_errorString = errorString;
    emit errorStringChanged();
}

QVariantMap Backend::toVariantMap() const{
    QVariantMap result;
    for (const auto &[key, value] : m_userConfig)
        result.insert(key, value);
    return result;
}

void Backend::setUserConfig(const QVariantMap &userConfig){
    m_userConfig = toUserConfigMap(userConfig);
}
