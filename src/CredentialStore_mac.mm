#include "CredentialStore.hpp"
#include <QCoreApplication>

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#include <Security/Security.h>

namespace {
NSString *serviceName(const QString &origin) {
    return [NSString stringWithFormat:@"dev.yunabraska.viamiora.portal.%@", origin.toNSString()];
}
NSDictionary *baseQuery(const QString &origin, const QString &username = {}) {
    NSMutableDictionary *query = [@{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: serviceName(origin)
    } mutableCopy];
    if (!username.isEmpty()) query[(__bridge id)kSecAttrAccount] = username.toNSString();
    return query;
}
void withoutDialog(NSMutableDictionary *query) {
    LAContext *context = [LAContext new];
    context.interactionNotAllowed = YES;
    query[(__bridge id)kSecUseAuthenticationContext] = context;
}
}

QStringList CredentialStore::accounts(const QString &origin) {
    NSMutableDictionary *query = [baseQuery(origin) mutableCopy];
    withoutDialog(query);
    query[(__bridge id)kSecReturnAttributes] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitAll;
    CFTypeRef result = nullptr;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) != errSecSuccess) return {};
    id matches = CFBridgingRelease(result);
    NSArray *items = [matches isKindOfClass:NSArray.class] ? matches : @[matches];
    QStringList names;
    for (NSDictionary *item in items) {
        if (![item isKindOfClass:NSDictionary.class]) continue;
        NSString *account = item[(__bridge id)kSecAttrAccount];
        if (![account isKindOfClass:NSString.class]) continue;
        const QString username = QString::fromNSString(account);
        if (!username.isEmpty() && !names.contains(username)) names.append(username);
    }
    return names;
}

std::optional<PortalCredential> CredentialStore::load(const QString &origin, const QString &username, QString *error) {
    if (username.isEmpty()) return std::nullopt;
    NSMutableDictionary *query = [baseQuery(origin, username) mutableCopy];
    withoutDialog(query);
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = nullptr;
    const auto status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status == errSecItemNotFound) return std::nullopt;
    if (status != errSecSuccess) {
        if (error) *error = QCoreApplication::translate("CredentialStore", "The saved login could not be read from Keychain.");
        return std::nullopt;
    }
    NSData *data = CFBridgingRelease(result);
    if (![data isKindOfClass:NSData.class]) {
        if (error) *error = QCoreApplication::translate("CredentialStore", "The saved login is invalid.");
        return std::nullopt;
    }
    return PortalCredential{username, QString::fromUtf8(static_cast<const char *>(data.bytes), static_cast<qsizetype>(data.length))};
}

bool CredentialStore::save(const QString &origin, const PortalCredential &credential, QString *error) {
    const auto saved = load(origin, credential.username, nullptr);
    if (saved && saved->password == credential.password) return true;
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    const auto password = credential.password.toUtf8();
    attributes[(__bridge id)kSecValueData] = [NSData dataWithBytes:password.constData() length:static_cast<NSUInteger>(password.size())];
    NSMutableDictionary *query = [baseQuery(origin, credential.username) mutableCopy];
    withoutDialog(query);
    auto status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)attributes);
    if (status == errSecItemNotFound) {
        NSMutableDictionary *item = [baseQuery(origin, credential.username) mutableCopy];
        [item addEntriesFromDictionary:attributes];
        withoutDialog(item);
        status = SecItemAdd((__bridge CFDictionaryRef)item, nullptr);
    }
    if (status == errSecSuccess) return true;
    if (error) *error = QCoreApplication::translate("CredentialStore", "The login worked, but it could not be saved in Keychain.");
    return false;
}

bool CredentialStore::remove(const QString &origin, const QString &username, QString *error) {
    if (username.trimmed().isEmpty()) {
        if (error) *error = QCoreApplication::translate("CredentialStore", "Choose an account to forget.");
        return false;
    }
    NSMutableDictionary *query = [baseQuery(origin, username) mutableCopy];
    withoutDialog(query);
    const auto status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status == errSecSuccess || status == errSecItemNotFound) return true;
    if (error) *error = QCoreApplication::translate("CredentialStore", "The saved login could not be removed from Keychain.");
    return false;
}
