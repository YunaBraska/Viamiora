#include "PortalConfig.hpp"
#include "MeetingUrl.hpp"

#include <QSettings>
#include <QLocale>
#include <QUrl>
#include <optional>

namespace {
QSettings openSettings(const QString &file) {
    if (file.isEmpty())
        return QSettings(QSettings::IniFormat, QSettings::UserScope,
                         QStringLiteral("Viamiora"), QStringLiteral("Viamiora"));
    return QSettings(file, QSettings::IniFormat);
}

std::optional<QString> normalizedPortalUrl(const QString &input) {
    const QUrl url(input.trimmed());
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()
        || !url.userInfo().isEmpty() || !url.query().isEmpty() || !url.fragment().isEmpty()
        || (url.path() != QString() && url.path() != QStringLiteral("/"))) return std::nullopt;
    return url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::StripTrailingSlash).toString();
}
std::optional<QString> normalizedRoomUrl(const QString &input) {
    const QUrl url(input.trimmed());
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()
        || !url.userInfo().isEmpty() || url.toString().size() > 2048) return std::nullopt;
    return url.toString(QUrl::FullyEncoded);
}
}

PortalConfig::PortalConfig(QString settingsFile, QObject *parent)
    : QObject(parent), m_settingsFile(std::move(settingsFile)) {
    QSettings settings = openSettings(m_settingsFile);
    m_lastUsernames = settings.value(QStringLiteral("lastUsernames")).toMap();
    const auto savedLanguage = settings.value(QStringLiteral("language")).toString();
    if (savedLanguage == QStringLiteral("en") || savedLanguage == QStringLiteral("de") ||
        savedLanguage == QStringLiteral("sv"))
        m_language = savedLanguage;
    const auto savedAppearance = settings.value(QStringLiteral("appearance")).toString();
    if (savedAppearance == QStringLiteral("light") || savedAppearance == QStringLiteral("dark"))
        m_appearance = savedAppearance;
    for (const auto &item : settings.value(QStringLiteral("roomLinks")).toList()) {
        const auto map = item.toMap();
        const auto url = normalizedRoomUrl(map.value(QStringLiteral("url")).toString());
        const auto title = map.value(QStringLiteral("title")).toString().trimmed();
        if (url && !title.isEmpty()) m_roomLinks.append(QVariantMap{
            {QStringLiteral("title"), title}, {QStringLiteral("url"), *url}});
    }
    for (const auto &candidate : settings.value(QStringLiteral("portalUrls")).toStringList()) {
        const auto normalized = normalizedPortalUrl(candidate);
        if (normalized && !m_portalUrls.contains(*normalized)) m_portalUrls.append(*normalized);
    }
    const auto stored = settings.value(QStringLiteral("portalUrl")).toString();
    if (stored.isEmpty()) return;
    const auto normalized = normalizedPortalUrl(stored);
    if (normalized) {
        m_portalUrl = *normalized;
        m_portalUrls.removeAll(*normalized);
        m_portalUrls.prepend(*normalized);
    } else m_error = tr("Saved portal address is invalid. Enter the portal address.");
}

QString PortalConfig::portalUrl() const { return m_portalUrl; }
QStringList PortalConfig::portalUrls() const { return m_portalUrls; }
QString PortalConfig::lastUsername() const { return m_lastUsernames.value(m_portalUrl).toString(); }
QString PortalConfig::lastUsernameFor(const QString &origin) const { return m_lastUsernames.value(origin).toString(); }
QVariantList PortalConfig::roomLinks() const { return m_roomLinks; }
QString PortalConfig::error() const { return m_error; }
QString PortalConfig::language() const { return m_language; }
QString PortalConfig::appearance() const { return m_appearance; }
bool PortalConfig::setAppearance(const QString &appearance) {
    if (appearance != QString() && appearance != QStringLiteral("light") &&
        appearance != QStringLiteral("dark")) return false;
    if (m_appearance == appearance) return true;
    QSettings settings = openSettings(m_settingsFile);
    settings.setValue(QStringLiteral("appearance"), appearance);
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;
    m_appearance = appearance;
    emit changed();
    return true;
}
bool PortalConfig::setLanguage(const QString &language) {
    if (language != QString() && language != QStringLiteral("en") &&
        language != QStringLiteral("de") && language != QStringLiteral("sv")) return false;
    if (m_language == language) return true;
    QSettings settings = openSettings(m_settingsFile);
    settings.setValue(QStringLiteral("language"), language);
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;
    m_language = language;
    emit changed();
    return true;
}
QString PortalConfig::safeMeetingUrl(const QString &address) const {
    const auto normalized = normalizedRoomUrl(address);
    return normalized ? *normalized : QString();
}
QString PortalConfig::recognizedMeetingUrl(const QString &address) const {
    return ::recognizedMeetingUrl(address, m_portalUrls);
}
QString PortalConfig::safePortalUrl(const QString &address) const {
    const auto normalized = normalizedPortalUrl(address);
    return normalized ? *normalized : QString();
}

bool PortalConfig::saveRoomLink(const QString &title, const QString &address) {
    const auto cleanTitle = title.trimmed();
    const auto url = normalizedRoomUrl(address);
    if (cleanTitle.isEmpty() || cleanTitle.size() > 120 || !url) {
        m_error = tr("Enter a name and a complete HTTPS meeting link.");
        emit changed();
        return false;
    }
    QVariantList updated;
    for (const auto &item : m_roomLinks)
        if (item.toMap().value(QStringLiteral("url")).toString() != *url) updated.append(item);
    updated.prepend(QVariantMap{{QStringLiteral("title"), cleanTitle},
                               {QStringLiteral("url"), *url}});
    QSettings settings = openSettings(m_settingsFile);
    settings.setValue(QStringLiteral("roomLinks"), updated);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        m_error = tr("Meeting link could not be saved.");
        emit changed();
        return false;
    }
    m_roomLinks = updated;
    m_error.clear();
    emit changed();
    return true;
}

bool PortalConfig::removeRoomLink(const QString &address) {
    QVariantList updated;
    for (const auto &item : m_roomLinks)
        if (item.toMap().value(QStringLiteral("url")).toString() != address) updated.append(item);
    if (updated.size() == m_roomLinks.size()) return false;
    QSettings settings = openSettings(m_settingsFile);
    settings.setValue(QStringLiteral("roomLinks"), updated);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        m_error = tr("Meeting link could not be removed.");
        emit changed();
        return false;
    }
    m_roomLinks = updated;
    m_error.clear();
    emit changed();
    return true;
}

bool PortalConfig::setPortalUrl(const QString &input) {
    const auto normalized = normalizedPortalUrl(input);
    if (!normalized) {
        m_error = tr("Enter a complete HTTPS portal address.");
        emit changed();
        return false;
    }
    if (*normalized == m_portalUrl) {
        if (m_error.isEmpty()) return true;
        m_error.clear();
        emit changed();
        return true;
    }
    QSettings settings = openSettings(m_settingsFile);
    auto urls = m_portalUrls;
    urls.removeAll(*normalized);
    urls.prepend(*normalized);
    settings.setValue(QStringLiteral("portalUrl"), *normalized);
    settings.setValue(QStringLiteral("portalUrls"), urls);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        m_error = tr("Portal address could not be saved.");
        emit changed();
        return false;
    }
    m_portalUrl = *normalized;
    m_portalUrls = urls;
    m_error.clear();
    emit changed();
    return true;
}

void PortalConfig::rememberUsername(const QString &username) {
    if (m_portalUrl.isEmpty() || username.trimmed().isEmpty()) return;
    auto updated = m_lastUsernames;
    updated.insert(m_portalUrl, username.trimmed());
    QSettings settings = openSettings(m_settingsFile);
    settings.setValue(QStringLiteral("lastUsernames"), updated);
    settings.sync();
    if (settings.status() != QSettings::NoError) return;
    m_lastUsernames = updated;
    emit changed();
}

bool PortalConfig::forgetUsername(const QString &origin, const QString &username) {
    if (origin.isEmpty() || lastUsernameFor(origin) != username.trimmed()) return true;
    auto updated = m_lastUsernames;
    updated.remove(origin);
    QSettings settings = openSettings(m_settingsFile);
    settings.setValue(QStringLiteral("lastUsernames"), updated);
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;
    m_lastUsernames = updated;
    emit changed();
    return true;
}
