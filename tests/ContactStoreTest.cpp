#include "ContactStore.hpp"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class ContactStoreTest final : public QObject {
    Q_OBJECT
private slots:
    void remembersAndExpiresAddresses() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        {
            ContactStore store(directory.path());
            QVERIFY(store.rememberAddress(QStringLiteral("ada@example.org")));
            QVERIFY(store.rememberAddress(QStringLiteral("ADA@example.org")));
            QCOMPARE(store.suggestions(QStringLiteral("ada")).size(), 1);
            const auto permissions = QFileInfo(directory.filePath(QStringLiteral("contacts.json"))).permissions();
            QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                      QFileDevice::ReadOther | QFileDevice::WriteOther)));
        }
        ContactStore reloaded(directory.path());
        QCOMPARE(reloaded.suggestions(QStringLiteral("ada")), QStringList({QStringLiteral("ADA@example.org")}));
        QFile file(directory.filePath(QStringLiteral("contacts.json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto old = QDateTime::currentDateTimeUtc().addYears(-1).addDays(-1).toString(Qt::ISODateWithMs);
        const auto bytes = QJsonDocument(QJsonObject{{QStringLiteral("version"), 2},
            {QStringLiteral("addresses"), QJsonArray{QJsonObject{
                {QStringLiteral("email"), QStringLiteral("ada@example.org")},
                {QStringLiteral("lastUsed"), old}}}}}).toJson();
        QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        ContactStore expired(directory.path());
        QVERIFY(expired.suggestions(QStringLiteral("ada")).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("addresses")).toArray().size(), 0);
    }

    void rejectsUnsupportedContactFormat() {
        QTemporaryDir directory;
        QFile file(directory.filePath(QStringLiteral("contacts.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const auto bytes = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
            {QStringLiteral("contacts"), QJsonArray{QJsonObject{
                {QStringLiteral("name"), QStringLiteral("Ada")},
                {QStringLiteral("email"), QStringLiteral("ada@example.org")}}}}}).toJson();
        QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        ContactStore store(directory.path());
        QVERIFY(store.suggestions(QStringLiteral("ada")).isEmpty());
        QVERIFY(!store.error().isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("version")).toInt(), 1);
    }

    void rejectsInvalidInputAndPreservesBrokenFile() {
        QTemporaryDir directory;
        ContactStore store(directory.path());
        QVERIFY(!store.rememberAddress(QStringLiteral("not-an-address")));
        QVERIFY(store.suggestions(QStringLiteral("not")).isEmpty());
        QFile file(directory.filePath(QStringLiteral("contacts.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("invalid"), 7);
        file.close();
        ContactStore broken(directory.path());
        QVERIFY(!broken.error().isEmpty());
        QVERIFY(!broken.rememberAddress(QStringLiteral("ada@example.org")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("invalid"));
    }

    void searchRanksInitialsAndContainsMatches() {
        QTemporaryDir directory;
        ContactStore store(directory.path());
        QVERIFY(store.matchScore(QStringLiteral("AK"), QStringLiteral("Alexander Kamzelak")) > 0);
        QCOMPARE(store.matchScore(QStringLiteral("AK"), QStringLiteral("Alex Brandom")), 0);
        QVERIFY(store.matchScore(QStringLiteral("alex"), QStringLiteral("Alexander Kamzelak")) >
                store.matchScore(QStringLiteral("alex"), QStringLiteral("The Alexander Kamzelak Team")));
        QVERIFY(store.matchScore(QStringLiteral("yuna morgen"), QStringLiteral("Yuna Morgenstern")) > 0);
    }

    void boundsRecentAddressesAndRejectsOversizedFiles() {
        QTemporaryDir directory;
        ContactStore store(directory.path());
        for (int index = 0; index < 501; ++index)
            QVERIFY(store.rememberAddress(QStringLiteral("person%1@example.org").arg(index)));
        QFile file(directory.filePath(QStringLiteral("contacts.json")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(file.readAll()).object()
            .value(QStringLiteral("addresses")).toArray();
        QCOMPARE(saved.size(), 500);
        QCOMPARE(saved.last().toObject().value(QStringLiteral("email")).toString(),
                 QStringLiteral("person1@example.org"));
        file.close();
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(QByteArray(1024 * 1024 + 1, 'x')), qint64(1024 * 1024 + 1));
        file.close();
        ContactStore oversized(directory.path());
        QVERIFY(!oversized.error().isEmpty());
        QVERIFY(!oversized.rememberAddress(QStringLiteral("new@example.org")));
        QCOMPARE(file.size(), qint64(1024 * 1024 + 1));
    }
};
QTEST_MAIN(ContactStoreTest)
#include "ContactStoreTest.moc"
