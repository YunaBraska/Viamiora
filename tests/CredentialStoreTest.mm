#include "CredentialStore.hpp"

#include <QUuid>
#include <QtTest>
#import <Foundation/Foundation.h>
#include <Security/Security.h>

class CredentialStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsSeparateLoginsForOnePortal() {
        const auto host = QStringLiteral("https://test-%1.invalid").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        const auto otherHost = QStringLiteral("https://test-%1.invalid").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        struct Cleanup {
            QString host;
            QString otherHost;
            ~Cleanup() {
                static_cast<void>(CredentialStore::remove(host, QStringLiteral("first@example.invalid"), nullptr));
                static_cast<void>(CredentialStore::remove(host, QStringLiteral("second@example.invalid"), nullptr));
                static_cast<void>(CredentialStore::remove(otherHost, QStringLiteral("first@example.invalid"), nullptr));
            }
        } cleanup{host, otherHost};
        QString error;
        QVERIFY(!CredentialStore::load(host, QStringLiteral("first@example.invalid"), &error));
        QVERIFY(CredentialStore::accounts(host).isEmpty());
        QVERIFY(error.isEmpty());

        QVERIFY2(CredentialStore::save(host, {QStringLiteral("first@example.invalid"), QStringLiteral("secret-a")}, &error), qPrintable(error));
        QVERIFY(!CredentialStore::accounts(host).isEmpty());
        auto saved = CredentialStore::load(host, QStringLiteral("first@example.invalid"), &error);
        QVERIFY2(saved.has_value(), qPrintable(error));
        QCOMPARE(saved->username, QStringLiteral("first@example.invalid"));
        QCOMPARE(saved->password, QStringLiteral("secret-a"));
        QVERIFY2(CredentialStore::save(host, {QStringLiteral("first@example.invalid"), QStringLiteral("secret-a")}, &error), qPrintable(error));

        QVERIFY2(CredentialStore::save(host, {QStringLiteral("second@example.invalid"), QStringLiteral("secret-b")}, &error), qPrintable(error));
        QCOMPARE(CredentialStore::accounts(host).size(), 2);
        saved = CredentialStore::load(host, QStringLiteral("first@example.invalid"), &error);
        QVERIFY(saved.has_value());
        QCOMPARE(saved->password, QStringLiteral("secret-a"));
        saved = CredentialStore::load(host, QStringLiteral("second@example.invalid"), &error);
        QVERIFY2(saved.has_value(), qPrintable(error));
        QCOMPARE(saved->username, QStringLiteral("second@example.invalid"));
        QCOMPARE(saved->password, QStringLiteral("secret-b"));
        QVERIFY2(CredentialStore::save(otherHost, {QStringLiteral("first@example.invalid"), QStringLiteral("other-secret")}, &error), qPrintable(error));
        QCOMPARE(CredentialStore::accounts(otherHost), QStringList({QStringLiteral("first@example.invalid")}));
        const auto otherSaved = CredentialStore::load(otherHost, QStringLiteral("first@example.invalid"), &error);
        QVERIFY(otherSaved.has_value());
        QCOMPARE(otherSaved->password, QStringLiteral("other-secret"));
        const auto firstSaved = CredentialStore::load(host, QStringLiteral("first@example.invalid"), &error);
        QVERIFY(firstSaved.has_value());
        QCOMPARE(firstSaved->password, QStringLiteral("secret-a"));

        QVERIFY2(CredentialStore::remove(host, QStringLiteral("second@example.invalid"), &error), qPrintable(error));
        QVERIFY(!CredentialStore::accounts(host).isEmpty());
        QVERIFY(CredentialStore::load(host, QStringLiteral("first@example.invalid"), &error).has_value());
        QVERIFY(!CredentialStore::load(host, QStringLiteral("second@example.invalid"), &error));
        QVERIFY(error.isEmpty());
    }


};

QTEST_GUILESS_MAIN(CredentialStoreTest)
#include "CredentialStoreTest.moc"
