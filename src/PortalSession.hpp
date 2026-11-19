#pragma once

#include "CredentialStore.hpp"

#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVariantList>
#include <QTemporaryFile>
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <optional>

class PortalConfig;
class QNetworkReply;
class QJsonObject;

class SessionCookieJar final : public QNetworkCookieJar {
public:
    void clear();
};

class PortalSession final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(QStringList savedAccounts READ savedAccounts NOTIFY changed)
    Q_PROPERTY(QVariantList savedLogins READ savedLogins NOTIFY changed)
    Q_PROPERTY(QString username READ username NOTIFY changed)
    Q_PROPERTY(QString prefillPassword READ prefillPassword NOTIFY prefillChanged)
    Q_PROPERTY(QString prefillUsername READ prefillUsername NOTIFY prefillChanged)
    Q_PROPERTY(QString prefillOrigin READ prefillOrigin NOTIFY prefillChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList meetings READ meetings NOTIFY meetingsChanged)
    Q_PROPERTY(QVariantList historicalMeetings READ historicalMeetings NOTIFY historicalMeetingsChanged)
    Q_PROPERTY(bool historicalBusy READ historicalBusy NOTIFY historicalMeetingsChanged)
    Q_PROPERTY(QString historicalStatus READ historicalStatus NOTIFY historicalMeetingsChanged)
    Q_PROPERTY(QVariantList bookingTypes READ bookingTypes NOTIFY changed)
    Q_PROPERTY(bool moderatorPinSupported READ moderatorPinSupported NOTIFY changed)
    Q_PROPERTY(QVariantMap meetingLinks READ meetingLinks NOTIFY linksChanged)
    Q_PROPERTY(QString linkStatus READ linkStatus NOTIFY linksChanged)
    Q_PROPERTY(bool linksBusy READ linksBusy NOTIFY linksChanged)
    Q_PROPERTY(qint64 meetingsUpdatedAtMs READ meetingsUpdatedAtMs NOTIFY changed)
    Q_PROPERTY(bool creatingMeeting READ creatingMeeting NOTIFY changed)
    Q_PROPERTY(QString creationStatus READ creationStatus NOTIFY changed)
    Q_PROPERTY(bool inviting READ inviting NOTIFY changed)
    Q_PROPERTY(int invitationMeetingId READ invitationMeetingId NOTIFY changed)
    Q_PROPERTY(QString invitationStatus READ invitationStatus NOTIFY changed)

public:
    explicit PortalSession(PortalConfig &config, QObject *parent = nullptr);
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool signedIn() const;
    [[nodiscard]] QStringList savedAccounts() const;
    [[nodiscard]] QVariantList savedLogins() const;
    [[nodiscard]] QString username() const;
    [[nodiscard]] QString prefillPassword() const;
    [[nodiscard]] QString prefillUsername() const;
    [[nodiscard]] QString prefillOrigin() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] QVariantList meetings() const;
    [[nodiscard]] QVariantList historicalMeetings() const;
    [[nodiscard]] bool historicalBusy() const;
    [[nodiscard]] QString historicalStatus() const;
    [[nodiscard]] QVariantList bookingTypes() const;
    [[nodiscard]] bool moderatorPinSupported() const;
    [[nodiscard]] QVariantMap meetingLinks() const;
    [[nodiscard]] QString linkStatus() const;
    [[nodiscard]] bool linksBusy() const;
    [[nodiscard]] qint64 meetingsUpdatedAtMs() const;
    [[nodiscard]] qint64 automaticRefreshDueAtMs() const;
    [[nodiscard]] bool creatingMeeting() const;
    [[nodiscard]] QString creationStatus() const;
    [[nodiscard]] bool inviting() const;
    [[nodiscard]] int invitationMeetingId() const;
    [[nodiscard]] QString invitationStatus() const;
    Q_INVOKABLE void signIn(const QString &username, const QString &password);
    Q_INVOKABLE void requestSavedPassword(const QString &username);
    Q_INVOKABLE QString newPin() const;
    Q_INVOKABLE QVariantList browserCookies(const QString &address) const;
    Q_INVOKABLE void refreshMeetings();
    Q_INVOKABLE void loadMeetingWeek(const QString &firstDay, const QString &lastDay);
    Q_INVOKABLE bool refreshMeetingsIfDue(bool inactive);
    Q_INVOKABLE void inspectBookingType(int type);
    Q_INVOKABLE void fetchMeetingLinks(int id);
    Q_INVOKABLE void resolveCalendarMatches(const QVariantList &calendarEvents);
    Q_INVOKABLE bool inviteToMeeting(int id, const QStringList &invitees,
                                    const QStringList &moderators);
    Q_INVOKABLE bool removeInvitation(int meetingId, int inviteId);
    Q_INVOKABLE QUrl calendarInvite(int id);
    Q_INVOKABLE void cancelMeetingLinks();
    Q_INVOKABLE void createMeeting(const QString &title, qint64 startMs, qint64 endMs,
                                  int type, const QString &pin, const QStringList &invitees = {},
                                  const QStringList &moderators = {}, const QString &moderatorPin = {});
    Q_INVOKABLE void forgetSavedLogin(const QString &origin, const QString &username);
    Q_INVOKABLE void switchSavedLogin(const QString &origin, const QString &username);
    Q_INVOKABLE void signOut();

signals:
    void changed();
    void meetingsChanged();
    void historicalMeetingsChanged();
    void prefillChanged();
    void linksChanged();
    void meetingCreated(int id, const QString &title);

private:
    static void cancelReply(QPointer<QNetworkReply> &pending);
    static bool containsLogin(const QVariantList &logins, const QString &origin, const QString &username);
    void rememberMeetingIdentity(int id, const QJsonObject &meeting);
    void beginSignIn(PortalCredential credential);
    void fetchMeetings();
    void scheduleAutomaticRefresh(bool success, const QByteArray &retryAfter = {});
    void discoverBookingTypes();
    void loadSavedAccounts();
    void sendInvitations(int id, const QStringList &invitees, const QStringList &moderators);
    void sendNextInvitation();
    void fail(const QString &message);
    void resetSession();

    PortalConfig &m_config;
    QNetworkAccessManager m_network;
    SessionCookieJar *m_cookieJar;
    QPointer<QNetworkReply> m_reply;
    QPointer<QNetworkReply> m_historicalReply;
    QPointer<QNetworkReply> m_bookingReply;
    QPointer<QNetworkReply> m_pinInspectReply;
    QPointer<QNetworkReply> m_detailReply;
    QPointer<QNetworkReply> m_createReply;
    QPointer<QNetworkReply> m_invitationReply;
    QList<QSharedPointer<QTemporaryFile>> m_calendarInvites;
    QString m_portalUrl;
    QVariantList m_savedLogins;
    QString m_username;
    QString m_prefillPassword;
    QString m_prefillUsername;
    QString m_prefillOrigin;
    QString m_loadingPrefillUsername;
    QString m_loadingPrefillOrigin;
    quint64 m_prefillRequestId = 0;
    QString m_status;
    QVariantList m_meetings;
    QVariantList m_historicalMeetings;
    struct HistoricalWeek {
        QVariantList meetings;
        QString status;
        qint64 updatedAtMs;
    };
    QHash<QString, HistoricalWeek> m_historicalCache;
    QString m_historicalWeek;
    QString m_historicalStatus;
    qint64 m_historicalUpdatedAtMs = 0;
    qint64 m_historicalRetryAtMs = 0;
    bool m_historicalBusy = false;
    QHash<int, QVariantMap> m_meetingIdentityCache;
    QHash<int, QPointer<QNetworkReply>> m_identityReplies;
    QHash<int, qint64> m_identityRetryAtMs;
    qint64 m_nextIdentityFetchAtMs = 0;
    QVariantList m_bookingTypes;
    int m_inspectedBookingType = -1;
    bool m_moderatorPinSupported = false;
    QVariantMap m_meetingLinks;
    int m_detailMeetingId = 0;
    QString m_linkStatus;
    qint64 m_meetingsUpdatedAtMs = 0;
    qint64 m_lastMeetingFetchAtMs = 0;
    qint64 m_automaticRefreshDueAtMs = 0;
    int m_automaticRefreshFailures = 0;
    QString m_creationStatus;
    int m_createdMeetingId = 0;
    struct PendingCreation {
        QString title;
        qint64 startMs;
        qint64 endMs;
        QSet<int> knownIds;
        QStringList invitees;
        QStringList moderators;
    };
    std::optional<PendingCreation> m_pendingCreation;
    QList<QPair<QString, QStringList>> m_invitationQueue;
    QString m_invitationStatus;
    int m_invitationMeetingId = 0;
    int m_pendingRemovedInviteId = 0;
    int m_invitationFailures = 0;
    std::optional<PortalCredential> m_pendingCredential;
    quint64 m_generation = 0;
    bool m_busy = false;
    bool m_signedIn = false;
    bool m_signInAfterPrefill = false;
    bool m_linksBusy = false;
    bool m_creatingMeeting = false;
    bool m_inviting = false;
};
