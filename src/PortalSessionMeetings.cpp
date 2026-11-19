#include "PortalSession.hpp"
#include "PortalConfig.hpp"
#include "PortalRequest.hpp"
#include "MeetingBooking.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QUrlQuery>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <algorithm>
#include <cstdlib>

namespace {
QVariantList parseMeetingList(const QJsonArray &results, PortalConfig &config,
                              const QHash<int, QVariantMap> &identityCache) {
    QVariantList meetings;
    static const QRegularExpression meetingPath(QStringLiteral(R"(^/meeting/[0-9]+/$)"));
    for (qsizetype index = 0; index < results.size() && index < 100; ++index) {
        const auto item = results.at(index).toObject();
        if (!item.value(QStringLiteral("title")).isString()
            || !item.value(QStringLiteral("ts_start")).isString()) continue;
        const auto path = item.value(QStringLiteral("url")).toString();
        const auto id = item.value(QStringLiteral("id")).toInt();
        if (id <= 0) continue;
        const auto listLink = config.safeMeetingUrl(item.value(QStringLiteral("full_url")).toString());
        QVariantMap entry{{QStringLiteral("id"), id},
                          {QStringLiteral("title"), item.value(QStringLiteral("title")).toString()},
                          {QStringLiteral("start"), item.value(QStringLiteral("ts_start")).toString()},
                          {QStringLiteral("end"), item.value(QStringLiteral("ts_stop")).toString()},
                          {QStringLiteral("fullUrl"), QUrl(listLink).path().startsWith(QStringLiteral("/webapp/"))
                              ? listLink : QString()},
                          {QStringLiteral("identityUrl"), config.safeMeetingUrl(item.value(QStringLiteral("moderator_url")).toString())},
                          {QStringLiteral("roomName"), item.value(QStringLiteral("room_name")).toString().left(120)},
                          {QStringLiteral("manageable"), item.value(QStringLiteral("can_change")).toBool()},
                          {QStringLiteral("url"), meetingPath.match(path).hasMatch() ? path : QString()}};
        if (identityCache.contains(id))
            for (auto iterator = identityCache[id].cbegin(); iterator != identityCache[id].cend(); ++iterator)
                entry.insert(iterator.key(), iterator.value());
        meetings.append(entry);
    }
    return meetings;
}
}

void PortalSession::refreshMeetings() {
    if (!m_signedIn || m_busy || m_creatingMeeting) {
        if (!m_signedIn) {
            m_status = tr("Sign in to see upcoming meetings.");
            emit changed();
        }
        return;
    }
    m_busy = true;
    m_status = m_meetingsUpdatedAtMs == 0 ? tr("Loading meetings…") : QString();
    emit changed();
    fetchMeetings();
}

void PortalSession::loadMeetingWeek(const QString &firstDay, const QString &lastDay) {
    const auto first = QDate::fromString(firstDay, Qt::ISODate);
    const auto last = QDate::fromString(lastDay, Qt::ISODate);
    if (!m_signedIn || !first.isValid() || !last.isValid() || last < first ||
        first.daysTo(last) > 6) return;
    if (first >= QDate::currentDate()) {
        if (m_historicalWeek.isEmpty() && m_historicalMeetings.isEmpty() &&
            m_historicalStatus.isEmpty() && !m_historicalBusy) return;
        cancelReply(m_historicalReply);
        m_historicalWeek.clear();
        m_historicalMeetings.clear();
        m_historicalStatus.clear();
        m_historicalBusy = false;
        emit historicalMeetingsChanged();
        return;
    }
    const auto key = firstDay + QLatin1Char('|') + lastDay;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (key == m_historicalWeek && (m_historicalBusy || now < m_historicalRetryAtMs ||
        (m_historicalUpdatedAtMs > 0 && now - m_historicalUpdatedAtMs < 10 * 60 * 1000))) return;
    cancelReply(m_historicalReply);
    if (key != m_historicalWeek) {
        m_historicalMeetings.clear();
        m_historicalUpdatedAtMs = 0;
    }
    m_historicalWeek = key;
    const auto cached = m_historicalCache.constFind(key);
    if (cached != m_historicalCache.cend() && now - cached->updatedAtMs < 10 * 60 * 1000) {
        m_historicalMeetings = cached->meetings;
        m_historicalStatus = cached->status;
        m_historicalUpdatedAtMs = cached->updatedAtMs;
        m_historicalBusy = false;
        emit historicalMeetingsChanged();
        return;
    }
    if (now < m_historicalRetryAtMs) {
        m_historicalStatus = tr("Past meetings could not be loaded. Open the portal if needed.");
        m_historicalBusy = false;
        emit historicalMeetingsChanged();
        return;
    }
    m_historicalBusy = true;
    m_historicalStatus.clear();
    emit historicalMeetingsChanged();

    const auto begins = QDateTime(first.addDays(-1), QTime(0, 0), QTimeZone::systemTimeZone());
    const auto ends = QDateTime(last.addDays(1), QTime(0, 0), QTimeZone::systemTimeZone());
    QUrl address(m_portalUrl + QStringLiteral("/api/v1/meeting/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start"), begins.toUTC().toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("stop"), ends.toUTC().toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    address.setQuery(query);
    const auto generation = m_generation;
    auto *reply = m_network.get(portalRequest(address, "application/json", m_config.language()));
    m_historicalReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, key] {
        if (generation != m_generation || key != m_historicalWeek || m_historicalReply != reply) return;
        m_historicalReply = nullptr;
        const auto body = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        const auto retryAfter = reply->rawHeader("Retry-After");
        reply->deleteLater();
        m_historicalBusy = false;
        const auto now = QDateTime::currentMSecsSinceEpoch();
        if (error != QNetworkReply::NoError || status != 200 || body.size() > 2 * 1024 * 1024) {
            m_historicalRetryAtMs = std::max(now + 2 * 60 * 1000,
                                               portalRetryAfterDeadlineMs(retryAfter, now));
            m_historicalStatus = tr("Past meetings could not be loaded. Open the portal if needed.");
            emit historicalMeetingsChanged();
            return;
        }
        const auto json = QJsonDocument::fromJson(body);
        if (!json.isObject() || !json.object().value(QStringLiteral("results")).isArray()) {
            m_historicalRetryAtMs = now + 2 * 60 * 1000;
            m_historicalStatus = tr("This portal does not provide a usable meeting history.");
            emit historicalMeetingsChanged();
            return;
        }
        const auto results = json.object().value(QStringLiteral("results")).toArray();
        m_historicalMeetings = parseMeetingList(results, m_config, m_meetingIdentityCache);
        m_historicalUpdatedAtMs = now;
        m_historicalStatus = json.object().value(QStringLiteral("next")).isNull() &&
            json.object().value(QStringLiteral("count")).toInt(results.size()) <= results.size() &&
            m_historicalMeetings.size() == results.size()
            ? QString() : tr("More past meetings are available in the portal.");
        m_historicalRetryAtMs = 0;
        m_historicalCache.insert(key, {m_historicalMeetings, m_historicalStatus, now});
        if (m_historicalCache.size() > 8) {
            auto oldest = m_historicalCache.begin();
            for (auto entry = m_historicalCache.begin(); entry != m_historicalCache.end(); ++entry)
                if (entry->updatedAtMs < oldest->updatedAtMs) oldest = entry;
            m_historicalCache.erase(oldest);
        }
        emit historicalMeetingsChanged();
    });
}

bool PortalSession::refreshMeetingsIfDue(bool inactive) {
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!m_signedIn || m_busy || m_creatingMeeting || m_automaticRefreshDueAtMs == 0 ||
        now < m_automaticRefreshDueAtMs ||
        (inactive && now - m_lastMeetingFetchAtMs < 15 * 60 * 1000)) return false;
    refreshMeetings();
    return true;
}

void PortalSession::scheduleAutomaticRefresh(bool success, const QByteArray &retryAfter) {
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (success) m_automaticRefreshFailures = 0;
    else m_automaticRefreshFailures = std::min(m_automaticRefreshFailures + 1, 5);
    const qint64 base = success ? 5 * 60 * 1000
        : std::min<qint64>(60 * 60 * 1000, (5 * 60 * 1000LL) << m_automaticRefreshFailures);
    const qint64 delay = std::min<qint64>(60 * 60 * 1000,
        base + QRandomGenerator::global()->bounded(2 * 60 * 1000));
    m_automaticRefreshDueAtMs = now + delay;
    m_automaticRefreshDueAtMs = std::max(m_automaticRefreshDueAtMs,
                                         portalRetryAfterDeadlineMs(retryAfter, now));
}

void PortalSession::fetchMeetings() {
    m_lastMeetingFetchAtMs = QDateTime::currentMSecsSinceEpoch();
    const quint64 generation = m_generation;
    QNetworkRequest request = portalRequest(QUrl(m_portalUrl + QStringLiteral("/api/v1/meeting/upcoming/")),
                                            "application/json", m_config.language());
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, generation] {
        QNetworkReply *reply = m_reply;
        if (!reply || generation != m_generation) return;
        m_reply = nullptr;
        const auto body = reply->readAll();
        const auto error = reply->error();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto retryAfter = reply->rawHeader("Retry-After");
        const auto path = reply->url().path();
        reply->deleteLater();
        const bool expired = status == 401 || status == 403
            || path.startsWith(QStringLiteral("/accounts/login"));
        if (expired && !m_pendingCredential && !m_username.isEmpty()) {
            const auto origin = m_portalUrl;
            const auto username = m_username;
            auto *watcher = new QFutureWatcher<std::optional<PortalCredential>>(this);
            connect(watcher, &QFutureWatcher<std::optional<PortalCredential>>::finished, this,
                    [this, watcher, generation, origin] {
                const auto saved = watcher->result();
                watcher->deleteLater();
                if (generation != m_generation || origin != m_portalUrl || !m_busy) return;
                if (saved) beginSignIn(*saved);
                else {
                    m_signedIn = false;
                    if (!m_meetings.isEmpty()) { m_meetings.clear(); emit meetingsChanged(); }
                    fail(tr("The session expired. Sign in again."));
                }
            });
            watcher->setFuture(QtConcurrent::run([origin, username] {
                return CredentialStore::load(origin, username, nullptr);
            }));
            return;
        }
        if (error != QNetworkReply::NoError || status != 200 || body.size() > 2 * 1024 * 1024
            || expired) {
            scheduleAutomaticRefresh(false, retryAfter);
            if (m_pendingCreation) {
                m_creationStatus = tr("Booking could not be confirmed. Check the portal before trying again.");
                m_pendingCreation.reset();
            }
            if (expired) {
                m_signedIn = false;
                if (!m_meetings.isEmpty()) { m_meetings.clear(); emit meetingsChanged(); }
                fail(tr("The session expired. Sign in again."));
            } else if (m_meetingsUpdatedAtMs == 0) {
                fail(tr("Meetings could not be loaded. Try signing in again."));
            } else {
                m_busy = false;
                m_status = tr("Meetings could not be refreshed. The last results remain visible.");
                emit changed();
            }
            return;
        }
        const auto json = QJsonDocument::fromJson(body);
        if (!json.isObject() || !json.object().value(QStringLiteral("results")).isArray()) {
            scheduleAutomaticRefresh(false);
            fail(m_meetingsUpdatedAtMs == 0
                ? tr("The portal returned an unsupported meeting list. Open the portal in your browser.")
                : tr("The portal returned an unsupported update. The last meetings remain visible."));
            return;
        }
        const auto meetings = parseMeetingList(json.object().value(QStringLiteral("results")).toArray(),
                                               m_config, m_meetingIdentityCache);
        QSet<int> currentIds;
        for (const auto &value : meetings) currentIds.insert(value.toMap().value(QStringLiteral("id")).toInt());
        for (auto it = m_identityRetryAtMs.begin(); it != m_identityRetryAtMs.end(); ) {
            if (!currentIds.contains(it.key())) it = m_identityRetryAtMs.erase(it);
            else ++it;
        }
        if (m_meetings != meetings) { m_meetings = meetings; emit meetingsChanged(); }
        std::optional<PendingCreation> confirmedCreation;
        int confirmedId = 0;
        if (m_pendingCreation) {
            const auto id = MeetingBooking::newMatchingId(m_meetings, m_pendingCreation->knownIds,
                                                           m_pendingCreation->title,
                                                           m_pendingCreation->startMs,
                                                           m_pendingCreation->endMs);
            if (id > 0) {
                m_createdMeetingId = id;
                m_creationStatus.clear();
                confirmedCreation = *m_pendingCreation;
                confirmedId = id;
            } else {
                m_creationStatus = tr("Booking could not be confirmed. Check the overview or portal before trying again.");
            }
            m_pendingCreation.reset();
        }
        m_meetingsUpdatedAtMs = QDateTime::currentMSecsSinceEpoch();
        scheduleAutomaticRefresh(true);
        m_signedIn = true;
        m_busy = false;
        m_status.clear();
        if (m_pendingCredential) {
            m_username = m_pendingCredential->username;
            const auto credential = *m_pendingCredential;
            const auto origin = m_portalUrl;
            const auto currentGeneration = m_generation;
            m_config.rememberUsername(m_username);
            m_pendingCredential.reset();
            auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
            connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this,
                    [this, watcher, currentGeneration, origin, credential] {
                const auto result = watcher->result();
                watcher->deleteLater();
                if (currentGeneration != m_generation || origin != m_portalUrl || !m_signedIn) return;
                if (result.first) {
                    if (!containsLogin(m_savedLogins, origin, credential.username))
                        m_savedLogins.prepend(QVariantMap{
                        {QStringLiteral("origin"), origin},
                        {QStringLiteral("username"), credential.username}});
                } else m_status = tr("Login could not be saved for next time.");
                emit changed();
            });
            watcher->setFuture(QtConcurrent::run([origin, credential] {
                QString error;
                const bool saved = CredentialStore::save(origin, credential, &error);
                return qMakePair(saved, error);
            }));
        }
        discoverBookingTypes();
        emit changed();
        if (confirmedCreation) {
            emit meetingCreated(confirmedId, confirmedCreation->title);
            sendInvitations(confirmedId, confirmedCreation->invitees, confirmedCreation->moderators);
        }
    });
}

void PortalSession::rememberMeetingIdentity(int id, const QJsonObject &meeting) {
    m_identityRetryAtMs.remove(id);
    const auto participant = m_config.safeMeetingUrl(meeting.value(QStringLiteral("full_url")).toString());
    const auto moderator = m_config.safeMeetingUrl(meeting.value(QStringLiteral("moderator_url")).toString());
    const QVariantMap identity{
        {QStringLiteral("fullUrl"), QUrl(participant).path().startsWith(QStringLiteral("/webapp/"))
            ? participant : QString()},
        {QStringLiteral("identityUrl"), QUrl(moderator).path().startsWith(QStringLiteral("/webapp/"))
            ? moderator : QString()},
        {QStringLiteral("roomName"), meeting.value(QStringLiteral("room_name")).toString().left(120)},
        {QStringLiteral("manageable"), meeting.value(QStringLiteral("can_change")).toBool()}
    };
    m_meetingIdentityCache.insert(id, identity);
    for (auto &value : m_meetings) {
        auto entry = value.toMap();
        if (entry.value(QStringLiteral("id")).toInt() != id) continue;
        const auto previous = entry;
        for (auto iterator = identity.cbegin(); iterator != identity.cend(); ++iterator)
            entry.insert(iterator.key(), iterator.value());
        if (entry != previous) {
            value = entry;
            emit meetingsChanged();
        }
        break;
    }
    for (auto &value : m_historicalMeetings) {
        auto entry = value.toMap();
        if (entry.value(QStringLiteral("id")).toInt() != id) continue;
        const auto previous = entry;
        for (auto iterator = identity.cbegin(); iterator != identity.cend(); ++iterator)
            entry.insert(iterator.key(), iterator.value());
        if (entry != previous) {
            value = entry;
            if (m_historicalCache.contains(m_historicalWeek))
                m_historicalCache[m_historicalWeek].meetings = m_historicalMeetings;
            emit historicalMeetingsChanged();
        }
        break;
    }
}

void PortalSession::resolveCalendarMatches(const QVariantList &calendarEvents) {
    if (!m_signedIn || calendarEvents.isEmpty()) return;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now < m_nextIdentityFetchAtMs || !m_identityReplies.isEmpty()) return;
    const QUrl portal(m_portalUrl);
    const auto labels = portal.host().split(QLatin1Char('.'));
    const auto parentHost = labels.size() >= 3 ? labels.mid(1).join(QLatin1Char('.')) : QString();
    for (const auto &value : m_meetings + m_historicalMeetings) {
        const auto item = value.toMap();
        const int id = item.value(QStringLiteral("id")).toInt();
        if (id <= 0 || m_meetingIdentityCache.contains(id) || m_identityReplies.contains(id) ||
            now < m_identityRetryAtMs.value(id)
            || !item.value(QStringLiteral("fullUrl")).toString().isEmpty()
            || !item.value(QStringLiteral("roomName")).toString().isEmpty()) continue;
        const auto start = QDateTime::fromString(item.value(QStringLiteral("start")).toString(), Qt::ISODate);
        const auto end = QDateTime::fromString(item.value(QStringLiteral("end")).toString(), Qt::ISODate);
        if (!start.isValid() || !end.isValid()) continue;
        bool possibleMatch = false;
        for (const auto &eventValue : calendarEvents) {
            const auto event = eventValue.toMap();
            if (std::abs(start.toMSecsSinceEpoch() - event.value(QStringLiteral("startMs")).toLongLong()) >= 60000 ||
                std::abs(end.toMSecsSinceEpoch() - event.value(QStringLiteral("endMs")).toLongLong()) >= 60000)
                continue;
            const QUrl link(event.value(QStringLiteral("url")).toString());
            if (link.scheme() != QStringLiteral("https") || link.host().isEmpty()) continue;
            const auto title = item.value(QStringLiteral("title")).toString().trimmed();
            const auto calendarTitle = event.value(QStringLiteral("title")).toString().trimmed();
            const bool titleMatch = !title.isEmpty() && (title.compare(calendarTitle, Qt::CaseInsensitive) == 0 ||
                calendarTitle.startsWith(title + QStringLiteral(" ("), Qt::CaseInsensitive));
            if (titleMatch || link.host().compare(portal.host(), Qt::CaseInsensitive) == 0 ||
                (!parentHost.isEmpty() && link.host().compare(parentHost, Qt::CaseInsensitive) == 0)) {
                possibleMatch = true;
                break;
            }
        }
        if (!possibleMatch) continue;
        m_identityRetryAtMs.insert(id, now + 15 * 60 * 1000 +
            QRandomGenerator::global()->bounded(5 * 60 * 1000));
        m_nextIdentityFetchAtMs = now + 30 * 1000 +
            QRandomGenerator::global()->bounded(30 * 1000);
        const auto generation = m_generation;
        const auto address = QUrl(m_portalUrl + QStringLiteral("/api/v1/meeting/") +
                                  QString::number(id) + QStringLiteral("/"));
        auto *reply = m_network.get(portalRequest(address, "application/json", m_config.language()));
        m_identityReplies.insert(id, reply);
        connect(reply, &QNetworkReply::finished, this, [this, reply, generation, id] {
            if (m_identityReplies.value(id) != reply) return;
            m_identityReplies.remove(id);
            const auto body = reply->readAll();
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto retryAfter = reply->rawHeader("Retry-After");
            const auto error = reply->error();
            reply->deleteLater();
            if (status == 429 || status == 503) {
                scheduleAutomaticRefresh(false, retryAfter);
                m_nextIdentityFetchAtMs = std::max(m_nextIdentityFetchAtMs, m_automaticRefreshDueAtMs);
            }
            if (generation != m_generation || error != QNetworkReply::NoError || status != 200 ||
                body.size() > 256 * 1024) return;
            const auto document = QJsonDocument::fromJson(body);
            const auto meeting = document.object().value(QStringLiteral("meeting")).toObject();
            if (document.isObject() && meeting.value(QStringLiteral("id")).toInt() == id)
                rememberMeetingIdentity(id, meeting);
        });
        break;
    }
}
