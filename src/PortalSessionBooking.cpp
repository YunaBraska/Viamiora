#include "PortalSession.hpp"
#include "PortalConfig.hpp"
#include "PortalRequest.hpp"
#include "MeetingBooking.hpp"
#include "MeetingInvitation.hpp"

#include <QDateTime>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>


void PortalSession::inspectBookingType(int type) {
    if (!m_signedIn || m_inspectedBookingType == type) return;
    cancelReply(m_pinInspectReply);
    m_inspectedBookingType = type;
    m_moderatorPinSupported = false;
    emit changed();
    QUrl formUrl;
    for (const auto &item : m_bookingTypes)
        if (item.toMap().value(QStringLiteral("id")).toInt() == type) {
            formUrl = QUrl(item.toMap().value(QStringLiteral("url")).toString());
            break;
        }
    if (formUrl.isEmpty()) { m_inspectedBookingType = -1; return; }
    const auto generation = m_generation;
    QNetworkRequest request = portalRequest(formUrl, "text/html", m_config.language());
    m_pinInspectReply = m_network.get(request);
    auto *reply = m_pinInspectReply.data();
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, type] {
        if (m_pinInspectReply != reply) return;
        m_pinInspectReply = nullptr;
        const auto html = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        const auto url = reply->url();
        reply->deleteLater();
        if (generation != m_generation || m_inspectedBookingType != type) return;
        m_moderatorPinSupported = error == QNetworkReply::NoError && status == 200 &&
            url.path() == QStringLiteral("/book/") && html.size() <= 512 * 1024 &&
            MeetingBooking::supportsModeratorPin(html);
        emit changed();
    });
}

void PortalSession::createMeeting(const QString &title, qint64 startMs, qint64 endMs,
                                  int type, const QString &pin, const QStringList &invitees,
                                  const QStringList &moderators, const QString &moderatorPin) {
    if (m_creatingMeeting || m_inviting) return;
    const auto cleanTitle = title.trimmed();
    const auto recipients = MeetingInvitation::normalizeRecipients(invitees, moderators, m_username);
    if (!recipients) {
        m_creationStatus = tr("Check the email addresses.");
        emit changed();
        return;
    }
    const auto cleanInvitees = recipients->invitees;
    const auto cleanModerators = recipients->moderators;
    QUrl bookingUrl;
    for (const auto &item : m_bookingTypes)
        if (item.toMap().value(QStringLiteral("id")).toInt() == type) {
            bookingUrl = QUrl(item.toMap().value(QStringLiteral("url")).toString());
            break;
        }
    static const QRegularExpression pinPattern(QStringLiteral(R"(^[0-9]{6}$)"));
    if (!m_signedIn || m_busy || cleanTitle.isEmpty() || cleanTitle.size() > 100 ||
        startMs <= QDateTime::currentMSecsSinceEpoch() || endMs <= startMs ||
        endMs - startMs > 24 * 60 * 60 * 1000 || bookingUrl.isEmpty() ||
        !pinPattern.match(pin).hasMatch() ||
        (!moderatorPin.isEmpty() && !pinPattern.match(moderatorPin).hasMatch())) {
        m_creationStatus = tr("Check the meeting details and try again.");
        emit changed();
        return;
    }
    QByteArray csrf;
    for (const auto &cookie : m_cookieJar->cookiesForUrl(QUrl(m_portalUrl)))
        if (cookie.name() == QByteArrayLiteral("csrftoken")) { csrf = cookie.value(); break; }
    if (csrf.isEmpty()) {
        m_creationStatus = tr("The portal session cannot confirm this booking. Open the portal in your browser.");
        emit changed();
        return;
    }
    m_creatingMeeting = true;
    m_creationStatus = tr("Creating meeting…");
    m_invitationStatus.clear();
    emit changed();
    QSet<int> knownIds;
    for (const auto &item : m_meetings) knownIds.insert(item.toMap().value(QStringLiteral("id")).toInt());
    const auto generation = m_generation;
    QNetworkRequest formRequest = portalRequest(bookingUrl, "text/html", m_config.language());
    m_createReply = m_network.get(formRequest);
    auto *formReply = m_createReply.data();
    connect(formReply, &QNetworkReply::finished, this,
            [this, formReply, generation, bookingUrl, csrf, cleanTitle, startMs, endMs,
             type, pin, moderatorPin, knownIds, cleanInvitees, cleanModerators] {
        if (m_createReply != formReply) return;
        m_createReply = nullptr;
        const auto html = formReply->readAll();
        const auto status = formReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = formReply->error();
        const auto finalUrl = formReply->url();
        formReply->deleteLater();
        if (generation != m_generation) return;
        const auto action = MeetingBooking::formAction(html);
        const auto originalPin = MeetingBooking::initialPin(html);
        const auto actionUrl = finalUrl.resolved(QUrl(action));
        const QUrl portal(m_portalUrl);
        const QUrlQuery actionQuery(actionUrl);
        if (error != QNetworkReply::NoError || status != 200 || html.size() > 512 * 1024 ||
            finalUrl.path() != QStringLiteral("/book/") || action.isEmpty() ||
            actionUrl.scheme() != portal.scheme() || actionUrl.host() != portal.host() ||
            actionUrl.port(443) != portal.port(443) ||
            actionUrl.path() != QStringLiteral("/book/") ||
            actionQuery.queryItemValue(QStringLiteral("wizard_current")) != QStringLiteral("1") ||
            originalPin.isEmpty() ||
            (!moderatorPin.isEmpty() && !MeetingBooking::supportsModeratorPin(html))) {
            m_creatingMeeting = false;
            m_creationStatus = tr("The portal booking form is unavailable. Open the portal in your browser.");
            emit changed();
            return;
        }
        QNetworkRequest request = portalRequest(actionUrl, "text/html", m_config.language());
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/x-www-form-urlencoded"));
        request.setRawHeader("X-CSRFToken", csrf);
        request.setRawHeader("Origin", portal.toEncoded());
        request.setRawHeader("Referer", bookingUrl.toEncoded());
        const auto body = MeetingBooking::requestBody(csrf, cleanTitle, startMs, endMs,
                                                    type, pin, originalPin, moderatorPin);
        m_createReply = m_network.post(request, body);
        auto *reply = m_createReply.data();
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, generation, cleanTitle, startMs, endMs, knownIds,
                 cleanInvitees, cleanModerators] {
            if (m_createReply != reply) return;
            m_createReply = nullptr;
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto error = reply->error();
            const auto finalUrl = reply->url();
            reply->deleteLater();
            if (generation != m_generation) return;
            m_creatingMeeting = false;
            const int id = MeetingBooking::responseId(status, error, finalUrl);
            if (id > 0) {
                m_createdMeetingId = id;
                m_creationStatus.clear();
                emit changed();
                emit meetingCreated(id, cleanTitle);
                sendInvitations(id, cleanInvitees, cleanModerators);
                refreshMeetings();
                return;
            }
            if (status == 400 || status == 401 || status == 403 || status == 404) {
                m_creationStatus = status == 403 || status == 401
                    ? tr("The portal refused the booking. Sign in again or open the portal.")
                    : tr("The portal could not accept these meeting details. Check them in the portal.");
                emit changed();
                return;
            }
            m_pendingCreation = PendingCreation{cleanTitle, startMs, endMs, knownIds,
                                                cleanInvitees, cleanModerators};
            m_creationStatus = tr("Checking whether the meeting was created…");
            emit changed();
            refreshMeetings();
        });
    });
}

void PortalSession::sendInvitations(int id, const QStringList &invitees,
                                    const QStringList &moderators) {
    m_invitationQueue.clear();
    if (!moderators.isEmpty()) m_invitationQueue.append({QStringLiteral("moderator"), moderators});
    if (!invitees.isEmpty()) m_invitationQueue.append({QStringLiteral("participant"), invitees});
    if (m_invitationQueue.isEmpty()) return;
    m_invitationMeetingId = id;
    m_invitationFailures = 0;
    m_inviting = true;
    m_invitationStatus = tr("Sending invitations…");
    emit changed();
    sendNextInvitation();
}

void PortalSession::sendNextInvitation() {
    if (m_invitationQueue.isEmpty()) {
        m_inviting = false;
        m_invitationStatus = m_invitationFailures == 0
            ? tr("Invitations submitted.")
            : tr("Some invitations could not be confirmed.");
        emit changed();
        if (m_detailMeetingId == m_invitationMeetingId) fetchMeetingLinks(m_invitationMeetingId);
        return;
    }
    const auto current = m_invitationQueue.takeFirst();
    const auto path = QStringLiteral("/meeting/%1/invite/").arg(m_invitationMeetingId)
        + (current.first == QStringLiteral("moderator") ? QStringLiteral("moderator/") : QString());
    const auto detailPath = QStringLiteral("/meeting/%1/").arg(m_invitationMeetingId);
    const QUrl formUrl(m_portalUrl + path + QStringLiteral("?popup=1"));
    const auto generation = m_generation;
    QNetworkRequest get = portalRequest(formUrl, "text/html", m_config.language());
    m_invitationReply = m_network.get(get);
    auto *formReply = m_invitationReply.data();
    connect(formReply, &QNetworkReply::finished, this,
            [this, formReply, generation, current, path, detailPath, formUrl] {
        if (m_invitationReply != formReply) return;
        m_invitationReply = nullptr;
        const auto html = formReply->readAll();
        const auto status = formReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = formReply->error();
        const auto finalPath = formReply->url().path();
        formReply->deleteLater();
        if (generation != m_generation) return;
        const auto subject = MeetingInvitation::subjectFromForm(html);
        const auto message = MeetingInvitation::bodyFromForm(html);
        QByteArray csrf;
        for (const auto &cookie : m_cookieJar->cookiesForUrl(QUrl(m_portalUrl)))
            if (cookie.name() == QByteArrayLiteral("csrftoken")) { csrf = cookie.value(); break; }
        if (error != QNetworkReply::NoError || status != 200 || finalPath != path ||
            html.size() > 256 * 1024 || subject.isEmpty() || message.isEmpty() || csrf.isEmpty()) {
            ++m_invitationFailures;
            sendNextInvitation();
            return;
        }
        QNetworkRequest post = portalRequest(formUrl, "text/html", m_config.language());
        post.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/x-www-form-urlencoded"));
        post.setRawHeader("X-CSRFToken", csrf);
        post.setRawHeader("Origin", QUrl(m_portalUrl).toEncoded());
        post.setRawHeader("Referer", formUrl.toEncoded());
        const auto body = MeetingInvitation::requestBody(csrf, current.second, subject, message);
        m_invitationReply = m_network.post(post, body);
        auto *inviteReply = m_invitationReply.data();
        connect(inviteReply, &QNetworkReply::finished, this,
                [this, inviteReply, generation, detailPath] {
            if (m_invitationReply != inviteReply) return;
            m_invitationReply = nullptr;
            const auto status = inviteReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto error = inviteReply->error();
            const auto finalPath = inviteReply->url().path();
            inviteReply->deleteLater();
            if (generation != m_generation) return;
            if (error != QNetworkReply::NoError || status < 200 || status >= 300 ||
                finalPath != detailPath) ++m_invitationFailures;
            sendNextInvitation();
        });
    });
}

void PortalSession::discoverBookingTypes() {
    const quint64 generation = m_generation;
    auto *reply = m_network.get(portalRequest(QUrl(m_portalUrl + QStringLiteral("/")),
                                              "text/html", m_config.language()));
    m_bookingReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        if (m_bookingReply == reply) m_bookingReply = nullptr;
        const auto bytes = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto path = reply->url().path();
        const auto error = reply->error();
        reply->deleteLater();
        if (generation != m_generation || error != QNetworkReply::NoError || status != 200 ||
            path != QStringLiteral("/") || bytes.size() > 512 * 1024) return;
        const QString html = QString::fromUtf8(bytes);
        const QRegularExpression links(QStringLiteral(R"(href\s*=\s*['"]([^'"]+)['"])"),
                                       QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression tags(QStringLiteral(R"(<[^>]+>)"));
        QVariantList types;
        auto matches = links.globalMatch(html);
        while (matches.hasNext() && types.size() < 20) {
            const auto match = matches.next();
            QString href = match.captured(1);
            href.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
            const QUrl resolved = QUrl(m_portalUrl).resolved(QUrl(href));
            if (resolved.scheme() != QStringLiteral("https") ||
                resolved.host() != QUrl(m_portalUrl).host() ||
                resolved.port(443) != QUrl(m_portalUrl).port(443) ||
                resolved.path() != QStringLiteral("/book/")) continue;
            const QUrlQuery query(resolved);
            bool valid = false;
            const int id = query.queryItemValue(QStringLiteral("meeting_type")).toInt(&valid);
            if (!valid || id < 0) continue;
            bool duplicate = false;
            for (const auto &item : types)
                if (item.toMap().value(QStringLiteral("id")).toInt() == id) { duplicate = true; break; }
            if (duplicate) continue;
            const auto openEnd = html.indexOf(QLatin1Char('>'), match.capturedEnd());
            const auto close = html.indexOf(QStringLiteral("</a>"), openEnd, Qt::CaseInsensitive);
            QString title;
            if (openEnd >= 0 && close > openEnd && close - openEnd < 1000)
                title = html.mid(openEnd + 1, close - openEnd - 1).remove(tags).simplified();
            if (title.isEmpty() || title.size() > 80) title = QStringLiteral("Meeting");
            types.append(QVariantMap{{QStringLiteral("id"), id},
                                     {QStringLiteral("title"), title},
                                     {QStringLiteral("url"), resolved.toString()}});
        }
        m_bookingTypes = types;
        emit changed();
    });
}
