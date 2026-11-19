#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QPointer>
#include <QUrl>

class QNetworkReply;

class PortalConfig;

class BrandingService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString brandName READ brandName NOTIFY changed)
    Q_PROPERTY(QUrl logoUrl READ logoUrl NOTIFY changed)
public:
    explicit BrandingService(PortalConfig &config, QObject *parent = nullptr);
    [[nodiscard]] QString accent() const;
    [[nodiscard]] QString brandName() const;
    [[nodiscard]] QUrl logoUrl() const;
    Q_INVOKABLE void load(const QString &portalOrigin);
    Q_INVOKABLE void clear();
signals:
    void changed();
private:
    PortalConfig &m_config;
    void requestNext();
    void backOff(const QByteArray &retryAfter);
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QPointer<QNetworkReply> m_loginReply;
    QList<QUrl> m_candidates;
    QString m_accent = QStringLiteral("#315da8");
    QString m_brandName;
    QUrl m_logoUrl;
    QString m_lastOrigin;
    qint64 m_nextLoadAtMs = 0;
    quint64 m_generation = 0;
};
