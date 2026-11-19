#include "CalendarService.hpp"
#include "AppearanceService.hpp"
#include "MeetingStatus.hpp"
#include "BrandingService.hpp"
#include "ContactStore.hpp"
#include "ClipboardService.hpp"
#include "PortalConfig.hpp"
#include "PortalSession.hpp"
#include "SystemContacts.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QLocale>
#include <QTranslator>
#include <QVariantMap>
#include <QtWebView>

int main(int argc, char *argv[]) {
    QtWebView::initialize();
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Viamiora"));
    QCoreApplication::setApplicationName(QStringLiteral("Viamiora"));
    QCoreApplication::setApplicationVersion(QStringLiteral(VIAMIORA_VERSION));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    ContactStore contacts;
    ClipboardService clipboard;
    PortalConfig portal;
    AppearanceService appearance(portal);
    MeetingStatus meetingStatus;
    QObject::connect(&meetingStatus, &MeetingStatus::changed, &app, [&] {
        app.setQuitOnLastWindowClosed(!meetingStatus.active());
    });
    QTranslator translator;
    auto selectedLanguage = portal.language();
    const auto useLanguage = [&] {
        app.removeTranslator(&translator);
        const QLocale locale(selectedLanguage.isEmpty() ? QLocale::system() : QLocale(selectedLanguage));
        QLocale::setDefault(locale);
        if (translator.load(locale, QStringLiteral("Viamiora"), QStringLiteral("_"),
                            QStringLiteral(":/i18n"))) app.installTranslator(&translator);
    };
    useLanguage();
    BrandingService branding(portal);
    CalendarService calendar(portal);
    PortalSession portalSession(portal);
    QObject::connect(&portalSession, &PortalSession::changed, &app, [&] {
        static bool wasSignedIn = false;
        if (portalSession.signedIn() && !wasSignedIn) branding.load(portal.portalUrl());
        wasSignedIn = portalSession.signedIn();
    });
    SystemContacts systemContacts;
    QQmlApplicationEngine engine;
    QObject::connect(&portal, &PortalConfig::changed, &app, [&] {
        if (selectedLanguage == portal.language()) return;
        selectedLanguage = portal.language();
        useLanguage();
        engine.retranslate();
        meetingStatus.retranslate();
    });
    engine.setInitialProperties(QVariantMap{
        {QStringLiteral("contactStore"), QVariant::fromValue(&contacts)},
        {QStringLiteral("clipboardService"), QVariant::fromValue(&clipboard)},
        {QStringLiteral("brandingService"), QVariant::fromValue(&branding)},
        {QStringLiteral("calendarService"), QVariant::fromValue(&calendar)},
        {QStringLiteral("appearanceService"), QVariant::fromValue(&appearance)},
        {QStringLiteral("meetingStatus"), QVariant::fromValue(&meetingStatus)},
        {QStringLiteral("portalConfig"), QVariant::fromValue(&portal)},
        {QStringLiteral("portalSession"), QVariant::fromValue(&portalSession)},
        {QStringLiteral("systemContacts"), QVariant::fromValue(&systemContacts)},
        {QStringLiteral("externalDependencies"), QVariantList{
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Qt")},
                    {QStringLiteral("version"), QStringLiteral(VIAMIORA_QT_VERSION)},
                    {QStringLiteral("license"), QStringLiteral("LGPL-3.0-only")},
                    {QStringLiteral("url"), QStringLiteral("https://www.qt.io/licensing/")}}
        }},
        {QStringLiteral("appVersion"), QCoreApplication::applicationVersion()}
    });
    engine.loadFromModule("Viamiora", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
