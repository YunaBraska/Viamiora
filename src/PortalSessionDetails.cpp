#include "PortalSession.hpp"
#include "PortalConfig.hpp"
#include "PortalRequest.hpp"
#include "MeetingInvitation.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QUrlQuery>

void PortalSession::fetchMeetingLinks(int id) {
    if (m_invitationMeetingId != id && !m_inviting && !m_invitationStatus.isEmpty()) {
        m_invitationStatus.clear();
        m_invitationMeetingId = 0;
        m_pendingRemovedInviteId = 0;
        emit changed();
    }
    if (m_detailMeetingId != id) {
        m_meetingLinks.clear();
        m_detailMeetingId = 0;
    }
    m_linkStatus.clear();
    cancelReply(m_detailReply);
    bool listed = false;
    for (const auto &item : m_meetings + m_historicalMeetings)
        if (item.toMap().value(QStringLiteral("id")).toInt() == id && id > 0) {
            listed = true;
            break;
        }
    if (!m_signedIn || (!listed && id != m_createdMeetingId)) {
        m_pendingRemovedInviteId = 0;
        m_meetingLinks.clear();
        m_detailMeetingId = 0;
        m_linksBusy = false;
        m_linkStatus = tr("This meeting is no longer available. Refresh the overview.");
        emit linksChanged();
        return;
    }
    m_linksBusy = true;
    emit linksChanged();
    const auto generation = m_generation;
    QNetworkRequest request = portalRequest(QUrl(m_portalUrl + QStringLiteral("/api/v1/meeting/") +
                                QString::number(id) + QStringLiteral("/")),
                                "application/json", m_config.language());
    m_detailReply = m_network.get(request);
    auto *reply = m_detailReply.data();
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, id] {
        if (m_detailReply != reply) return;
        m_detailReply = nullptr;
        const auto body = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        reply->deleteLater();
        if (generation != m_generation) return;
        m_linksBusy = false;
        const auto unconfirmedRemoval = [this, id] {
            if (m_pendingRemovedInviteId <= 0 || m_invitationMeetingId != id) return;
            m_pendingRemovedInviteId = 0;
            m_invitationStatus = tr("Removal could not be confirmed. Check the meeting in the portal.");
            emit changed();
        };
        if (error != QNetworkReply::NoError || status != 200 || body.size() > 256 * 1024) {
            unconfirmedRemoval();
            m_meetingLinks.clear();
            m_detailMeetingId = 0;
            m_linkStatus = tr("Meeting links could not be loaded. Open the meeting in your browser.");
            emit linksChanged();
            return;
        }
        const auto document = QJsonDocument::fromJson(body);
        const auto meeting = document.object().value(QStringLiteral("meeting")).toObject();
        if (!document.isObject() || meeting.value(QStringLiteral("id")).toInt() != id) {
            unconfirmedRemoval();
            m_meetingLinks.clear();
            m_detailMeetingId = 0;
            m_linkStatus = tr("The portal returned an unsupported meeting detail.");
            emit linksChanged();
            return;
        }
        rememberMeetingIdentity(id, meeting);
        const auto safeLink = [](const QString &address) -> QString {
            const QUrl url(address);
            return url.isValid() && url.scheme() == QStringLiteral("https") &&
                   !url.host().isEmpty() && url.userInfo().isEmpty() && address.size() <= 2048
                ? url.toString(QUrl::FullyEncoded) : QString();
        };
        auto participant = safeLink(meeting.value(QStringLiteral("full_url")).toString());
        const auto moderator = safeLink(meeting.value(QStringLiteral("moderator_url")).toString());
        const auto numericPin = [](const QJsonValue &value) {
            const auto pin = value.isString() ? value.toString() : value.isDouble()
                ? QString::number(static_cast<qint64>(value.toDouble())) : QString();
            static const QRegularExpression digits(QStringLiteral(R"(^[0-9]{4,12}$)"));
            return digits.match(pin).hasMatch() ? pin : QString();
        };
        const auto pinFromLink = [](const QString &address) {
            const QUrl url(address);
            auto pin = QUrlQuery(url).queryItemValue(QStringLiteral("pin"));
            const auto fragment = url.fragment(QUrl::FullyDecoded);
            const auto marker = fragment.indexOf(QLatin1Char('?'));
            if (pin.isEmpty() && marker >= 0)
                pin = QUrlQuery(fragment.mid(marker + 1)).queryItemValue(QStringLiteral("pin"));
            static const QRegularExpression digits(QStringLiteral(R"(^[0-9]{4,12}$)"));
            return digits.match(pin).hasMatch() ? pin : QString();
        };
        const auto detailPath = meeting.value(QStringLiteral("url")).toString();
        static const QRegularExpression meetingPath(QStringLiteral(R"(^/meeting/[0-9]+/$)"));
        const auto participantPin = pinFromLink(participant);
        const auto moderatorPin = pinFromLink(moderator);
        const auto portalParticipantPin = numericPin(meeting.value(QStringLiteral("pin")));
        const auto portalModeratorPin = numericPin(meeting.value(QStringLiteral("lobby_pin")));
        const QUrl portalOrigin(m_portalUrl);
        const QUrl participantUrl(participant);
        const QUrl moderatorUrl(moderator);
        if (participantUrl.host() == portalOrigin.host() &&
            participantUrl.port(443) == portalOrigin.port(443) &&
            participantUrl.path() == QStringLiteral("/meeting/%1/").arg(id) &&
            participantUrl.query().isEmpty() && participantUrl.fragment().isEmpty() &&
            moderatorUrl.path().startsWith(QStringLiteral("/webapp/")) &&
            !portalParticipantPin.isEmpty() && !portalModeratorPin.isEmpty() &&
            portalParticipantPin != portalModeratorPin &&
            QUrlQuery(moderatorUrl).queryItemValue(QStringLiteral("pin")).isEmpty() &&
            moderatorPin == portalModeratorPin) {
            auto fragment = moderatorUrl.fragment(QUrl::FullyEncoded);
            static const QRegularExpression pinItem(
                QStringLiteral(R"((?:[?&])pin=([0-9]{4,12})(?=&|$))"));
            const auto match = pinItem.match(fragment);
            if (match.hasMatch() && match.captured(1) == portalModeratorPin) {
                fragment.replace(match.capturedStart(1), match.capturedLength(1), portalParticipantPin);
                QUrl guestUrl = moderatorUrl;
                guestUrl.setFragment(fragment, QUrl::StrictMode);
                const auto candidate = safeLink(guestUrl.toString(QUrl::FullyEncoded));
                if (pinFromLink(candidate) == portalParticipantPin) participant = candidate;
            }
        }
        const auto roomName = meeting.value(QStringLiteral("room_name")).toString().trimmed().left(120);
        const auto sipCandidate = meeting.value(QStringLiteral("sip")).toString().trimmed();
        static const QRegularExpression sipAddress(QStringLiteral(R"(^sip:[A-Za-z0-9._+%-]+@[A-Za-z0-9.-]+$)"));
        const auto sip = sipCandidate.size() <= 200 && sipAddress.match(sipCandidate).hasMatch()
            ? sipCandidate : QString();
        const auto inviteValue = meeting.value(QStringLiteral("invites"));
        QJsonArray rawInvites = inviteValue.toArray();
        bool inviteListReliable = inviteValue.isArray();
        if (inviteValue.isString()) {
            const auto inviteDocument = QJsonDocument::fromJson(inviteValue.toString().toUtf8());
            inviteListReliable = inviteDocument.isArray();
            if (inviteListReliable) rawInvites = inviteDocument.array();
        }
        QVariantList participants;
        QVariantList moderators;
        QSet<QString> seenInvites;
        static const QRegularExpression emailAddress(QStringLiteral(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)"));
        for (const bool moderatorRole : {true, false}) {
            for (const auto &entry : rawInvites) {
                const auto invite = entry.toObject();
                if (!entry.isObject() || !invite.value(QStringLiteral("is_moderator")).isBool())
                    inviteListReliable = false;
                if (invite.value(QStringLiteral("is_moderator")).toBool() != moderatorRole) continue;
                const auto email = invite.value(QStringLiteral("email")).toString().trimmed();
                const auto key = email.toCaseFolded();
                if (email.size() > 254 || !emailAddress.match(email).hasMatch()) {
                    inviteListReliable = false;
                    continue;
                }
                if (seenInvites.contains(key))
                    continue;
                seenInvites.insert(key);
                (moderatorRole ? moderators : participants).append(
                    QVariantMap{{QStringLiteral("email"), email},
                                {QStringLiteral("id"), invite.value(QStringLiteral("id")).toInt()}});
            }
        }
        if (m_pendingRemovedInviteId > 0 && m_invitationMeetingId == id) {
            bool stillInvited = false;
            for (const auto &entry : rawInvites)
                if (entry.toObject().value(QStringLiteral("id")).toInt() == m_pendingRemovedInviteId)
                    stillInvited = true;
            m_invitationStatus = inviteListReliable && !stillInvited
                ? tr("Invitation removed.")
                : tr("Removal could not be confirmed. Check the meeting in the portal.");
            m_pendingRemovedInviteId = 0;
            emit changed();
        }
        m_meetingLinks = QVariantMap{{QStringLiteral("participantUrl"), participant},
                                     {QStringLiteral("description"), meeting.value(QStringLiteral("description")).isString()
                                         ? meeting.value(QStringLiteral("description")).toString().left(65536) : QString()},
                                     {QStringLiteral("moderatorUrl"), moderator},
                                     {QStringLiteral("participantPin"), portalParticipantPin.isEmpty()
                                         ? participantPin : portalParticipantPin},
                                     {QStringLiteral("moderatorPin"), portalModeratorPin.isEmpty()
                                         ? moderatorPin : portalModeratorPin},
                                     {QStringLiteral("roomName"), roomName},
                                     {QStringLiteral("sip"), sip},
                                     {QStringLiteral("participants"), participants},
                                     {QStringLiteral("moderators"), moderators},
                                     {QStringLiteral("canChange"), inviteListReliable &&
                                         meeting.value(QStringLiteral("can_change")).toBool()},
                                     {QStringLiteral("detailsUrl"), meetingPath.match(detailPath).hasMatch()
                                         ? m_portalUrl + detailPath : QString()}};
        m_detailMeetingId = id;
        if (participant.isEmpty() && moderator.isEmpty())
            m_linkStatus = tr("This meeting has no shareable links in the portal response.");
        emit linksChanged();
    });
}

bool PortalSession::inviteToMeeting(int id, const QStringList &invitees,
                                    const QStringList &moderators) {
    if (m_inviting) return false;
    m_invitationMeetingId = id;
    if (!m_signedIn || m_linksBusy || m_creatingMeeting || id <= 0 ||
        id != m_detailMeetingId || !m_meetingLinks.value(QStringLiteral("canChange")).toBool()) {
        m_invitationStatus = tr("This meeting cannot be changed here.");
        emit changed();
        return false;
    }
    const auto recipients = MeetingInvitation::normalizeRecipients(invitees, moderators, {});
    if (!recipients) {
        m_invitationStatus = tr("Check the email addresses.");
        emit changed();
        return false;
    }
    QSet<QString> existing;
    for (const auto &role : {QStringLiteral("participants"), QStringLiteral("moderators")})
        for (const auto &entry : m_meetingLinks.value(role).toList())
            existing.insert(entry.toMap().value(QStringLiteral("email")).toString().toCaseFolded());
    QStringList newInvitees;
    QStringList newModerators;
    for (const auto &email : recipients->moderators)
        if (!existing.contains(email.toCaseFolded())) newModerators.append(email);
    for (const auto &email : recipients->invitees)
        if (!existing.contains(email.toCaseFolded())) newInvitees.append(email);
    if (newInvitees.isEmpty() && newModerators.isEmpty()) {
        m_invitationStatus = tr("Everyone selected is already invited.");
        emit changed();
        return false;
    }
    sendInvitations(id, newInvitees, newModerators);
    return true;
}

bool PortalSession::removeInvitation(int meetingId, int inviteId) {
    if (!m_signedIn || m_busy || m_inviting || m_creatingMeeting || m_linksBusy ||
        meetingId <= 0 || inviteId <= 0 || meetingId != m_detailMeetingId ||
        !m_meetingLinks.value(QStringLiteral("canChange")).toBool()) return false;
    QString email;
    for (const auto &role : {QStringLiteral("moderators"), QStringLiteral("participants")})
        for (const auto &entry : m_meetingLinks.value(role).toList())
            if (entry.toMap().value(QStringLiteral("id")).toInt() == inviteId)
                email = entry.toMap().value(QStringLiteral("email")).toString();
    if (email.isEmpty()) return false;

    m_invitationMeetingId = meetingId;
    m_invitationStatus = tr("Removing invitation…");
    m_inviting = true;
    emit changed();
    const auto generation = m_generation;
    const auto detailPath = QStringLiteral("/meeting/%1/").arg(meetingId);
    const auto removePath = QStringLiteral("/meeting/%1/invite/%2/remove/").arg(meetingId).arg(inviteId);
    const QUrl formUrl(m_portalUrl + removePath + QStringLiteral("?popup=1"));
    m_invitationReply = m_network.get(portalRequest(formUrl, "text/html", m_config.language()));
    auto *formReply = m_invitationReply.data();
    connect(formReply, &QNetworkReply::finished, this,
            [this, formReply, generation, meetingId, inviteId, email, formUrl, removePath, detailPath] {
        if (m_invitationReply != formReply) return;
        m_invitationReply = nullptr;
        const auto html = formReply->readAll();
        const auto status = formReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = formReply->error();
        const auto finalUrl = formReply->url();
        formReply->deleteLater();
        if (generation != m_generation) return;
        std::optional<MeetingInvitation::RemovalForm> form;
        if (html.size() <= 64 * 1024) form = MeetingInvitation::removalForm(html);
        const auto actionUrl = form ? finalUrl.resolved(QUrl(form->action)) : QUrl();
        const QUrl origin(m_portalUrl);
        const QUrlQuery actionQuery(actionUrl);
        QTextDocument visibleText;
        if (html.size() <= 64 * 1024) visibleText.setHtml(QString::fromUtf8(html));
        if (error != QNetworkReply::NoError || status != 200 || html.size() > 64 * 1024 ||
            finalUrl.path() != removePath ||
            !visibleText.toPlainText().contains(email, Qt::CaseInsensitive) || !form ||
            actionUrl.scheme() != origin.scheme() || actionUrl.host() != origin.host() ||
            actionUrl.port(443) != origin.port(443) || actionUrl.path() != removePath ||
            actionQuery.queryItemValue(QStringLiteral("popup")) != QStringLiteral("1")) {
            m_inviting = false;
            m_invitationStatus = tr("Invitation could not be removed. Open the meeting in the portal.");
            emit changed();
            return;
        }
        QUrlQuery fields;
        fields.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), form->csrf);
        fields.addQueryItem(QStringLiteral("confirm"), form->confirm);
        QNetworkRequest request = portalRequest(actionUrl, "text/html", m_config.language());
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/x-www-form-urlencoded"));
        request.setRawHeader("X-CSRFToken", form->csrf.toUtf8());
        request.setRawHeader("Origin", origin.toEncoded());
        request.setRawHeader("Referer", formUrl.toEncoded());
        m_invitationReply = m_network.post(request, fields.query(QUrl::FullyEncoded).toUtf8());
        auto *reply = m_invitationReply.data();
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, generation, meetingId, inviteId, detailPath] {
            if (m_invitationReply != reply) return;
            m_invitationReply = nullptr;
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto error = reply->error();
            const auto finalUrl = reply->url();
            const QUrl origin(m_portalUrl);
            reply->deleteLater();
            if (generation != m_generation) return;
            m_inviting = false;
            if (error != QNetworkReply::NoError || status < 200 || status >= 400 ||
                finalUrl.scheme() != origin.scheme() || finalUrl.host() != origin.host() ||
                finalUrl.port(443) != origin.port(443) ||
                (finalUrl.path() != detailPath &&
                 finalUrl.path() != QStringLiteral("/meeting/%1/invite/%2/remove/").arg(meetingId).arg(inviteId))) {
                m_invitationStatus = tr("Invitation could not be removed. Open the meeting in the portal.");
                emit changed();
                return;
            }
            m_pendingRemovedInviteId = inviteId;
            m_invitationStatus = tr("Checking invitation…");
            emit changed();
            fetchMeetingLinks(meetingId);
        });
    });
    return true;
}

QUrl PortalSession::calendarInvite(int id) {
    if (!m_signedIn || id <= 0) return {};
    QVariantMap meeting;
    for (const auto &entry : m_meetings)
        if (entry.toMap().value(QStringLiteral("id")).toInt() == id) {
            meeting = entry.toMap();
            break;
        }
    const auto title = meeting.value(QStringLiteral("title")).toString();
    const auto start = QDateTime::fromString(meeting.value(QStringLiteral("start")).toString(), Qt::ISODate);
    const auto end = QDateTime::fromString(meeting.value(QStringLiteral("end")).toString(), Qt::ISODate);
    if (title.isEmpty() || !start.isValid() || !end.isValid() || end <= start) return {};
    const auto escaped = [](QString value) {
        value.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        value.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        value.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
        value.replace(QLatin1Char(','), QStringLiteral("\\,"));
        value.replace(QLatin1Char(';'), QStringLiteral("\\;"));
        return value;
    };
    const auto originId = QCryptographicHash::hash(m_portalUrl.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    const auto utc = [](const QDateTime &time) { return time.toUTC().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'")); };
    const auto detailsUrl = m_portalUrl + meeting.value(QStringLiteral("url")).toString();
    const QString content = QStringLiteral("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Viamiora//EN\r\n"
        "BEGIN:VEVENT\r\nUID:%1-%2@viamiora.local\r\nDTSTAMP:%3\r\nDTSTART:%4\r\nDTEND:%5\r\n"
        "SUMMARY:%6\r\nURL:%7\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n")
        .arg(QString::fromLatin1(originId), QString::number(id), utc(QDateTime::currentDateTimeUtc()),
             utc(start), utc(end), escaped(title), escaped(detailsUrl));
    auto file = QSharedPointer<QTemporaryFile>::create(QDir::tempPath() + QStringLiteral("/Viamiora-XXXXXX.ics"));
    if (!file->open() || file->write(content.toUtf8()) != content.toUtf8().size() || !file->flush()) return {};
    file->close();
    m_calendarInvites.append(file);
    if (m_calendarInvites.size() > 8) m_calendarInvites.removeFirst();
    return QUrl::fromLocalFile(file->fileName());
}

void PortalSession::cancelMeetingLinks() {
    cancelReply(m_detailReply);
    m_pendingRemovedInviteId = 0;
    m_linksBusy = false;
    m_meetingLinks.clear();
    m_detailMeetingId = 0;
    m_linkStatus.clear();
    emit linksChanged();
}
