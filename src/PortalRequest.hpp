#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QNetworkRequest>
#include <QSet>
#include <QSslCertificate>
#include <QSysInfo>
#include <QSslConfiguration>
#include <algorithm>

inline bool addPortalCaCertificates(const QString &path) {
    const auto certificates = QSslCertificate::fromPath(path);
    if (certificates.isEmpty()) return false;
    auto tls = QSslConfiguration::defaultConfiguration();
    auto authorities = QSslConfiguration::systemCaCertificates();
    if (authorities.isEmpty()) authorities = tls.caCertificates();
    authorities.append(certificates);
    tls.setCaCertificates(authorities);
    QSslConfiguration::setDefaultConfiguration(tls);
    return true;
}

inline QNetworkRequest portalRequest(const QUrl &url, const QByteArray &accept,
                                     const QString &language = {}) {
    QNetworkRequest request(url);
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    request.setRawHeader("Accept", accept);
    const QStringList candidates = language.isEmpty()
        ? QLocale::system().uiLanguages() : QLocale(language).uiLanguages();
    QStringList preferred;
    QSet<QString> seen;
    for (const auto &candidate : candidates) {
        const auto tag = candidate.toLower();
        if (seen.contains(tag) || preferred.size() == 5) continue;
        seen.insert(tag);
        preferred.append(candidate);
    }
    if (!seen.contains(QStringLiteral("en"))) {
        if (preferred.size() == 5) preferred.removeLast();
        preferred.append(QStringLiteral("en"));
    }
    if (preferred.isEmpty()) preferred.append(QStringLiteral("en"));
    QStringList weighted;
    for (int i = 0; i < preferred.size(); ++i)
        weighted.append(i == 0 ? preferred[i]
            : preferred[i] + QStringLiteral(";q=0.%1").arg(10 - i));
    request.setRawHeader("Accept-Language", weighted.join(QStringLiteral(", ")).toLatin1());
    request.setRawHeader("User-Agent", QStringLiteral("Viamiora/%1 (%2; QtNetwork/%3)")
        .arg(QCoreApplication::applicationVersion(), QSysInfo::productType(),
             QString::fromLatin1(qVersion())).toLatin1());
    return request;
}

inline qint64 portalRetryAfterDeadlineMs(const QByteArray &header, qint64 now) {
    if (header.isEmpty()) return 0;
    bool numeric = false;
    auto seconds = QString::fromLatin1(header.trimmed()).toLongLong(&numeric);
    if (!numeric) {
        const auto date = QDateTime::fromString(QString::fromLatin1(header), Qt::RFC2822Date);
        seconds = date.isValid() ? (date.toMSecsSinceEpoch() - now + 999) / 1000 : 0;
    }
    return seconds > 0 ? now + std::min<qint64>(seconds, 7 * 24 * 60 * 60) * 1000 : 0;
}
