#pragma once

#include <QByteArray>
#include <QStringList>
#include <optional>

namespace MeetingInvitation {
struct Recipients {
    QStringList invitees;
    QStringList moderators;
};
struct RemovalForm {
    QString action;
    QString csrf;
    QString confirm;
};
[[nodiscard]] std::optional<Recipients> normalizeRecipients(const QStringList &invitees,
                                                              const QStringList &moderators,
                                                              const QString &creator);
[[nodiscard]] QString subjectFromForm(const QByteArray &html);
[[nodiscard]] QString bodyFromForm(const QByteArray &html);
[[nodiscard]] std::optional<RemovalForm> removalForm(const QByteArray &html);
[[nodiscard]] QByteArray requestBody(const QByteArray &csrf, const QStringList &emails,
                                     const QString &subject, const QString &message);
}
