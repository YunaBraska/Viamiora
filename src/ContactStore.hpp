#pragma once
#include <QJsonArray>
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QStringList>

class ContactStore final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit ContactStore(QString directory = {}, QObject *parent = nullptr);
    [[nodiscard]] QString error() const;
    Q_INVOKABLE QStringList suggestions(const QString &query) const;
    Q_INVOKABLE int matchScore(const QString &query, const QString &candidate) const;
    Q_INVOKABLE bool rememberAddress(const QString &email);
signals:
    void changed();
private:
    QString m_path;
    QJsonArray m_addresses;
    QString m_error;
    bool m_loadFailed = false;
};
