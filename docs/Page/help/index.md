---
layout: default
help: true
title: Viamiora help
description: Use a Mividas meeting portal from a focused macOS workspace.
---

# Viamiora help

Viamiora is a native workspace for an HTTPS Mividas meeting portal. It keeps portal meetings and recognised calendar calls together without replacing the portal's authority.

The images below are illustrative previews with fictional data. Available actions and branding depend on the portal you connect.

## Add a portal

<a id="add-a-portal"></a>

On the sign-in screen, enter your username, password and the full HTTPS address of your Mividas portal. Viamiora does not guess a portal from an email address. A saved portal and username pair uses its own macOS Keychain entry.

If the portal uses a sign-in method Viamiora cannot safely complete, use the browser option. Do not bypass a certificate warning. Viamiora blocks an untrusted certificate instead of sending your credentials.

## Find a meeting

<a id="find-a-meeting"></a>

The overview shows upcoming portal meetings and calendar entries that contain a recognised meeting link. Use the day filter and search to narrow the list. Viamiora keeps ambiguous portal and calendar entries separate rather than merging uncertain data.

Calendar access is optional. When you allow it, Viamiora can show recognised calls and use busy times for booking suggestions. It never changes a calendar event.

<figure class="doc-shot">
  <a href="{{ '/assets/screenshots/overview.png' | relative_url }}" aria-label="View the meeting overview preview at full size"><img src="{{ '/assets/screenshots/overview.png' | relative_url }}" width="1200" height="840" loading="lazy" alt="Example overview with a week selector, meeting search, an active call and upcoming meetings"></a>
  <figcaption>Choose a day or search across recognised calls.</figcaption>
</figure>

## Book a meeting

<a id="book-a-meeting"></a>

Open **New meeting**, choose the meeting type, title, date, time and participants. Viamiora can suggest a free daytime hour when Calendar access is available. It submits the portal's own booking form and checks the resulting portal state before treating the booking as complete.

If the result is ambiguous, inspect the portal before trying again. Viamiora deliberately does not repeat a request that might create a duplicate meeting.

<figure class="doc-shot">
  <a href="{{ '/assets/screenshots/schedule.png' | relative_url }}" aria-label="View the meeting planner preview at full size"><img src="{{ '/assets/screenshots/schedule.png' | relative_url }}" width="1200" height="840" loading="lazy" alt="Example meeting planner with a date picker, times, duration, calendar availability, two PINs and invitee fields"></a>
  <figcaption>Set the time and invitees together before creating a meeting.</figcaption>
</figure>

## Contacts and invitations

Viamiora can suggest email addresses from macOS Contacts while you add participants. Contacts remain in the system store. Manually entered addresses are kept locally only to make repeat invitations easier. You can remove saved portal accounts and their local settings at any time.

Meeting details show participant and moderator links and PINs when the portal provides them. They also show the invitation lists and let you change them when the portal allows it.

<figure class="doc-shot">
  <a href="{{ '/assets/screenshots/details.png' | relative_url }}" aria-label="View the meeting details preview at full size"><img src="{{ '/assets/screenshots/details.png' | relative_url }}" width="1200" height="840" loading="lazy" alt="Example meeting details with separate participant and moderator links, PINs, and invitation lists"></a>
  <figcaption>Keep the right joining link and invitation list at hand.</figcaption>
</figure>

## Join a call

<a id="join-a-call"></a>

Open a meeting, then choose **Join**. Viamiora uses an embedded meeting view where the portal supports it. The browser fallback remains available for portal-specific flows, unavailable media or a failed embedded view.

Camera and microphone access are requested by macOS only when the call needs them. Viamiora does not record meeting media.
