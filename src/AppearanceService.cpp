#include "AppearanceService.hpp"
#include "PortalConfig.hpp"

#include <QGuiApplication>
#include <QStyleHints>

AppearanceService::AppearanceService(PortalConfig &config, QObject *parent)
    : QObject(parent), m_config(config) {
    const auto apply = [this] {
        const auto preference = m_config.appearance();
        if (m_appliedPreference == preference) return;
        m_appliedPreference = preference;
        QGuiApplication::styleHints()->setColorScheme(
            preference == QStringLiteral("dark") ? Qt::ColorScheme::Dark :
            preference == QStringLiteral("light") ? Qt::ColorScheme::Light :
            Qt::ColorScheme::Unknown);
        emit changed();
    };
    connect(&m_config, &PortalConfig::changed, this, apply);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &AppearanceService::changed);
    apply();
}

bool AppearanceService::dark() const {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}
