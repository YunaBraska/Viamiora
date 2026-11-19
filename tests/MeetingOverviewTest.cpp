#include <QFile>
#include <QJSEngine>
#include <QtTest>

class MeetingOverviewTest final : public QObject {
    Q_OBJECT

private slots:
    void showsPastPresentAndFutureOnTheSameCalendar() {
        QFile file(QStringLiteral(VIAMIORA_SOURCE_DIR "/qml/MeetingOverview.js"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto script = QString::fromUtf8(file.readAll());
        script.remove(QStringLiteral(".pragma library"));
        QJSEngine engine;
        const auto loaded = engine.evaluate(script, file.fileName());
        QVERIFY2(!loaded.isError(), qPrintable(loaded.toString()));
        const auto result = engine.evaluate(QStringLiteral(R"JS(
            const local = (day, hour, minute = 0) => new Date(2026, 10, day, hour, minute);
            const portal = (id, day, start, end) => ({id, fromCalendar: false,
                start: local(day, start).toISOString(), end: local(day, end).toISOString()});
            const calendar = (id, start, end) => ({id, fromCalendar: true,
                startMs: start.getTime(), endMs: end.getTime()});
            const meetings = [
                portal(1, 19, 9, 10),
                calendar(2, local(19, 23, 30), local(20, 0, 30)),
                portal(3, 20, 8, 9),
                calendar(4, local(20, 11, 30), local(20, 12, 30)),
                portal(5, 21, 9, 10)
            ];
            const now = local(20, 12).getTime();
            const yesterday = groupMeetings(meetings, now, '2026-11-19');
            const today = groupMeetings(meetings, now, '2026-11-20');
            const tomorrow = groupMeetings(meetings, now, '2026-11-21');
            const all = groupMeetings(meetings, now, '');
            const visibleWeek = groupMeetings(meetings, now, '', '2026-11-19', '2026-11-20', true);
            const homeWeek = groupMeetings(meetings, now, '', '2026-11-19', '2026-11-20', false);
            JSON.stringify([
                meetingsOnDay(meetings, '2026-11-19'),
                meetingsOnDay(meetings, '2026-11-20'),
                meetingsOnDay(meetings, '2026-11-21'),
                yesterday.earlier.map(item => item.id),
                today.earlier.map(item => item.id),
                today.ongoing.map(item => item.id),
                tomorrow.upcoming.map(item => item.id),
                all.earlier.length,
                visibleWeek.earlier.map(item => item.id),
                visibleWeek.upcoming.map(item => item.id),
                homeWeek.earlier.map(item => item.id)
            ]);
        )JS"));
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
        QCOMPARE(result.toString(), QStringLiteral("[2,3,1,[1,2],[2,3],[4],[5],0,[1,2,3],[],[1,2,3]]"));
    }

    void mergesOnlyVerifiedMatchingCalls() {
        QFile file(QStringLiteral(VIAMIORA_SOURCE_DIR "/qml/MeetingOverview.js"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto script = QString::fromUtf8(file.readAll());
        script.remove(QStringLiteral(".pragma library"));
        QJSEngine engine;
        const auto loaded = engine.evaluate(script, file.fileName());
        QVERIFY2(!loaded.isError(), qPrintable(loaded.toString()));
        const auto result = engine.evaluate(QStringLiteral(R"JS(
            const portal = [{id: 321, title: 'Team review', start: '2026-11-20T09:00:00Z',
                end: '2026-11-20T10:00:00Z', roomName: '123456789', url: '/meeting/321/'}];
            const same = {title: 'Team review', startMs: Date.parse('2026-11-20T09:00:00Z'),
                endMs: Date.parse('2026-11-20T10:00:00Z'),
                url: 'https://video.example.invalid/webapp/#/?conference=123456789%40video.example.invalid&pin=1234'};
            const otherAccount = Object.assign({}, same,
                {url: 'https://video.example.invalid/webapp/#/?conference=987654321%40video.example.invalid'});
            const otherHost = Object.assign({}, same,
                {url: 'https://calls.example.net/webapp/#/?conference=123456789%40calls.example.net'});
            const differentTime = Object.assign({}, same,
                {startMs: Date.parse('2026-11-20T11:00:00Z')});
            const merged = combineMeetings(portal, [same, otherAccount, otherHost, differentTime],
                'https://tenant.video.example.invalid');
            JSON.stringify(merged.map(item => item.id || item.url));
        )JS"));
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
        const auto output = result.toString();
        QVERIFY2(output.startsWith(QStringLiteral("[321,")), qPrintable(output));
        QCOMPARE(output.count(QStringLiteral("pin=1234")), 1);
        QVERIFY(output.contains(QStringLiteral("987654321")));
        QVERIFY(output.contains(QStringLiteral("calls.example.net")));
        QCOMPARE(output.count(QStringLiteral("123456789")), 2);
    }
};

QTEST_GUILESS_MAIN(MeetingOverviewTest)
#include "MeetingOverviewTest.moc"
