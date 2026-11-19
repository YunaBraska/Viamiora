#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QString>

class MeetingStatus final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
    Q_PROPERTY(bool active READ active NOTIFY changed)

public:
    explicit MeetingStatus(QObject *parent = nullptr);
    ~MeetingStatus() override;
    [[nodiscard]] bool active() const;
    Q_INVOKABLE void setMeeting(const QString &url, const QString &title);
    Q_INVOKABLE void clear();
    void retranslate();

signals:
    void changed();
    void showMeetingRequested();
    void showOverviewRequested();
    void copyLinkRequested();
    void openBrowserRequested();
    void leaveRequested();

private:
    QString m_url;
    QString m_title;
#ifdef Q_OS_MACOS
    void *m_item = nullptr;
    void *m_target = nullptr;
#endif
};
