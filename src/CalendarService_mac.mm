#include "CalendarService.hpp"
#include <QCoreApplication>
#include "MeetingUrl.hpp"
#include "PortalConfig.hpp"

#include <EventKit/EventKit.h>
#include <AppKit/AppKit.h>
#include <QDate>
#include <QDateTime>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimeZone>
#include <dispatch/dispatch.h>
#include <algorithm>

namespace {
QStringList disabledCalendarsInAppleCalendar() {
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.iCal"];
    NSDictionary *groups = [defaults dictionaryForKey:@"DisabledCalendars"];
    if (![groups isKindOfClass:NSDictionary.class]) return {};
    QStringList ids;
    for (id value in groups.allValues) {
        if (![value isKindOfClass:NSArray.class]) continue;
        for (id identifier in (NSArray *)value) {
            if (![identifier isKindOfClass:NSString.class]) continue;
            const auto id = QString::fromNSString((NSString *)identifier);
            if (!id.isEmpty() && !ids.contains(id)) ids.append(id);
        }
    }
    ids.sort();
    return ids;
}
}

CalendarService::CalendarService(PortalConfig &portal, QObject *parent)
    : QObject(parent), m_portal(portal), m_status(QCoreApplication::translate("CalendarService", "Connect your local calendar to check a time.")),
      m_authorized([EKEventStore authorizationStatusForEntityType:EKEntityTypeEvent] == EKAuthorizationStatusFullAccess) {}

QVariantList CalendarService::events() const { return m_events; }
QStringList CalendarService::disabledCalendarIds() const { return m_disabledCalendarIds; }
QString CalendarService::status() const { return m_status; }
bool CalendarService::connected() const { return m_connected; }
bool CalendarService::authorized() const { return m_authorized; }
qint64 CalendarService::updatedAtMs() const { return m_updatedAtMs; }
bool CalendarService::suggesting() const { return m_suggesting; }
bool CalendarService::requestAccess() { return refresh(QDate::currentDate().toString(Qt::ISODate)); }
void CalendarService::refreshAuthorization() {
    const bool authorizedNow = [EKEventStore authorizationStatusForEntityType:EKEntityTypeEvent] == EKAuthorizationStatusFullAccess;
    if (m_authorized == authorizedNow) return;
    m_authorized = authorizedNow;
    if (!authorizedNow) {
        ++m_requestId;
        ++m_selectionRequestId;
        m_connected = false;
        if (!m_events.isEmpty()) { m_events.clear(); emit eventsChanged(); }
        m_disabledCalendarIds.clear();
        m_loadedDay.clear();
        m_updatedAtMs = 0;
        m_nextCalendarRefreshAtMs = 0;
        m_nextSelectionCheckAtMs = 0;
        m_loadingDay.clear();
        m_requestedDay.clear();
        m_pendingPermissionDay.clear();
        m_permissionRequestInFlight = false;
    }
    emit changed();
}

void CalendarService::refreshSelection() {
    if (!m_authorized) return;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now < m_nextSelectionCheckAtMs) return;
    m_nextSelectionCheckAtMs = now + 4 * 60 * 1000 + QRandomGenerator::global()->bounded(2 * 60 * 1000);
    const auto requestId = ++m_selectionRequestId;
    const QPointer<CalendarService> self(this);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        @autoreleasepool {
            const auto ids = disabledCalendarsInAppleCalendar();
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!self || self->m_selectionRequestId != requestId) return;
                if (self->m_disabledCalendarIds == ids) return;
                self->m_disabledCalendarIds = ids;
                emit self->changed();
            });
        }
    });
}

void CalendarService::openCalendar() {
    NSURL *appUrl = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:@"com.apple.iCal"];
    if (appUrl) [[NSWorkspace sharedWorkspace] openApplicationAtURL:appUrl
        configuration:[NSWorkspaceOpenConfiguration configuration] completionHandler:nil];
}

bool CalendarService::coversRange(const QString &firstDay, const QString &lastDay) const {
    const auto loaded = QDate::fromString(m_loadedDay, Qt::ISODate);
    const auto first = QDate::fromString(firstDay, Qt::ISODate);
    const auto last = QDate::fromString(lastDay, Qt::ISODate);
    return m_connected && loaded.isValid() && first.isValid() && last.isValid() &&
        first >= loaded && last >= first && last < loaded.addDays(31);
}

bool CalendarService::refresh(const QString &isoDate) {
    const auto day = QDate::fromString(isoDate, Qt::ISODate);
    if (!day.isValid()) {
        m_suggesting = false;
        ++m_requestId;
        m_loadingDay.clear();
        m_nextCalendarRefreshAtMs = 0;
        m_status = QCoreApplication::translate("CalendarService", "Choose a valid day.");
        emit changed();
        return false;
    }

    const auto authorization = [EKEventStore authorizationStatusForEntityType:EKEntityTypeEvent];
    m_authorized = authorization == EKAuthorizationStatusFullAccess;
    if (authorization == EKAuthorizationStatusNotDetermined) {
        m_pendingPermissionDay = isoDate;
        if (m_permissionRequestInFlight) return true;
        m_permissionRequestInFlight = true;
        const auto requestId = ++m_requestId;
        m_status = QCoreApplication::translate("CalendarService", "Waiting for calendar permission…");
        emit changed();
        auto *store = [EKEventStore new];
        const QPointer<CalendarService> self(this);
        void (^completion)(BOOL, NSError *) = ^(BOOL granted, NSError *error) {
            const bool failed = error != nil;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!self) return;
                self->m_permissionRequestInFlight = false;
                if (self->m_requestId != requestId) return;
                const auto requestedDay = self->m_pendingPermissionDay;
                self->m_pendingPermissionDay.clear();
                if (granted) self->refresh(requestedDay);
                else {
                    self->refreshAuthorization();
                    self->m_status = failed ? QCoreApplication::translate("CalendarService", "Calendar access failed. Try again in System Settings.")
                                            : QCoreApplication::translate("CalendarService", "Calendar access is off. Enable it in System Settings.");
                    emit self->changed();
                }
            });
        };
        if (@available(macOS 14.0, *)) [store requestFullAccessToEventsWithCompletion:completion];
        else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [store requestAccessToEntityType:EKEntityTypeEvent completion:completion];
#pragma clang diagnostic pop
        }
        return true;
    }
    if (authorization != EKAuthorizationStatusFullAccess) {
        m_suggesting = false;
        ++m_requestId;
        ++m_selectionRequestId;
        m_connected = false;
        if (!m_events.isEmpty()) { m_events.clear(); emit eventsChanged(); }
        m_disabledCalendarIds.clear();
        m_loadedDay.clear();
        m_updatedAtMs = 0;
        m_nextCalendarRefreshAtMs = 0;
        m_nextSelectionCheckAtMs = 0;
        m_loadingDay.clear();
        m_status = QCoreApplication::translate("CalendarService", "Calendar access is off. Enable it in System Settings.");
        emit changed();
        return false;
    }

    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (m_requestedDay == isoDate &&
        (m_loadingDay == isoDate || now < m_nextCalendarRefreshAtMs)) return true;
    m_suggesting = false;
    const auto requestId = ++m_requestId;
    m_requestedDay = isoDate;
    m_loadingDay = isoDate;
    m_nextCalendarRefreshAtMs = now + 3 * 60 * 1000;
    m_nextSelectionCheckAtMs = now + 4 * 60 * 1000 +
        QRandomGenerator::global()->bounded(2 * 60 * 1000);
    const auto selectionRequestId = ++m_selectionRequestId;
    m_status = QCoreApplication::translate("CalendarService", "Reading local calendar…");
    emit changed();
    const QPointer<CalendarService> self(this);
    const QString loadedDay = isoDate;
    const QStringList portalOrigins = m_portal.portalUrls();
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool {
            auto *store = [EKEventStore new];
            const auto disabledCalendarIds = disabledCalendarsInAppleCalendar();
            const auto start = QDateTime(day, QTime(0, 0), QTimeZone::systemTimeZone());
            const auto end = QDateTime(day.addDays(31), QTime(0, 0), QTimeZone::systemTimeZone());
            NSDate *startDate = [NSDate dateWithTimeIntervalSince1970:start.toSecsSinceEpoch()];
            NSDate *endDate = [NSDate dateWithTimeIntervalSince1970:end.toSecsSinceEpoch()];
            NSPredicate *predicate = [store predicateForEventsWithStartDate:startDate endDate:endDate calendars:nil];
            NSArray<EKEvent *> *matching = [store eventsMatchingPredicate:predicate];
            const bool unavailable = matching == nil;
            QVariantList result;
            for (EKEvent *event in matching) {
                if (event.status == EKEventStatusCanceled) continue;
                const auto eventStart = QDateTime::fromSecsSinceEpoch(
                    static_cast<qint64>(event.startDate.timeIntervalSince1970), QTimeZone::systemTimeZone());
                const auto eventEnd = QDateTime::fromSecsSinceEpoch(
                    static_cast<qint64>(event.endDate.timeIntervalSince1970), QTimeZone::systemTimeZone());
                QString link = event.URL ? ::recognizedMeetingUrl(QString::fromNSString(event.URL.absoluteString), portalOrigins) : QString();
                if (link.isEmpty() && event.location)
                    link = ::recognizedMeetingUrl(QString::fromNSString(event.location).trimmed(), portalOrigins);
                if (link.isEmpty() && event.notes && event.notes.length <= 65536) {
                    static const QRegularExpression address(QStringLiteral(R"(https://[^\s<>"']+)"));
                    auto matches = address.globalMatch(QString::fromNSString(event.notes));
                    while (matches.hasNext() && link.isEmpty()) {
                        auto candidate = matches.next().captured();
                        while (candidate.endsWith(QLatin1Char('.')) || candidate.endsWith(QLatin1Char(')')))
                            candidate.chop(1);
                        link = ::recognizedMeetingUrl(candidate, portalOrigins);
                    }
                }
                if (link.isEmpty()) continue;
                bool needsResponse = false;
                if (!event.organizer.isCurrentUser)
                    for (EKParticipant *participant in event.attendees)
                        if (participant.isCurrentUser &&
                            participant.participantStatus == EKParticipantStatusPending) {
                            needsResponse = true;
                            break;
                        }
                result.push_back(QVariantMap{
                    {QStringLiteral("title"), QString::fromNSString(event.title ? event.title : @"Untitled event")},
                    {QStringLiteral("url"), link},
                    {QStringLiteral("description"), event.notes && event.notes.length <= 65536
                        ? QString::fromNSString(event.notes).trimmed() : QString()},
                    {QStringLiteral("calendarId"), event.calendar.calendarIdentifier
                        ? QString::fromNSString(event.calendar.calendarIdentifier) : QString()},
                    {QStringLiteral("calendarName"), event.calendar.title
                        ? QString::fromNSString(event.calendar.title) : QString()},
                    {QStringLiteral("start"), eventStart.toString(QStringLiteral("HH:mm"))},
                    {QStringLiteral("end"), eventEnd.toString(QStringLiteral("HH:mm"))},
                    {QStringLiteral("startMs"), eventStart.toMSecsSinceEpoch()},
                    {QStringLiteral("endMs"), eventEnd.toMSecsSinceEpoch()},
                    {QStringLiteral("allDay"), event.allDay},
                    {QStringLiteral("needsResponse"), needsResponse},
                    {QStringLiteral("busy"), event.availability != EKEventAvailabilityFree}
                });
            }
            std::sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
                const auto a = left.toMap();
                const auto b = right.toMap();
                if (a.value(QStringLiteral("startMs")) != b.value(QStringLiteral("startMs")))
                    return a.value(QStringLiteral("startMs")).toLongLong() < b.value(QStringLiteral("startMs")).toLongLong();
                return a.value(QStringLiteral("title")).toString() < b.value(QStringLiteral("title")).toString();
            });
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!self || self->m_requestId != requestId) return;
                self->m_loadingDay.clear();
                if (unavailable) {
                    self->m_nextCalendarRefreshAtMs = QDateTime::currentMSecsSinceEpoch() +
                        3 * 60 * 1000 + QRandomGenerator::global()->bounded(2 * 60 * 1000);
                    self->m_status = QCoreApplication::translate("CalendarService", "Local calendar could not be read. Try again.");
                    emit self->changed();
                    return;
                }
                if (self->m_selectionRequestId == selectionRequestId)
                    self->m_disabledCalendarIds = disabledCalendarIds;
                if (self->m_events != result) {
                    self->m_events = result;
                    emit self->eventsChanged();
                }
                self->m_connected = true;
                self->m_loadedDay = loadedDay;
                self->m_updatedAtMs = QDateTime::currentMSecsSinceEpoch();
                self->m_nextCalendarRefreshAtMs = self->m_updatedAtMs + 8 * 60 * 1000 +
                    QRandomGenerator::global()->bounded(4 * 60 * 1000);
                self->m_nextSelectionCheckAtMs = self->m_updatedAtMs + 4 * 60 * 1000 +
                    QRandomGenerator::global()->bounded(2 * 60 * 1000);
                self->m_status = result.isEmpty() ? QCoreApplication::translate("CalendarService", "No calendar calls in this date range.") : QString();
                emit self->changed();
            });
        }
    });
    return true;
}

bool CalendarService::suggestNextHour() {
    if (m_suggesting) return true;
    if ([EKEventStore authorizationStatusForEntityType:EKEntityTypeEvent] != EKAuthorizationStatusFullAccess) {
        m_authorized = false;
        m_connected = false;
        m_status = QCoreApplication::translate("CalendarService", "Connect your local calendar to find a free time.");
        emit changed();
        return false;
    }
    const auto requestId = ++m_requestId;
    m_suggesting = true;
    m_status = QCoreApplication::translate("CalendarService", "Finding a free hour…");
    emit changed();
    const QPointer<CalendarService> self(this);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool {
            auto *store = [EKEventStore new];
            const auto now = QDateTime::currentDateTime();
            const auto deadline = now.addDays(14);
            NSDate *rangeStart = [NSDate dateWithTimeIntervalSince1970:now.toSecsSinceEpoch()];
            NSDate *rangeEnd = [NSDate dateWithTimeIntervalSince1970:deadline.toSecsSinceEpoch()];
            NSPredicate *predicate = [store predicateForEventsWithStartDate:rangeStart endDate:rangeEnd calendars:nil];
            NSArray<EKEvent *> *events = [store eventsMatchingPredicate:predicate];
            const bool unavailable = events == nil;
            const bool accessLost = [EKEventStore authorizationStatusForEntityType:EKEntityTypeEvent]
                != EKAuthorizationStatusFullAccess;
            auto next = now.addSecs(5 * 60);
            const int seconds = next.time().hour() * 3600 + next.time().minute() * 60 + next.time().second();
            const int roundedSeconds = ((seconds + 1799) / 1800) * 1800;
            next = QDateTime(next.date(), QTime(0, 0), QTimeZone::systemTimeZone())
                .addSecs(roundedSeconds);
            bool found = false;
            while (!accessLost && !unavailable && next < deadline) {
                const auto clock = next.time();
                if (clock < QTime(9, 0))
                    next = QDateTime(next.date(), QTime(9, 0), QTimeZone::systemTimeZone());
                if (next.time() > QTime(17, 0)) {
                    next = QDateTime(next.date().addDays(1), QTime(9, 0), QTimeZone::systemTimeZone());
                    continue;
                }
                bool busy = false;
                for (EKEvent *event in events) {
                    if (event.availability == EKEventAvailabilityFree || event.status == EKEventStatusCanceled)
                        continue;
                    const auto start = static_cast<qint64>(event.startDate.timeIntervalSince1970);
                    const auto end = static_cast<qint64>(event.endDate.timeIntervalSince1970);
                    if (next.toSecsSinceEpoch() < end && next.addSecs(3600).toSecsSinceEpoch() > start) {
                        busy = true;
                        break;
                    }
                }
                if (!busy) { found = true; break; }
                next = next.addSecs(30 * 60);
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!self || self->m_requestId != requestId) return;
                self->m_suggesting = false;
                self->m_status = accessLost ? QCoreApplication::translate("CalendarService", "Calendar access is off. Enable it in System Settings.")
                    : unavailable ? QCoreApplication::translate("CalendarService", "Local calendar could not be read. Try again.")
                    : found ? QCoreApplication::translate("CalendarService", "Next free hour on your calendar.")
                            : QCoreApplication::translate("CalendarService", "No free daytime hour found in the next 14 days.");
                if (accessLost) { self->m_authorized = false; self->m_connected = false; }
                emit self->changed();
                if (found) emit self->slotSuggested(next);
            });
        }
    });
    return true;
}
