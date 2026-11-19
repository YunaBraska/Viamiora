#include "PortalSession.hpp"

#include "PortalConfig.hpp"
#include "PortalRequest.hpp"

#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <algorithm>

void SessionCookieJar::clear() { setAllCookies({}); }

void PortalSession::cancelReply(QPointer<QNetworkReply> &pending) {
    if (!pending) return;
    auto *reply = pending.data();
    pending = nullptr;
    reply->abort();
    reply->deleteLater();
}

bool PortalSession::containsLogin(const QVariantList &logins, const QString &origin, const QString &username) {
    return std::any_of(logins.cbegin(), logins.cend(), [&](const QVariant &item) {
        const auto login = item.toMap();
        return login.value(QStringLiteral("origin")).toString() == origin &&
               login.value(QStringLiteral("username")).toString() == username;
    });
}

PortalSession::PortalSession(PortalConfig &config, QObject *parent)
    : QObject(parent), m_config(config), m_cookieJar(new SessionCookieJar), m_portalUrl(config.portalUrl()) {
    m_network.setCookieJar(m_cookieJar);
    m_network.setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
    m_network.setTransferTimeout(15000);
    loadSavedAccounts();
    connect(&m_config, &PortalConfig::changed, this, [this] {
        if (m_portalUrl == m_config.portalUrl()) return;
        resetSession();
        m_portalUrl = m_config.portalUrl();
        loadSavedAccounts();
        m_status = tr("Portal changed. Sign in to the new portal.");
        emit changed();
    });
}

bool PortalSession::busy() const { return m_busy; }
bool PortalSession::signedIn() const { return m_signedIn; }
QStringList PortalSession::savedAccounts() const {
    QStringList accounts;
    for (const auto &item : m_savedLogins) {
        const auto login = item.toMap();
        if (login.value(QStringLiteral("origin")).toString() == m_portalUrl)
            accounts.append(login.value(QStringLiteral("username")).toString());
    }
    return accounts;
}
QVariantList PortalSession::savedLogins() const { return m_savedLogins; }
QString PortalSession::username() const { return m_username; }
QString PortalSession::prefillPassword() const { return m_prefillPassword; }
QString PortalSession::prefillUsername() const { return m_prefillUsername; }
QString PortalSession::prefillOrigin() const { return m_prefillOrigin; }
QString PortalSession::status() const { return m_status; }
QVariantList PortalSession::meetings() const { return m_meetings; }
QVariantList PortalSession::historicalMeetings() const { return m_historicalMeetings; }
bool PortalSession::historicalBusy() const { return m_historicalBusy; }
QString PortalSession::historicalStatus() const { return m_historicalStatus; }
QVariantList PortalSession::bookingTypes() const { return m_bookingTypes; }
bool PortalSession::moderatorPinSupported() const { return m_moderatorPinSupported; }
QVariantMap PortalSession::meetingLinks() const { return m_meetingLinks; }
QString PortalSession::linkStatus() const { return m_linkStatus; }
bool PortalSession::linksBusy() const { return m_linksBusy; }
qint64 PortalSession::meetingsUpdatedAtMs() const { return m_meetingsUpdatedAtMs; }
qint64 PortalSession::automaticRefreshDueAtMs() const { return m_automaticRefreshDueAtMs; }
bool PortalSession::creatingMeeting() const { return m_creatingMeeting; }
QString PortalSession::creationStatus() const { return m_creationStatus; }
bool PortalSession::inviting() const { return m_inviting; }
int PortalSession::invitationMeetingId() const { return m_invitationMeetingId; }
QString PortalSession::invitationStatus() const { return m_invitationStatus; }

void PortalSession::loadSavedAccounts() {
    const auto origins = m_config.portalUrls();
    if (origins.isEmpty()) return;
    const auto generation = m_generation;
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this,
            [this, watcher, generation] {
        const auto logins = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation) return;
        m_savedLogins = logins;
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([origins] {
        QVariantList logins;
        for (const auto &origin : origins)
            for (const auto &username : CredentialStore::accounts(origin))
                logins.append(QVariantMap{{QStringLiteral("origin"), origin},
                                          {QStringLiteral("username"), username}});
        return logins;
    }));
}

void PortalSession::signIn(const QString &username, const QString &password) {
    if (m_busy) return;
    if (m_portalUrl.isEmpty()) {
        m_status = tr("Choose a valid portal address first.");
        emit changed();
        return;
    }
    if (username.trimmed().isEmpty()) {
        m_status = tr("Enter your username and password.");
        emit changed();
        return;
    }
    if (password.isEmpty()) {
        if (m_prefillOrigin == m_portalUrl && m_prefillUsername == username.trimmed()
            && !m_prefillPassword.isEmpty()) {
            beginSignIn({username.trimmed(), m_prefillPassword});
            return;
        }
        if (m_loadingPrefillOrigin == m_portalUrl &&
            m_loadingPrefillUsername == username.trimmed()) {
            m_signInAfterPrefill = true;
            m_busy = true;
            m_status = tr("Preparing sign-in…");
            emit changed();
            return;
        }
        if (!savedAccounts().contains(username.trimmed())) {
            m_status = tr("Enter your password.");
            emit changed();
            return;
        }
        requestSavedPassword(username);
        m_signInAfterPrefill = true;
        m_busy = true;
        m_status = tr("Preparing sign-in…");
        emit changed();
        return;
    }
    beginSignIn({username.trimmed(), password});
}

void PortalSession::requestSavedPassword(const QString &username) {
    const auto clean = username.trimmed();
    const auto origin = m_portalUrl;
    if (!clean.isEmpty() && origin == m_loadingPrefillOrigin &&
        clean == m_loadingPrefillUsername) return;
    if (!clean.isEmpty() && origin == m_prefillOrigin && clean == m_prefillUsername &&
        !m_prefillPassword.isEmpty()) return;
    const auto requestId = ++m_prefillRequestId;
    m_loadingPrefillUsername.clear();
    m_loadingPrefillOrigin.clear();
    if (m_signInAfterPrefill) {
        m_signInAfterPrefill = false;
        m_busy = false;
        m_status.clear();
        emit changed();
    }
    m_prefillPassword.clear();
    m_prefillUsername.clear();
    m_prefillOrigin.clear();
    emit prefillChanged();
    if (origin.isEmpty() || clean.isEmpty() || !savedAccounts().contains(clean)) return;
    m_loadingPrefillUsername = clean;
    m_loadingPrefillOrigin = origin;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
            [this, watcher, requestId, origin, clean] {
        if (requestId == m_prefillRequestId && origin == m_portalUrl) {
            m_prefillPassword = watcher->result();
            m_prefillUsername = clean;
            m_prefillOrigin = origin;
            m_loadingPrefillUsername.clear();
            m_loadingPrefillOrigin.clear();
            emit prefillChanged();
            if (m_signInAfterPrefill) {
                m_signInAfterPrefill = false;
                if (m_prefillPassword.isEmpty())
                    fail(tr("Saved password unavailable. Enter it again."));
                else
                    beginSignIn({clean, m_prefillPassword});
            }
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([origin, clean] {
        const auto saved = CredentialStore::load(origin, clean, nullptr);
        return saved ? saved->password : QString();
    }));
}
QString PortalSession::newPin() const {
    return QString::number(QRandomGenerator::system()->bounded(100000, 1000000));
}
QVariantList PortalSession::browserCookies(const QString &address) const {
    QVariantList result;
    const QUrl target(address);
    const QUrl portal(m_portalUrl);
    if (!m_signedIn || !target.isValid() || target.scheme() != QStringLiteral("https") ||
        target.scheme() != portal.scheme() || target.host() != portal.host() ||
        target.port(443) != portal.port(443)) return result;
    for (const auto &cookie : m_cookieJar->cookiesForUrl(portal)) {
        result.append(QVariantMap{
            {QStringLiteral("domain"), cookie.domain().isEmpty() ? portal.host() : cookie.domain()},
            {QStringLiteral("name"), QString::fromLatin1(cookie.name())},
            {QStringLiteral("value"), QString::fromLatin1(cookie.value())}
        });
    }
    return result;
}

void PortalSession::beginSignIn(PortalCredential credential) {
    resetSession();
    m_pendingCredential = std::move(credential);
    m_busy = true;
    m_status = tr("Signing in…");
    emit changed();

    const quint64 generation = m_generation;
    const QUrl loginUrl(m_portalUrl + QStringLiteral("/accounts/login/?next=/"));
    QNetworkRequest request = portalRequest(loginUrl, "text/html", m_config.language());
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, generation, loginUrl] {
        QNetworkReply *reply = m_reply;
        if (!reply || generation != m_generation) return;
        m_reply = nullptr;
        const auto body = reply->readAll();
        const auto error = reply->error();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (error != QNetworkReply::NoError || status != 200 || body.size() > 1024 * 1024) {
            fail(error == QNetworkReply::SslHandshakeFailedError
                ? tr("Secure connection failed. Check this Mac's certificate trust settings.")
                : tr("The portal login page could not be loaded. Try again or open the portal in your browser."));
            return;
        }

        QString csrf;
        const QString html = QString::fromUtf8(body);
        const QRegularExpression inputs(QStringLiteral("<input\\b[^>]*>"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression name(QStringLiteral("\\bname\\s*=\\s*(['\"])csrfmiddlewaretoken\\1"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpression value(QStringLiteral("\\bvalue\\s*=\\s*(['\"])([^'\"]+)\\1"), QRegularExpression::CaseInsensitiveOption);
        auto iterator = inputs.globalMatch(html);
        while (iterator.hasNext()) {
            const auto input = iterator.next().captured();
            if (!name.match(input).hasMatch()) continue;
            csrf = value.match(input).captured(2);
            break;
        }
        if (csrf.isEmpty()) {
            fail(tr("This portal's sign-in form is not supported. Open it in your browser."));
            return;
        }

        QUrlQuery form;
        form.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), csrf);
        form.addQueryItem(QStringLiteral("username"), m_pendingCredential->username);
        form.addQueryItem(QStringLiteral("password"), m_pendingCredential->password);
        QNetworkRequest post = portalRequest(loginUrl, "text/html,application/json", m_config.language());
        post.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        post.setRawHeader("Referer", loginUrl.toEncoded());
        post.setRawHeader("Origin", QUrl(m_portalUrl).toEncoded());
        m_reply = m_network.post(post, form.query(QUrl::FullyEncoded).toUtf8());
        connect(m_reply, &QNetworkReply::finished, this, [this, generation] {
            QNetworkReply *postReply = m_reply;
            if (!postReply || generation != m_generation) return;
            m_reply = nullptr;
            const auto error = postReply->error();
            const auto status = postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto path = postReply->url().path();
            postReply->deleteLater();
            if (error != QNetworkReply::NoError || status < 200 || status >= 400) {
                fail(tr("Sign-in failed. Check the portal address and try again in your browser if needed."));
                return;
            }
            if (path.startsWith(QStringLiteral("/accounts/login"))) {
                fail(tr("Sign-in was not accepted. Check your username and password."));
                return;
            }
            fetchMeetings();
        });
    });
}

void PortalSession::forgetSavedLogin(const QString &origin, const QString &username) {
    if (origin.isEmpty() || username.trimmed().isEmpty()) return;
    const auto clean = username.trimmed();
    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this,
            [this, watcher, origin, clean] {
        const auto result = watcher->result();
        watcher->deleteLater();
        bool usernameForgotten = true;
        if (result.first) {
            for (qsizetype index = m_savedLogins.size() - 1; index >= 0; --index) {
                const auto login = m_savedLogins.at(index).toMap();
                if (login.value(QStringLiteral("origin")).toString() == origin &&
                    login.value(QStringLiteral("username")).toString() == clean)
                    m_savedLogins.removeAt(index);
            }
            usernameForgotten = m_config.forgetUsername(origin, clean);
            if (m_prefillOrigin == origin && m_prefillUsername == clean) {
                m_prefillPassword.clear();
                m_prefillUsername.clear();
                m_prefillOrigin.clear();
                emit prefillChanged();
            }
        }
        m_status = !result.first ? result.second
            : usernameForgotten ? tr("Saved account removed.")
                                : tr("Password removed, but the username could not be forgotten.");
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([origin, clean] {
        QString error;
        const bool removed = CredentialStore::remove(origin, clean, &error);
        return qMakePair(removed, error);
    }));
}

void PortalSession::switchSavedLogin(const QString &origin, const QString &username) {
    if (m_busy || m_creatingMeeting || m_inviting) return;
    const auto clean = username.trimmed();
    if (!containsLogin(m_savedLogins, origin, clean) ||
        (m_signedIn && m_portalUrl == origin && m_username == clean) ||
        !m_config.setPortalUrl(origin)) return;
    m_busy = true;
    m_status = tr("Switching account…");
    emit changed();
    const auto generation = m_generation;
    auto *watcher = new QFutureWatcher<std::optional<PortalCredential>>(this);
    connect(watcher, &QFutureWatcher<std::optional<PortalCredential>>::finished, this,
            [this, watcher, generation, origin] {
        const auto saved = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation || origin != m_portalUrl || !m_busy) return;
        if (saved) beginSignIn(*saved);
        else fail(tr("Saved password unavailable. Enter it again."));
    });
    watcher->setFuture(QtConcurrent::run([origin, clean] {
        return CredentialStore::load(origin, clean, nullptr);
    }));
}

void PortalSession::signOut() {
    if (m_creatingMeeting) return;
    if (!m_signedIn) {
        resetSession();
        m_status = tr("Signed out.");
        emit changed();
        return;
    }
    if (m_busy) return;
    m_busy = true;
    m_status = tr("Signing out…");
    emit changed();
    const quint64 generation = m_generation;
    QNetworkRequest request = portalRequest(QUrl(m_portalUrl + QStringLiteral("/accounts/logout/")),
                                            "text/html", m_config.language());
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, generation] {
        QNetworkReply *reply = m_reply;
        if (!reply || generation != m_generation) return;
        m_reply = nullptr;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool confirmed = reply->error() == QNetworkReply::NoError && status >= 200 && status < 400;
        reply->deleteLater();
        resetSession();
        m_status = confirmed ? tr("Signed out.")
                             : tr("Signed out here. Portal sign-out could not be confirmed.");
        emit changed();
    });
}

void PortalSession::fail(const QString &message) {
    m_busy = false;
    m_pendingCredential.reset();
    if (m_pendingCreation) {
        m_creationStatus = tr("Booking could not be confirmed. Check the portal before trying again.");
        m_pendingCreation.reset();
    }
    m_status = message;
    emit changed();
}

void PortalSession::resetSession() {
    ++m_generation;
    ++m_prefillRequestId;
    m_prefillPassword.clear();
    m_prefillUsername.clear();
    m_prefillOrigin.clear();
    m_loadingPrefillUsername.clear();
    m_loadingPrefillOrigin.clear();
    m_signInAfterPrefill = false;
    emit prefillChanged();
    cancelReply(m_reply);
    cancelReply(m_historicalReply);
    m_historicalMeetings.clear();
    m_historicalCache.clear();
    m_historicalWeek.clear();
    m_historicalStatus.clear();
    m_historicalUpdatedAtMs = 0;
    m_historicalRetryAtMs = 0;
    m_historicalBusy = false;
    emit historicalMeetingsChanged();
    cancelReply(m_bookingReply);
    cancelReply(m_pinInspectReply);
    m_inspectedBookingType = -1;
    m_moderatorPinSupported = false;
    cancelReply(m_detailReply);
    for (auto &reply : m_identityReplies) cancelReply(reply);
    m_identityReplies.clear();
    m_meetingIdentityCache.clear();
    m_identityRetryAtMs.clear();
    m_nextIdentityFetchAtMs = 0;
    cancelReply(m_createReply);
    cancelReply(m_invitationReply);
    m_invitationQueue.clear();
    m_invitationStatus.clear();
    m_invitationMeetingId = 0;
    m_pendingRemovedInviteId = 0;
    m_invitationFailures = 0;
    m_inviting = false;
    m_calendarInvites.clear();
    m_cookieJar->clear();
    m_pendingCredential.reset();
    if (!m_meetings.isEmpty()) { m_meetings.clear(); emit meetingsChanged(); }
    m_bookingTypes.clear();
    m_meetingLinks.clear();
    m_detailMeetingId = 0;
    m_linkStatus.clear();
    m_linksBusy = false;
    m_meetingsUpdatedAtMs = 0;
    m_lastMeetingFetchAtMs = 0;
    m_automaticRefreshDueAtMs = 0;
    m_automaticRefreshFailures = 0;
    m_creationStatus.clear();
    m_pendingCreation.reset();
    m_createdMeetingId = 0;
    m_creatingMeeting = false;
    emit linksChanged();
    m_username.clear();
    m_signedIn = false;
    m_busy = false;
}
