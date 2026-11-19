#pragma once

#include <QByteArray>
#include <QNetworkReply>
#include <QSet>
#include <QUrl>
#include <QVariantList>

namespace MeetingBooking {
[[nodiscard]] QString formAction(const QByteArray &html);
[[nodiscard]] QString initialPin(const QByteArray &html);
[[nodiscard]] bool supportsModeratorPin(const QByteArray &html);
[[nodiscard]] QByteArray requestBody(const QByteArray &csrf, const QString &title,
                                     qint64 startMs, qint64 endMs, int type,
                                     const QString &pin, const QString &initialPin,
                                     const QString &moderatorPin = {});
[[nodiscard]] int responseId(int status, QNetworkReply::NetworkError error,
                             const QUrl &url);
[[nodiscard]] int newMatchingId(const QVariantList &meetings, const QSet<int> &knownIds,
                                const QString &title, qint64 startMs, qint64 endMs);
}
