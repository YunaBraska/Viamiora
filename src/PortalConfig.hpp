#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QStringList>
#include <QVariantMap>

class PortalConfig final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(QString portalUrl READ portalUrl NOTIFY changed)
    Q_PROPERTY(QStringList portalUrls READ portalUrls NOTIFY changed)
    Q_PROPERTY(QString lastUsername READ lastUsername NOTIFY changed)
    Q_PROPERTY(QVariantList roomLinks READ roomLinks NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QString appearance READ appearance NOTIFY changed)

public:
    explicit PortalConfig(QString settingsFile = {}, QObject *parent = nullptr);
    [[nodiscard]] QString portalUrl() const;
    [[nodiscard]] QStringList portalUrls() const;
    [[nodiscard]] QString lastUsername() const;
    [[nodiscard]] QString lastUsernameFor(const QString &origin) const;
    [[nodiscard]] QVariantList roomLinks() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QString language() const;
    [[nodiscard]] QString appearance() const;
    Q_INVOKABLE bool setLanguage(const QString &language);
    Q_INVOKABLE bool setAppearance(const QString &appearance);
    Q_INVOKABLE bool setPortalUrl(const QString &input);
    Q_INVOKABLE bool saveRoomLink(const QString &title, const QString &address);
    Q_INVOKABLE bool removeRoomLink(const QString &address);
    Q_INVOKABLE QString safeMeetingUrl(const QString &address) const;
    Q_INVOKABLE QString recognizedMeetingUrl(const QString &address) const;
    Q_INVOKABLE QString safePortalUrl(const QString &address) const;
    void rememberUsername(const QString &username);
    bool forgetUsername(const QString &origin, const QString &username);

signals:
    void changed();

private:
    QString m_settingsFile;
    QString m_portalUrl;
    QStringList m_portalUrls;
    QVariantMap m_lastUsernames;
    QVariantList m_roomLinks;
    QString m_error;
    QString m_language;
    QString m_appearance;
};
