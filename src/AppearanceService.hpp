#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QString>

class PortalConfig;

class AppearanceService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(bool dark READ dark NOTIFY changed)

public:
    explicit AppearanceService(PortalConfig &config, QObject *parent = nullptr);
    [[nodiscard]] bool dark() const;

signals:
    void changed();

private:
    PortalConfig &m_config;
    QString m_appliedPreference = QStringLiteral("initial");
};
