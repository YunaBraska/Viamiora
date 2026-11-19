#include "PortalConfig.hpp"

#include <QTemporaryDir>
#include <QSettings>
#include <QtTest>

class PortalConfigTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithoutAssumingACompany() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        QCOMPARE(config.portalUrl(), QString());
        QCOMPARE(config.error(), QString());
        QCOMPARE(config.language(), QString());
    }

    void remembersLanguageOrUsesSystemDefault() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("settings.ini"));
        PortalConfig config(file);
        QVERIFY(config.setLanguage(QStringLiteral("de")));
        QCOMPARE(config.language(), QStringLiteral("de"));
        QVERIFY(!config.setLanguage(QStringLiteral("invalid")));
        QCOMPARE(config.language(), QStringLiteral("de"));
        PortalConfig loaded(file);
        QCOMPARE(loaded.language(), QStringLiteral("de"));
        QVERIFY(loaded.setLanguage({}));
        PortalConfig systemDefault(file);
        QCOMPARE(systemDefault.language(), QString());
    }

    void savesTenantAddress() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("settings.ini"));
        PortalConfig config(file);
        QVERIFY(config.setPortalUrl(QStringLiteral("https://tenant.example.invalid/")));
        QCOMPARE(config.portalUrl(), QStringLiteral("https://tenant.example.invalid"));
        PortalConfig loaded(file);
        QCOMPARE(loaded.portalUrl(), config.portalUrl());
    }

    void unchangedPortalDoesNotNotifyAgain() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        QSignalSpy changed(&config, &PortalConfig::changed);
        QVERIFY(config.setPortalUrl(QStringLiteral("https://tenant.example.invalid")));
        QCOMPARE(changed.size(), 1);
        QVERIFY(config.setPortalUrl(QStringLiteral("https://tenant.example.invalid/")));
        QCOMPARE(changed.size(), 1);
        QVERIFY(!config.setPortalUrl(QStringLiteral("http://tenant.example.invalid")));
        QCOMPARE(changed.size(), 2);
        QVERIFY(config.setPortalUrl(QStringLiteral("https://tenant.example.invalid")));
        QCOMPARE(config.error(), QString());
        QCOMPARE(changed.size(), 3);
    }

    void keepsPortalSuggestionsAcrossSwitches() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("settings.ini"));
        PortalConfig config(file);
        QVERIFY(config.setPortalUrl(QStringLiteral("https://one.example.org")));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://two.example.org")));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://one.example.org")));
        QCOMPARE(config.portalUrls(), QStringList({QStringLiteral("https://one.example.org"),
                                                   QStringLiteral("https://two.example.org")}));
        PortalConfig loaded(file);
        QCOMPARE(loaded.portalUrl(), QStringLiteral("https://one.example.org"));
        QCOMPARE(loaded.portalUrls(), config.portalUrls());
    }

    void remembersTheLastUsernameForEachPortal() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("settings.ini"));
        PortalConfig config(file);
        QVERIFY(config.setPortalUrl(QStringLiteral("https://one.example.org")));
        config.rememberUsername(QStringLiteral("one@example.org"));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://two.example.org")));
        QCOMPARE(config.lastUsername(), QString());
        config.rememberUsername(QStringLiteral("two@example.org"));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://one.example.org")));
        QCOMPARE(config.lastUsername(), QStringLiteral("one@example.org"));
        PortalConfig loaded(file);
        QCOMPARE(loaded.lastUsername(), config.lastUsername());
        QVERIFY(config.forgetUsername(QStringLiteral("https://one.example.org"), QStringLiteral("one@example.org")));
        QCOMPARE(config.lastUsername(), QString());
        PortalConfig forgotten(file);
        QCOMPARE(forgotten.lastUsername(), QString());
    }

    void savesRoomLinksAcrossLaunches() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("settings.ini"));
        PortalConfig config(file);
        const auto link = QStringLiteral("https://meet.example.org/webapp/#/?conference=room@meet.example.org");
        QVERIFY(config.saveRoomLink(QStringLiteral("My room"), link));
        QCOMPARE(config.safeMeetingUrl(link), link);
        QCOMPARE(config.safeMeetingUrl(QStringLiteral("https://user:secret@meet.example.org/room")), QString());
        QVERIFY(!config.saveRoomLink(QStringLiteral("Unsafe"), QStringLiteral("http://meet.example.org/room")));
        QVERIFY(!config.saveRoomLink(QStringLiteral("Unsafe"), QStringLiteral("https://user:secret@meet.example.org/room")));
        QCOMPARE(config.roomLinks().size(), 1);
        PortalConfig loaded(file);
        QCOMPARE(loaded.roomLinks().size(), 1);
        QCOMPARE(loaded.roomLinks().first().toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("My room"));
        QVERIFY(loaded.removeRoomLink(link));
        PortalConfig empty(file);
        QVERIFY(empty.roomLinks().isEmpty());
    }

    void recognizesOnlyJoinableCalendarLinks() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://tenant.example.org")));
        const QStringList meetings{
            QStringLiteral("https://tenant.example.org/webapp/#/?conference=123@tenant.example.org"),
            QStringLiteral("https://vc.example.org/webapp/#/?conference=123@vc.example.org"),
            QStringLiteral("https://meet.google.com/abc-defg-hij"),
            QStringLiteral("https://teams.microsoft.com/l/meetup-join/abc"),
            QStringLiteral("https://example.zoom.us/j/123456789"),
            QStringLiteral("https://example.webex.com/meet/alex"),
            QStringLiteral("https://example.webex.com/team/j.php?MTID=abc"),
            QStringLiteral("https://meet.jit.si/ExampleRoom"),
            QStringLiteral("https://whereby.com/example-room")};
        for (const auto &url : meetings) QVERIFY2(!config.recognizedMeetingUrl(url).isEmpty(), qPrintable(url));
        const QStringList unrelated{
            QStringLiteral("https://example.org/notes"),
            QStringLiteral("https://tenant.example.org/dashboard"),
            QStringLiteral("https://tenant.example.org/webapp/"),
            QStringLiteral("https://meet.google.com/about"),
            QStringLiteral("https://user:secret@tenant.example.org/webapp/#/?conference=123"),
            QStringLiteral("http://meet.google.com/abc-defg-hij")};
        for (const auto &url : unrelated) QVERIFY2(config.recognizedMeetingUrl(url).isEmpty(), qPrintable(url));
    }

    void remembersAppearanceOrUsesSystemDefault() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("settings.ini"));
        PortalConfig config(file);
        QCOMPARE(config.appearance(), QString());
        QVERIFY(config.setAppearance(QStringLiteral("dark")));
        QVERIFY(!config.setAppearance(QStringLiteral("invalid")));
        PortalConfig loaded(file);
        QCOMPARE(loaded.appearance(), QStringLiteral("dark"));
        QVERIFY(loaded.setAppearance({}));
        PortalConfig systemDefault(file);
        QCOMPARE(systemDefault.appearance(), QString());
    }


    void rejectsUnsafeAddresses() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        const auto original = config.portalUrl();
        QVERIFY(!config.setPortalUrl(QStringLiteral("http://example.org")));
        QVERIFY(!config.setPortalUrl(QStringLiteral("https://user:pass@example.org")));
        QVERIFY(!config.setPortalUrl(QStringLiteral("https://example.org/other")));
        QCOMPARE(config.portalUrl(), original);
    }

    void rejectsUnsafeSavedAddress() {
        QTemporaryDir directory;
        const auto file = directory.filePath(QStringLiteral("settings.ini"));
        QSettings settings(file, QSettings::IniFormat);
        settings.setValue(QStringLiteral("portalUrl"), QStringLiteral("http://example.org"));
        settings.sync();
        PortalConfig config(file);
        QCOMPARE(config.portalUrl(), QString());
        QVERIFY(!config.error().isEmpty());
        QVERIFY(config.setPortalUrl(QStringLiteral("https://company.example.org")));
        QCOMPARE(config.portalUrl(), QStringLiteral("https://company.example.org"));
    }
};

QTEST_MAIN(PortalConfigTest)
#include "PortalConfigTest.moc"
