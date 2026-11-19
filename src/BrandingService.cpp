#include "BrandingService.hpp"
#include "PortalConfig.hpp"
#include "PortalRequest.hpp"
#include <QColor>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <algorithm>

BrandingService::BrandingService(PortalConfig &config, QObject *parent)
    : QObject(parent), m_config(config) {
    m_network.setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
    m_network.setTransferTimeout(8000);
}
QString BrandingService::accent() const { return m_accent; }
QString BrandingService::brandName() const { return m_brandName; }
QUrl BrandingService::logoUrl() const { return m_logoUrl; }

void BrandingService::clear() {
    ++m_generation;
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); m_reply = nullptr; }
    if (m_loginReply) { m_loginReply->abort(); m_loginReply->deleteLater(); m_loginReply = nullptr; }
    m_candidates.clear();
    m_accent = QStringLiteral("#315da8");
    m_brandName.clear();
    m_logoUrl.clear();
    m_lastOrigin.clear();
    m_nextLoadAtMs = 0;
    emit changed();
}

void BrandingService::load(const QString &portalOrigin) {
    const QUrl portal(portalOrigin);
    if (portal.scheme() != QStringLiteral("https") || portal.host().isEmpty()) {
        clear();
        return;
    }
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastOrigin == portalOrigin && now < m_nextLoadAtMs) return;
    clear();
    m_lastOrigin = portalOrigin;
    m_nextLoadAtMs = now + 10 * 60 * 1000 + QRandomGenerator::global()->bounded(2 * 60 * 1000);
    const QUrl loginUrl = portal.resolved(QUrl(QStringLiteral("/accounts/login/")));
    QNetworkRequest loginRequest = portalRequest(loginUrl, "text/html", m_config.language());
    m_loginReply = m_network.get(loginRequest);
    auto *loginReply = m_loginReply.data();
    const quint64 generation = m_generation;
    connect(loginReply, &QNetworkReply::finished, this, [this, loginReply, generation, loginUrl, portal] {
        if (m_loginReply != loginReply) return;
        m_loginReply = nullptr;
        const auto bytes = loginReply->readAll();
        const auto status = loginReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto finalUrl = loginReply->url();
        const auto error = loginReply->error();
        const auto retryAfter = loginReply->rawHeader("Retry-After");
        loginReply->deleteLater();
        if (status == 429 || status == 503) {
            backOff(retryAfter);
            return;
        }
        if (generation != m_generation || error != QNetworkReply::NoError || status != 200 ||
            finalUrl.host() != loginUrl.host() || bytes.size() > 256 * 1024) return;
        const auto html = QString::fromUtf8(bytes);
        static const QRegularExpression logoBlock(
            QStringLiteral(R"REGEX(<[^>]+class\s*=\s*['"][^'"]*\blogin-logo\b[^'"]*['"][^>]*>[\s\S]{0,2000}?</div>)REGEX"),
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression imageTag(QStringLiteral(R"(<img\b[^>]*>)"),
                                                  QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression imageSource(
            QStringLiteral(R"REGEX(\bsrc\s*=\s*['"]([^'"]+)['"])REGEX"),
            QRegularExpression::CaseInsensitiveOption);
        const auto block = logoBlock.match(html);
        const auto tag = imageTag.match(block.captured());
        const auto src = imageSource.match(tag.captured()).captured(1);
        if (src.isEmpty()) return;
        const auto resolved = loginUrl.resolved(QUrl(src));
        if (resolved.scheme() != QStringLiteral("https") || resolved.host() != portal.host() ||
            resolved.port(443) != portal.port(443) || !resolved.userInfo().isEmpty()) return;
        m_logoUrl = resolved;
        emit changed();
    });
    m_candidates.append(portal.resolved(QUrl(QStringLiteral("/webapp/branding/manifest.json"))));
    const auto labels = portal.host().split(QLatin1Char('.'));
    if (labels.size() >= 4) {
        QUrl parent = portal;
        parent.setHost(labels.mid(1).join(QLatin1Char('.')));
        m_candidates.append(parent.resolved(QUrl(QStringLiteral("/webapp/branding/manifest.json"))));
    }
    requestNext();
}

void BrandingService::requestNext() {
    if (m_candidates.isEmpty()) return;
    const auto url = m_candidates.takeFirst();
    QNetworkRequest request = portalRequest(url, "application/json", m_config.language());
    m_reply = m_network.get(request);
    const quint64 generation = m_generation;
    connect(m_reply, &QNetworkReply::finished, this, [this, generation, url] {
        if (generation != m_generation || !m_reply) return;
        auto *reply = m_reply.data();
        m_reply = nullptr;
        const auto bytes = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto retryAfter = reply->rawHeader("Retry-After");
        const bool okay = reply->error() == QNetworkReply::NoError && status == 200 &&
                          bytes.size() <= 64 * 1024 && reply->url().host() == url.host();
        reply->deleteLater();
        if (status == 429 || status == 503) {
            backOff(retryAfter);
            return;
        }
        if (!okay) { requestNext(); return; }
        const auto document = QJsonDocument::fromJson(bytes);
        const auto root = document.object();
        if (!document.isObject() || !root.value(QStringLiteral("meta")).isObject() ||
            !root.value(QStringLiteral("images")).isObject()) {
            requestNext();
            return;
        }
        const auto name = root.value(QStringLiteral("brandName")).toString().trimmed();
        if (!name.isEmpty() && name.size() <= 80) m_brandName = name;
        const auto logo = root.value(QStringLiteral("images")).toObject().value(QStringLiteral("logo")).toString();
        const auto resolved = url.resolved(QUrl(logo));
        if (m_logoUrl.isEmpty() && !logo.isEmpty() && resolved.scheme() == QStringLiteral("https") &&
            resolved.host() == url.host()) m_logoUrl = resolved;
        const auto meta = root.value(QStringLiteral("meta")).toObject();
        if (meta.value(QStringLiteral("name")).toString().compare(QStringLiteral("DEFAULT"), Qt::CaseInsensitive) != 0) {
            const QColor candidate(meta.value(QStringLiteral("baseColor")).toString());
            if (candidate.isValid() && candidate.alpha() == 255) {
                const double luminance = 0.2126 * candidate.redF() +
                                         0.7152 * candidate.greenF() +
                                         0.0722 * candidate.blueF();
                if (luminance <= 0.35) m_accent = candidate.name();
            }
        }
        emit changed();
    });
}

void BrandingService::backOff(const QByteArray &retryAfter) {
    const auto now = QDateTime::currentMSecsSinceEpoch();
    m_nextLoadAtMs = std::max(m_nextLoadAtMs, portalRetryAfterDeadlineMs(retryAfter, now));
    m_candidates.clear();
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); m_reply = nullptr; }
    if (m_loginReply) { m_loginReply->abort(); m_loginReply->deleteLater(); m_loginReply = nullptr; }
}
