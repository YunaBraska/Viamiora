#include "MeetingInvitation.hpp"

#include <QRegularExpression>
#include <QSet>
#include <QTextDocument>
#include <QUrlQuery>

std::optional<MeetingInvitation::Recipients> MeetingInvitation::normalizeRecipients(
    const QStringList &invitees, const QStringList &moderators, const QString &creator) {
    static const QRegularExpression emailPattern(QStringLiteral(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)"));
    Recipients result;
    QSet<QString> seen;
    QStringList moderatorCandidates = moderators;
    const auto ownAddress = creator.trimmed();
    if (emailPattern.match(ownAddress).hasMatch()) moderatorCandidates.prepend(ownAddress);
    for (const auto &candidate : moderatorCandidates) {
        const auto email = candidate.trimmed();
        if (!emailPattern.match(email).hasMatch()) return std::nullopt;
        const auto key = email.toCaseFolded();
        if (seen.contains(key)) continue;
        seen.insert(key);
        result.moderators.append(email);
    }
    for (const auto &candidate : invitees) {
        const auto email = candidate.trimmed();
        if (!emailPattern.match(email).hasMatch()) return std::nullopt;
        const auto key = email.toCaseFolded();
        if (seen.contains(key)) continue;
        seen.insert(key);
        result.invitees.append(email);
    }
    return result;
}

QString MeetingInvitation::subjectFromForm(const QByteArray &html) {
    static const QRegularExpression inputs(QStringLiteral(R"(<input\b[^>]*>)"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression name(QStringLiteral(R"REGEX(\bname\s*=\s*(['"])message_title\1)REGEX"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression value(QStringLiteral(R"REGEX(\bvalue\s*=\s*(['"])(.*?)\1)REGEX"),
                                          QRegularExpression::CaseInsensitiveOption);
    const auto source = QString::fromUtf8(html);
    auto match = inputs.globalMatch(source);
    while (match.hasNext()) {
        const auto tag = match.next().captured();
        if (!name.match(tag).hasMatch()) continue;
        const auto attribute = value.match(tag);
        if (!attribute.hasMatch()) return {};
        QTextDocument text;
        text.setHtml(attribute.captured(2));
        const auto subject = text.toPlainText().trimmed();
        return subject.size() <= 300 ? subject : QString();
    }
    return {};
}

QString MeetingInvitation::bodyFromForm(const QByteArray &html) {
    static const QRegularExpression containers(QStringLiteral(R"(<div\b[^>]*>)"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression name(QStringLiteral(R"REGEX(\bdata-name\s*=\s*(['"])message\1)REGEX"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression value(QStringLiteral(R"REGEX(\bdata-value\s*=\s*(['"])(.*?)\1)REGEX"),
                                          QRegularExpression::CaseInsensitiveOption);
    auto matches = containers.globalMatch(QString::fromUtf8(html));
    while (matches.hasNext()) {
        const auto tag = matches.next().captured();
        if (!name.match(tag).hasMatch()) continue;
        const auto encoded = value.match(tag).captured(2);
        if (encoded.isEmpty() || encoded.size() > 20000) return {};
        QTextDocument text;
        text.setHtml(encoded);
        const auto message = text.toPlainText();
        return message.size() <= 20000 ? message : QString();
    }
    return {};
}

std::optional<MeetingInvitation::RemovalForm> MeetingInvitation::removalForm(const QByteArray &html) {
    const auto source = QString::fromUtf8(html);
    static const QRegularExpression formTag(QStringLiteral(R"(<form\b[^>]*>)"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression actionAttribute(QStringLiteral(R"REGEX(\baction\s*=\s*(['"])(.*?)\1)REGEX"),
                                                     QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression postMethod(QStringLiteral(R"REGEX(\bmethod\s*=\s*(['"])post\1)REGEX"),
                                               QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression inputTag(QStringLiteral(R"(<input\b[^>]*>)"),
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression nameAttribute(QStringLiteral(R"REGEX(\bname\s*=\s*(['"])(.*?)\1)REGEX"),
                                                   QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression valueAttribute(QStringLiteral(R"REGEX(\bvalue\s*=\s*(['"])(.*?)\1)REGEX"),
                                                    QRegularExpression::CaseInsensitiveOption);
    const auto form = formTag.match(source);
    if (!form.hasMatch() || !postMethod.match(form.captured()).hasMatch()) return std::nullopt;
    const auto end = source.indexOf(QStringLiteral("</form>"), form.capturedEnd(), Qt::CaseInsensitive);
    if (end < 0) return std::nullopt;
    RemovalForm result;
    result.action = actionAttribute.match(form.captured()).captured(2);
    result.action.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    auto inputs = inputTag.globalMatch(source.mid(form.capturedEnd(), end - form.capturedEnd()));
    while (inputs.hasNext()) {
        const auto tag = inputs.next().captured();
        const auto name = nameAttribute.match(tag).captured(2);
        const auto raw = valueAttribute.match(tag).captured(2);
        QTextDocument decoded;
        decoded.setHtml(raw);
        if (name == QStringLiteral("csrfmiddlewaretoken")) result.csrf = decoded.toPlainText();
        if (name == QStringLiteral("confirm")) result.confirm = decoded.toPlainText();
    }
    if (result.action.isEmpty() || result.action.size() > 2048 ||
        result.csrf.isEmpty() || result.csrf.size() > 512 ||
        result.confirm.isEmpty() || result.confirm.size() > 128) return std::nullopt;
    return result;
}

QByteArray MeetingInvitation::requestBody(const QByteArray &csrf, const QStringList &emails,
                                          const QString &subject, const QString &message) {
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("csrfmiddlewaretoken"), QString::fromLatin1(csrf));
    form.addQueryItem(QStringLiteral("emails"), emails.join(QLatin1Char(' ')));
    form.addQueryItem(QStringLiteral("message_title"), subject);
    form.addQueryItem(QStringLiteral("message"), message);
    return form.query(QUrl::FullyEncoded).toUtf8();
}
