#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <unordered_map>

struct QStringHash {
    std::size_t operator()(const QString &key) const noexcept;
};

using UserConfigMap = std::unordered_map<QString, QVariant, QStringHash>;

class Backend final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString userConfigPath READ userConfigPath CONSTANT)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QVariantMap userConfig READ userConfig CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);

    QString userConfigPath() const;
    QString errorString() const;
    QVariantMap userConfig() const;

    Q_INVOKABLE bool save(const QVariantMap &userConfig);

signals:
    void errorStringChanged();

private:
    void load();
    void setErrorString(const QString &errorString);
    QVariantMap toVariantMap() const;
    void setUserConfig(const QVariantMap &userConfig);

    QString m_userConfigPath;
    QString m_errorString;
    UserConfigMap m_userConfig;
};
