#include "BrandingService.hpp"
#include "PortalConfig.hpp"
#include "PortalRequest.hpp"
#include "PortalSession.hpp"
#include "MeetingBooking.hpp"
#include "MeetingInvitation.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QUrlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

struct RemoveCredential {
    QString origin;
    QString username;
    ~RemoveCredential() { static_cast<void>(CredentialStore::remove(origin, username, nullptr)); }
};

class PortalSessionTest final : public QObject {
    Q_OBJECT

private slots:
    void retryAfterHonorsSecondsAndHttpDate() {
        const auto now = QDateTime(QDate(2026, 11, 19), QTime(12, 0), QTimeZone::UTC)
            .toMSecsSinceEpoch();
        QCOMPARE(portalRetryAfterDeadlineMs("600", now), now + 600000);
        QCOMPARE(portalRetryAfterDeadlineMs("invalid", now), qint64(0));
        QCOMPARE(portalRetryAfterDeadlineMs("999999999", now), now + 7LL * 24 * 60 * 60 * 1000);
        const auto httpDate = QDateTime::fromMSecsSinceEpoch(now + 600000, QTimeZone::UTC)
            .toString(Qt::RFC2822Date).toLatin1();
        QCOMPARE(portalRetryAfterDeadlineMs(httpDate, now), now + 600000);
    }

    void nativeRequestsUseSelectedOrSystemLanguage() {
        QCoreApplication::setApplicationVersion(QStringLiteral(VIAMIORA_VERSION));
        const QUrl url(QStringLiteral("https://example.invalid/"));
        const auto automatic = portalRequest(url, "application/json");
        QCOMPARE(automatic.rawHeader("Accept"), QByteArray("application/json"));
        QVERIFY(automatic.rawHeader("User-Agent").startsWith("Viamiora/" VIAMIORA_VERSION " ("));
        const auto systemLanguages = QLocale::system().uiLanguages();
        const auto expected = systemLanguages.isEmpty() ? QByteArray("en")
            : systemLanguages.first().toLatin1();
        QVERIFY(automatic.rawHeader("Accept-Language").startsWith(expected));
        const auto german = portalRequest(url, "text/html", QStringLiteral("de"));
        QVERIFY(german.rawHeader("Accept-Language").startsWith("de"));
        QVERIFY(german.rawHeader("Accept-Language").contains("en"));
    }

    void signingOutWithoutSessionCanBeRepeated() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        PortalSession session(config);

        session.signOut();
        QVERIFY(!session.signedIn());
        QCOMPARE(session.status(), QStringLiteral("Signed out."));

        session.signOut();
        QVERIFY(!session.signedIn());
        QCOMPARE(session.status(), QStringLiteral("Signed out."));
    }

    void meetingActionsRejectMissingSession() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://127.0.0.1:1")));
        PortalSession session(config);

        session.fetchMeetingLinks(42);
        QVERIFY(!session.linksBusy());
        QVERIFY(session.meetingLinks().isEmpty());
        QVERIFY(session.linkStatus().contains(QStringLiteral("no longer available")));

        session.createMeeting(QStringLiteral("Example"), QDateTime::currentMSecsSinceEpoch() + 3600000,
                              QDateTime::currentMSecsSinceEpoch() + 7200000, 1, QStringLiteral("123456"));
        QVERIFY(!session.creatingMeeting());
        QVERIFY(session.creationStatus().contains(QStringLiteral("Check the meeting details")));
    }

    void configuredSignInStartsWithoutCrashing() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://127.0.0.1:1")));
        PortalSession session(config);

        session.signIn(QStringLiteral("local-test"), QStringLiteral("local-test"));
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 3000);
        QVERIFY(!session.signedIn());
        QVERIFY(session.status().contains(QStringLiteral("could not be loaded")));
    }

    void rejectsIncompleteSignInWithoutNetwork() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        PortalSession session(config);
        session.signIn(QStringLiteral("person@example.invalid"), QStringLiteral("password"));
        QCOMPARE(session.status(), QStringLiteral("Choose a valid portal address first."));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://127.0.0.1:1")));
        session.signIn(QString(), QStringLiteral("password"));
        QCOMPARE(session.status(), QStringLiteral("Enter your username and password."));
        session.signIn(QStringLiteral("person@example.invalid"), QString());
        QCOMPARE(session.status(), QStringLiteral("Enter your password."));
        QVERIFY(!session.busy());
    }

    void savedPasswordCanStartSignInWhilePrefillRuns() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        const auto origin = QStringLiteral("https://127.0.0.1:1");
        const auto username = QStringLiteral("prefill-test@example.invalid");
        QVERIFY(config.setPortalUrl(origin));
        RemoveCredential cleanup{origin, username};
        QString error;
        QVERIFY2(CredentialStore::save(origin, {username, QStringLiteral("local-test")}, &error), qPrintable(error));
        PortalSession session(config);
        QTRY_VERIFY_WITH_TIMEOUT(session.savedAccounts().contains(username), 3000);

        session.requestSavedPassword(username);
        session.signIn(username, {});
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 3000);
        QVERIFY(!session.signedIn());
        QVERIFY(session.status().contains(QStringLiteral("could not be loaded")));
    }

    void repeatedPrefillRequestsReadOneSavedPassword() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        const auto origin = QStringLiteral("https://prefill-%1.invalid")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        const auto username = QStringLiteral("prefill@example.invalid");
        QVERIFY(config.setPortalUrl(origin));
        RemoveCredential cleanup{origin, username};
        QString error;
        QVERIFY2(CredentialStore::save(origin, {username, QStringLiteral("local-test")}, &error), qPrintable(error));
        PortalSession session(config);
        QTRY_VERIFY_WITH_TIMEOUT(session.savedAccounts().contains(username), 3000);

        QSignalSpy changed(&session, &PortalSession::prefillChanged);
        session.requestSavedPassword(username);
        session.requestSavedPassword(username);
        QTRY_COMPARE_WITH_TIMEOUT(session.prefillPassword(), QStringLiteral("local-test"), 3000);
        QCOMPARE(changed.size(), 2);
        session.requestSavedPassword(username);
        QCOMPARE(changed.size(), 2);
    }

    void savedAccountsFollowCurrentPortal() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        const auto suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto first = QStringLiteral("https://first-%1.invalid").arg(suffix);
        const auto second = QStringLiteral("https://second-%1.invalid").arg(suffix);
        const auto firstUser = QStringLiteral("first@example.invalid");
        const auto secondUser = QStringLiteral("second@example.invalid");
        QVERIFY(config.setPortalUrl(first));
        QVERIFY(config.setPortalUrl(second));
        QVERIFY(config.setPortalUrl(first));
        RemoveCredential cleanupFirst{first, firstUser};
        RemoveCredential cleanupSecond{second, secondUser};
        QString error;
        QVERIFY2(CredentialStore::save(first, {firstUser, QStringLiteral("local-test")}, &error), qPrintable(error));
        QVERIFY2(CredentialStore::save(second, {secondUser, QStringLiteral("local-test")}, &error), qPrintable(error));

        PortalSession session(config);
        QTRY_COMPARE_WITH_TIMEOUT(session.savedAccounts(), QStringList({firstUser}), 3000);
        QVERIFY(config.setPortalUrl(second));
        QTRY_COMPARE_WITH_TIMEOUT(session.savedAccounts(), QStringList({secondUser}), 3000);
        QVERIFY(config.setPortalUrl(first));
        QTRY_COMPARE_WITH_TIMEOUT(session.savedAccounts(), QStringList({firstUser}), 3000);
    }

    void switchesAndForgetsSavedLoginAcrossPortals() {
        QTemporaryDir directory;
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        const auto first = QStringLiteral("https://127.0.0.1:1");
        const auto second = QStringLiteral("https://127.0.0.1:2");
        const auto firstUser = QStringLiteral("switch-%1@example.invalid")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        const auto secondUser = QStringLiteral("second@example.invalid");
        QVERIFY(config.setPortalUrl(first));
        config.rememberUsername(firstUser);
        QVERIFY(config.setPortalUrl(second));
        QVERIFY(config.setPortalUrl(first));
        RemoveCredential cleanupFirst{first, firstUser};
        RemoveCredential cleanupSecond{second, secondUser};
        QString error;
        QVERIFY2(CredentialStore::save(first, {firstUser, QStringLiteral("local-test")}, &error), qPrintable(error));
        QVERIFY2(CredentialStore::save(second, {secondUser, QStringLiteral("local-test")}, &error), qPrintable(error));
        PortalSession session(config);
        QTRY_VERIFY_WITH_TIMEOUT(session.savedAccounts().contains(firstUser), 3000);
        session.switchSavedLogin(second, secondUser);
        QTRY_COMPARE_WITH_TIMEOUT(config.portalUrl(), second, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(session.savedAccounts().contains(secondUser), 3000);
        session.forgetSavedLogin(first, firstUser);
        const QVariantMap forgotten{{QStringLiteral("origin"), first},
                                    {QStringLiteral("username"), firstUser}};
        QTRY_VERIFY_WITH_TIMEOUT(!session.savedLogins().contains(forgotten), 3000);
        QVERIFY(config.lastUsernameFor(first).isEmpty());
        QVERIFY(!CredentialStore::load(first, firstUser, nullptr).has_value());
        QCOMPARE(CredentialStore::accounts(second), QStringList({secondUser}));
    }

    void nativeBookingUsesThePortalForm() {
        const QByteArray html = R"(<form action="/book/?wizard=1&amp;wizard_current=1&amp;wizard_total=3&amp;popup=1"><input name="initial-pin" value="6107"><input name="lobby_pin"></form>)";
        QCOMPARE(MeetingBooking::formAction(html),
                 QStringLiteral("/book/?wizard=1&wizard_current=1&wizard_total=3&popup=1"));
        QCOMPARE(MeetingBooking::initialPin(html), QStringLiteral("6107"));
        QVERIFY(MeetingBooking::supportsModeratorPin(html));
        QVERIFY(!MeetingBooking::supportsModeratorPin(QByteArrayLiteral("<input name='pin'>")));
        QVERIFY(MeetingBooking::formAction(QByteArrayLiteral("<html>Sign in</html>")).isEmpty());
        QVERIFY(MeetingBooking::initialPin(QByteArrayLiteral("<input name='initial-pin' value='bad'>")).isEmpty());
        const auto start = QDateTime(QDate(2026, 11, 19), QTime(17, 0));
        const auto body = MeetingBooking::requestBody(QByteArrayLiteral("test-csrf"),
            QStringLiteral("Example"), start.toMSecsSinceEpoch(),
            start.addSecs(3600).toMSecsSinceEpoch(), 3,
            QStringLiteral("123456"), QStringLiteral("6107"), QStringLiteral("654321"));
        const QUrlQuery query(QString::fromUtf8(body));
        QCOMPARE(query.queryItemValue(QStringLiteral("title")), QStringLiteral("Example"));
        QCOMPARE(query.queryItemValue(QStringLiteral("meeting_type")), QStringLiteral("3"));
        QCOMPARE(query.queryItemValue(QStringLiteral("date_start")), QStringLiteral("2026-11-19"));
        QCOMPARE(query.queryItemValue(QStringLiteral("time_start")), QStringLiteral("17:00"));
        QCOMPARE(query.queryItemValue(QStringLiteral("time_stop")), QStringLiteral("18:00"));
        QCOMPARE(query.queryItemValue(QStringLiteral("pin")), QStringLiteral("123456"));
        QCOMPARE(query.queryItemValue(QStringLiteral("initial-pin")), QStringLiteral("6107"));
        QCOMPARE(query.queryItemValue(QStringLiteral("use_pin")), QStringLiteral("on"));
        QCOMPARE(query.queryItemValue(QStringLiteral("use_lobby_pin")), QStringLiteral("on"));
        QCOMPARE(query.queryItemValue(QStringLiteral("lobby_pin")), QStringLiteral("654321"));
        QCOMPARE(query.queryItemValue(QStringLiteral("source_meeting")), QString());
    }

    void bookingAcceptsConfirmedSuccessResponses() {
        const QUrl response(QStringLiteral("https://portal.example.invalid/meeting/104/invite/?wizard=1&wizard_current=2&wizard_total=3"));
        QCOMPARE(MeetingBooking::responseId(200, QNetworkReply::NoError, response), 104);
        QCOMPARE(MeetingBooking::responseId(500, QNetworkReply::InternalServerError, response), 0);
        QCOMPARE(MeetingBooking::responseId(200, QNetworkReply::NoError,
            QUrl(QStringLiteral("https://portal.example.invalid/book/?wizard_current=1"))), 0);
    }

    void bookingReconcilesOnlyOneNewExactMeeting() {
        const auto start = QDateTime::currentDateTime().addDays(1);
        const auto stop = start.addSecs(3600);
        const auto meeting = [&](int id, const QString &title) {
            return QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("title"), title},
                {QStringLiteral("start"), start.toString(Qt::ISODateWithMs)},
                {QStringLiteral("end"), stop.toString(Qt::ISODateWithMs)}};
        };
        const QSet<int> known{103};
        QCOMPARE(MeetingBooking::newMatchingId({meeting(103, QStringLiteral("Example")),
                                               meeting(104, QStringLiteral("Example"))},
                                              known, QStringLiteral("Example"),
                                              start.toMSecsSinceEpoch(), stop.toMSecsSinceEpoch()), 104);
        QCOMPARE(MeetingBooking::newMatchingId({meeting(104, QStringLiteral("Example")),
                                               meeting(105, QStringLiteral("Example"))},
                                              known, QStringLiteral("Example"),
                                              start.toMSecsSinceEpoch(), stop.toMSecsSinceEpoch()), 0);
        QCOMPARE(MeetingBooking::newMatchingId({meeting(104, QStringLiteral("Different"))},
                                              known, QStringLiteral("Example"),
                                              start.toMSecsSinceEpoch(), stop.toMSecsSinceEpoch()), 0);
    }

    void invitationUsesPortalSubjectAndMultipleAddresses() {
        const QByteArray html = R"(<form><input type="text" name="message_title" value="Team &amp; guests" required><div data-value="&lt;p&gt;Join &amp;amp; ready&lt;/p&gt;" data-name="message"></div></form>)";
        QCOMPARE(MeetingInvitation::subjectFromForm(html), QStringLiteral("Team & guests"));
        QCOMPARE(MeetingInvitation::bodyFromForm(html), QStringLiteral("<p>Join &amp; ready</p>"));
        const auto body = MeetingInvitation::requestBody(
            QByteArrayLiteral("test-csrf"),
            {QStringLiteral("one@example.invalid"), QStringLiteral("two@example.invalid")},
            QStringLiteral("Team & guests"), MeetingInvitation::bodyFromForm(html));
        const QUrlQuery query(QString::fromUtf8(body));
        QCOMPARE(query.queryItemValue(QStringLiteral("emails")), QStringLiteral("one@example.invalid two@example.invalid"));
        QCOMPARE(query.queryItemValue(QStringLiteral("message_title")), QStringLiteral("Team & guests"));
        QCOMPARE(query.queryItemValue(QStringLiteral("message")), QStringLiteral("<p>Join &amp; ready</p>"));
        QCOMPARE(query.queryItemValue(QStringLiteral("csrfmiddlewaretoken")), QStringLiteral("test-csrf"));
        QVERIFY(MeetingInvitation::subjectFromForm(QByteArrayLiteral("<html>Sign in</html>")).isEmpty());
        QVERIFY(MeetingInvitation::bodyFromForm(QByteArrayLiteral("<html>Sign in</html>")).isEmpty());
    }

    void invitationRecipientsPreferModeratorsAndSendOnce() {
        const auto recipients = MeetingInvitation::normalizeRecipients(
            {QStringLiteral("Guest@example.invalid"), QStringLiteral("creator@example.invalid"),
             QStringLiteral("guest@example.invalid")},
            {QStringLiteral("GUEST@example.invalid"), QStringLiteral("moderator@example.invalid")},
            QStringLiteral("creator@example.invalid"));
        QVERIFY(recipients.has_value());
        QCOMPARE(recipients->moderators, QStringList({QStringLiteral("creator@example.invalid"),
                                                     QStringLiteral("GUEST@example.invalid"),
                                                     QStringLiteral("moderator@example.invalid")}));
        QVERIFY(recipients->invitees.isEmpty());
        QVERIFY(!MeetingInvitation::normalizeRecipients({QStringLiteral("bad address")}, {}, {}).has_value());
    }

    void localHttpsJourneySignsInBooksInvitesSharesAndSignsOut() {
        const auto certificatePath = qEnvironmentVariable("VIAMIORA_TEST_TRUSTED_CERTIFICATE");
        const auto keyPath = qEnvironmentVariable("VIAMIORA_TEST_TRUSTED_KEY");
        if (certificatePath.isEmpty() || keyPath.isEmpty()) {
            QSKIP("Set VIAMIORA_TEST_TRUSTED_CERTIFICATE and VIAMIORA_TEST_TRUSTED_KEY to run the portal journey.");
        }
        QVERIFY(QFile::exists(certificatePath));
        QVERIFY(QFile::exists(keyPath));
        QCoreApplication::setApplicationVersion(QStringLiteral(VIAMIORA_VERSION));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto logPath = directory.filePath(QStringLiteral("invitations.json"));
        const auto start = QDateTime(QDate::currentDate().addDays(2), QTime(10, 0),
                                     QTimeZone::systemTimeZone());
        const auto end = start.addSecs(3600);
        QProcess server;
        struct StopServer {
            QProcess &process;
            ~StopServer() {
                if (process.state() != QProcess::NotRunning) {
                    process.kill();
                    process.waitForFinished(3000);
                }
            }
        } stopServer{server};
        server.setProcessChannelMode(QProcess::SeparateChannels);
        server.start(QStringLiteral("/usr/bin/python3"), {QStringLiteral("-u"),
            QStringLiteral(VIAMIORA_SOURCE_DIR "/tests/fake_portal.py"), certificatePath,
            keyPath, logPath, start.toString(Qt::ISODateWithMs), end.toString(Qt::ISODateWithMs),
            QStringLiteral(VIAMIORA_VERSION)});
        QVERIFY(server.waitForStarted(3000));
        QVERIFY(server.waitForReadyRead(3000));
        bool validPort = false;
        const auto port = QString::fromUtf8(server.readLine().trimmed()).toInt(&validPort);
        QVERIFY(validPort && port > 0);
        const auto origin = QStringLiteral("https://127.0.0.1:%1").arg(port);
        const auto username = QStringLiteral("creator@example.invalid");
        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(config.setPortalUrl(origin));
        QVERIFY(config.setLanguage(QStringLiteral("de")));
        BrandingService branding(config);
        branding.load(origin);
        const QUrl expectedLogo(origin + QStringLiteral("/site_media/brand.svg"));
        QElapsedTimer brandingWait;
        brandingWait.start();
        while (branding.logoUrl() != expectedLogo && brandingWait.elapsed() < 7000)
            QTest::qWait(50);
        if (branding.logoUrl() != expectedLogo) {
            QNetworkAccessManager probe;
            auto *reply = probe.get(portalRequest(QUrl(origin + QStringLiteral("/accounts/login/")),
                                                  "text/html", QStringLiteral("de")));
            QSignalSpy finished(reply, &QNetworkReply::finished);
            const bool completed = finished.wait(8000);
            const auto diagnostic = QStringLiteral("branding logo missing; probe finished=%1, error=%2, HTTP=%3, server=%4")
                .arg(completed)
                .arg(reply->errorString())
                .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                .arg(QString::fromUtf8(server.readAllStandardError().right(500)));
            QFAIL(qPrintable(diagnostic));
        }
        QCOMPARE(branding.accent(), QStringLiteral("#113355"));
        QFile brandingCount(directory.filePath(QStringLiteral("branding-login-count")));
        QVERIFY(brandingCount.open(QIODevice::ReadOnly));
        const auto firstBrandingRequestCount = brandingCount.readAll();
        brandingCount.close();
        QSignalSpy brandingChanges(&branding, &BrandingService::changed);
        branding.load(origin);
        QCOMPARE(brandingChanges.size(), 0);
        QTest::qWait(150);
        QVERIFY(brandingCount.open(QIODevice::ReadOnly));
        QCOMPARE(brandingCount.readAll(), firstBrandingRequestCount);
        brandingCount.close();
        RemoveCredential removeCredential{origin, username};

        QFile missingCsrf(directory.filePath(QStringLiteral("missing-login-csrf")));
        QVERIFY(missingCsrf.open(QIODevice::WriteOnly));
        missingCsrf.close();
        {
            PortalSession unsupported(config);
            unsupported.signIn(username, QStringLiteral("local-password"));
            QTRY_VERIFY_WITH_TIMEOUT(!unsupported.busy(), 7000);
            QVERIFY(!unsupported.signedIn());
            QVERIFY(unsupported.status().contains(QStringLiteral("sign-in form is not supported")));
            QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("login-post-count"))));
        }
        QVERIFY(missingCsrf.remove());
        PortalSession session(config);
        session.signIn(username, QStringLiteral("wrong-password"));
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        QVERIFY(!session.signedIn());
        QVERIFY(session.status().contains(QStringLiteral("not accepted")));
        QVERIFY(CredentialStore::accounts(origin).isEmpty());
        session.signIn(username, QStringLiteral("local-password"));
        QTRY_VERIFY_WITH_TIMEOUT(session.signedIn(), 7000);
        QTRY_VERIFY_WITH_TIMEOUT(session.savedAccounts().contains(username), 7000);
        QFile loginPosts(directory.filePath(QStringLiteral("login-post-count")));
        QVERIFY(loginPosts.open(QIODevice::ReadOnly));
        const auto beforeCurrentSwitch = loginPosts.readAll();
        loginPosts.close();
        session.switchSavedLogin(origin, username);
        QVERIFY(session.signedIn());
        QVERIFY(!session.busy());
        QVERIFY(loginPosts.open(QIODevice::ReadOnly));
        QCOMPARE(loginPosts.readAll(), beforeCurrentSwitch);
        loginPosts.close();
        QTRY_VERIFY_WITH_TIMEOUT(CredentialStore::load(origin, username, nullptr).has_value(), 7000);
        QFile expireOnce(directory.filePath(QStringLiteral("expire-upcoming-once")));
        QVERIFY(expireOnce.open(QIODevice::WriteOnly));
        expireOnce.close();
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(session.signedIn() && !session.busy(), 7000);
        QVERIFY(loginPosts.open(QIODevice::ReadOnly));
        QCOMPARE(loginPosts.readAll().toInt(), beforeCurrentSwitch.toInt() + 1);
        loginPosts.close();
        QVERIFY(session.browserCookies(QStringLiteral("https://other.invalid/")).isEmpty());
        QVERIFY(session.browserCookies(QStringLiteral("http://127.0.0.1/")).isEmpty());
        QByteArray cookieHeader;
        for (const auto &cookie : session.browserCookies(origin)) {
            const auto item = cookie.toMap();
            if (!cookieHeader.isEmpty()) cookieHeader.append("; ");
            cookieHeader.append(item.value(QStringLiteral("name")).toString().toUtf8());
            cookieHeader.append('=');
            cookieHeader.append(item.value(QStringLiteral("value")).toString().toUtf8());
        }
        QVERIFY(!cookieHeader.isEmpty());
        QNetworkAccessManager contractClient;
        auto restRequest = portalRequest(QUrl(origin + QStringLiteral("/api/v1/meeting/book/")),
                                         "application/json", QStringLiteral("de"));
        restRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        restRequest.setRawHeader("Cookie", cookieHeader);
        auto *invalidReply = contractClient.post(restRequest, QByteArrayLiteral("{"));
        QSignalSpy invalidFinished(invalidReply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(invalidFinished.size(), 1, 7000);
        QCOMPARE(invalidReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 400);
        invalidReply->deleteLater();
        auto *restReply = contractClient.post(restRequest, QByteArrayLiteral(
            R"({"title":"Synthetic API booking","source":null,"send_emails":false,"recipients":[],"moderators":[]})"));
        QSignalSpy restFinished(restReply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(restFinished.size(), 1, 7000);
        QCOMPARE(restReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        const auto restResult = QJsonDocument::fromJson(restReply->readAll()).object();
        QCOMPARE(restResult.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
        QCOMPARE(restResult.value(QStringLiteral("meeting")).toObject().value(QStringLiteral("id")).toInt(), 901);
        QCOMPARE(restResult.value(QStringLiteral("meeting")).toObject().value(QStringLiteral("invites")).toArray().size(), 0);
        restReply->deleteLater();

        auto homeRequest = portalRequest(QUrl(origin + QStringLiteral("/")), "text/html", QStringLiteral("de"));
        homeRequest.setRawHeader("Cookie", cookieHeader);
        auto *homeReply = contractClient.get(homeRequest);
        QSignalSpy homeFinished(homeReply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(homeFinished.size(), 1, 7000);
        QVERIFY(homeReply->readAll().contains("Via Outlook"));
        homeReply->deleteLater();

        auto unbookRequest = portalRequest(QUrl(origin + QStringLiteral("/api/v1/meeting/901/unbook/")),
                                           "application/json", QStringLiteral("de"));
        unbookRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        unbookRequest.setRawHeader("Cookie", cookieHeader);
        auto *unbookReply = contractClient.post(unbookRequest, QByteArrayLiteral("{}"));
        QSignalSpy unbookFinished(unbookReply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(unbookFinished.size(), 1, 7000);
        QCOMPARE(unbookReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 204);
        unbookReply->deleteLater();
        homeReply = contractClient.get(homeRequest);
        QSignalSpy cleanHomeFinished(homeReply, &QNetworkReply::finished);
        QTRY_COMPARE_WITH_TIMEOUT(cleanHomeFinished.size(), 1, 7000);
        QVERIFY(!homeReply->readAll().contains("Via Outlook"));
        homeReply->deleteLater();
        QFile restBookCount(directory.filePath(QStringLiteral("rest-book-count")));
        QVERIFY(restBookCount.open(QIODevice::ReadOnly));
        QCOMPARE(restBookCount.readAll(), QByteArray("2"));
        restBookCount.close();

        const auto firstHistoryDay = QDate::currentDate().addDays(-7);
        const auto lastHistoryDay = firstHistoryDay.addDays(6);
        session.loadMeetingWeek(firstHistoryDay.toString(Qt::ISODate),
                                lastHistoryDay.toString(Qt::ISODate));
        QTRY_VERIFY_WITH_TIMEOUT(!session.historicalBusy(), 7000);
        QCOMPARE(session.historicalMeetings().size(), 1);
        QCOMPARE(session.historicalMeetings().first().toMap().value(QStringLiteral("id")).toInt(), 320);
        session.fetchMeetingLinks(320);
        QTRY_VERIFY_WITH_TIMEOUT(!session.linksBusy(), 7000);
        QCOMPARE(session.meetingLinks().value(QStringLiteral("participantPin")).toString(),
                 QStringLiteral("0047"));
        QFile historyCount(directory.filePath(QStringLiteral("history-count")));
        QVERIFY(historyCount.open(QIODevice::ReadOnly));
        const auto firstHistoryCount = historyCount.readAll();
        historyCount.close();
        session.loadMeetingWeek(firstHistoryDay.toString(Qt::ISODate),
                                lastHistoryDay.toString(Qt::ISODate));
        QVERIFY(historyCount.open(QIODevice::ReadOnly));
        QCOMPARE(historyCount.readAll(), firstHistoryCount);
        historyCount.close();
        QFile limitedHistory(directory.filePath(QStringLiteral("rate-limit-history")));
        QVERIFY(limitedHistory.open(QIODevice::WriteOnly));
        limitedHistory.close();
        const auto olderFirst = firstHistoryDay.addDays(-7).toString(Qt::ISODate);
        const auto olderLast = lastHistoryDay.addDays(-7).toString(Qt::ISODate);
        session.loadMeetingWeek(olderFirst, olderLast);
        QTRY_VERIFY_WITH_TIMEOUT(!session.historicalBusy(), 7000);
        QVERIFY(session.historicalStatus().contains(QStringLiteral("could not be loaded")));
        QVERIFY(historyCount.open(QIODevice::ReadOnly));
        const auto rateLimitedHistoryCount = historyCount.readAll();
        historyCount.close();
        session.loadMeetingWeek(olderFirst, olderLast);
        QVERIFY(historyCount.open(QIODevice::ReadOnly));
        QCOMPARE(historyCount.readAll(), rateLimitedHistoryCount);
        historyCount.close();
        QVERIFY(limitedHistory.remove());
        session.loadMeetingWeek(firstHistoryDay.toString(Qt::ISODate),
                                lastHistoryDay.toString(Qt::ISODate));
        QCOMPARE(session.historicalMeetings().size(), 1);
        QVERIFY(historyCount.open(QIODevice::ReadOnly));
        QCOMPARE(historyCount.readAll(), rateLimitedHistoryCount);
        historyCount.close();
        QFile upcomingCount(directory.filePath(QStringLiteral("upcoming-count")));
        QVERIFY(upcomingCount.open(QIODevice::ReadOnly));
        const auto initialRefreshCount = upcomingCount.readAll();
        upcomingCount.close();
        QVERIFY(!session.refreshMeetingsIfDue(false));
        QVERIFY(upcomingCount.open(QIODevice::ReadOnly));
        QCOMPARE(upcomingCount.readAll(), initialRefreshCount);
        upcomingCount.close();
        QTRY_VERIFY_WITH_TIMEOUT(!session.bookingTypes().isEmpty(), 7000);
        session.inspectBookingType(1);
        QTRY_VERIFY_WITH_TIMEOUT(session.moderatorPinSupported(), 7000);

        QSignalSpy created(&session, &PortalSession::meetingCreated);
        session.createMeeting(QStringLiteral("Local journey"), start.toMSecsSinceEpoch(),
                              end.toMSecsSinceEpoch(), 1, QStringLiteral("123456"),
                              {QStringLiteral("guest@example.invalid"), username},
                              {QStringLiteral("GUEST@example.invalid")}, QStringLiteral("654321"));
        QTRY_COMPARE_WITH_TIMEOUT(created.size(), 1, 7000);
        QCOMPARE(created.first().at(0).toInt(), 321);
        QVERIFY(restBookCount.open(QIODevice::ReadOnly));
        QCOMPARE(restBookCount.readAll(), QByteArray("2"));
        restBookCount.close();
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting() && QFile::exists(logPath), 7000);
        QFile log(logPath);
        QVERIFY(log.open(QIODevice::ReadOnly));
        const auto invited = QJsonDocument::fromJson(log.readAll()).object();
        log.close();
        QCOMPARE(invited.value(QStringLiteral("moderator")).toArray().size(), 2);
        QCOMPARE(invited.value(QStringLiteral("participant")).toArray().size(), 0);

        QTRY_VERIFY_WITH_TIMEOUT(!session.meetings().isEmpty(), 7000);
        const auto listedMeeting = session.meetings().first().toMap();
        QVERIFY(!listedMeeting.value(QStringLiteral("manageable")).toBool());
        QVERIFY(listedMeeting.value(QStringLiteral("roomName")).toString().isEmpty());
        session.resolveCalendarMatches({QVariantMap{
            {QStringLiteral("title"), QStringLiteral("Local journey (Moderator)")},
            {QStringLiteral("startMs"), start.toMSecsSinceEpoch()},
            {QStringLiteral("endMs"), end.toMSecsSinceEpoch()},
            {QStringLiteral("url"), origin + QStringLiteral("/webapp/#/?conference=moderator&pin=0782")}
        }});
        QTRY_VERIFY_WITH_TIMEOUT(session.meetings().first().toMap().value(QStringLiteral("manageable")).toBool(), 7000);
        QCOMPARE(session.meetings().first().toMap().value(QStringLiteral("roomName")).toString(),
                 QStringLiteral("555123"));
        QVERIFY(session.meetings().first().toMap().value(QStringLiteral("identityUrl")).toString()
            .contains(QStringLiteral("conference=moderator")));
        const auto calendarUrl = session.calendarInvite(321);
        QVERIFY(calendarUrl.isLocalFile());
        QFile calendarFile(calendarUrl.toLocalFile());
        QVERIFY(calendarFile.open(QIODevice::ReadOnly));
        const auto calendarData = calendarFile.readAll();
        QVERIFY(calendarData.contains("SUMMARY:Local journey"));
        QVERIFY(calendarData.contains("BEGIN:VEVENT"));
        QVERIFY(!session.calendarInvite(999).isValid());

        QFile titleNewline(directory.filePath(QStringLiteral("calendar-title-newline")));
        QVERIFY(titleNewline.open(QIODevice::WriteOnly));
        titleNewline.close();
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        const auto escapedCalendarUrl = session.calendarInvite(321);
        QVERIFY(escapedCalendarUrl.isLocalFile());
        QFile escapedCalendarFile(escapedCalendarUrl.toLocalFile());
        QVERIFY(escapedCalendarFile.open(QIODevice::ReadOnly));
        const auto escapedCalendarData = escapedCalendarFile.readAll();
        QVERIFY(escapedCalendarData.contains("SUMMARY:Review\\nATTENDEE:mailto:unexpected@example.invalid"));
        QVERIFY(!escapedCalendarData.contains("\r\nATTENDEE:"));
        QVERIFY(titleNewline.remove());
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);

        session.fetchMeetingLinks(321);
        QTRY_VERIFY_WITH_TIMEOUT(!session.linksBusy(), 7000);
        QVERIFY(session.meetingLinks().value(QStringLiteral("participantUrl")).toString().contains(QStringLiteral("conference=guest")));
        QVERIFY(session.meetingLinks().value(QStringLiteral("moderatorUrl")).toString().contains(QStringLiteral("conference=moderator")));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("participantPin")).toString(), QStringLiteral("0047"));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("moderatorPin")).toString(), QStringLiteral("0782"));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("roomName")).toString(), QStringLiteral("555123"));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("description")).toString(),
                 QStringLiteral("Agenda\nReview the next release."));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("sip")).toString(), QStringLiteral("sip:555123@example.invalid"));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("participants")).toList().size(), 0);
        QCOMPARE(session.meetingLinks().value(QStringLiteral("moderators")).toList().size(), 2);
        QVERIFY(session.meetingLinks().value(QStringLiteral("canChange")).toBool());

        QFile slowDetail(directory.filePath(QStringLiteral("slow-detail")));
        QVERIFY(slowDetail.open(QIODevice::WriteOnly));
        slowDetail.close();
        session.fetchMeetingLinks(321);
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory.filePath(QStringLiteral("detail-started"))), 7000);
        session.cancelMeetingLinks();
        QVERIFY(!session.linksBusy());
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory.filePath(QStringLiteral("detail-finished"))), 7000);
        QVERIFY(session.meetingLinks().isEmpty());
        QVERIFY(session.linkStatus().isEmpty());
        QVERIFY(slowDetail.remove());

        QFile portalGuestLink(directory.filePath(QStringLiteral("portal-guest-link")));
        QVERIFY(portalGuestLink.open(QIODevice::WriteOnly));
        portalGuestLink.close();
        session.fetchMeetingLinks(321);
        QTRY_VERIFY_WITH_TIMEOUT(!session.linksBusy(), 7000);
        const auto guestLink = session.meetingLinks().value(QStringLiteral("participantUrl")).toString();
        QVERIFY(guestLink.contains(QStringLiteral("/webapp/#/?conference=room&pin=0047")));
        QVERIFY(!guestLink.contains(QStringLiteral("/meeting/321/")));
        QFile wrongModeratorPin(directory.filePath(QStringLiteral("wrong-moderator-pin")));
        QVERIFY(wrongModeratorPin.open(QIODevice::WriteOnly));
        wrongModeratorPin.close();
        session.fetchMeetingLinks(321);
        QTRY_VERIFY_WITH_TIMEOUT(!session.linksBusy(), 7000);
        QCOMPARE(session.meetingLinks().value(QStringLiteral("participantUrl")).toString(),
                 origin + QStringLiteral("/meeting/321/"));
        QVERIFY(wrongModeratorPin.remove());
        QVERIFY(portalGuestLink.remove());

        QVERIFY(!session.inviteToMeeting(321, {QStringLiteral("GUEST@example.invalid")},
                                         {username}));
        QVERIFY(session.invitationStatus().contains(QStringLiteral("already invited")));
        QVERIFY(session.inviteToMeeting(321,
            {QStringLiteral("guest@example.invalid"), QStringLiteral("newguest@example.invalid"),
             QStringLiteral("NEW&mod@example.invalid")},
            {username, QStringLiteral("new&mod@example.invalid")}));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting() && !session.linksBusy(), 7000);
        QVERIFY(log.open(QIODevice::ReadOnly));
        const auto additionalInvites = QJsonDocument::fromJson(log.readAll()).object();
        log.close();
        QCOMPARE(additionalInvites.value(QStringLiteral("participant")).toArray().size(), 1);
        QCOMPARE(additionalInvites.value(QStringLiteral("participant")).toArray().at(0).toString(),
                 QStringLiteral("newguest@example.invalid"));
        QCOMPARE(additionalInvites.value(QStringLiteral("moderator")).toArray().size(), 1);
        QCOMPARE(additionalInvites.value(QStringLiteral("moderator")).toArray().at(0).toString(),
                 QStringLiteral("new&mod@example.invalid"));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("participants")).toList().size(), 1);
        QCOMPARE(session.meetingLinks().value(QStringLiteral("moderators")).toList().size(), 3);
        const int participantId = session.meetingLinks().value(QStringLiteral("participants"))
            .toList().first().toMap().value(QStringLiteral("id")).toInt();
        QVERIFY(participantId > 0);
        QVERIFY(!session.removeInvitation(999, participantId));
        QVERIFY(!session.removeInvitation(321, 999));
        QVERIFY(session.removeInvitation(321, participantId));
        QVERIFY(!session.removeInvitation(321, participantId));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting() && !session.linksBusy(), 7000);
        QCOMPARE(session.meetingLinks().value(QStringLiteral("participants")).toList().size(), 0);
        QFile removalCount(directory.filePath(QStringLiteral("remove-post-count")));
        QVERIFY(removalCount.open(QIODevice::ReadOnly));
        QCOMPARE(removalCount.readAll(), QByteArray("1"));
        removalCount.close();
        const int moderatorId = session.meetingLinks().value(QStringLiteral("moderators"))
            .toList().last().toMap().value(QStringLiteral("id")).toInt();
        QFile brokenRemoval(directory.filePath(QStringLiteral("broken-removal-form")));
        QVERIFY(brokenRemoval.open(QIODevice::WriteOnly));
        brokenRemoval.close();
        QVERIFY(session.removeInvitation(321, moderatorId));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting(), 7000);
        QVERIFY(session.invitationStatus().contains(QStringLiteral("could not be removed")));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("moderators")).toList().size(), 3);
        QVERIFY(removalCount.open(QIODevice::ReadOnly));
        QCOMPARE(removalCount.readAll(), QByteArray("1"));
        removalCount.close();
        QVERIFY(brokenRemoval.remove());
        QFile rejectedRemoval(directory.filePath(QStringLiteral("reject-removal-post")));
        QVERIFY(rejectedRemoval.open(QIODevice::WriteOnly));
        rejectedRemoval.close();
        QVERIFY(session.removeInvitation(321, moderatorId));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting(), 7000);
        QVERIFY(session.invitationStatus().contains(QStringLiteral("could not be removed")));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("moderators")).toList().size(), 3);
        QVERIFY(removalCount.open(QIODevice::ReadOnly));
        QCOMPARE(removalCount.readAll(), QByteArray("2"));
        removalCount.close();
        QVERIFY(rejectedRemoval.remove());
        QFile unconfirmedRemoval(directory.filePath(QStringLiteral("pretend-remove-success")));
        QVERIFY(unconfirmedRemoval.open(QIODevice::WriteOnly));
        unconfirmedRemoval.close();
        QVERIFY(session.removeInvitation(321, moderatorId));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting() && !session.linksBusy(), 7000);
        QVERIFY(session.invitationStatus().contains(QStringLiteral("could not be confirmed")));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("moderators")).toList().size(), 3);
        QVERIFY(removalCount.open(QIODevice::ReadOnly));
        QCOMPARE(removalCount.readAll(), QByteArray("3"));
        removalCount.close();
        QVERIFY(unconfirmedRemoval.remove());
        QFile inlineRemoval(directory.filePath(QStringLiteral("inline-remove-success")));
        QVERIFY(inlineRemoval.open(QIODevice::WriteOnly));
        inlineRemoval.close();
        QVERIFY(session.removeInvitation(321, moderatorId));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting() && !session.linksBusy(), 7000);
        QVERIFY(session.invitationStatus().contains(QStringLiteral("Invitation removed")));
        QCOMPARE(session.meetingLinks().value(QStringLiteral("moderators")).toList().size(), 2);
        QVERIFY(inlineRemoval.remove());
        QFile crossOriginRemoval(directory.filePath(QStringLiteral("cross-origin-removal-form")));
        QVERIFY(crossOriginRemoval.open(QIODevice::WriteOnly));
        crossOriginRemoval.close();
        const int remainingModeratorId = session.meetingLinks().value(QStringLiteral("moderators"))
            .toList().last().toMap().value(QStringLiteral("id")).toInt();
        QVERIFY(session.removeInvitation(321, remainingModeratorId));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting(), 7000);
        QVERIFY(session.invitationStatus().contains(QStringLiteral("could not be removed")));
        QVERIFY(removalCount.open(QIODevice::ReadOnly));
        QCOMPARE(removalCount.readAll(), QByteArray("4"));
        removalCount.close();
        QVERIFY(crossOriginRemoval.remove());
        QVERIFY(log.open(QIODevice::ReadOnly));
        const auto sentBeforeBrokenForm = log.readAll();
        log.close();
        QFile brokenInvitationForm(directory.filePath(QStringLiteral("broken-invitation-form")));
        QVERIFY(brokenInvitationForm.open(QIODevice::WriteOnly));
        brokenInvitationForm.close();
        QVERIFY(session.inviteToMeeting(321, {QStringLiteral("unsent@example.invalid")}, {}));
        QTRY_VERIFY_WITH_TIMEOUT(!session.inviting(), 7000);
        QVERIFY(session.invitationStatus().contains(QStringLiteral("could not be confirmed")));
        QVERIFY(log.open(QIODevice::ReadOnly));
        QCOMPARE(log.readAll(), sentBeforeBrokenForm);
        log.close();
        QVERIFY(brokenInvitationForm.remove());
        QFile malformedDetail(directory.filePath(QStringLiteral("malformed-detail-json")));
        QVERIFY(malformedDetail.open(QIODevice::WriteOnly));
        malformedDetail.close();
        session.fetchMeetingLinks(321);
        QTRY_VERIFY_WITH_TIMEOUT(!session.linksBusy(), 7000);
        QVERIFY(session.meetingLinks().isEmpty());
        QVERIFY(session.linkStatus().contains(QStringLiteral("unsupported meeting detail")));
        QVERIFY(session.signedIn());
        QCOMPARE(session.meetings().size(), 1);
        QVERIFY(malformedDetail.remove());
        session.fetchMeetingLinks(321);
        QTRY_VERIFY_WITH_TIMEOUT(!session.linksBusy(), 7000);
        QVERIFY(!session.inviteToMeeting(999, {QStringLiteral("other@example.invalid")}, {}));
        QFile unsupportedInvites(directory.filePath(QStringLiteral("unsupported-invites")));
        QVERIFY(unsupportedInvites.open(QIODevice::WriteOnly));
        unsupportedInvites.close();
        session.fetchMeetingLinks(321);
        QTRY_VERIFY_WITH_TIMEOUT(!session.linksBusy(), 7000);
        QVERIFY(!session.meetingLinks().value(QStringLiteral("canChange")).toBool());
        QVERIFY(!session.inviteToMeeting(321, {QStringLiteral("other@example.invalid")}, {}));
        QSignalSpy meetingChanges(&session, &PortalSession::meetingsChanged);
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        QCOMPARE(meetingChanges.size(), 0);
        QFile failedRefresh(directory.filePath(QStringLiteral("fail-next-refresh")));
        QVERIFY(failedRefresh.open(QIODevice::WriteOnly));
        failedRefresh.close();
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        QVERIFY(session.signedIn());
        QCOMPARE(session.meetings().size(), 1);
        QCOMPARE(meetingChanges.size(), 0);
        QFile malformedUpcoming(directory.filePath(QStringLiteral("malformed-upcoming-json")));
        QVERIFY(malformedUpcoming.open(QIODevice::WriteOnly));
        malformedUpcoming.close();
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        QVERIFY(session.signedIn());
        QCOMPARE(session.meetings().size(), 1);
        QCOMPARE(meetingChanges.size(), 0);
        QVERIFY(session.status().contains(QStringLiteral("unsupported update")));
        QVERIFY(malformedUpcoming.remove());
        QFile brokenBookingForm(directory.filePath(QStringLiteral("broken-booking-form")));
        QVERIFY(brokenBookingForm.open(QIODevice::WriteOnly));
        brokenBookingForm.close();
        session.createMeeting(QStringLiteral("Local journey"), start.toMSecsSinceEpoch(),
                              end.toMSecsSinceEpoch(), 1, QStringLiteral("123456"));
        QTRY_VERIFY_WITH_TIMEOUT(!session.creatingMeeting(), 7000);
        QVERIFY(session.creationStatus().contains(QStringLiteral("booking form is unavailable")));
        QCOMPARE(created.size(), 1);
        QVERIFY(brokenBookingForm.remove());
        QFile rejectedBooking(directory.filePath(QStringLiteral("reject-booking-post")));
        QVERIFY(rejectedBooking.open(QIODevice::WriteOnly));
        rejectedBooking.close();
        session.createMeeting(QStringLiteral("Local journey"), start.toMSecsSinceEpoch(),
                              end.toMSecsSinceEpoch(), 1, QStringLiteral("123456"));
        QTRY_VERIFY_WITH_TIMEOUT(!session.creatingMeeting(), 7000);
        QVERIFY(session.creationStatus().contains(QStringLiteral("could not accept")));
        QCOMPARE(created.size(), 1);
        QVERIFY(rejectedBooking.remove());
        QFile bookPosts(directory.filePath(QStringLiteral("book-post-count")));
        QVERIFY(bookPosts.open(QIODevice::ReadOnly));
        const auto beforeAmbiguousBooking = bookPosts.readAll().toInt();
        bookPosts.close();
        QFile ambiguousBooking(directory.filePath(QStringLiteral("ambiguous-booking-response")));
        QVERIFY(ambiguousBooking.open(QIODevice::WriteOnly));
        ambiguousBooking.close();
        session.createMeeting(QStringLiteral("Local journey"), start.toMSecsSinceEpoch(),
                              end.toMSecsSinceEpoch(), 1, QStringLiteral("123456"));
        QTRY_VERIFY_WITH_TIMEOUT(!session.creatingMeeting() && !session.busy(), 7000);
        QVERIFY(session.creationStatus().contains(QStringLiteral("could not be confirmed")));
        QCOMPARE(created.size(), 1);
        QVERIFY(bookPosts.open(QIODevice::ReadOnly));
        QCOMPARE(bookPosts.readAll().toInt(), beforeAmbiguousBooking + 1);
        bookPosts.close();
        QVERIFY(ambiguousBooking.remove());
        QFile reconciledBooking(directory.filePath(QStringLiteral("ambiguous-booking-created")));
        QVERIFY(reconciledBooking.open(QIODevice::WriteOnly));
        reconciledBooking.close();
        session.createMeeting(QStringLiteral("Local journey"), start.toMSecsSinceEpoch(),
                              end.toMSecsSinceEpoch(), 1, QStringLiteral("123456"), {}, {},
                              QStringLiteral("654321"));
        QTRY_COMPARE_WITH_TIMEOUT(created.size(), 2, 7000);
        QTRY_VERIFY_WITH_TIMEOUT(!session.creatingMeeting() && !session.busy(), 7000);
        QCOMPARE(created.at(1).at(0).toInt(), 323);
        QCOMPARE(session.meetings().size(), 2);
        QVERIFY(reconciledBooking.remove());
        QFile rateLimit(directory.filePath(QStringLiteral("rate-limit-upcoming")));
        QVERIFY(rateLimit.open(QIODevice::WriteOnly));
        rateLimit.close();
        const auto beforeRateLimit = QDateTime::currentMSecsSinceEpoch();
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        QVERIFY(session.signedIn());
        QVERIFY(session.automaticRefreshDueAtMs() >= beforeRateLimit + 600000);
        QVERIFY(upcomingCount.open(QIODevice::ReadOnly));
        const auto rateLimitedCount = upcomingCount.readAll();
        upcomingCount.close();
        QVERIFY(!session.refreshMeetingsIfDue(false));
        QVERIFY(upcomingCount.open(QIODevice::ReadOnly));
        QCOMPARE(upcomingCount.readAll(), rateLimitedCount);
        upcomingCount.close();
        QVERIFY(rateLimit.remove());
        QVERIFY(QFile::exists(calendarUrl.toLocalFile()));
        session.signOut();
        QTRY_VERIFY_WITH_TIMEOUT(!session.signedIn() && !session.busy(), 7000);
        QVERIFY(!QFile::exists(calendarUrl.toLocalFile()));
        QVERIFY(session.historicalMeetings().isEmpty());
        session.signIn(username, QStringLiteral("local-password"));
        QTRY_VERIFY_WITH_TIMEOUT(session.signedIn(), 7000);
        QFile unresolvedMeeting(directory.filePath(QStringLiteral("include-unresolvable-meeting")));
        QVERIFY(unresolvedMeeting.open(QIODevice::WriteOnly));
        unresolvedMeeting.close();
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        QCOMPARE(session.meetings().size(), 3);
        const QVariantList otherCalendarCall{QVariantMap{
            {QStringLiteral("title"), QStringLiteral("Other meeting")},
            {QStringLiteral("startMs"), start.toMSecsSinceEpoch()},
            {QStringLiteral("endMs"), end.toMSecsSinceEpoch()},
            {QStringLiteral("url"), QStringLiteral("https://video.example.invalid/webapp/#/?conference=other")}
        }};
        session.resolveCalendarMatches(otherCalendarCall);
        QFile detailCount(directory.filePath(QStringLiteral("detail-322-count")));
        QTRY_VERIFY_WITH_TIMEOUT(detailCount.exists(), 7000);
        QVERIFY(detailCount.open(QIODevice::ReadOnly));
        const auto firstDetailCount = detailCount.readAll();
        detailCount.close();
        QTest::qWait(200);
        session.resolveCalendarMatches(otherCalendarCall);
        QTest::qWait(150);
        QVERIFY(detailCount.open(QIODevice::ReadOnly));
        QCOMPARE(detailCount.readAll(), firstDetailCount);
        detailCount.close();
        QVERIFY(unresolvedMeeting.remove());
        QVERIFY(CredentialStore::remove(origin, username, nullptr));
        QVERIFY(expireOnce.open(QIODevice::WriteOnly));
        expireOnce.close();
        session.refreshMeetings();
        QTRY_VERIFY_WITH_TIMEOUT(!session.signedIn() && !session.busy(), 7000);
        QVERIFY(session.meetings().isEmpty());
        QVERIFY(session.status().contains(QStringLiteral("session expired")));
        session.signOut();
        QTRY_VERIFY_WITH_TIMEOUT(!session.signedIn() && !session.busy(), 7000);
        server.kill();
        QVERIFY(server.waitForFinished(3000));
    }

    void localHttpsRejectsUnknownCertificate() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto certificatePath = directory.filePath(QStringLiteral("certificate.pem"));
        const auto keyPath = directory.filePath(QStringLiteral("key.pem"));
        const auto logPath = directory.filePath(QStringLiteral("invitations.json"));
        QCOMPARE(QProcess::execute(QStringLiteral("openssl"), {
            QStringLiteral("req"), QStringLiteral("-x509"), QStringLiteral("-newkey"),
            QStringLiteral("rsa:2048"), QStringLiteral("-nodes"), QStringLiteral("-keyout"),
            keyPath, QStringLiteral("-out"), certificatePath, QStringLiteral("-days"),
            QStringLiteral("1"), QStringLiteral("-subj"), QStringLiteral("/CN=127.0.0.1"),
            QStringLiteral("-addext"), QStringLiteral("subjectAltName=IP:127.0.0.1")}), 0);

        QProcess server;
        struct StopServer {
            QProcess &process;
            ~StopServer() {
                if (process.state() != QProcess::NotRunning) {
                    process.kill();
                    process.waitForFinished(3000);
                }
            }
        } stopServer{server};
        server.setProcessChannelMode(QProcess::SeparateChannels);
        const auto start = QDateTime::currentDateTimeUtc().addDays(2);
        server.start(QStringLiteral("/usr/bin/python3"), {QStringLiteral("-u"),
            QStringLiteral(VIAMIORA_SOURCE_DIR "/tests/fake_portal.py"), certificatePath,
            keyPath, logPath, start.toString(Qt::ISODateWithMs),
            start.addSecs(3600).toString(Qt::ISODateWithMs), QStringLiteral(VIAMIORA_VERSION)});
        QVERIFY(server.waitForStarted(3000));
        QVERIFY(server.waitForReadyRead(3000));
        bool validPort = false;
        const auto port = QString::fromUtf8(server.readLine().trimmed()).toInt(&validPort);
        QVERIFY(validPort && port > 0);

        PortalConfig config(directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(config.setPortalUrl(QStringLiteral("https://127.0.0.1:%1").arg(port)));
        PortalSession session(config);
        session.signIn(QStringLiteral("creator@example.invalid"), QStringLiteral("local-password"));
        QTRY_VERIFY_WITH_TIMEOUT(!session.busy(), 7000);
        QVERIFY(!session.signedIn());
        QVERIFY(session.status().contains(QStringLiteral("Secure connection failed")));
    }
};

QTEST_MAIN(PortalSessionTest)
#include "PortalSessionTest.moc"
