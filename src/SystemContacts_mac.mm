#include "SystemContacts.hpp"
#include <QCoreApplication>
#include "SearchMatcher.hpp"

#include <Contacts/Contacts.h>
#include <QPointer>
#include <dispatch/dispatch.h>
#include <algorithm>

SystemContacts::SystemContacts(QObject *parent)
    : QObject(parent), m_status(QCoreApplication::translate("SystemContacts", "Search your macOS Contacts when needed.")),
      m_authorized([CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts] == CNAuthorizationStatusAuthorized) {}

QVariantList SystemContacts::results() const { return m_results; }
QString SystemContacts::status() const { return m_status; }
bool SystemContacts::authorized() const { return m_authorized; }
void SystemContacts::refreshAuthorization() {
    const bool authorizedNow = [CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts] == CNAuthorizationStatusAuthorized;
    if (m_authorized == authorizedNow) return;
    m_authorized = authorizedNow;
    emit changed();
}

bool SystemContacts::requestAccess() {
    const auto authorization = [CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts];
    m_authorized = authorization == CNAuthorizationStatusAuthorized;
    if (authorization == CNAuthorizationStatusAuthorized) {
        m_status = QCoreApplication::translate("SystemContacts", "Contacts access is on.");
        emit changed();
        return true;
    }
    if (authorization != CNAuthorizationStatusNotDetermined) {
        m_status = QCoreApplication::translate("SystemContacts", "Contacts access is off. Enable it in System Settings.");
        emit changed();
        return false;
    }
    const auto requestId = ++m_requestId;
    m_status = QCoreApplication::translate("SystemContacts", "Waiting for Contacts permission…");
    emit changed();
    auto *store = [CNContactStore new];
    const QPointer<SystemContacts> self(this);
    [store requestAccessForEntityType:CNEntityTypeContacts completionHandler:^(BOOL granted, NSError *error) {
        const bool failed = error != nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!self || self->m_requestId != requestId) return;
            self->refreshAuthorization();
            self->m_status = failed ? QCoreApplication::translate("SystemContacts", "Contacts access failed. Try again in System Settings.")
                                    : granted ? QCoreApplication::translate("SystemContacts", "Contacts access is on.")
                                              : QCoreApplication::translate("SystemContacts", "Contacts access is off. Enable it in System Settings.");
            emit self->changed();
        });
    }];
    return true;
}

bool SystemContacts::search(const QString &query) {
    const auto cleanQuery = query.trimmed();
    const auto requestId = ++m_requestId;
    m_results.clear();
    if (cleanQuery.size() < 2) {
        m_status = QCoreApplication::translate("SystemContacts", "Enter at least two characters to search Contacts.");
        emit changed();
        return false;
    }

    const auto authorization = [CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts];
    m_authorized = authorization == CNAuthorizationStatusAuthorized;
    if (authorization == CNAuthorizationStatusNotDetermined) {
        m_status = QCoreApplication::translate("SystemContacts", "Waiting for Contacts permission…");
        emit changed();
        auto *store = [CNContactStore new];
        const QPointer<SystemContacts> self(this);
        [store requestAccessForEntityType:CNEntityTypeContacts completionHandler:^(BOOL granted, NSError *error) {
            const bool failed = error != nil;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!self || self->m_requestId != requestId) return;
                if (granted) self->search(cleanQuery);
                else {
                    self->refreshAuthorization();
                    self->m_status = failed ? QCoreApplication::translate("SystemContacts", "Contacts access failed. Try again in System Settings.")
                                            : QCoreApplication::translate("SystemContacts", "Contacts access is off. Enable it in System Settings.");
                    emit self->changed();
                }
            });
        }];
        return true;
    }
    if (authorization != CNAuthorizationStatusAuthorized) {
        m_status = QCoreApplication::translate("SystemContacts", "Contacts access is off. Enable it in System Settings.");
        emit changed();
        return false;
    }

    m_status = QCoreApplication::translate("SystemContacts", "Searching Contacts…");
    emit changed();
    const QPointer<SystemContacts> self(this);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool {
            auto *store = [CNContactStore new];
            auto *request = [[CNContactFetchRequest alloc] initWithKeysToFetch:@[
                CNContactGivenNameKey, CNContactFamilyNameKey, CNContactEmailAddressesKey
            ]];
            __block QList<QPair<int, QVariantMap>> ranked;
            NSError *error = nil;
            const BOOL success = [store enumerateContactsWithFetchRequest:request error:&error
                usingBlock:^(CNContact *contact, BOOL *stop) {
                    Q_UNUSED(stop)
                    const auto given = QString::fromNSString(contact.givenName);
                    const auto family = QString::fromNSString(contact.familyName);
                    const auto name = (given + QStringLiteral(" ") + family).trimmed();
                    for (CNLabeledValue<NSString *> *entry in contact.emailAddresses) {
                        const auto email = QString::fromNSString(entry.value);
                        const int score = qMax(SearchMatcher::score(cleanQuery, name),
                                               SearchMatcher::score(cleanQuery, email) - 30);
                        if (score <= 0) continue;
                        ranked.append({score, QVariantMap{
                            {QStringLiteral("name"), name.isEmpty() ? email : name},
                            {QStringLiteral("email"), email}
                        }});
                    }
                }];
            const bool failed = !success || error != nil;
            std::stable_sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
                return a.first > b.first;
            });
            QVariantList matches;
            for (qsizetype index = 0; index < ranked.size() && index < 25; ++index)
                matches.append(ranked.at(index).second);
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!self || self->m_requestId != requestId) return;
                self->m_results = failed ? QVariantList() : matches;
                self->m_status = failed ? QCoreApplication::translate("SystemContacts", "Contacts search failed.")
                                        : matches.isEmpty() ? QCoreApplication::translate("SystemContacts", "No matching Contacts.")
                                                            : QString();
                emit self->changed();
            });
        }
    });
    return true;
}
