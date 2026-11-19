---
layout: default
help: true
title: Viamiora help
description: Use a Mividas meeting portal from a focused macOS workspace.
---

# Viamiora help

Viamiora is a native workspace for an HTTPS Mividas meeting portal. It keeps portal meetings and recognised calendar calls together without replacing the portal's authority.

## Add a portal

<a id="add-a-portal"></a>

Open **Settings** and enter the full HTTPS address of your organisation's Mividas portal. Viamiora does not guess a portal from an email address. Sign in with the account for that portal. A saved portal and username pair uses its own macOS Keychain entry.

If the portal uses a sign-in method Viamiora cannot safely complete, use the browser option. Do not bypass a certificate warning. Viamiora blocks an untrusted certificate instead of sending your credentials.

## Find a meeting

<a id="find-a-meeting"></a>

The overview shows upcoming portal meetings and calendar entries that contain a recognised meeting link. Use the day filter and search to narrow the list. Viamiora keeps ambiguous portal and calendar entries separate rather than merging uncertain data.

Calendar access is optional. When you allow it, Viamiora can show recognised calls and use busy times for booking suggestions. It never changes a calendar event.

## Book a meeting

<a id="book-a-meeting"></a>

Open **New meeting**, choose the meeting type, title, date, time and participants. Viamiora can suggest a free daytime hour when Calendar access is available. It submits the portal's own booking form and checks the resulting portal state before treating the booking as complete.

If the result is ambiguous, inspect the portal before trying again. Viamiora deliberately does not repeat a request that might create a duplicate meeting.

## Join a call

<a id="join-a-call"></a>

Open a meeting, then choose **Join**. Viamiora uses an embedded meeting view where the portal supports it. The browser fallback remains available for portal-specific flows, unavailable media or a failed embedded view.

Camera and microphone access are requested by macOS only when the call needs them. Viamiora does not record meeting media.

## Contacts and invitations

Viamiora can suggest email addresses from macOS Contacts while you add participants. Contacts remain in the system store. Manually entered addresses are kept locally only to make repeat invitations easier. You can remove saved portal accounts and their local settings at any time.
