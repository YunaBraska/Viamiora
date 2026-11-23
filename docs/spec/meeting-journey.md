# Meeting journey

Viamiora is currently a local macOS app for HTTPS Mividas meeting portals. The portal address is chosen by the user. The [platform plan](platform-delivery.md) covers other operating systems and releases.

## Sign in

| Decision | Current behavior |
| --- | --- |
| Portal address | The user enters an HTTPS URL. Viamiora never guesses a host from an email address and sends credentials to it. |
| Authentication | The portal's CSRF-protected sign-in form starts an app-owned session. Its public API does not provide a browser-session login endpoint. Other login flows have a system-browser fallback. |
| Saved accounts | Each portal URL and username pair has a separate macOS Keychain item. Saved-password access runs off the UI thread without opening a Keychain prompt. Cookies stay in memory and are shared with the embedded meeting view only for the signed-in origin. |
| TLS | Qt keeps certificate and hostname verification enabled. On macOS, the app adds the system CA bundle to Qt's trust configuration. An untrusted certificate blocks sign-in. |

## Screens

| Screen | Current behavior |
| --- | --- |
| Overview | One chronological list combines portal meetings and recognized Calendar calls. Search and a day filter narrow the list. “Scheduled now” means the booked time has started, not that someone has joined. |
| Meeting details | The main content area shows time, description, invitees, role-specific links and PINs, copy and join actions, and calendar export. Missing portal data has a browser fallback. |
| New meeting | A single page has title, date, time, busy-time feedback, generated PINs, and removable participant and moderator addresses. The default is the next half-hour boundary for one hour. Calendar access can suggest a free daytime hour. |
| Rooms | Named HTTPS room links are saved independently of the current portal. URLs containing credentials are rejected. |
| Settings and About | The active account, portal URL, account switching, Calendar and Contacts access, appearance, version, and external dependencies are available without mixing them into scheduling. About is also available before sign-in. |

Contacts suggestions come from macOS Contacts after permission or from at most 500 manually entered addresses retained for at most one year. System contacts are never copied into that cache.

## Meeting data

- Portal lists and details come from `GET /api/v1/meeting/upcoming/`, dated `GET /api/v1/meeting/`, and `GET /api/v1/meeting/{id}/`. PINs are kept as strings to preserve leading zeroes. The app reads details when opened instead of saving a second PIN copy.
- EventKit events appear in the overview only when their URL, location, or notes contain a recognized meeting link. Other events still count as busy time. Calendar details are read-only. [EventKit participant status cannot send an RSVP](https://developer.apple.com/documentation/eventkit/ekparticipant/participantstatus), so Viamiora shows no fake Accept or Decline action.
- Portal and Calendar records merge only when a link or room identity and time match. An ambiguous match stays as two records. The person symbol means the portal confirms edit rights. It does not prove who created the meeting.
- Apple Calendar does not expose its visible-calendar selection through EventKit. The macOS build reads the undocumented `DisabledCalendars` preference to hide unchecked calendars in the overview. This still needs a live comparison against Apple Calendar's sidebar. Hidden events continue to block free-time suggestions.

| Passive request | Limit |
| --- | --- |
| Upcoming portal meetings | At least 5–7 minutes while active, 15 minutes while inactive, with longer waits after errors and `Retry-After`. |
| Earlier portal weeks | Ten-minute in-memory cache, bounded to eight weeks. |
| Calendar events | Normally 8–12 minutes between reads; visible-calendar selection checked after 4–6 minutes. |
| Branding | Repeated requests for the same origin coalesced for 10–12 minutes. |

User actions may request current data immediately. Unchanged lists keep their visible rows rather than rebuilding the view.

## Booking and invitations

The app discovers meeting types from the signed-in portal and submits its same-origin booking form. A redirect to the invitation step confirms creation. If the result is ambiguous, Viamiora checks for one matching new meeting and asks the user to inspect the portal before retrying. It does not blindly repeat a request that might create a duplicate.

The documented REST booking endpoint is **not** the current creation path. One live synthetic booking through `POST /api/v1/meeting/book/` returned HTTP 200 but appeared in the portal as “Via Outlook”. It was removed through the API with HTTP 204. [Mividas distinguishes portal and external booking sources](https://docs.mividas.com/?doc=documentation/mividas-meeting-portal/backend-admin/meetings). This label does not mean Viamiora contacted Outlook. The local fake reproduces the observed status codes and label with synthetic data. The complete live bodies were not retained, so it is not an exact API transcript.

At booking, selected addresses use the portal's role-specific invitation forms. Addresses are deduplicated, with moderator selection taking priority. A redirect confirms submission, **not email delivery**. The observed Safespring form fills its email subject with JavaScript; the native HTTP request cannot read that configured value. Adding people after booking therefore opens that meeting in the portal. A failed booking invitation remains visible in meeting details with the same fallback. Invite removal uses the portal's confirmed same-origin form, then reloads meeting details to verify that the invite ID disappeared. The local HTTPS fake covers success and changed forms; live removal has not been exercised. Rescheduling remains in the portal until its request and notification behavior are verified. Guest-link derivation is allowed only when the portal's room identity and moderator PIN agree with its detail API; otherwise the app shows the portal link.

## Branding and video

Viamiora uses a valid same-origin logo and suitable accent from the video branding manifest when available. It can also read the portal's public login logo. Otherwise it uses local defaults. Remote portal CSS is not imported into the native UI.

Video runs in an embedded WebView with a system-browser fallback. A local macOS call reached camera preview and the guest waiting room. Microphone audio, host controls, and media continuity while the view is hidden remain unverified. Each other operating system needs its own installed-app media test.

## Verification boundary

The [local HTTPS journey](../../tests/PortalSessionTest.cpp) covers certificate rejection and trust, CSRF, wrong passwords, expired sessions, ambiguous bookings, invitation forms, duplicate recipients, malformed API data, rate limits, sign-out, and repeat sign-in. It does not prove compatibility with every portal deployment or a future HTML change. The [release gates](platform-delivery.md) track package, platform, translation, and media work before production claims.
