.pragma library

function conferenceIdentity(address) {
    const match = String(address || "").match(/^https:\/\/([^/?#]+)\/webapp\/[^#]*#.*[?&]conference=([^&#]+)/i)
    if (!match) return ""
    try { return match[1].toLowerCase() + "/" + decodeURIComponent(match[2]).toLowerCase() }
    catch (error) { return "" }
}

function mergePortalMeetings(upcoming, historical) {
    const known = new Set(upcoming.map(item => item.id))
    return upcoming.concat(historical.filter(item => !known.has(item.id)))
}

function combineMeetings(portalMeetings, calendarEvents, portalUrl) {
    const portal = portalMeetings.map(item => Object.assign({ fromCalendar: false }, item))
    const origin = String(portalUrl || "").replace(/\/$/, "")
    for (const entry of calendarEvents) {
        const link = String(entry.url)
        const identity = conferenceIdentity(link)
        const duplicate = portalMeetings.some(item => {
                const sameTime = Math.abs(new Date(item.start).getTime() - entry.startMs) < 60000 &&
                                 Math.abs(new Date(item.end).getTime() - entry.endMs) < 60000
                if (!sameTime) return false
                if (item.url && link === origin + item.url) return true
                const known = conferenceIdentity(item.fullUrl) || conferenceIdentity(item.identityUrl)
                if (identity !== "" && known !== "" && identity === known) return true
                if (identity === "" || !item.roomName) return false
                const portalHost = origin.match(/^https:\/\/([^/:]+)/i)
                const videoHost = link.match(/^https:\/\/([^/:]+)/i)
                if (!portalHost || !videoHost) return false
                const labels = portalHost[1].split(".")
                if (labels.length < 3) return false
                const parentHost = labels.slice(1).join(".")
                const room = identity.split("/")[1].split("@")[0]
                return parentHost !== "" && videoHost[1].toLowerCase() === parentHost.toLowerCase() &&
                       room === String(item.roomName).toLowerCase().split("@")[0]
        })
        if (!duplicate) portal.push(Object.assign({ fromCalendar: true }, entry))
    }
    portal.sort((left, right) => {
        const delta = new Date(left.fromCalendar ? left.startMs : left.start).getTime() -
                      new Date(right.fromCalendar ? right.startMs : right.start).getTime()
        if (delta !== 0) return delta
        if (left.fromCalendar !== right.fromCalendar) return left.fromCalendar ? 1 : -1
        return String(left.title).localeCompare(String(right.title))
    })
    return portal
}

function startMs(item) {
    return new Date(item.fromCalendar ? item.startMs : item.start).getTime()
}

function endMs(item) {
    const start = startMs(item)
    const end = new Date(item.fromCalendar ? item.endMs : item.end).getTime()
    return Number.isFinite(end) && end > start ? end : start + 1
}

function overlapsDay(item, isoDay) {
    const start = new Date(isoDay + "T00:00:00")
    if (!Number.isFinite(start.getTime())) return false
    const next = new Date(start.getTime())
    next.setDate(next.getDate() + 1)
    return startMs(item) < next.getTime() && endMs(item) > start.getTime()
}

function meetingsOnDay(meetings, isoDay) {
    return meetings.filter(item => overlapsDay(item, isoDay)).length
}

function groupMeetings(meetings, nowMs, isoDay, firstDay = "", lastDay = "", limitToWeek = false) {
    const groups = { ongoing: [], upcoming: [], earlier: [] }
    for (const item of meetings) {
        const start = startMs(item)
        if (!Number.isFinite(start) || (isoDay !== "" && !overlapsDay(item, isoDay))) continue
        const end = endMs(item)
        const inWeek = firstDay !== "" && lastDay !== "" &&
            (overlapsDay(item, firstDay) || overlapsDay(item, lastDay) ||
             (start >= new Date(firstDay + "T00:00:00").getTime() &&
              start < new Date(lastDay + "T23:59:59.999").getTime()))
        if (isoDay === "" && limitToWeek && !inWeek) continue
        if (start <= nowMs && end > nowMs) groups.ongoing.push(item)
        else if (start > nowMs) groups.upcoming.push(item)
        else if (isoDay !== "" || inWeek) groups.earlier.push(item)
    }
    return groups
}
