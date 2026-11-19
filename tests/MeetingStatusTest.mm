#include "MeetingStatus.hpp"

#include <QSignalSpy>
#include <QtTest>

class MeetingStatusTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsOneMenuItemAcrossMeetingChangesAndRemovesItOnLeave() {
        MeetingStatus status;
        QSignalSpy changes(&status, &MeetingStatus::changed);
        QVERIFY(!status.active());

        status.setMeeting(QStringLiteral("https://meet.example.invalid/room/one"),
                          QStringLiteral("First call"));
        QVERIFY(status.active());
        status.setMeeting(QStringLiteral("https://meet.example.invalid/room/two"),
                          QStringLiteral("Second call"));
        status.retranslate();
        QVERIFY(status.active());

        status.clear();
        QVERIFY(!status.active());
        status.clear();
        QCOMPARE(changes.size(), 4);
    }
};

QTEST_MAIN(MeetingStatusTest)
#include "MeetingStatusTest.moc"
