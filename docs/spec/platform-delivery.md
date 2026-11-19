# Platform delivery

**Current support: local macOS build and package smoke test.** CMake rejects iOS, iPadOS, Windows, Android, and Linux until their native adapters exist. The Qt 6.12.1 macOS kit supports macOS 14.4 and later. No Developer ID signed, notarized distribution or automatic updater has been qualified.

## One journey, adaptable layout

The [meeting journey](meeting-journey.md) owns the user flow. Keep one portal session, meeting model, and set of actions in C++ and Qt Quick. The root QML view receives typed required properties at creation so lint can check service usage. Layout follows window width and input method. Compact windows use bottom navigation and keep actions above the keyboard and safe area; wide windows use a sidebar. An active meeting remains reachable without reloading its WebView. A narrow iPad window also needs the compact layout.

Before claiming a layout, inspect empty, populated, invalid, and active-call states on that platform. Include long translations, touch targets, keyboard and screen-reader order, large text, dark mode, rotation, and resize. A narrow Mac window is not proof of phone behavior. This follows the intent of [Apple](https://developer.apple.com/design/human-interface-guidelines/sidebars), [Android](https://developer.android.com/design/ui/mobile/guides/layout-and-content/layout-and-nav-patterns), and [Windows](https://learn.microsoft.com/en-us/windows/apps/design/controls/navigationview) navigation guidance.

## Installed-platform gates

| Platform | Native integration and proof required |
| --- | --- |
| macOS | Keep the current Keychain, EventKit, Contacts, and menu bar behavior. Verify Apple Calendar visibility against its sidebar and test permissions, media, join, return, and browser fallback in a signed installed app. |
| iOS and iPadOS | Add native Keychain, EventKit, Contacts, permissions, and a compact layout. Test layout in a simulator and camera, microphone, audio routing, WebView, interruption, and restart on physical devices. |
| Windows | Add OS-protected credentials and real calendar/contact providers before showing their controls. Test a clean VM package, WebView2, devices, links, install, update, and uninstall. |
| Android | Add Keystore-backed credentials and working Calendar/Contacts adapters. Test layout in an emulator and WebView media, permissions, audio, and lifecycle on physical devices. |
| Linux | Add a desktop secret store and supported calendar/contact path. Test package dependencies, WebEngine media, permissions, and updates on a clean desktop. |

[Qt WebView uses different backends by platform](https://doc.qt.io/qt-6/qtwebview-index.html). A passing portal API test does not qualify video on another OS.

## Identity, dependencies, and updates

- Keep the Viamiora bundle, Keychain service, and settings identity stable. The earlier local name was migrated once; there is no runtime legacy path. A new bundle identity may require new macOS privacy grants.
- Display versions as `year.month.day`, such as `2026.11.19`. Distribution builds also need a monotonic platform build number. [Apple requires a numeric bundle build version](https://developer.apple.com/documentation/BundleResources/Information-Property-List/CFBundleVersion).
- About must list **only external dependencies actually shipped**, with exact version, license, and link, derived from the resolved build. Include their license texts in the package. OS frameworks are excluded. The current local About lists Qt and no updater.
- The direct macOS package uses dynamically linked Qt and allows compatible user-rebuilt Qt libraries to load. This is required for the LGPL path and removes macOS library validation for the app process. A release test must check that a substituted library can run without the publisher's signing key.

| Channel | Update mechanism to qualify |
| --- | --- |
| Direct macOS | [Sparkle 2](https://sparkle-project.org/documentation/) is a candidate after Developer ID signing, notarization, a signed HTTPS feed, and installed update and rollback tests. |
| Apple stores | Store-managed updates. Test signed packages and device media before submission. Store builds must not contain a direct-download updater. |
| Windows | Store-managed updates or a signed MSIX with a tested [App Installer feed](https://learn.microsoft.com/en-us/windows/msix/app-installer/auto-update-and-repair--overview). |
| Android and Linux | Play-managed Android updates or a separately tested, signed direct distribution channel. Choose the channel before adding an updater. |

The [Podlord release pattern](https://github.com/YunaBraska/podlord/blob/main/docs/release-channels.md) separates source qualification, direct downloads, and store submission. Viamiora has source qualification for Apple Silicon and Intel, plus a manual workflow that builds a universal app, signs it with Developer ID, notarizes it, and uploads a private candidate artifact. These workflows have not run on GitHub. The candidate workflow does not publish a release.

The candidate workflow needs repository secrets `MACOS_SIGNING_CERTIFICATE`, `MACOS_SIGNING_CERTIFICATE_PASSWORD`, `MACOS_NOTARY_KEY`, `MACOS_NOTARY_KEY_ID`, and `MACOS_NOTARY_ISSUER_ID`. It refuses to create a distribution artifact without them. No credential values belong in the repository.

## Release blockers

| Gate | Current gap |
| --- | --- |
| macOS package and OSS notices | A local universal package with Qt 6.12.1 passes dependency, architecture, notice, and local signature checks at macOS 14.4. The 22 required Qt frameworks and source attribution are bundled. The Qt archive currently publishes SHA-1 checksums, so the CI installer verifies those checksums through `tools/aqt-sha1.ini`. Qt schedules the final 6.12.1 release for 23 November 2026. The new package has not been opened locally to avoid another Keychain prompt. Developer ID signing, notarization, and a clean-Mac installation remain untested. |
| Portal compatibility and terms | Booking and invitations depend on observed HTML forms. The local HTTPS fake covers changed fields, not every installation. Hosted CI uses Qt/OpenSSL for that test because the macOS 15 runners reject its temporary CA with SecureTransport. The shipped macOS trust path needs an installed-app check. This tenant's API page mentions an additional production license whose scope is unconfirmed. Verify supported portal variants and clarify terms before distribution. |
| Product completion | English and German have complete current catalogs. Swedish UI labels are translated, while 85 new status messages use English fallback until reviewed. The other requested languages still need reviewed translations. Invitation add/remove has local HTTPS tests; live removal and rescheduling compatibility are unverified. Qualify these before claiming the requested product scope. |
| Other platforms and media | Native adapters and responsive layouts are missing. The macOS video check reached camera preview and the waiting room, but microphone audio and other platforms need installed-app tests. |
| Release automation | Qualification and signed-candidate workflows are authored but have no hosted run yet. An update feed, installed upgrade, rollback, and publication jobs do not exist. Keep releases separate from pull-request checks. |

Syft identifies the app bundle but not its embedded Qt frameworks as packages, so a zero-finding Grype scan is not evidence that Qt is vulnerability-free. Check Qt's advisories against the exact bundled version before distribution.

The source and project artwork are under Apache-2.0. Qt and any future updater retain their own licenses. Do not claim production or public-binary readiness until the relevant gates pass.
