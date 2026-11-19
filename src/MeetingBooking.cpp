#include "MeetingBooking.hpp"

#include <QDateTime>
#include <QRegularExpression>
#include <QUrlQuery>

QString MeetingBooking::formAction(const QByteArray &html) {
    static const QRegularExpression form(QStringLiteral(R"(<form\b[^>]*>)"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression action(QStringLiteral(R"REGEX(\baction\s*=\s*(['"])(.*?)\1)REGEX"),
                                           QRegularExpression::CaseInsensitiveOption);
    const auto match = form.match(QString::fromUtf8(html));
    if (!match.hasMatch()) return {};
    const auto attribute = action.match(match.captured());
    auto path = attribute.captured(2);
    path.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return path.size() <= 2048 ? path : QString();
}

QString MeetingBooking::initialPin(const QByteArray &html) {
    static const QRegularExpression input(QStringLiteral(R"(<input\b[^>]*>)"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression name(QStringLiteral(R"REGEX(\bname\s*=\s*(['"])initial-pin\1)REGEX"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression value(QStringLiteral(R"REGEX(\bvalue\s*=\s*(['"])(.*?)\1)REGEX"),
                                          QRegularExpression::CaseInsensitiveOption);
    auto matches = input.globalMatch(QString::fromUtf8(html));
    while (matches.hasNext()) {
        const auto tag = matches.next().captured();
        if (!name.match(tag).hasMatch()) continue;
        const auto pin = value.match(tag).captured(2);
        static const QRegularExpression digits(QStringLiteral(R"(^[0-9]{4,12}$)"));
        return digits.match(pin).hasMatch() ? pin : QString();
    }
    return {};
}

bool MeetingBooking::supportsModeratorPin(const QByteArray &html) {
    static const QRegularExpression field(
        QStringLiteral(R"REGEX(<input\b[^>]*\bname\s*=\s*['"]lobby_pin['"][^>]*>)REGEX"),
        QRegularExpression::CaseInsensitiveOption);
    return field.match(QString::fromUtf8(html)).hasMatch();
}

QByteArray MeetingBooking::requestBody(const QByteArray &csrf, const QString &title,
                                       qint64 startMs, qint64 endMs, int type,
                                       const QString &pin, const QString &initialPin,
                                       const QString &moderatorPin) {
    const auto start = QDateTime::fromMSecsSinceEpoch(startMs);
    const auto end = QDateTime::fromMSecsSinceEpoch(endMs);
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), QString::fromLatin1(csrf));
    form.addQueryItem(QStringLiteral("wizard"), QStringLiteral("1"));
    form.addQueryItem(QStringLiteral("meeting_type"), QString::number(type));
    form.addQueryItem(QStringLiteral("title"), title);
    form.addQueryItem(QStringLiteral("date"), start.toString(QStringLiteral("yyyy-MM-dd")));
    form.addQueryItem(QStringLiteral("date_start"), start.toString(QStringLiteral("yyyy-MM-dd")));
    form.addQueryItem(QStringLiteral("date"), start.toString(QStringLiteral("HH:mm")));
    form.addQueryItem(QStringLiteral("time_start"), start.toString(QStringLiteral("HH:mm")));
    form.addQueryItem(QStringLiteral("date"), end.toString(QStringLiteral("HH:mm")));
    form.addQueryItem(QStringLiteral("time_stop"), end.toString(QStringLiteral("HH:mm")));
    form.addQueryItem(QStringLiteral("recurring_rule"), QString());
    form.addQueryItem(QStringLiteral("layout"), QStringLiteral("automatic"));
    form.addQueryItem(QStringLiteral("use_pin"), QStringLiteral("on"));
    form.addQueryItem(QStringLiteral("pin"), pin);
    form.addQueryItem(QStringLiteral("initial-pin"), initialPin);
    if (!moderatorPin.isEmpty()) {
        form.addQueryItem(QStringLiteral("use_lobby_pin"), QStringLiteral("on"));
        form.addQueryItem(QStringLiteral("lobby_pin"), moderatorPin);
    }
    form.addQueryItem(QStringLiteral("source_meeting"), QString());
    return form.query(QUrl::FullyEncoded).toUtf8();
}

int MeetingBooking::responseId(int status, QNetworkReply::NetworkError error,
                               const QUrl &url) {
    if (status < 200 || status >= 300 || error != QNetworkReply::NoError) return 0;
    static const QRegularExpression invitePath(QStringLiteral(R"(^/meeting/([0-9]+)/invite/$)"));
    const auto match = invitePath.match(url.path());
    if (!match.hasMatch()) return 0;
    const QUrlQuery query(url);
    if (query.queryItemValue(QStringLiteral("wizard_current")) != QStringLiteral("2")) return 0;
    return match.captured(1).toInt();
}

int MeetingBooking::newMatchingId(const QVariantList &meetings, const QSet<int> &knownIds,
                                  const QString &title, qint64 startMs, qint64 endMs) {
    int found = 0;
    for (const auto &entry : meetings) {
        const auto item = entry.toMap();
        const int id = item.value(QStringLiteral("id")).toInt();
        if (id <= 0 || knownIds.contains(id) ||
            item.value(QStringLiteral("title")).toString() != title) continue;
        const auto start = QDateTime::fromString(item.value(QStringLiteral("start")).toString(), Qt::ISODate);
        const auto end = QDateTime::fromString(item.value(QStringLiteral("end")).toString(), Qt::ISODate);
        if (!start.isValid() || !end.isValid() ||
            qAbs(start.toMSecsSinceEpoch() - startMs) > 1000 ||
            qAbs(end.toMSecsSinceEpoch() - endMs) > 1000) continue;
        if (found) return 0;
        found = id;
    }
    return found;
}
