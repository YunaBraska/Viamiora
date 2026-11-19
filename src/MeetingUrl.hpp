#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

inline QString recognizedMeetingUrl(const QString &address, const QStringList &portalOrigins = {}) {
    const QUrl url(address.trimmed());
    if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()
        || !url.userInfo().isEmpty() || address.size() > 2048) return {};

    const auto host = url.host().toLower();
    const auto path = url.path().toLower();
    const auto fragment = url.fragment(QUrl::FullyDecoded).toLower();
    bool onPortal = false;
    for (const auto &origin : portalOrigins)
        if (QUrl(origin).host().compare(host, Qt::CaseInsensitive) == 0 &&
            QUrl(origin).port(-1) == url.port(-1)) onPortal = true;
    const bool mividas = path.startsWith(QStringLiteral("/webapp/")) &&
        (fragment.contains(QStringLiteral("conference=")) ||
         QUrlQuery(url).hasQueryItem(QStringLiteral("conference")));
    static const QRegularExpression googleCode(QStringLiteral(R"(^/[a-z]{3}-[a-z]{4}-[a-z]{3}/?$)"));
    const bool google = host == QStringLiteral("meet.google.com") &&
        (googleCode.match(path).hasMatch() ||
         (path.startsWith(QStringLiteral("/lookup/")) && path.size() > 8));
    const bool teams = (host == QStringLiteral("teams.microsoft.com") &&
                        path.startsWith(QStringLiteral("/l/meetup-join/"))) ||
                       (host == QStringLiteral("teams.live.com") && path.contains(QStringLiteral("/meet/")));
    const bool zoom = (host == QStringLiteral("zoom.us") || host.endsWith(QStringLiteral(".zoom.us"))) &&
                      (path.startsWith(QStringLiteral("/j/")) || path.startsWith(QStringLiteral("/my/")) ||
                       path.startsWith(QStringLiteral("/s/")));
    const bool webex = (host == QStringLiteral("webex.com") || host.endsWith(QStringLiteral(".webex.com"))) &&
                       (path.contains(QStringLiteral("/meet/")) || path.contains(QStringLiteral("/join/")) ||
                        (path.endsWith(QStringLiteral("/j.php")) && QUrlQuery(url).hasQueryItem(QStringLiteral("MTID"))));
    const bool jitsi = host == QStringLiteral("meet.jit.si") && path.size() > 1;
    const bool whereby = host == QStringLiteral("whereby.com") && path.size() > 1;
    const bool configuredPortal = onPortal &&
        ((path.startsWith(QStringLiteral("/meeting/")) && path.size() > 9) ||
         (path.startsWith(QStringLiteral("/room/")) && path.size() > 6));
    return mividas || google || teams || zoom || webex || jitsi || whereby || configuredPortal
        ? url.toString(QUrl::FullyEncoded) : QString();
}
