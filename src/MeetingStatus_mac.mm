#include "MeetingStatus.hpp"

#import <AppKit/AppKit.h>
#include <QCoreApplication>

@interface ViamioraMeetingMenuTarget : NSObject
@property(nonatomic, assign) MeetingStatus *owner;
- (void)showMeeting:(id)sender;
- (void)showOverview:(id)sender;
- (void)copyLink:(id)sender;
- (void)openBrowser:(id)sender;
- (void)leave:(id)sender;
@end

@implementation ViamioraMeetingMenuTarget
- (void)showMeeting:(id)sender { Q_UNUSED(sender); emit _owner->showMeetingRequested(); }
- (void)showOverview:(id)sender { Q_UNUSED(sender); emit _owner->showOverviewRequested(); }
- (void)copyLink:(id)sender { Q_UNUSED(sender); emit _owner->copyLinkRequested(); }
- (void)openBrowser:(id)sender { Q_UNUSED(sender); emit _owner->openBrowserRequested(); }
- (void)leave:(id)sender { Q_UNUSED(sender); emit _owner->leaveRequested(); }
@end

MeetingStatus::MeetingStatus(QObject *parent) : QObject(parent) {}
MeetingStatus::~MeetingStatus() { clear(); }
bool MeetingStatus::active() const { return !m_url.isEmpty(); }

void MeetingStatus::setMeeting(const QString &url, const QString &title) {
    if (url.isEmpty()) return;
    m_url = url;
    m_title = title.trimmed().isEmpty() ? QStringLiteral("Meeting") : title.trimmed();
    if (!m_item) {
        auto *item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        m_item = (__bridge_retained void *)item;
        auto *target = [ViamioraMeetingMenuTarget new];
        target.owner = this;
        m_target = (__bridge_retained void *)target;
        NSImage *icon = [NSImage imageWithSystemSymbolName:@"video.fill" accessibilityDescription:@"Meeting"];
        if (icon) {
            [icon setTemplate:YES];
            item.button.image = icon;
        } else item.button.title = @"●";
    }
    auto *item = (__bridge NSStatusItem *)m_item;
    auto *target = (__bridge ViamioraMeetingMenuTarget *)m_target;
    item.button.toolTip = m_title.toNSString();
    NSMenu *menu = [NSMenu new];
    NSMenuItem *heading = [menu addItemWithTitle:m_title.toNSString() action:nil keyEquivalent:@""];
    heading.enabled = NO;
    [menu addItem:[NSMenuItem separatorItem]];
    for (NSDictionary *entry in @[
             @{ @"title": QCoreApplication::translate("MeetingStatus", "Show meeting").toNSString(), @"action": @"showMeeting:" },
             @{ @"title": QCoreApplication::translate("MeetingStatus", "Show overview").toNSString(), @"action": @"showOverview:" },
             @{ @"title": QCoreApplication::translate("MeetingStatus", "Copy meeting link").toNSString(), @"action": @"copyLink:" },
             @{ @"title": QCoreApplication::translate("MeetingStatus", "Open in browser").toNSString(), @"action": @"openBrowser:" },
             @{ @"title": QCoreApplication::translate("MeetingStatus", "Leave meeting").toNSString(), @"action": @"leave:" }]) {
        if ([entry[@"action"] isEqualToString:@"leave:"]) [menu addItem:[NSMenuItem separatorItem]];
        NSMenuItem *action = [menu addItemWithTitle:entry[@"title"]
                                               action:NSSelectorFromString(entry[@"action"])
                                        keyEquivalent:@""];
        action.target = target;
    }
    item.menu = menu;
    emit changed();
}

void MeetingStatus::retranslate() {
    if (active()) setMeeting(m_url, m_title);
}

void MeetingStatus::clear() {
    if (m_item) {
        auto *item = (__bridge_transfer NSStatusItem *)m_item;
        item.menu = nil;
        [[NSStatusBar systemStatusBar] removeStatusItem:item];
        m_item = nullptr;
    }
    if (m_target) {
        auto *target = (__bridge_transfer ViamioraMeetingMenuTarget *)m_target;
        target.owner = nullptr;
        m_target = nullptr;
    }
    if (!m_url.isEmpty()) {
        m_url.clear();
        m_title.clear();
        emit changed();
    }
}
