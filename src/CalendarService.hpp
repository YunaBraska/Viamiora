#pragma once

#include <QDateTime>
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QVariantList>
#include <QStringList>
class PortalConfig;

class CalendarService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(QVariantList events READ events NOTIFY eventsChanged)
    Q_PROPERTY(QStringList disabledCalendarIds READ disabledCalendarIds NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool authorized READ authorized NOTIFY changed)
    Q_PROPERTY(qint64 updatedAtMs READ updatedAtMs NOTIFY changed)
    Q_PROPERTY(bool suggesting READ suggesting NOTIFY changed)

public:
    explicit CalendarService(PortalConfig &portal, QObject *parent = nullptr);
    [[nodiscard]] QVariantList events() const;
    [[nodiscard]] QStringList disabledCalendarIds() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] bool connected() const;
    [[nodiscard]] bool authorized() const;
    [[nodiscard]] qint64 updatedAtMs() const;
    [[nodiscard]] bool suggesting() const;
    Q_INVOKABLE bool requestAccess();
    Q_INVOKABLE bool coversRange(const QString &firstDay, const QString &lastDay) const;
    Q_INVOKABLE bool refresh(const QString &isoDate);
    Q_INVOKABLE bool suggestNextHour();
    Q_INVOKABLE void refreshAuthorization();
    Q_INVOKABLE void refreshSelection();
    Q_INVOKABLE void openCalendar();

signals:
    void changed();
    void eventsChanged();
    void slotSuggested(const QDateTime &start);

private:
    PortalConfig &m_portal;
    QVariantList m_events;
    QStringList m_disabledCalendarIds;
    QString m_status;
    bool m_connected = false;
    bool m_authorized = false;
    bool m_suggesting = false;
    QString m_loadedDay;
    qint64 m_updatedAtMs = 0;
    qint64 m_nextCalendarRefreshAtMs = 0;
    qint64 m_nextSelectionCheckAtMs = 0;
    QString m_requestedDay;
    QString m_loadingDay;
    QString m_pendingPermissionDay;
    bool m_permissionRequestInFlight = false;
    quint64 m_requestId = 0;
    quint64 m_selectionRequestId = 0;
};
