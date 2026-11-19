#pragma once

#include <QClipboard>
#include <QGuiApplication>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class ClipboardService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")
public:
    using QObject::QObject;
    Q_INVOKABLE void copy(const QString &text) {
        if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
    }
};
