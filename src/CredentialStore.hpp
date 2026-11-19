#pragma once

#include <QString>
#include <QStringList>
#include <optional>

struct PortalCredential {
    QString username;
    QString password;
};

class CredentialStore final {
public:
    [[nodiscard]] static QStringList accounts(const QString &origin);
    [[nodiscard]] static std::optional<PortalCredential> load(const QString &origin, const QString &username, QString *error);
    [[nodiscard]] static bool save(const QString &origin, const PortalCredential &credential, QString *error);
    [[nodiscard]] static bool remove(const QString &origin, const QString &username, QString *error);
};
