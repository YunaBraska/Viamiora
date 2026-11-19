#include "ContactStore.hpp"
#include "SearchMatcher.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace {
bool validAddress(const QString &email) {
    static const QRegularExpression pattern(QStringLiteral(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)"));
    return pattern.match(email).hasMatch();
}
bool saveAddresses(const QString &path, const QJsonArray &addresses) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) return false;
    const auto bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 2}, {QStringLiteral("addresses"), addresses}}).toJson();
    return file.write(bytes) == bytes.size() && file.commit();
}
}

ContactStore::ContactStore(QString directory, QObject *parent) : QObject(parent) {
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    m_path = QDir(directory).filePath(QStringLiteral("contacts.json"));
    QFile file(m_path);
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = tr("Recent addresses could not be read.");
        m_loadFailed = true;
        return;
    }
    if (file.size() > 1024 * 1024) {
        m_error = tr("Recent addresses file is too large. It was not changed.");
        m_loadFailed = true;
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto root = document.object();
    const int version = root.value(QStringLiteral("version")).toInt();
    const auto source = root.value(QStringLiteral("addresses"));
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        version != 2 || !source.isArray()) {
        m_error = tr("Recent addresses file is invalid. It was not changed.");
        m_loadFailed = true;
        return;
    }
    const auto now = QDateTime::currentDateTimeUtc();
    bool needsSave = false;
    for (const auto &item : source.toArray()) {
        if (!item.isObject() || !item.toObject().value(QStringLiteral("email")).isString() ||
            !item.toObject().value(QStringLiteral("lastUsed")).isString()) {
            m_error = tr("Recent addresses file is invalid. It was not changed.");
            m_loadFailed = true;
            return;
        }
        const auto email = item.toObject().value(QStringLiteral("email")).toString();
        const auto used = QDateTime::fromString(
            item.toObject().value(QStringLiteral("lastUsed")).toString(), Qt::ISODateWithMs);
        if (!validAddress(email) || !used.isValid()) {
            m_error = tr("Recent addresses file is invalid. It was not changed.");
            m_loadFailed = true;
            return;
        }
        if (used < now.addYears(-1)) { needsSave = true; continue; }
        if (m_addresses.size() == 500) { needsSave = true; continue; }
        m_addresses.append(QJsonObject{{QStringLiteral("email"), email},
            {QStringLiteral("lastUsed"), used.toUTC().toString(Qt::ISODateWithMs)}});
    }
    if (needsSave && !saveAddresses(m_path, m_addresses))
        m_error = tr("Recent addresses could not be updated.");
}

QString ContactStore::error() const { return m_error; }

QStringList ContactStore::suggestions(const QString &query) const {
    QStringList result;
    const auto clean = query.trimmed();
    if (clean.size() < 2) return result;
    const auto cutoff = QDateTime::currentDateTimeUtc().addYears(-1);
    QList<QPair<int, QString>> ranked;
    for (const auto &item : m_addresses) {
        const auto object = item.toObject();
        const auto used = QDateTime::fromString(object.value(QStringLiteral("lastUsed")).toString(), Qt::ISODateWithMs);
        const auto email = object.value(QStringLiteral("email")).toString();
        const auto score = SearchMatcher::score(clean, email);
        if (used >= cutoff && score > 0) ranked.append({score, email});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
        return a.first > b.first;
    });
    for (const auto &[score, email] : ranked) {
        Q_UNUSED(score)
        result.append(email);
    }
    return result;
}

int ContactStore::matchScore(const QString &query, const QString &candidate) const {
    return SearchMatcher::score(query, candidate);
}

bool ContactStore::rememberAddress(const QString &email) {
    if (m_loadFailed) return false;
    const auto clean = email.trimmed();
    if (!validAddress(clean)) {
        m_error = tr("Enter a valid email address.");
        emit changed();
        return false;
    }
    const auto now = QDateTime::currentDateTimeUtc();
    QJsonArray updated;
    for (const auto &item : m_addresses) {
        const auto object = item.toObject();
        const auto used = QDateTime::fromString(object.value(QStringLiteral("lastUsed")).toString(), Qt::ISODateWithMs);
        if (used < now.addYears(-1) ||
            object.value(QStringLiteral("email")).toString().compare(clean, Qt::CaseInsensitive) == 0) continue;
        updated.append(item);
    }
    updated.prepend(QJsonObject{{QStringLiteral("email"), clean},
        {QStringLiteral("lastUsed"), now.toString(Qt::ISODateWithMs)}});
    while (updated.size() > 500) updated.removeLast();
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()) || !saveAddresses(m_path, updated)) {
        m_error = tr("Recent address could not be saved.");
        emit changed();
        return false;
    }
    m_addresses = updated;
    m_error.clear();
    emit changed();
    return true;
}
