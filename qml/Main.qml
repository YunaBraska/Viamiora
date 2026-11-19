pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebView
import Viamiora
import "MeetingOverview.js" as MeetingOverview

ApplicationWindow {
    id: appWindow
    enum View { Overview, Schedule, Settings, About, Video, MeetingDetails, CalendarDetails }
    required property ContactStore contactStore
    required property ClipboardService clipboardService
    required property BrandingService brandingService
    required property CalendarService calendarService
    required property AppearanceService appearanceService
    required property MeetingStatus meetingStatus
    required property PortalConfig portalConfig
    required property PortalSession portalSession
    required property SystemContacts systemContacts
    required property var externalDependencies
    required property string appVersion
    readonly property WebView browserView: browserLoader.item as WebView
    objectName: "viamioraWindow"
    visible: true
    title: "Viamiora"
    width: 900
    height: 700
    minimumWidth: 720
    minimumHeight: 500
    color: appWindow.appearanceService.dark ? "#15191f" : "#ffffff"
    palette.window: color
    palette.windowText: ink
    palette.base: appWindow.appearanceService.dark ? "#202630" : "#ffffff"
    palette.text: ink
    palette.button: appWindow.appearanceService.dark ? "#2b3440" : "#f7f8fa"
    palette.buttonText: ink
    palette.highlight: accent
    palette.highlightedText: "#ffffff"

    Image {
        anchors.fill: parent
        source: "assets/login-hall.jpg"
        fillMode: Image.PreserveAspectCrop
        opacity: (appWindow.portalSession.signedIn && appWindow.section !== Main.View.Video) ||
                 (!appWindow.portalSession.signedIn && appWindow.section === Main.View.About)
            ? (appWindow.appearanceService.dark ? 0.045 : 0.10) : 0
    }

    readonly property color ink: appWindow.appearanceService.dark ? "#edf0f5" : "#1f2937"
    readonly property color muted: appWindow.appearanceService.dark ? "#adb7c6" : "#667085"
    readonly property color line: appWindow.appearanceService.dark ? "#39434f" : "#e4e7ec"
    readonly property color surface: appWindow.appearanceService.dark ? "#202630" : "#f7f8fa"
    readonly property color raised: appWindow.appearanceService.dark ? "#252d38" : "#f8fbff"
    readonly property color hoverColor: appWindow.appearanceService.dark ? "#334052" : "#eef3fb"
    readonly property color selectedColor: appWindow.appearanceService.dark ? "#243b5c" : "#eaf0fb"
    readonly property color errorColor: appWindow.appearanceService.dark ? "#ffaaa1" : "#b42318"
    readonly property color successColor: appWindow.appearanceService.dark ? "#69d3a1" : "#157347"
    readonly property color accent: appWindow.brandingService.accent
    readonly property color linkBlue: appWindow.appearanceService.dark ? "#8cbdff" : "#1765c1"
    component PrimaryAction: Button {
        id: action
        implicitWidth: 128
        implicitHeight: 42
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.Button
        Accessible.name: text
        background: Rectangle {
            radius: 8
            color: action.enabled ? appWindow.accent : (appWindow.appearanceService.dark ? "#4b5563" : "#aab4c3")
            border.color: action.activeFocus ? appWindow.linkBlue : "transparent"
            border.width: action.activeFocus ? 2 : 0
        }
        contentItem: Text {
            text: action.text
            color: "#ffffff"
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
    component NavAction: Button {
        id: nav
        property string symbol: ""
        property string label: ""
        property bool selected: false
        property bool emphasized: false
        property bool destructive: false
        text: symbol === "" ? label : symbol + "  " + label
        implicitHeight: 40
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.Button
        Accessible.name: label
        background: Rectangle {
            radius: 8
            color: nav.selected || nav.emphasized ? appWindow.selectedColor : nav.hovered ? appWindow.hoverColor : "transparent"
            border.color: nav.activeFocus ? appWindow.accent : "transparent"
            border.width: nav.activeFocus ? 2 : 0
        }
        contentItem: Item {
            Text {
                anchors.fill: parent
                text: nav.destructive ? nav.label : nav.text
                color: nav.destructive ? appWindow.errorColor : nav.selected || nav.emphasized ? appWindow.linkBlue : appWindow.ink
                font.weight: nav.selected || nav.emphasized ? Font.DemiBold : Font.Normal
                font.pixelSize: 14
                leftPadding: nav.destructive ? 38 : 14
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }
            Canvas {
                visible: nav.destructive
                width: 19
                height: 20
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                property color tint: appWindow.errorColor
                onTintChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = tint
                    ctx.lineWidth = 1.7
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(2, 2); ctx.lineTo(2, 18); ctx.lineTo(10, 18)
                    ctx.moveTo(2, 2); ctx.lineTo(10, 2); ctx.lineTo(10, 6)
                    ctx.moveTo(7, 10); ctx.lineTo(17, 10)
                    ctx.moveTo(13, 6); ctx.lineTo(17, 10); ctx.lineTo(13, 14)
                    ctx.stroke()
                }
            }
        }
    }
    component LinkAction: Button {
        id: link
        property bool underlined: false
        flat: true
        implicitHeight: 30
        focusPolicy: Qt.StrongFocus
        Accessible.name: text
        background: Rectangle { color: "transparent"; border.color: link.activeFocus ? appWindow.linkBlue : "transparent"; border.width: 2; radius: 4 }
        contentItem: Text {
            text: link.text
            color: appWindow.linkBlue
            font.underline: link.hovered || link.underlined
            font.pixelSize: 13
            elide: Text.ElideMiddle
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
        }
    }
    component SelectableText: TextEdit {
        property int elide: Text.ElideNone
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        textFormat: TextEdit.PlainText
        wrapMode: TextEdit.NoWrap
        clip: true
        font.pixelSize: 14
        color: appWindow.ink
        Accessible.role: Accessible.StaticText
    }
    component SelectableLink: TextEdit {
        property string address: ""
        property string meetingTitle: ""
        property bool openExternally: false
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        textFormat: TextEdit.RichText
        wrapMode: TextEdit.WrapAnywhere
        clip: true
        text: "<a href=\"" + appWindow.escapeHtml(address) + "\">" + appWindow.escapeHtml(address) + "</a>"
        color: appWindow.linkBlue
        font.pixelSize: 14
        Accessible.name: address
        onLinkActivated: if (openExternally) Qt.openUrlExternally(address)
                         else appWindow.openMeetingLink(address, meetingTitle)
    }
    component DayChoice: Button {
        id: choice
        required property date day
        property bool selected: false
        property int meetingCount: 0
        implicitHeight: 60
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.Button
        Accessible.name: Qt.formatDate(day, "dddd d MMMM") +
                         (meetingCount > 0 ? ", " + meetingCount + " " +
                          (meetingCount === 1 ? qsTr("meeting") : qsTr("meetings")) : "")
        background: Rectangle {
            radius: 10
            color: choice.selected ? appWindow.accent : choice.hovered ? appWindow.hoverColor : appWindow.surface
            border.color: choice.activeFocus ? appWindow.accent : choice.selected ? appWindow.accent : appWindow.line
            border.width: choice.activeFocus ? 2 : 1
        }
        contentItem: Column {
            spacing: 2
            anchors.centerIn: parent
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Qt.formatDate(choice.day, "ddd")
                color: choice.selected ? "white" : appWindow.muted
                font.pixelSize: 12
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Qt.formatDate(choice.day, "d")
                color: choice.selected ? "white" : appWindow.ink
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: choice.meetingCount > 0
                width: 16
                height: 16
                radius: 8
                color: choice.selected ? "#ffffff" : appWindow.accent
                Text {
                    anchors.centerIn: parent
                    text: choice.meetingCount > 9 ? "9+" : String(choice.meetingCount)
                    color: choice.selected ? appWindow.accent : "#ffffff"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }
    }
    component MeetingLine: RowLayout {
        id: meetingLine
        required property var modelData
        readonly property bool fromCalendar: modelData.fromCalendar === true
        Layout.fillWidth: true
        spacing: 8
        SelectableText {
            text: appWindow.meetingRange(meetingLine.fromCalendar ? meetingLine.modelData.startMs : meetingLine.modelData.start,
                                         meetingLine.fromCalendar ? meetingLine.modelData.endMs : meetingLine.modelData.end,
                                         meetingLine.fromCalendar && meetingLine.modelData.allDay)
            color: appWindow.muted
            Layout.preferredWidth: 190
        }
        SelectableText {
            text: meetingLine.modelData.title
            color: appWindow.ink
            font.weight: Font.Medium
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }
        Label {
            visible: meetingLine.modelData.manageable === true
            text: "👤"
            Accessible.name: qsTr("You can manage this meeting")
            HoverHandler { id: ownerHover }
            ToolTip.visible: ownerHover.hovered
            ToolTip.text: Accessible.name
        }
        ToolButton {
            visible: meetingLine.fromCalendar || meetingLine.modelData.id > 0
            text: "▶"
            Accessible.name: qsTr("Join") + " " + meetingLine.modelData.title
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: {
                if (meetingLine.fromCalendar) appWindow.openMeetingLink(meetingLine.modelData.url, meetingLine.modelData.title)
                else if (meetingLine.modelData.fullUrl !== "") appWindow.openMeetingLink(meetingLine.modelData.fullUrl, meetingLine.modelData.title)
                else {
                    appWindow.pendingJoinMeetingId = meetingLine.modelData.id
                    appWindow.showMeetingLinks(meetingLine.modelData)
                }
            }
        }
        LinkAction {
            text: qsTr("Details")
            enabled: meetingLine.fromCalendar || meetingLine.modelData.id > 0
            onClicked: {
                if (meetingLine.fromCalendar) appWindow.showCalendarDetails(meetingLine.modelData)
                else appWindow.showMeetingLinks(meetingLine.modelData)
            }
        }
    }
    component ExpandableDescription: ColumnLayout {
        id: descriptionView
        property string description: ""
        property bool expanded: false
        visible: description.trim() !== ""
        Layout.fillWidth: true
        spacing: 4
        SelectableText { text: qsTr("Description"); color: appWindow.ink; font.weight: Font.Medium }
        SelectableText {
            id: descriptionBody
            text: descriptionView.description
            color: appWindow.muted
            wrapMode: TextEdit.Wrap
            Layout.fillWidth: true
            Layout.preferredHeight: descriptionView.expanded ? contentHeight : Math.min(contentHeight, font.pixelSize * 3.8)
        }
        LinkAction {
            visible: descriptionBody.contentHeight > descriptionBody.font.pixelSize * 3.8 + 2
            text: descriptionView.expanded ? qsTr("Show less") : qsTr("Show more")
            onClicked: descriptionView.expanded = !descriptionView.expanded
        }
    }
    component AddressEntry: ColumnLayout {
        id: entry
        property string role: ""
        property var selectedEmails: []
        readonly property string addresses: selectedEmails.join(", ")
        readonly property bool hasAddresses: selectedEmails.length > 0 || addressText.text.trim() !== ""
        readonly property var suggestions: {
            const query = addressText.text.trim()
            if (appWindow.activeAddressRole !== entry.role || query.length < 2) return []
            const choices = []
            for (const email of appWindow.contactStore.suggestions(query).slice(0, 20))
                choices.push({ name: "", email: email, source: "recent" })
            if (appWindow.lastSearchedQuery === query)
                for (const contact of appWindow.systemContacts.results)
                    choices.push({ name: contact.name, email: contact.email, source: "system" })
            const seen = new Set(entry.selectedEmails.map(email => email.toLowerCase()))
            const ranked = []
            for (const choice of choices) {
                const key = choice.email.toLowerCase()
                if (seen.has(key)) continue
                seen.add(key)
                const score = Math.max(appWindow.contactStore.matchScore(query, choice.name),
                                       appWindow.contactStore.matchScore(query, choice.email) - 30)
                if (score > 0) ranked.push({ name: choice.name, email: choice.email,
                                             source: choice.source, score: score })
            }
            ranked.sort((a, b) => b.score - a.score || a.email.localeCompare(b.email))
            return ranked.slice(0, 5)
        }
        spacing: 6
        function add(email, remember) {
            if (entry.selectedEmails.some(item => item.toLowerCase() === email.toLowerCase())) {
                addressText.clear()
                appWindow.activeAddressRole = ""
                return
            }
            entry.selectedEmails = entry.selectedEmails.concat([email])
            if (remember !== false) appWindow.contactStore.rememberAddress(email)
            addressText.clear()
            appWindow.contactQuery = ""
            appWindow.activeAddressRole = ""
            addressText.forceActiveFocus()
        }
        function remove(index) {
            entry.selectedEmails = entry.selectedEmails.filter((_, position) => position !== index)
        }
        function clear() {
            entry.selectedEmails = []
            addressText.clear()
            appWindow.activeAddressRole = ""
        }
        function commitPending() {
            const parts = addressText.text.split(/[;,]/).map(item => item.trim()).filter(item => item !== "")
            if (parts.length === 0) return true
            if (parts.length === 1 && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(parts[0]) &&
                entry.suggestions.length > 0) {
                entry.add(entry.suggestions[0].email, entry.suggestions[0].source !== "system")
                return true
            }
            if (parts.some(email => !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email))) return false
            for (const email of parts) entry.add(email)
            return true
        }
        SelectableText { text: entry.role; color: appWindow.ink; font.weight: Font.Medium }
        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? childrenRect.height : 0
            visible: entry.selectedEmails.length > 0
            spacing: 6
            Repeater {
                model: entry.selectedEmails
                delegate: Rectangle {
                    id: chip
                    required property int index
                    required property string modelData
                    height: 27
                    width: Math.min(entry.width, chipContent.implicitWidth + 14)
                    radius: 7
                    color: appWindow.selectedColor
                    Row {
                        id: chipContent
                        anchors.centerIn: parent
                        spacing: 4
                        SelectableText { text: chip.modelData; color: appWindow.ink; width: Math.min(implicitWidth, entry.width - 45); elide: Text.ElideMiddle; verticalAlignment: Text.AlignVCenter; height: 27 }
                        Button {
                            text: "×"
                            flat: true
                            implicitWidth: 24
                            implicitHeight: 24
                            Accessible.name: qsTr("Remove ") + chip.modelData + " from " + entry.role
                            onClicked: entry.remove(chip.index)
                        }
                    }
                }
            }
        }
        TextField {
            id: addressText
            Layout.fillWidth: true
            placeholderText: qsTr("Search contacts or enter an email")
            Accessible.name: entry.role + " email addresses"
            onTextEdited: {
                appWindow.activeAddressRole = entry.role
                appWindow.contactQuery = text.split(/[;,]/).pop().trim()
                appWindow.lastSearchedQuery = ""
                contactSearch.restart()
            }
            onAccepted: if (!entry.commitPending()) {
                if (appWindow.detailsVisible) appWindow.shareInviteError = qsTr("Check the email addresses.")
                else appWindow.scheduleError = qsTr("Check the email addresses.")
            }
            onActiveFocusChanged: if (!activeFocus) hideSuggestions.restart()
            Keys.onEscapePressed: appWindow.activeAddressRole = ""
        }
        Timer { id: hideSuggestions; interval: 250; onTriggered: if (!addressText.activeFocus) appWindow.activeAddressRole = "" }
        Repeater {
            model: entry.suggestions
            delegate: Button {
                id: suggestion
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 36
                background: Rectangle { radius: 6; color: suggestion.hovered ? appWindow.hoverColor : appWindow.color }
                contentItem: RowLayout {
                    spacing: 8
                    Label { text: suggestion.modelData.name || suggestion.modelData.email; color: appWindow.ink; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { visible: suggestion.modelData.name !== ""; text: suggestion.modelData.email; color: appWindow.muted; elide: Text.ElideMiddle; Layout.maximumWidth: 240 }
                }
                Accessible.name: (modelData.name ? modelData.name + ", " : "") + modelData.email
                onClicked: entry.add(modelData.email, modelData.source !== "system")
            }
        }
    }
    readonly property var timeSlots: {
        let slots = []
        for (let hour = 0; hour < 24; ++hour) {
            let prefix = hour < 10 ? "0" + hour : "" + hour
            slots.push(prefix + ":00")
            slots.push(prefix + ":30")
        }
        return slots
    }
    readonly property var overviewMeetings: MeetingOverview.combineMeetings(
        MeetingOverview.mergePortalMeetings(appWindow.portalSession.meetings, appWindow.portalSession.historicalMeetings),
        appWindow.calendarService.events.filter(item =>
            appWindow.portalConfig.recognizedMeetingUrl(item.url) !== "" &&
            !appWindow.calendarService.disabledCalendarIds.includes(item.calendarId)), appWindow.portalConfig.portalUrl)
    readonly property var overviewGroups: MeetingOverview.groupMeetings(
        overviewMeetings, currentTime.getTime(), overviewDayFilter,
        Qt.formatDate(overviewDay(0), "yyyy-MM-dd"),
        Qt.formatDate(overviewDay(6), "yyyy-MM-dd"), overviewWeekOffset !== 0)
    property date currentTime: new Date()
    property int section: Main.View.Overview
    property bool wasSignedIn: false
    property bool calendarRequested: false
    property date selectedDay: new Date()
    property string prefilledOrigin: ""
    property string prefilledUsername: ""
    property string accountListKey: ""
    property string overviewQuery: ""
    property string overviewDayFilter: ""
    property int overviewWeekOffset: 0
    property string shareTitle: ""
    property string shareTime: ""
    property string sharePortalUrl: ""
    property int shareMeetingId: 0
    property int pendingJoinMeetingId: 0
    property string shareCopyStatus: ""
    property var calendarDetail: ({})
    readonly property bool detailsVisible: section === Main.View.MeetingDetails
    property bool shareInviteOpen: false
    property string shareInviteError: ""
    property bool scheduleCompleted: false
    property bool passwordEdited: false
    property bool loginTouched: false
    property string activeAddressRole: ""
    property string contactQuery: ""
    property string lastSearchedQuery: ""
    property string schedulePin: appWindow.portalSession.newPin()
    property string scheduleModeratorPin: appWindow.portalSession.newPin()
    property string browserUrl: ""
    property string pendingBrowserUrl: ""
    property int pendingBrowserCookies: 0
    property string browserError: ""
    property bool browserLoading: false
    property string browserTitle: "Meeting"
    property string invitationError: ""
    property string scheduleError: ""
    readonly property var filteredOngoingMeetings: rankOverview(overviewGroups.ongoing, overviewQuery)
    readonly property var filteredUpcomingMeetings: rankOverview(overviewGroups.upcoming, overviewQuery)
    readonly property var filteredEarlierMeetings: rankOverview(overviewGroups.earlier, overviewQuery)
    readonly property var filteredRooms: rankOverview(appWindow.portalConfig.roomLinks, overviewQuery)

    function rankOverview(items, query) {
        const clean = query.trim()
        if (clean === "") return items
        const ranked = items.map(item => ({ item: item,
            score: Math.max(appWindow.contactStore.matchScore(clean, item.title),
                            appWindow.contactStore.matchScore(clean, item.url || item.start || "") - 60) }))
            .filter(entry => entry.score > 0)
        ranked.sort((a, b) => b.score - a.score)
        return ranked.map(entry => entry.item)
    }

    function escapeHtml(value) {
        return String(value).replace(/&/g, "&amp;").replace(/</g, "&lt;")
            .replace(/>/g, "&gt;").replace(/\"/g, "&quot;").replace(/'/g, "&#39;")
    }

    function overviewDay(index) {
        let day = new Date(currentTime.getTime())
        day.setHours(12, 0, 0, 0)
        day.setDate(day.getDate() + overviewWeekOffset * 7 + index)
        return day
    }

    function scheduleWeekDay(index) {
        let day = new Date(selectedDay.getTime())
        day.setHours(12, 0, 0, 0)
        day.setDate(day.getDate() - (day.getDay() + 6) % 7 + index)
        return day
    }

    function overviewMonth() {
        const first = overviewDay(0)
        const last = overviewDay(6)
        const month = Qt.formatDate(first, "MMMM yyyy")
        return first.getMonth() === last.getMonth() ? month :
            Qt.formatDate(first, "MMM") + "–" + Qt.formatDate(last, "MMM yyyy")
    }

    function meetingsOnDay(day) {
        return MeetingOverview.meetingsOnDay(overviewMeetings, Qt.formatDate(day, "yyyy-MM-dd"))
    }

    function showMeetingLinks(item) {
        shareTitle = item.title
        shareTime = item.start && item.end ? meetingTime(item) + "–" +
            Qt.formatDateTime(new Date(item.end), "HH:mm") : ""
        sharePortalUrl = item.url ? appWindow.portalConfig.portalUrl + item.url : ""
        shareMeetingId = item.id
        shareCopyStatus = ""
        shareInviteOpen = false
        shareInviteError = ""
        section = Main.View.MeetingDetails
        appWindow.portalSession.fetchMeetingLinks(item.id)
    }

    function showCalendarDetails(item) {
        calendarDetail = item
        section = Main.View.CalendarDetails
    }

    function openSchedule() {
        if (scheduleCompleted) {
            scheduleCompleted = false
            scheduleTitle.clear()
            inviteeAddresses.clear()
            moderatorAddresses.clear()
            schedulePin = appWindow.portalSession.newPin()
            scheduleModeratorPin = appWindow.portalSession.newPin()
            proposeStart()
        }
        if (overviewDayFilter !== "") {
            const day = new Date(overviewDayFilter + "T12:00:00")
            if (!isNaN(day.getTime())) selectedDay = day
        }
        section = Main.View.Schedule
        if (appWindow.portalSession.bookingTypes.length > 0)
            appWindow.portalSession.inspectBookingType(appWindow.portalSession.bookingTypes[Math.max(0, bookingType.currentIndex)].id)
        refreshScheduleCalendar()
    }

    function openInside(address, title) {
        const safeUrl = appWindow.portalConfig.safeMeetingUrl(String(address))
        if (safeUrl === "") return false
        if (String(pendingBrowserUrl) === safeUrl) {
            section = Main.View.Video
            return true
        }
        const alreadyOpen = String(pendingBrowserUrl) !== ""
        browserLoad.stop()
        browserCookieTimeout.stop()
        browserReadyCheck.stop()
        browserUrl = ""
        pendingBrowserUrl = safeUrl
        browserError = ""
        browserLoading = true
        browserTitle = title
        appWindow.meetingStatus.setMeeting(safeUrl, title)
        section = Main.View.Video
        if (alreadyOpen) configureVideoView()
        return true
    }

    function configureVideoView() {
        if (!appWindow.browserView || String(pendingBrowserUrl) === "") return
        browserLoad.stop()
        browserCookieTimeout.stop()
        browserTimeout.stop()
        browserReadyCheck.stop()
        appWindow.browserView.stop()
        browserUrl = ""
        browserError = ""
        browserLoading = true
        const cookies = appWindow.portalSession.browserCookies(pendingBrowserUrl)
        pendingBrowserCookies = cookies.length
        for (const cookie of cookies)
            appWindow.browserView.setCookie(cookie.domain, cookie.name, cookie.value)
        if (cookies.length === 0) browserLoad.restart()
        else browserCookieTimeout.restart()
    }

    function closeVideo() {
        browserLoad.stop()
        browserCookieTimeout.stop()
        browserTimeout.stop()
        browserReadyCheck.stop()
        if (appWindow.browserView) {
            appWindow.browserView.stop()
            appWindow.browserView.deleteAllCookies()
        }
        pendingBrowserUrl = ""
        pendingBrowserCookies = 0
        browserError = ""
        browserLoading = false
        browserUrl = ""
        appWindow.meetingStatus.clear()
        section = Main.View.Overview
    }

    function openMeetingLink(address, title) {
        const safeUrl = appWindow.portalConfig.safeMeetingUrl(String(address))
        if (safeUrl === "") return false
        if (safeUrl.includes("/webapp/")) return openInside(safeUrl, title)
        Qt.openUrlExternally(safeUrl)
        return true
    }

    function ensureCalendarWeek(first, last) {
        if (!appWindow.calendarService.authorized) return
        if (!appWindow.calendarService.coversRange(first, last) ||
            Date.now() - appWindow.calendarService.updatedAtMs > 600000)
            appWindow.calendarService.refresh(first)
    }

    function refreshOverviewCalendar() {
        if (!appWindow.portalSession.signedIn || section !== Main.View.Overview) return
        appWindow.portalSession.loadMeetingWeek(Qt.formatDate(overviewDay(0), "yyyy-MM-dd"),
                                      Qt.formatDate(overviewDay(6), "yyyy-MM-dd"))
        ensureCalendarWeek(Qt.formatDate(overviewDay(0), "yyyy-MM-dd"),
                           Qt.formatDate(overviewDay(6), "yyyy-MM-dd"))
    }

    function refreshScheduleCalendar() {
        if (section !== Main.View.Schedule) return
        ensureCalendarWeek(Qt.formatDate(scheduleWeekDay(0), "yyyy-MM-dd"),
                           Qt.formatDate(scheduleWeekDay(6), "yyyy-MM-dd"))
    }

    function resolveOverviewMatches() {
        if (section !== Main.View.Overview || !appWindow.calendarService.coversRange(
                Qt.formatDate(overviewDay(0), "yyyy-MM-dd"),
                Qt.formatDate(overviewDay(6), "yyyy-MM-dd"))) return
        appWindow.portalSession.resolveCalendarMatches(appWindow.calendarService.events.filter(item =>
            !appWindow.calendarService.disabledCalendarIds.includes(item.calendarId)))
    }

    function authenticate() {
        if (!appWindow.portalConfig.setPortalUrl(portalAddress.text)) return
        if (prefilledOrigin !== "" && prefilledOrigin !== appWindow.portalConfig.portalUrl) {
            portalPassword.clear()
            prefilledOrigin = ""
            prefilledUsername = ""
            prefillPassword()
        }
        appWindow.portalSession.signIn(portalUsername.text, portalPassword.text)
    }

    function prefillPassword() {
        const username = portalUsername.text.trim()
        if (prefilledOrigin === appWindow.portalConfig.portalUrl && prefilledUsername === username &&
            portalPassword.text !== "") return
        passwordEdited = false
        portalPassword.clear()
        prefilledOrigin = ""
        prefilledUsername = ""
        appWindow.portalSession.requestSavedPassword(username)
    }

    function prefillLogin() {
        portalAddress.text = appWindow.portalConfig.portalUrl
        const preferred = appWindow.portalConfig.lastUsername
        portalUsername.text = preferred !== "" ? preferred
            : appWindow.portalSession.savedAccounts.length > 0 ? appWindow.portalSession.savedAccounts[0] : ""
        prefillPassword()
    }

    function choosePortal() {
        const previous = appWindow.portalConfig.portalUrl
        if (!appWindow.portalConfig.setPortalUrl(portalAddress.text)) return
        if (previous !== appWindow.portalConfig.portalUrl) {
            portalPassword.clear()
            prefilledOrigin = ""
            prefilledUsername = ""
            portalUsername.text = appWindow.portalConfig.lastUsername !== "" ? appWindow.portalConfig.lastUsername
                : appWindow.portalSession.savedAccounts.length > 0 ? appWindow.portalSession.savedAccounts[0] : ""
            prefillPassword()
            appWindow.brandingService.load(appWindow.portalConfig.portalUrl)
        }
    }

    function selectLogin(login) {
        loginTouched = true
        passwordEdited = true
        portalAddress.text = login.origin
        portalUsername.text = login.username
        appWindow.portalConfig.setPortalUrl(login.origin)
        passwordEdited = false
        prefillPassword()
        appWindow.brandingService.load(login.origin)
    }

    Connections {
        target: appWindow.portalSession
        function onPrefillChanged() {
            if (appWindow.passwordEdited || appWindow.portalSession.signedIn ||
                appWindow.portalSession.prefillOrigin !== appWindow.portalConfig.portalUrl ||
                appWindow.portalSession.prefillUsername !== portalUsername.text.trim()) return
            portalPassword.text = appWindow.portalSession.prefillPassword
            appWindow.prefilledOrigin = appWindow.portalSession.prefillOrigin
            appWindow.prefilledUsername = appWindow.portalSession.prefillUsername
        }
        function onChanged() {
            const accountKey = appWindow.portalConfig.portalUrl + "|" + appWindow.portalSession.savedAccounts.join("|")
            if (appWindow.accountListKey !== accountKey) {
                appWindow.accountListKey = accountKey
                if (!appWindow.portalSession.signedIn && !appWindow.passwordEdited &&
                    appWindow.portalSession.savedAccounts.includes(portalUsername.text.trim()))
                    appWindow.prefillPassword()
            }
            if (!appWindow.portalSession.signedIn && !appWindow.loginTouched && portalUsername.text.trim() === "" &&
                appWindow.portalSession.savedLogins.length > 0)
                appWindow.selectLogin(appWindow.portalSession.savedLogins[0])
            if (appWindow.portalSession.signedIn && !appWindow.wasSignedIn) {
                appWindow.section = Main.View.Overview
                appWindow.currentTime = new Date()
                portalPassword.clear()
                Qt.callLater(appWindow.refreshOverviewCalendar)
            }
            if (!appWindow.portalSession.signedIn && appWindow.wasSignedIn) {
                appWindow.closeVideo()
                appWindow.prefillLogin()
            }
            if (appWindow.portalSession.signedIn && appWindow.section === Main.View.Schedule && appWindow.portalSession.bookingTypes.length > 0)
                appWindow.portalSession.inspectBookingType(appWindow.portalSession.bookingTypes[Math.max(0, bookingType.currentIndex)].id)
            appWindow.wasSignedIn = appWindow.portalSession.signedIn
            if (appWindow.portalSession.signedIn) appWindow.resolveOverviewMatches()
        }
        function onHistoricalMeetingsChanged() { appWindow.resolveOverviewMatches() }
        function onMeetingCreated(id, title) {
            appWindow.scheduleCompleted = true
            appWindow.currentTime = new Date()
            const created = appWindow.portalSession.meetings.find(item => item.id === id)
            appWindow.showMeetingLinks(created || { id: id, title: title })
        }
    }

    Timer {
        interval: 60000
        repeat: true
        running: appWindow.portalSession.signedIn && appWindow.visible
        onTriggered: {
            appWindow.currentTime = new Date()
            if (appWindow.section === Main.View.Overview) {
                appWindow.portalSession.refreshMeetingsIfDue(!appWindow.active)
                if (appWindow.active && appWindow.calendarService.authorized) {
                    appWindow.calendarService.refreshSelection()
                    appWindow.refreshOverviewCalendar()
                    appWindow.resolveOverviewMatches()
                }
            } else if (appWindow.section === Main.View.Schedule && appWindow.active) appWindow.refreshScheduleCalendar()
        }
    }
    Timer {
        id: calendarDayDelay
        interval: 350
        repeat: false
        onTriggered: appWindow.refreshScheduleCalendar()
    }
    Timer {
        id: overviewCalendarDelay
        interval: 350
        repeat: false
        onTriggered: appWindow.refreshOverviewCalendar()
    }
    Timer {
        id: contactSearch
        interval: 300
        repeat: false
        onTriggered: if ((appWindow.section === Main.View.Schedule || appWindow.detailsVisible) && appWindow.contactQuery.length >= 2) {
            appWindow.lastSearchedQuery = appWindow.contactQuery
            appWindow.systemContacts.search(appWindow.contactQuery)
        }
    }
    Timer {
        id: browserLoad
        interval: 50
        repeat: false
        onTriggered: if (appWindow.browserView && String(appWindow.pendingBrowserUrl) !== "") {
            appWindow.browserUrl = appWindow.pendingBrowserUrl
            browserTimeout.restart()
        }
    }
    Timer {
        id: browserCookieTimeout
        interval: 3000
        repeat: false
        onTriggered: if (String(appWindow.pendingBrowserUrl) !== "") browserLoad.restart()
    }
    Timer {
        id: browserTimeout
        interval: 15000
        repeat: false
        onTriggered: if (String(appWindow.pendingBrowserUrl) !== "" && appWindow.browserLoading)
            appWindow.browserError = "The meeting did not finish loading. Try again or open it in your browser."
    }
    Timer {
        id: browserReadyCheck
        interval: 8000
        repeat: false
        onTriggered: {
            if (String(appWindow.pendingBrowserUrl) === "" || !appWindow.browserView || appWindow.browserLoading ||
                appWindow.browserError !== "") return
            const expected = String(appWindow.pendingBrowserUrl)
            appWindow.browserView.runJavaScript("(function(){const body=document.body;if(!body)return false;return (body.innerText||'').trim().length>0||!!body.querySelector('video,canvas,img,iframe')})()",
                function(hasContent) {
                    if (String(appWindow.pendingBrowserUrl) === expected &&
                        !hasContent && appWindow.browserError === "")
                        appWindow.browserError = "The meeting opened without content. Try again or use the browser."
                })
        }
    }

    function shiftDay(amount) {
        let day = new Date(selectedDay.getTime())
        day.setDate(day.getDate() + amount)
        selectedDay = day
    }

    function refreshOverviewIfStale() {
        if (appWindow.portalSession.signedIn && section === Main.View.Overview)
            appWindow.portalSession.refreshMeetingsIfDue(!active)
    }

    function proposeStart() {
        let start = new Date(Date.now() + 5 * 60000)
        start.setSeconds(0, 0)
        start.setMinutes(Math.ceil(start.getMinutes() / 30) * 30)
        if (start.getHours() > 22) {
            start.setDate(start.getDate() + 1)
            start.setHours(9, 0, 0, 0)
        }
        selectedDay = start
        const index = timeSlots.indexOf(Qt.formatDateTime(start, "HH:mm"))
        startTime.currentIndex = Math.max(0, index)
        endTime.currentIndex = Math.min(47, startTime.currentIndex + 2)
    }

    function slotSummary() {
        if (!appWindow.calendarService.connected)
            return appWindow.calendarService.authorized ? "Checking calendar…" : calendarRequested ? "Calendar access is unavailable." : ""
        if (!appWindow.calendarService.coversRange(Qt.formatDate(selectedDay, "yyyy-MM-dd"),
                                         Qt.formatDate(selectedDay, "yyyy-MM-dd")) ||
            appWindow.calendarService.updatedAtMs === 0)
            return "Checking calendar…"
        let startParts = startTime.currentText.split(":")
        let endParts = endTime.currentText.split(":")
        let start = new Date(selectedDay.getFullYear(), selectedDay.getMonth(), selectedDay.getDate(), Number(startParts[0]), Number(startParts[1]))
        let end = new Date(selectedDay.getFullYear(), selectedDay.getMonth(), selectedDay.getDate(), Number(endParts[0]), Number(endParts[1]))
        if (end <= start) return "Choose an end time after the start time."
        for (let event of appWindow.calendarService.events) {
            if (event.busy && start.getTime() < event.endMs && end.getTime() > event.startMs)
                return "Busy on your calendar"
        }
        return "✓ Free on your calendar"
    }

    function prepareBooking() {
        const title = scheduleTitle.text.trim()
        if (title === "") {
            scheduleError = "Add a meeting title."
            return
        }
        const starts = startTime.currentText.split(":")
        const ends = endTime.currentText.split(":")
        const start = new Date(selectedDay.getFullYear(), selectedDay.getMonth(), selectedDay.getDate(),
                               Number(starts[0]), Number(starts[1]))
        const end = new Date(selectedDay.getFullYear(), selectedDay.getMonth(), selectedDay.getDate(),
                             Number(ends[0]), Number(ends[1]))
        if (start <= new Date() || end <= start) {
            scheduleError = "Choose a future time with an end after the start."
            return
        }
        if (!inviteeAddresses.commitPending() || !moderatorAddresses.commitPending()) {
            scheduleError = qsTr("Check the email addresses.")
            return
        }
        const choices = appWindow.portalSession.bookingTypes
        if (choices.length === 0) {
            scheduleError = "Meeting types are unavailable here. Open the portal to schedule."
            return
        }
        scheduleError = ""
        const selected = choices[Math.max(0, bookingType.currentIndex)]
        appWindow.portalSession.createMeeting(title, start.getTime(), end.getTime(), selected.id, schedulePin,
                                    inviteeAddresses.selectedEmails, moderatorAddresses.selectedEmails,
                                    appWindow.portalSession.moderatorPinSupported ? scheduleModeratorPin : "")
    }

    function meetingTime(item) {
        return Qt.formatDateTime(new Date(item.start), "ddd, d MMM · HH:mm")
    }

    function meetingRange(start, end, allDay) {
        const begin = new Date(start)
        const finish = new Date(end)
        if (isNaN(begin.getTime()) || isNaN(finish.getTime())) return ""
        if (allDay) return Qt.formatDate(begin, "ddd, d MMM")
        const sameDay = Qt.formatDate(begin, "yyyy-MM-dd") === Qt.formatDate(finish, "yyyy-MM-dd")
        return Qt.formatDateTime(begin, "ddd, d MMM · HH:mm") + "–" +
            (sameDay ? Qt.formatTime(finish, "HH:mm") : Qt.formatDateTime(finish, "ddd HH:mm"))
    }

    onSelectedDayChanged: if (section === Main.View.Schedule && (calendarRequested || appWindow.calendarService.authorized))
        calendarDayDelay.restart()
    onOverviewWeekOffsetChanged: {
        overviewDayFilter = ""
        if (section === Main.View.Overview) overviewCalendarDelay.restart()
    }
    onSectionChanged: {
        if (section !== Main.View.CalendarDetails) calendarDetail = ({})
        if (section !== Main.View.MeetingDetails && shareMeetingId > 0) {
            pendingJoinMeetingId = 0
            shareInvitees.clear()
            shareModerators.clear()
            shareInviteOpen = false
            shareInviteError = ""
            appWindow.portalSession.cancelMeetingLinks()
            shareMeetingId = 0
        }
        if (section === Main.View.Overview) {
            refreshOverviewIfStale()
            refreshOverviewCalendar()
            resolveOverviewMatches()
        }
        if (section === Main.View.Schedule) refreshScheduleCalendar()
    }
    onActiveChanged: if (active) {
        appWindow.currentTime = new Date()
        appWindow.calendarService.refreshAuthorization()
        if (appWindow.portalSession.signedIn && appWindow.calendarService.authorized) appWindow.calendarService.refreshSelection()
        appWindow.systemContacts.refreshAuthorization()
        refreshOverviewIfStale()
        refreshOverviewCalendar()
        appWindow.refreshScheduleCalendar()
    }
    Connections {
        target: appWindow.calendarService
        function onChanged() {
            appWindow.resolveOverviewMatches()
            if (appWindow.section === Main.View.Schedule && appWindow.calendarService.authorized)
                appWindow.refreshScheduleCalendar()
        }
        function onSlotSuggested(start) {
            appWindow.selectedDay = start
            const index = appWindow.timeSlots.indexOf(Qt.formatDateTime(start, "HH:mm"))
            startTime.currentIndex = Math.max(0, index)
            endTime.currentIndex = Math.min(47, startTime.currentIndex + 2)
        }
    }
    Connections {
        target: appWindow.portalSession
        function onMeetingsChanged() { appWindow.resolveOverviewMatches() }
        function onLinksChanged() {
            if (appWindow.portalSession.linksBusy || appWindow.pendingJoinMeetingId <= 0 ||
                appWindow.pendingJoinMeetingId !== appWindow.shareMeetingId) return
            appWindow.pendingJoinMeetingId = 0
            const address = appWindow.portalSession.meetingLinks.participantUrl || ""
            if (address.includes("/webapp/")) appWindow.openMeetingLink(address, appWindow.shareTitle)
        }
    }
    Component.onCompleted: {
        prefillLogin()
        proposeStart()
        if (appWindow.portalConfig.portalUrl !== "") appWindow.brandingService.load(appWindow.portalConfig.portalUrl)
    }
    Connections {
        target: appWindow.meetingStatus
        function onShowMeetingRequested() {
            appWindow.section = Main.View.Video
            appWindow.show()
            appWindow.raise()
            appWindow.requestActivate()
        }
        function onShowOverviewRequested() {
            appWindow.section = Main.View.Overview
            appWindow.show()
            appWindow.raise()
            appWindow.requestActivate()
        }
        function onCopyLinkRequested() { appWindow.clipboardService.copy(String(appWindow.pendingBrowserUrl)) }
        function onOpenBrowserRequested() { Qt.openUrlExternally(appWindow.pendingBrowserUrl) }
        function onLeaveRequested() {
            appWindow.closeVideo()
            appWindow.show()
            appWindow.raise()
            appWindow.requestActivate()
        }
    }
    Popup {
        id: loginChooser
        parent: appWindow.contentItem
        modal: true
        focus: true
        x: (appWindow.width - width) / 2
        y: Math.max(24, (appWindow.height - height) / 2)
        width: Math.min(540, appWindow.width - 32)
        height: Math.min(360, 56 + appWindow.portalSession.savedLogins.length * 52)
        padding: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        ScrollView {
            anchors.fill: parent
            ColumnLayout {
                width: loginChooser.availableWidth - 18
                SelectableText { text: qsTr("Saved logins"); color: appWindow.ink; font.weight: Font.DemiBold }
                Repeater {
                    model: appWindow.portalSession.savedLogins
                    delegate: RowLayout {
                        id: loginRow
                        required property var modelData
                        Layout.fillWidth: true
                        Button {
                            text: loginRow.modelData.username + " · " + loginRow.modelData.origin
                            Layout.fillWidth: true
                            Accessible.name: qsTr("Use ") + loginRow.modelData.username + " at " + loginRow.modelData.origin
                            onClicked: { appWindow.selectLogin(loginRow.modelData); loginChooser.close() }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: removeInviteDialog
        objectName: "removeInvitationDialog"
        property int inviteId: 0
        property string email: ""
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(appWindow.width - 48, 420)
        title: qsTr("Remove invitation")
        contentItem: Label {
            text: qsTr("Remove the invitation for %1?").arg(removeInviteDialog.email)
            wrapMode: Text.WordWrap
            color: appWindow.ink
            padding: 14
        }
        footer: RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Cancel"); onClicked: removeInviteDialog.close() }
            Button {
                objectName: "confirmRemoveInvitation"
                text: qsTr("Remove")
                palette.buttonText: appWindow.errorColor
                enabled: !appWindow.portalSession.busy && !appWindow.portalSession.creatingMeeting &&
                         !appWindow.portalSession.inviting && !appWindow.portalSession.linksBusy
                onClicked: {
                    const inviteId = removeInviteDialog.inviteId
                    removeInviteDialog.close()
                    appWindow.portalSession.removeInvitation(appWindow.shareMeetingId, inviteId)
                }
            }
        }
    }


    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            visible: appWindow.portalSession.signedIn && appWindow.section !== Main.View.Video
            Layout.preferredWidth: visible ? 196 : 0
            Layout.fillHeight: true
            color: appWindow.surface
            border.color: appWindow.line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Item { Layout.preferredHeight: 22 }
                PrimaryAction { text: qsTr("+  New meeting"); Layout.fillWidth: true; onClicked: appWindow.openSchedule() }
                Item { Layout.preferredHeight: 8 }
                NavAction { symbol: "◫"; label: qsTr("Overview"); Layout.fillWidth: true; selected: appWindow.section === Main.View.Overview; onClicked: appWindow.section = Main.View.Overview }
                NavAction { symbol: "▶"; label: qsTr("Return to meeting"); Layout.fillWidth: true; visible: appWindow.pendingBrowserUrl !== ""; emphasized: true; onClicked: appWindow.section = Main.View.Video }
                NavAction { symbol: "⚙"; label: qsTr("Settings"); Layout.fillWidth: true; selected: appWindow.section === Main.View.Settings; onClicked: appWindow.section = Main.View.Settings }
                NavAction { symbol: "ⓘ"; label: qsTr("About"); Layout.fillWidth: true; selected: appWindow.section === Main.View.About; onClicked: appWindow.section = Main.View.About }
                Item { Layout.fillHeight: true }
                NavAction { label: qsTr("Sign out"); Layout.fillWidth: true; destructive: true; enabled: !appWindow.portalSession.busy && !appWindow.portalSession.creatingMeeting && !appWindow.portalSession.inviting; onClicked: appWindow.portalSession.signOut() }
                LinkAction { text: qsTr("↗  Open portal"); Layout.fillWidth: true; onClicked: Qt.openUrlExternally(appWindow.portalConfig.portalUrl) }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Child order follows Main.View, with sign-in at index zero.
            currentIndex: !appWindow.portalSession.signedIn ? (appWindow.section === Main.View.About ? 4 : 0) : appWindow.section + 1

            // Sign-in
            Item {
                Image { anchors.fill: parent; source: "assets/login-hall.jpg"; fillMode: Image.PreserveAspectCrop; opacity: 0.72 }
                Rectangle { anchors.fill: parent; color: appWindow.color; opacity: appWindow.appearanceService.dark ? 0.84 : 0.68 }
            Flickable {
                id: loginScroll
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: loginColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ColumnLayout {
                    id: loginColumn
                    width: Math.min(loginScroll.width - 48, 440)
                    x: Math.max(24, (loginScroll.width - width) / 2)
                    spacing: 14
                    Item { Layout.preferredHeight: 54 }
                    Image { source: appWindow.brandingService.logoUrl; visible: status === Image.Ready; sourceSize.width: 200; sourceSize.height: 68; Layout.preferredWidth: visible ? 200 : 0; Layout.preferredHeight: visible ? 68 : 0; fillMode: Image.PreserveAspectFit }
                    SelectableText { text: qsTr("Sign in"); color: appWindow.ink; font.pixelSize: 29; font.weight: Font.DemiBold; Layout.topMargin: 8 }
                    SelectableText { text: qsTr("Username"); color: appWindow.ink; font.weight: Font.Medium; Layout.topMargin: 4 }
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        TextField {
                            id: portalUsername
                            objectName: "portalUsername"
                            anchors.left: parent.left
                            anchors.right: savedLoginAction.visible ? savedLoginAction.left : parent.right
                            anchors.rightMargin: savedLoginAction.visible ? 6 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: qsTr("Username")
                            onEditingFinished: appWindow.prefillPassword()
                            onTextEdited: {
                                appWindow.loginTouched = true
                                portalPassword.clear()
                                appWindow.prefilledOrigin = ""
                                appWindow.prefilledUsername = ""
                                appWindow.portalSession.requestSavedPassword("")
                            }
                        }
                        Button {
                            id: savedLoginAction
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            visible: appWindow.portalSession.savedLogins.length > 1
                            text: "⌄"
                            Accessible.name: qsTr("Choose saved login")
                            ToolTip.visible: hovered
                            ToolTip.text: Accessible.name
                            onClicked: loginChooser.open()
                        }
                    }
                    SelectableText { text: qsTr("Password"); color: appWindow.ink; font.weight: Font.Medium; Layout.topMargin: 4 }
                    TextField { id: portalPassword; objectName: "portalPassword"; Layout.fillWidth: true; placeholderText: qsTr("Password"); Accessible.name: qsTr("Password"); echoMode: TextInput.Password; onAccepted: appWindow.authenticate(); onTextEdited: { appWindow.passwordEdited = true; appWindow.prefilledOrigin = ""; appWindow.prefilledUsername = "" } }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        SelectableText { text: qsTr("Portal URL"); color: appWindow.ink; font.weight: Font.Medium; Layout.fillWidth: true }
                        LinkAction { text: qsTr("↗ Browser"); visible: appWindow.portalConfig.safePortalUrl(portalAddress.text) !== ""; onClicked: Qt.openUrlExternally(appWindow.portalConfig.safePortalUrl(portalAddress.text)) }
                    }
                    TextField {
                        id: portalAddress
                        objectName: "portalAddress"
                        Layout.fillWidth: true
                        placeholderText: qsTr("https://meet.example.org")
                        Accessible.name: qsTr("Portal URL")
                        onEditingFinished: appWindow.choosePortal()
                        onTextEdited: {
                            appWindow.loginTouched = true
                            if (appWindow.prefilledOrigin !== "" && text.trim() !== appWindow.prefilledOrigin) {
                                portalPassword.clear()
                                appWindow.prefilledOrigin = ""
                                appWindow.prefilledUsername = ""
                            }
                        }
                    }
                    SelectableText { visible: appWindow.portalConfig.error !== ""; text: appWindow.portalConfig.error; color: appWindow.errorColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    SelectableText { visible: appWindow.portalSession.status !== "" && appWindow.portalSession.status !== "Signed out." && !appWindow.portalSession.status.startsWith("Portal changed."); text: appWindow.portalSession.status; color: appWindow.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    PrimaryAction { objectName: "signInButton"; text: appWindow.portalSession.busy ? qsTr("Connecting…") : qsTr("Sign in"); Layout.fillWidth: true; enabled: !appWindow.portalSession.busy && portalAddress.text.trim() !== "" && portalUsername.text.trim() !== "" && (portalPassword.text !== "" || appWindow.portalSession.savedAccounts.includes(portalUsername.text.trim())); onClicked: appWindow.authenticate() }
                    LinkAction { text: "ⓘ " + qsTr("About"); Accessible.name: qsTr("About Viamiora"); Layout.alignment: Qt.AlignHCenter; onClicked: appWindow.section = Main.View.About }
                    Item { Layout.preferredHeight: 24 }
                }
            }
            }

            // Overview
            ScrollView {
                id: overviewScroll
                clip: true
                ColumnLayout {
                    width: Math.min(overviewScroll.availableWidth - 56, 700)
                    x: Math.max(28, (overviewScroll.availableWidth - width) / 2)
                    spacing: 16
                    Item { Layout.preferredHeight: 12 }
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            SelectableText { text: qsTr("Meetings"); color: appWindow.ink; font.pixelSize: 29; font.weight: Font.DemiBold }
                        }
                    }
                    SelectableText {
                        text: appWindow.overviewMonth()
                        color: appWindow.muted
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    RowLayout {
                        visible: appWindow.overviewQuery.trim() === ""
                        Layout.fillWidth: true
                        spacing: 5
                        ToolButton { text: "‹"; Accessible.name: qsTr("Previous seven days"); onClicked: appWindow.overviewWeekOffset-- }
                        Repeater {
                            model: 7
                            delegate: DayChoice {
                                required property int index
                                day: appWindow.overviewDay(index)
                                meetingCount: appWindow.meetingsOnDay(day)
                                selected: appWindow.overviewDayFilter === Qt.formatDate(day, "yyyy-MM-dd")
                                Layout.fillWidth: true
                                onClicked: appWindow.overviewDayFilter = Qt.formatDate(day, "yyyy-MM-dd")
                            }
                        }
                        ToolButton { text: "›"; Accessible.name: qsTr("Next seven days"); onClicked: appWindow.overviewWeekOffset++ }
                    }
                    RowLayout {
                        visible: appWindow.overviewDayFilter !== ""
                        Layout.fillWidth: true
                        SelectableText { text: Qt.formatDate(new Date(appWindow.overviewDayFilter + "T12:00:00"), "dddd d MMMM"); color: appWindow.accent; Layout.fillWidth: true; font.weight: Font.Medium }
                        ToolButton { text: "×"; Accessible.name: qsTr("Clear day filter"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.overviewDayFilter = "" }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            TextField { id: overviewSearch; anchors.fill: parent; rightPadding: clearOverview.visible ? 34 : 12; placeholderText: qsTr("Search meetings and rooms"); Accessible.name: qsTr("Search meetings and rooms"); onTextChanged: appWindow.overviewQuery = text }
                            ToolButton { id: clearOverview; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "×"; visible: overviewSearch.text !== ""; Accessible.name: qsTr("Clear search"); onClicked: overviewSearch.clear() }
                        }
                    }
                    RowLayout {
                        visible: appWindow.portalSession.status !== "" && !appWindow.portalSession.busy
                        Layout.fillWidth: true
                        SelectableText { text: appWindow.portalSession.status; color: appWindow.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    }
                    BusyIndicator { running: appWindow.portalSession.busy && appWindow.portalSession.meetingsUpdatedAtMs === 0; visible: running; Layout.alignment: Qt.AlignHCenter }
                    BusyIndicator { running: appWindow.portalSession.historicalBusy && appWindow.overviewWeekOffset < 0; visible: running; Layout.alignment: Qt.AlignHCenter }
                    SelectableText { visible: appWindow.portalSession.historicalStatus !== "" && appWindow.overviewWeekOffset < 0; text: appWindow.portalSession.historicalStatus; color: appWindow.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    SelectableText { visible: appWindow.overviewQuery.trim() !== "" && appWindow.filteredOngoingMeetings.length === 0 && appWindow.filteredUpcomingMeetings.length === 0 && appWindow.filteredEarlierMeetings.length === 0 && appWindow.filteredRooms.length === 0; text: qsTr("No meetings or rooms match your search."); color: appWindow.muted; Layout.fillWidth: true }
                    Rectangle {
                        visible: appWindow.filteredOngoingMeetings.length > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? ongoingContent.implicitHeight + 36 : 0
                        color: appWindow.raised
                        border.color: appWindow.line
                        radius: 12
                        ColumnLayout {
                            id: ongoingContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 18
                            spacing: 10
                            SelectableText { text: qsTr("Scheduled now"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                            Repeater { model: appWindow.filteredOngoingMeetings; delegate: MeetingLine {} }
                        }
                    }
                    Rectangle {
                        visible: appWindow.filteredUpcomingMeetings.length > 0 ||
                            (appWindow.overviewQuery.trim() === "" && appWindow.filteredOngoingMeetings.length === 0 &&
                             appWindow.filteredEarlierMeetings.length === 0 && !appWindow.portalSession.historicalBusy)
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? upcomingContent.implicitHeight + 36 : 0
                        color: appWindow.color
                        border.color: appWindow.line
                        radius: 12
                        ColumnLayout {
                            id: upcomingContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 18
                            spacing: 12
                            SelectableText { text: appWindow.overviewWeekOffset < 0 ? qsTr("Meetings") : qsTr("Upcoming"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                            SelectableText { visible: appWindow.filteredUpcomingMeetings.length === 0; text: appWindow.overviewDayFilter !== "" ? qsTr("No meetings for this day.") : appWindow.overviewWeekOffset !== 0 ? qsTr("No meetings this week.") : qsTr("No upcoming meetings."); color: appWindow.muted }
                            Repeater { model: appWindow.filteredUpcomingMeetings; delegate: MeetingLine {} }
                        }
                    }
                    Rectangle {
                        visible: appWindow.filteredEarlierMeetings.length > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? earlierContent.implicitHeight + 36 : 0
                        color: appWindow.color
                        border.color: appWindow.line
                        radius: 12
                        ColumnLayout {
                            id: earlierContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 18
                            spacing: 12
                            SelectableText { text: qsTr("Past meetings"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                            Repeater { model: appWindow.filteredEarlierMeetings; delegate: MeetingLine {} }
                        }
                    }
                    Rectangle {
                        visible: appWindow.overviewQuery.trim() === ""
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? invitationContent.implicitHeight + 36 : 0
                        color: appWindow.surface
                        border.color: appWindow.line
                        radius: 12
                        ColumnLayout {
                            id: invitationContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 18
                            spacing: 10
                            SelectableText { text: qsTr("Open an invitation"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField { id: invitationLink; Layout.fillWidth: true; placeholderText: qsTr("Paste an HTTPS invitation link"); Accessible.name: qsTr("Invitation link") }
                                Button {
                                    text: qsTr("Open")
                                    onClicked: {
                                        if (!appWindow.openMeetingLink(invitationLink.text.trim(), "Invitation")) {
                                            appWindow.invitationError = "Enter a complete HTTPS invitation link."
                                            return
                                        }
                                        appWindow.invitationError = ""
                                    }
                                }
                            }
                            SelectableText { visible: appWindow.invitationError !== ""; text: appWindow.invitationError; color: appWindow.errorColor }
                        }
                    }
                    Rectangle {
                        visible: appWindow.overviewQuery.trim() === "" || appWindow.filteredRooms.length > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? roomContent.implicitHeight + 36 : 0
                        color: appWindow.color
                        border.color: appWindow.line
                        radius: 12
                        ColumnLayout {
                            id: roomContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 18
                            spacing: 10
                            SelectableText { text: qsTr("Your rooms"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                            Repeater {
                                model: appWindow.filteredRooms
                                delegate: RowLayout {
                                    id: roomRow
                                    required property var modelData
                                    Layout.fillWidth: true
                                    SelectableText { text: roomRow.modelData.title; font.weight: Font.Medium; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                    ToolButton { text: "⧉"; Accessible.name: qsTr("Copy ") + roomRow.modelData.title + " link"; ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(roomRow.modelData.url) }
                                    ToolButton { text: "▶"; Accessible.name: qsTr("Join") + " " + roomRow.modelData.title; ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.openMeetingLink(roomRow.modelData.url, roomRow.modelData.title) }
                                    ToolButton {
                                        id: removeRoom
                                        contentItem: Canvas {
                                            property color tint: appWindow.errorColor
                                            onTintChanged: requestPaint()
                                            onPaint: {
                                                const ctx = getContext("2d")
                                                ctx.clearRect(0, 0, width, height)
                                                ctx.strokeStyle = tint
                                                ctx.lineWidth = 1.7
                                                ctx.lineCap = "round"
                                                ctx.lineJoin = "round"
                                                const x = (width - 20) / 2
                                                const y = (height - 20) / 2
                                                ctx.beginPath()
                                                ctx.moveTo(x + 3, y + 5); ctx.lineTo(x + 17, y + 5)
                                                ctx.moveTo(x + 7, y + 3); ctx.lineTo(x + 13, y + 3)
                                                ctx.moveTo(x + 5, y + 7); ctx.lineTo(x + 6, y + 18)
                                                ctx.lineTo(x + 14, y + 18); ctx.lineTo(x + 15, y + 7)
                                                ctx.moveTo(x + 8, y + 9); ctx.lineTo(x + 8.5, y + 15)
                                                ctx.moveTo(x + 12, y + 9); ctx.lineTo(x + 11.5, y + 15)
                                                ctx.stroke()
                                            }
                                        }
                                        Accessible.name: qsTr("Remove ") + roomRow.modelData.title
                                        ToolTip.visible: hovered
                                        ToolTip.text: Accessible.name
                                        onClicked: appWindow.portalConfig.removeRoomLink(roomRow.modelData.url)
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField { id: roomName; Layout.preferredWidth: 130; placeholderText: qsTr("Room name"); Accessible.name: qsTr("Room name") }
                                TextField { id: roomLink; Layout.fillWidth: true; Layout.minimumWidth: 0; placeholderText: qsTr("https://…"); Accessible.name: qsTr("Room link") }
                                Button {
                                    text: "+"
                                    Layout.preferredWidth: 40
                                    Layout.preferredHeight: 40
                                    background: Rectangle { color: appWindow.successColor; radius: 8 }
                                    contentItem: Text { text: "+"; color: "white"; font.pixelSize: 24; font.weight: Font.Medium; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    Accessible.name: qsTr("Save room link")
                                    ToolTip.visible: hovered
                                    ToolTip.text: Accessible.name
                                    onClicked: if (appWindow.portalConfig.saveRoomLink(roomName.text, roomLink.text)) {
                                        roomName.clear()
                                        roomLink.clear()
                                    }
                                }
                            }
                            SelectableText { visible: appWindow.portalConfig.error !== ""; text: appWindow.portalConfig.error; color: appWindow.errorColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                    Item { Layout.preferredHeight: 20 }
                }
            }

            // Schedule
            ScrollView {
                id: scheduleScroll
                clip: true
                ColumnLayout {
                    width: Math.min(scheduleScroll.availableWidth - 56, 680)
                    x: Math.max(28, (scheduleScroll.availableWidth - width) / 2)
                    spacing: 8
                    Item { Layout.preferredHeight: 4 }
                    SelectableText { text: qsTr("Plan a meeting"); color: appWindow.ink; font.pixelSize: 29; font.weight: Font.DemiBold }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: scheduleContent.implicitHeight + 24
                        color: appWindow.color
                        border.color: appWindow.line
                        radius: 10
                        ColumnLayout {
                            id: scheduleContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 8
                            SelectableText { text: qsTr("Meeting title"); color: appWindow.ink; font.weight: Font.Medium }
                            TextField { id: scheduleTitle; Layout.fillWidth: true; placeholderText: qsTr("What is this meeting about?"); Accessible.name: qsTr("Meeting title") }
                            ComboBox {
                                id: bookingType
                                Layout.fillWidth: true
                                visible: appWindow.portalSession.bookingTypes.length > 1
                                model: appWindow.portalSession.bookingTypes
                                textRole: "title"
                                Accessible.name: qsTr("Meeting type")
                                onActivated: appWindow.portalSession.inspectBookingType(appWindow.portalSession.bookingTypes[currentIndex].id)
                            }
                            SelectableText { text: qsTr("Date and time"); color: appWindow.ink; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.topMargin: 4 }
                            RowLayout {
                                Layout.fillWidth: true
                                Button { text: "‹"; Accessible.name: qsTr("Previous week"); onClicked: appWindow.shiftDay(-7) }
                                SelectableText { text: Qt.formatDate(appWindow.selectedDay, "MMMM yyyy"); color: appWindow.ink; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; font.weight: Font.DemiBold }
                                Button { text: qsTr("Today"); onClicked: appWindow.selectedDay = new Date() }
                                Button { text: "›"; Accessible.name: qsTr("Next week"); onClicked: appWindow.shiftDay(7) }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Repeater {
                                    model: 7
                                    delegate: DayChoice {
                                        required property int index
                                        day: appWindow.scheduleWeekDay(index)
                                        meetingCount: appWindow.meetingsOnDay(day)
                                        Layout.fillWidth: true
                                        selected: Qt.formatDate(day, "yyyy-MM-dd") === Qt.formatDate(appWindow.selectedDay, "yyyy-MM-dd")
                                        onClicked: appWindow.selectedDay = day
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    SelectableText { text: qsTr("Starts"); color: appWindow.muted }
                                    ComboBox { id: startTime; Layout.fillWidth: true; model: appWindow.timeSlots; currentIndex: 20; Accessible.name: qsTr("Start time"); onActivated: endTime.currentIndex = Math.min(47, currentIndex + 2) }
                                }
                                SelectableText { text: "→"; color: appWindow.muted; font.pixelSize: 20; Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 7 }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    SelectableText { text: qsTr("Ends"); color: appWindow.muted }
                                    ComboBox { id: endTime; Layout.fillWidth: true; model: appWindow.timeSlots; currentIndex: 22; Accessible.name: qsTr("End time") }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                SelectableText { text: qsTr("Duration"); color: appWindow.muted; Layout.rightMargin: 4 }
                                Repeater {
                                    model: [{ label: qsTr("30 min"), slots: 1 }, { label: qsTr("1 hour"), slots: 2 }, { label: qsTr("2 hours"), slots: 4 }]
                                    delegate: Button {
                                        id: durationChoice
                                        required property var modelData
                                        readonly property bool selected: endTime.currentIndex - startTime.currentIndex === modelData.slots
                                        text: modelData.label
                                        enabled: startTime.currentIndex + modelData.slots < appWindow.timeSlots.length
                                        background: Rectangle { radius: 8; color: durationChoice.selected ? appWindow.selectedColor : appWindow.surface; border.color: durationChoice.selected ? appWindow.accent : appWindow.line }
                                        onClicked: endTime.currentIndex = startTime.currentIndex + modelData.slots
                                    }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                SelectableText { text: appWindow.calendarService.suggesting || (appWindow.calendarRequested && !appWindow.calendarService.connected) ? appWindow.calendarService.status : appWindow.slotSummary(); visible: text !== ""; color: text.startsWith("✓") ? appWindow.successColor : appWindow.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                LinkAction {
                                    visible: !appWindow.calendarService.authorized
                                    text: qsTr("Connect calendar")
                                    onClicked: {
                                        appWindow.calendarRequested = true
                                        appWindow.calendarService.refresh(Qt.formatDate(appWindow.scheduleWeekDay(0), "yyyy-MM-dd"))
                                    }
                                }
                                LinkAction { text: qsTr("Next free hour"); visible: appWindow.calendarService.authorized; enabled: !appWindow.calendarService.suggesting; onClicked: appWindow.calendarService.suggestNextHour() }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: appWindow.line; Layout.topMargin: 4 }
                            RowLayout {
                                Layout.fillWidth: true
                                SelectableText { text: qsTr("Meeting PIN"); color: appWindow.ink; font.weight: Font.Medium; Layout.fillWidth: true }
                                SelectableText { text: appWindow.schedulePin; color: appWindow.accent; font.pixelSize: 19; font.weight: Font.DemiBold; Accessible.name: qsTr("Meeting PIN ") + text }
                                ToolButton { text: "⧉"; Accessible.name: qsTr("Copy meeting PIN"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(appWindow.schedulePin) }
                            }
                            RowLayout {
                                visible: appWindow.portalSession.moderatorPinSupported
                                Layout.fillWidth: true
                                SelectableText { text: qsTr("Moderator PIN"); color: appWindow.ink; font.weight: Font.Medium; Layout.fillWidth: true }
                                SelectableText { text: appWindow.scheduleModeratorPin; color: appWindow.accent; font.pixelSize: 19; font.weight: Font.DemiBold; Accessible.name: qsTr("Moderator PIN ") + text }
                                ToolButton { text: "⧉"; Accessible.name: qsTr("Copy moderator PIN"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(appWindow.scheduleModeratorPin) }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: appWindow.line; Layout.topMargin: 4 }
                            AddressEntry { id: inviteeAddresses; role: "Invitees"; Layout.fillWidth: true }
                            AddressEntry { id: moderatorAddresses; role: "Moderators"; Layout.fillWidth: true }
                            SelectableText { visible: appWindow.contactStore.error !== ""; text: appWindow.contactStore.error; color: appWindow.errorColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                    SelectableText { visible: appWindow.scheduleError !== ""; text: appWindow.scheduleError; color: appWindow.errorColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    SelectableText { visible: appWindow.portalSession.creationStatus !== ""; text: appWindow.portalSession.creationStatus; color: appWindow.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    PrimaryAction {
                        text: appWindow.portalSession.creatingMeeting ? qsTr("Creating…") : qsTr("Create meeting")
                        Layout.fillWidth: true
                        enabled: !appWindow.portalSession.creatingMeeting && !appWindow.portalSession.busy && !appWindow.portalSession.inviting
                        onClicked: appWindow.prepareBooking()
                    }
                    Item { Layout.preferredHeight: 20 }
                }
            }

            // Settings
            ScrollView {
                id: accountScroll
                clip: true
                ColumnLayout {
                    width: Math.min(accountScroll.availableWidth - 56, 680)
                    x: Math.max(28, (accountScroll.availableWidth - width) / 2)
                    spacing: 14
                    Item { Layout.preferredHeight: 12 }
                    SelectableText { text: qsTr("Settings"); color: appWindow.ink; font.pixelSize: 29; font.weight: Font.DemiBold }
                    SelectableText { text: qsTr("Signed in as"); color: appWindow.muted; Layout.topMargin: 12 }
                    SelectableText { text: appWindow.portalSession.username; font.pixelSize: 20; font.weight: Font.DemiBold }
                    RowLayout {
                        Layout.fillWidth: true
                        SelectableText { text: appWindow.portalConfig.portalUrl; color: appWindow.muted; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy portal address"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(appWindow.portalConfig.portalUrl) }
                    }
                    RowLayout {
                        visible: appWindow.brandingService.brandName !== ""
                        Layout.fillWidth: true
                        Image { source: appWindow.brandingService.logoUrl; visible: status === Image.Ready; sourceSize.width: 180; sourceSize.height: 60; Layout.preferredWidth: visible ? 180 : 0; Layout.preferredHeight: visible ? 60 : 0; fillMode: Image.PreserveAspectFit }
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: appWindow.line; Layout.topMargin: 12 }
                    RowLayout {
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("Language"); color: appWindow.ink; Layout.fillWidth: true }
                        ComboBox {
                            id: languageChoice
                            model: [
                                { name: qsTr("System default"), code: "" },
                                { name: "English", code: "en" },
                                { name: "Deutsch", code: "de" },
                                { name: "Svenska", code: "sv" }
                            ]
                            textRole: "name"
                            Accessible.name: qsTr("Language")
                            onActivated: function(index) {
                                appWindow.portalConfig.setLanguage(index === 1 ? "en" : index === 2 ? "de" : index === 3 ? "sv" : "")
                            }
                        }
                        Binding {
                            target: languageChoice
                            property: "currentIndex"
                            value: appWindow.portalConfig.language === "sv" ? 3 : appWindow.portalConfig.language === "de" ? 2 : appWindow.portalConfig.language === "en" ? 1 : 0
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("Appearance"); color: appWindow.ink; Layout.fillWidth: true }
                        ComboBox {
                            id: appearanceChoice
                            model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                            Accessible.name: qsTr("Appearance")
                            onActivated: function(index) {
                                appWindow.portalConfig.setAppearance(index === 1 ? "light" : index === 2 ? "dark" : "")
                            }
                        }
                        Binding {
                            target: appearanceChoice
                            property: "currentIndex"
                            value: appWindow.portalConfig.appearance === "dark" ? 2 : appWindow.portalConfig.appearance === "light" ? 1 : 0
                        }
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: appWindow.line }
                    SelectableText { text: qsTr("System access"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                    RowLayout {
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("Calendar"); color: appWindow.ink; Layout.fillWidth: true }
                        SelectableText { visible: appWindow.calendarService.authorized; text: qsTr("✓ Connected"); color: appWindow.successColor }
                        Button { visible: !appWindow.calendarService.authorized; text: qsTr("Allow access"); onClicked: { appWindow.calendarRequested = true; appWindow.calendarService.requestAccess() } }
                    }
                    SelectableText { visible: !appWindow.calendarService.authorized && appWindow.calendarRequested; text: appWindow.calendarService.status; color: appWindow.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("Contacts"); color: appWindow.ink; Layout.fillWidth: true }
                        SelectableText { visible: appWindow.systemContacts.authorized; text: qsTr("✓ Connected"); color: appWindow.successColor }
                        Button { visible: !appWindow.systemContacts.authorized; text: qsTr("Allow access"); onClicked: appWindow.systemContacts.requestAccess() }
                    }
                    SelectableText { visible: !appWindow.systemContacts.authorized && appWindow.systemContacts.status.includes("off"); text: appWindow.systemContacts.status; color: appWindow.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: appWindow.line; Layout.topMargin: 12 }
                    SelectableText { text: qsTr("Saved logins"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                    Repeater {
                        model: appWindow.portalSession.savedLogins
                        delegate: RowLayout {
                            id: savedLoginRow
                            required property var modelData
                            readonly property bool isCurrent: modelData.origin === appWindow.portalConfig.portalUrl &&
                                                              modelData.username === appWindow.portalSession.username
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                SelectableText { text: savedLoginRow.modelData.username; color: appWindow.ink; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideMiddle }
                                SelectableText { text: savedLoginRow.modelData.origin; color: appWindow.muted; Layout.fillWidth: true; elide: Text.ElideMiddle }
                            }
                            SelectableText {
                                visible: savedLoginRow.isCurrent
                                text: qsTr("Current")
                                color: appWindow.successColor
                                Accessible.name: qsTr("Current account")
                            }
                            Button {
                                visible: !savedLoginRow.isCurrent
                                text: qsTr("Switch")
                                enabled: !appWindow.portalSession.busy && !appWindow.portalSession.inviting && !appWindow.portalSession.creatingMeeting
                                Accessible.name: qsTr("Switch to %1 at %2").arg(savedLoginRow.modelData.username).arg(savedLoginRow.modelData.origin)
                                onClicked: appWindow.portalSession.switchSavedLogin(savedLoginRow.modelData.origin, savedLoginRow.modelData.username)
                            }
                            Button {
                                text: qsTr("Forget")
                                enabled: !appWindow.portalSession.busy && !appWindow.portalSession.inviting && !appWindow.portalSession.creatingMeeting
                                Accessible.name: qsTr("Forget ") + savedLoginRow.modelData.username + " at " + savedLoginRow.modelData.origin
                                onClicked: appWindow.portalSession.forgetSavedLogin(savedLoginRow.modelData.origin, savedLoginRow.modelData.username)
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 20 }
                }
            }

            // About
            ScrollView {
                id: aboutScroll
                clip: true
                ColumnLayout {
                    width: Math.min(aboutScroll.availableWidth - 56, 680)
                    x: Math.max(28, (aboutScroll.availableWidth - width) / 2)
                    spacing: 14
                    Item { Layout.preferredHeight: 12 }
                    RowLayout {
                        Layout.fillWidth: true
                        ToolButton {
                            visible: !appWindow.portalSession.signedIn
                            text: "‹"
                            Accessible.name: qsTr("Back to sign in")
                            ToolTip.visible: hovered
                            ToolTip.text: Accessible.name
                            onClicked: appWindow.section = Main.View.Overview
                        }
                        SelectableText { text: qsTr("About Viamiora"); color: appWindow.ink; font.pixelSize: 29; font.weight: Font.DemiBold; Layout.fillWidth: true }
                        Image { source: "assets/app-icon.png"; sourceSize.width: 96; sourceSize.height: 96; Layout.preferredWidth: 96; Layout.preferredHeight: 96; fillMode: Image.PreserveAspectFit }
                    }
                    SelectableText { text: qsTr("Version ") + appWindow.appVersion; color: appWindow.muted }
                    LinkAction { text: "Apache-2.0 ↗"; onClicked: Qt.openUrlExternally("https://www.apache.org/licenses/LICENSE-2.0") }
                    LinkAction { text: qsTr("Independent app for Mividas") + " ↗"; onClicked: Qt.openUrlExternally("https://www.mividas.com/product-suite/meeting-portal/") }
                    SelectableText { text: qsTr("Dependencies"); color: appWindow.ink; font.pixelSize: 19; font.weight: Font.DemiBold; Layout.topMargin: 12 }
                    Repeater {
                        model: appWindow.externalDependencies
                        delegate: LinkAction {
                            required property var modelData
                            text: modelData.name + " " + modelData.version + " · " + modelData.license + " ↗"
                            onClicked: Qt.openUrlExternally(modelData.url)
                        }
                    }
                    Item { Layout.preferredHeight: 20 }
                }
            }
            // Video
            ColumnLayout {
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    Button { text: qsTr("‹ Overview"); onClicked: appWindow.section = Main.View.Overview }
                    SelectableText { text: appWindow.browserTitle; color: appWindow.ink; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                    Button { text: qsTr("Leave meeting"); onClicked: appWindow.closeVideo() }
                    Button { text: qsTr("Open in browser ↗"); onClicked: Qt.openUrlExternally(appWindow.pendingBrowserUrl) }
                    ToolButton {
                        text: "ⓘ"
                        Accessible.name: qsTr("About Viamiora")
                        ToolTip.visible: hovered
                        ToolTip.text: Accessible.name
                        onClicked: appWindow.section = Main.View.About
                    }
                }
                RowLayout {
                    visible: appWindow.browserLoading || appWindow.browserError !== ""
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    BusyIndicator { running: appWindow.browserLoading && appWindow.browserError === ""; visible: running; implicitWidth: 20; implicitHeight: 20 }
                    SelectableText { text: appWindow.browserError !== "" ? appWindow.browserError : qsTr("Opening meeting…"); color: appWindow.browserError !== "" ? appWindow.errorColor : appWindow.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    LinkAction { visible: appWindow.browserError !== ""; text: qsTr("Retry"); onClicked: appWindow.configureVideoView() }
                }
                Loader {
                    id: browserLoader
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: String(appWindow.pendingBrowserUrl) !== ""
                    onLoaded: appWindow.configureVideoView()
                    sourceComponent: Component {
                        WebView {
                            url: appWindow.browserUrl
                            Component.onCompleted: {
                                settings.javaScriptEnabled = true
                                settings.localStorageEnabled = true
                            }
                            onCookieAdded: function(domain, name) {
                                if (appWindow.pendingBrowserCookies <= 0) return
                                appWindow.pendingBrowserCookies--
                                if (appWindow.pendingBrowserCookies === 0) {
                                    browserCookieTimeout.stop()
                                    browserLoad.restart()
                                }
                            }
                            onLoadingChanged: function(loadRequest) {
                                if (String(appWindow.pendingBrowserUrl) === "") return
                                if (loadRequest.status === WebView.LoadStartedStatus) {
                                    browserReadyCheck.stop()
                                    appWindow.browserLoading = true
                                    appWindow.browserError = ""
                                } else if (loadRequest.status === WebView.LoadSucceededStatus) {
                                    browserTimeout.stop()
                                    appWindow.browserLoading = false
                                    appWindow.browserError = ""
                                    browserReadyCheck.restart()
                                } else if (loadRequest.status === WebView.LoadFailedStatus) {
                                    browserTimeout.stop()
                                    browserReadyCheck.stop()
                                    appWindow.browserLoading = false
                                    appWindow.browserError = "The meeting could not be opened here. Try again or use the browser."
                                }
                            }
                        }
                    }
                }
            }
            // Portal meeting details
            ScrollView {
                id: detailsScroll
                objectName: "meetingDetails"
                clip: true
                ColumnLayout {
                    id: detailContent
                    width: Math.min(detailsScroll.availableWidth - 56, 720)
                    x: Math.max(28, (detailsScroll.availableWidth - width) / 2)
                    spacing: 12
                    Item { Layout.preferredHeight: 12 }
                    Button { objectName: "meetingDetailsBack"; text: qsTr("‹ Overview"); enabled: !appWindow.portalSession.inviting; onClicked: appWindow.section = Main.View.Overview }
                    SelectableText { text: appWindow.shareTitle; font.pixelSize: 22; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: TextEdit.Wrap }
                    SelectableText { visible: appWindow.shareTime !== ""; text: appWindow.shareTime; color: appWindow.muted; Layout.fillWidth: true }
                    ExpandableDescription { description: appWindow.portalSession.meetingLinks.description || "" }
                    BusyIndicator { running: appWindow.portalSession.linksBusy; visible: running }
                    SelectableText { visible: appWindow.portalSession.linkStatus !== ""; text: appWindow.portalSession.linkStatus; color: appWindow.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    SelectableText { visible: participantLink.address !== ""; text: qsTr("Participant link"); color: appWindow.ink; font.weight: Font.Medium }
                    RowLayout {
                        visible: participantLink.address !== ""
                        Layout.fillWidth: true
                        SelectableLink { id: participantLink; address: appWindow.portalSession.meetingLinks.participantUrl || ""; meetingTitle: appWindow.shareTitle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy participant link"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: { appWindow.clipboardService.copy(participantLink.address); appWindow.shareCopyStatus = qsTr("Copied") } }
                    }
                    RowLayout {
                        visible: (appWindow.portalSession.meetingLinks.participantPin || "") !== ""
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("Participant PIN"); color: appWindow.muted; Layout.fillWidth: true }
                        SelectableText { text: appWindow.portalSession.meetingLinks.participantPin || ""; font.weight: Font.DemiBold }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy participant PIN"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(appWindow.portalSession.meetingLinks.participantPin) }
                    }
                    SelectableText { visible: moderatorLink.address !== ""; text: qsTr("Moderator link"); color: appWindow.ink; font.weight: Font.Medium; Layout.topMargin: 6 }
                    RowLayout {
                        visible: moderatorLink.address !== ""
                        Layout.fillWidth: true
                        SelectableLink { id: moderatorLink; address: appWindow.portalSession.meetingLinks.moderatorUrl || ""; meetingTitle: appWindow.shareTitle; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy moderator link"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: { appWindow.clipboardService.copy(moderatorLink.address); appWindow.shareCopyStatus = qsTr("Copied") } }
                    }
                    RowLayout {
                        visible: (appWindow.portalSession.meetingLinks.moderatorPin || "") !== ""
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("Moderator PIN"); color: appWindow.muted; Layout.fillWidth: true }
                        SelectableText { text: appWindow.portalSession.meetingLinks.moderatorPin || ""; font.weight: Font.DemiBold }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy moderator PIN"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(appWindow.portalSession.meetingLinks.moderatorPin) }
                    }
                    RowLayout {
                        visible: (appWindow.portalSession.meetingLinks.roomName || "") !== ""
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("Room"); color: appWindow.muted; Layout.fillWidth: true }
                        SelectableText { text: appWindow.portalSession.meetingLinks.roomName || "" }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy room"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(appWindow.portalSession.meetingLinks.roomName) }
                    }
                    RowLayout {
                        visible: (appWindow.portalSession.meetingLinks.sip || "") !== ""
                        Layout.fillWidth: true
                        SelectableText { text: qsTr("SIP"); color: appWindow.muted }
                        SelectableLink { address: appWindow.portalSession.meetingLinks.sip || ""; openExternally: true; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy SIP address"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(appWindow.portalSession.meetingLinks.sip) }
                    }
                    Repeater {
                        model: [
                            { label: qsTr("Moderators"), entries: appWindow.portalSession.meetingLinks.moderators || [] },
                            { label: qsTr("Participants"), entries: appWindow.portalSession.meetingLinks.participants || [] }
                        ]
                        delegate: ColumnLayout {
                            id: roleList
                            required property var modelData
                            visible: modelData.entries.length > 0
                            Layout.fillWidth: true
                            SelectableText { text: roleList.modelData.label; color: appWindow.ink; font.weight: Font.Medium }
                            Repeater {
                                model: roleList.modelData.entries
                                delegate: RowLayout {
                                    id: inviteRow
                                    required property var modelData
                                    Layout.fillWidth: true
                                    SelectableText { text: inviteRow.modelData.email; color: appWindow.muted; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                                    ToolButton { text: "⧉"; Accessible.name: qsTr("Copy ") + inviteRow.modelData.email; ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(inviteRow.modelData.email) }
                                    ToolButton {
                                        visible: appWindow.portalSession.meetingLinks.canChange === true && inviteRow.modelData.id > 0
                                        enabled: !appWindow.portalSession.busy && !appWindow.portalSession.creatingMeeting &&
                                                 !appWindow.portalSession.inviting && !appWindow.portalSession.linksBusy
                                        Accessible.name: qsTr("Remove invitation for %1").arg(inviteRow.modelData.email)
                                        ToolTip.visible: hovered
                                        ToolTip.text: Accessible.name
                                        contentItem: Canvas {
                                            implicitWidth: 20
                                            implicitHeight: 20
                                            property color tint: appWindow.errorColor
                                            onTintChanged: requestPaint()
                                            onPaint: {
                                                const ctx = getContext("2d")
                                                ctx.clearRect(0, 0, width, height)
                                                ctx.strokeStyle = tint
                                                ctx.lineWidth = 1.7
                                                ctx.lineCap = "round"
                                                ctx.beginPath()
                                                ctx.moveTo(3, 5); ctx.lineTo(17, 5)
                                                ctx.moveTo(8, 3); ctx.lineTo(12, 3)
                                                ctx.moveTo(6, 6); ctx.lineTo(7, 17); ctx.lineTo(13, 17); ctx.lineTo(14, 6)
                                                ctx.moveTo(9, 8); ctx.lineTo(9, 14)
                                                ctx.moveTo(11, 8); ctx.lineTo(11, 14)
                                                ctx.stroke()
                                            }
                                        }
                                        onClicked: {
                                            removeInviteDialog.inviteId = inviteRow.modelData.id
                                            removeInviteDialog.email = inviteRow.modelData.email
                                            removeInviteDialog.open()
                                        }
                                    }
                                }
                            }
                        }
                    }
                    LinkAction {
                        visible: appWindow.portalSession.meetingLinks.canChange === true
                        text: appWindow.shareInviteOpen ? qsTr("Hide invitation") : qsTr("+ Invite people")
                        onClicked: appWindow.shareInviteOpen = !appWindow.shareInviteOpen
                    }
                    ColumnLayout {
                        visible: appWindow.shareInviteOpen && appWindow.portalSession.meetingLinks.canChange === true
                        Layout.fillWidth: true
                        spacing: 10
                        AddressEntry { id: shareInvitees; role: qsTr("Invitees"); Layout.fillWidth: true }
                        AddressEntry { id: shareModerators; role: qsTr("Moderators"); Layout.fillWidth: true }
                        RowLayout {
                            Layout.fillWidth: true
                            SelectableText { visible: appWindow.shareInviteError !== ""; text: appWindow.shareInviteError; color: appWindow.errorColor; Layout.fillWidth: true }
                            Item { Layout.fillWidth: true; visible: appWindow.shareInviteError === "" }
                            Button {
                                text: appWindow.portalSession.inviting ? qsTr("Sending…") : qsTr("Send invitations")
                                enabled: !appWindow.portalSession.inviting && !appWindow.portalSession.linksBusy &&
                                         (shareInvitees.hasAddresses || shareModerators.hasAddresses)
                                onClicked: {
                                    if (!shareInvitees.commitPending() || !shareModerators.commitPending()) {
                                        appWindow.shareInviteError = qsTr("Check the email addresses.")
                                        return
                                    }
                                    appWindow.shareInviteError = ""
                                    if (appWindow.portalSession.inviteToMeeting(appWindow.shareMeetingId,
                                                                      shareInvitees.selectedEmails,
                                                                      shareModerators.selectedEmails)) {
                                        shareInvitees.clear()
                                        shareModerators.clear()
                                        appWindow.shareInviteOpen = false
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        visible: appWindow.shareMeetingId === appWindow.portalSession.invitationMeetingId &&
                                 appWindow.portalSession.invitationStatus !== ""
                        Layout.fillWidth: true
                        BusyIndicator { running: appWindow.portalSession.inviting; visible: running; implicitWidth: 20; implicitHeight: 20 }
                        SelectableText { text: appWindow.portalSession.invitationStatus; color: appWindow.muted; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        LinkAction { text: qsTr("🗓  Add to Calendar"); onClicked: Qt.openUrlExternally(appWindow.portalSession.calendarInvite(appWindow.shareMeetingId)) }
                        Item { Layout.fillWidth: true }
                        LinkAction {
                            visible: (appWindow.portalSession.meetingLinks.detailsUrl || appWindow.sharePortalUrl) !== ""
                            text: qsTr("↗  Portal")
                            onClicked: Qt.openUrlExternally(appWindow.portalSession.meetingLinks.detailsUrl || appWindow.sharePortalUrl)
                        }
                    }
                    SelectableText { visible: appWindow.shareCopyStatus !== ""; text: appWindow.shareCopyStatus; color: appWindow.accent; Accessible.role: Accessible.StaticText }
                    Item { Layout.preferredHeight: 20 }
                }
            }
            // Calendar meeting details
            ScrollView {
                id: calendarDetailsScroll
                objectName: "calendarMeetingDetails"
                clip: true
                ColumnLayout {
                    width: Math.min(calendarDetailsScroll.availableWidth - 56, 720)
                    x: Math.max(28, (calendarDetailsScroll.availableWidth - width) / 2)
                    spacing: 14
                    Item { Layout.preferredHeight: 12 }
                    Button { text: qsTr("‹ Overview"); onClicked: appWindow.section = Main.View.Overview }
                    SelectableText { text: appWindow.calendarDetail.title || ""; font.pixelSize: 22; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: TextEdit.Wrap }
                    SelectableText {
                        text: {
                            if (!appWindow.calendarDetail.startMs) return ""
                            const start = new Date(appWindow.calendarDetail.startMs)
                            const end = new Date(appWindow.calendarDetail.endMs)
                            const sameDay = Qt.formatDate(start, "yyyy-MM-dd") === Qt.formatDate(end, "yyyy-MM-dd")
                            if (appWindow.calendarDetail.allDay)
                                return qsTr("All day") + " · " + Qt.formatDate(start, "dddd, d MMMM yyyy")
                            return Qt.formatDateTime(start, "dddd, d MMMM yyyy · HH:mm") + " – " +
                                (sameDay ? Qt.formatTime(end, "HH:mm") : Qt.formatDateTime(end, "ddd, d MMM · HH:mm"))
                        }
                        color: appWindow.muted
                    }
                    SelectableText { visible: !!appWindow.calendarDetail.calendarName; text: qsTr("Calendar") + " · " + (appWindow.calendarDetail.calendarName || ""); color: appWindow.muted }
                    SelectableText { visible: appWindow.calendarDetail.needsResponse === true; text: qsTr("Invitation"); color: appWindow.accent; font.weight: Font.DemiBold }
                    ExpandableDescription { description: appWindow.calendarDetail.description || "" }
                    SelectableText { text: qsTr("Meeting link"); color: appWindow.ink; font.weight: Font.Medium; Layout.topMargin: 8 }
                    RowLayout {
                        Layout.fillWidth: true
                        SelectableLink { id: calendarMeetingLink; address: appWindow.calendarDetail.url || ""; meetingTitle: appWindow.calendarDetail.title || ""; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                        ToolButton { text: "⧉"; Accessible.name: qsTr("Copy meeting link"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.clipboardService.copy(calendarMeetingLink.address) }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ToolButton { text: "▶"; Accessible.name: qsTr("Join"); ToolTip.visible: hovered; ToolTip.text: Accessible.name; onClicked: appWindow.openMeetingLink(calendarMeetingLink.address, appWindow.calendarDetail.title) }
                        Item { Layout.fillWidth: true }
                        LinkAction { text: qsTr("↗ Browser"); onClicked: Qt.openUrlExternally(calendarMeetingLink.address) }
                        LinkAction { text: qsTr("↗  Calendar"); onClicked: appWindow.calendarService.openCalendar() }
                    }
                    Item { Layout.preferredHeight: 20 }
                }
            }
        }
    }
}
