#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QVariantList>

class SystemContacts final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool authorized READ authorized NOTIFY changed)

public:
    explicit SystemContacts(QObject *parent = nullptr);
    [[nodiscard]] QVariantList results() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] bool authorized() const;
    Q_INVOKABLE bool requestAccess();
    Q_INVOKABLE bool search(const QString &query);
    Q_INVOKABLE void refreshAuthorization();

signals:
    void changed();

private:
    QVariantList m_results;
    QString m_status;
    quint64 m_requestId = 0;
    bool m_authorized = false;
};
