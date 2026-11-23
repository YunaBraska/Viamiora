#!/usr/bin/env python3
"""Synthetic HTTPS portal for the public session journey test."""

import json
from html import escape
import re
import ssl
import sys
import time
from datetime import datetime, timedelta
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def send(self, status, body="", content_type="text/html", headers=()):
        encoded = body.encode()
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(encoded)))
        for key, value in headers:
            self.send_header(key, value)
        self.end_headers()
        try:
            self.wfile.write(encoded)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def authenticated(self):
        return "session=local-test" in self.headers.get("Cookie", "")

    def valid_headers(self):
        return (self.headers.get("User-Agent", "").startswith(f"Viamiora/{self.server.version} ")
                and self.headers.get("Accept-Language", "").startswith("de")
                and bool(self.headers.get("Accept", "")))

    def do_GET(self):
        if not self.valid_headers():
            self.send(400, "Invalid request headers")
            return
        url = urlparse(self.path)
        path = url.path
        if path == "/accounts/login/":
            count_path = self.server.log_path.with_name("branding-login-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            if self.server.log_path.with_name("missing-login-csrf").exists():
                self.send(200, '<form><input name="username"></form>')
                return
            self.send(200, '<div class="login-logo"><img src="/site_media/brand.svg"></div>'
                      '<form><input name="csrfmiddlewaretoken" value="test-csrf"></form>',
                      headers=(("Set-Cookie", "csrftoken=test-csrf; Path=/; Secure"),))
            return
        if path == "/webapp/branding/manifest.json":
            self.send(200, json.dumps({"meta": {"name": "CUSTOM", "baseColor": "#113355"},
                "brandName": "Local portal", "images": {"logo": "/webapp/manifest-logo.png"}}),
                "application/json")
            return
        if path == "/accounts/logout/":
            self.send(200, "Signed out")
            return
        if not self.authenticated():
            self.send(403, "Sign in")
            return
        if path == "/":
            outlook_label = '<span>Via Outlook</span>' if self.server.rest_meeting else ''
            self.send(200, '<a href="/book/?wizard=1&amp;meeting_type=1">Meeting</a>' + outlook_label)
        elif path == "/api/v1/meeting/upcoming/":
            count_path = self.server.log_path.with_name("upcoming-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            expire_once = self.server.log_path.with_name("expire-upcoming-once")
            if expire_once.exists():
                expire_once.unlink()
                self.send(403, "Session expired")
                return
            if self.server.log_path.with_name("rate-limit-upcoming").exists():
                self.send(429, "Slow down", headers=(("Retry-After", "600"),))
                return
            if self.server.log_path.with_name("malformed-upcoming-json").exists():
                self.send(200, '{"results":null}', "application/json")
                return
            fail_once = self.server.log_path.with_name("fail-next-refresh")
            if fail_once.exists():
                fail_once.unlink()
                self.send(503, "Temporarily unavailable")
                return
            meetings = [dict(meeting) for meeting in self.server.meetings]
            if meetings and self.server.log_path.with_name("calendar-title-newline").exists():
                meetings[0]["title"] = "Review\r\nATTENDEE:mailto:unexpected@example.invalid"
            if self.server.log_path.with_name("include-unresolvable-meeting").exists():
                meetings.append({"id": 322, "title": "Other meeting",
                    "ts_start": self.server.start, "ts_stop": self.server.end,
                    "url": "/meeting/322/"})
            self.send(200, json.dumps({"results": meetings}), "application/json")
        elif path == "/api/v1/meeting/":
            params = parse_qs(url.query)
            first = datetime.fromisoformat(params.get("start", [""])[0].replace("Z", "+00:00"))
            last = datetime.fromisoformat(params.get("stop", [""])[0].replace("Z", "+00:00"))
            if params.get("limit") != ["100"] or last <= first:
                self.send(400, "Invalid history range")
                return
            count_path = self.server.log_path.with_name("history-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            if self.server.log_path.with_name("rate-limit-history").exists():
                self.send(429, "Slow down", headers=(("Retry-After", "600"),))
                return
            start = first + timedelta(days=2, hours=10)
            self.send(200, json.dumps({"count": 1, "next": None, "previous": None,
                "results": [{"id": 320, "title": "Past call",
                    "ts_start": start.isoformat(), "ts_stop": (start + timedelta(hours=1)).isoformat(),
                    "url": "/meeting/320/"}]}), "application/json")
        elif path == "/book/":
            if self.server.log_path.with_name("broken-booking-form").exists():
                self.send(200, '<form action="/book/?wizard=1&amp;wizard_current=1&amp;wizard_total=3">'
                          '<input name="renamed-pin" value="5678"></form>')
            else:
                self.send(200, '<form action="/book/?wizard=1&amp;wizard_current=1&amp;wizard_total=3">'
                          '<input name="initial-pin" value="5678"><input name="lobby_pin"></form>')
        elif path == "/meeting/321/invite/":
            if "wizard_current" in url.query:
                self.send(200, "Next step")
            elif self.server.log_path.with_name("client-side-invitation-subject").exists():
                self.send(200, '<form><input name="message_title" required>'
                          '<div data-name="message" data-value="&lt;p&gt;Join&lt;/p&gt;"></div></form>')
            else:
                self.send(200, '<form><input name="message_title" value="Invitation">'
                          '<div data-name="message" data-value="&lt;p&gt;Join&lt;/p&gt;"></div></form>')
        elif path in ("/meeting/321/invite/moderator/", "/meeting/323/invite/moderator/"):
            if self.server.log_path.with_name("client-side-invitation-subject").exists():
                self.send(200, '<form><input name="message_title" required>'
                          '<div data-name="message" data-value="&lt;p&gt;Join&lt;/p&gt;"></div></form>')
            else:
                self.send(200, '<form><input name="message_title" value="Moderator invitation">'
                          '<div data-name="message" data-value="&lt;p&gt;Join&lt;/p&gt;"></div></form>')
        elif re.fullmatch(r"/meeting/321/invite/\d+/remove/", path):
            invite_id = int(path.split("/")[4])
            invite = next((item for item in self.server.detail_invites
                           if item["id"] == invite_id), None)
            if invite is None:
                self.send(404)
            elif self.server.log_path.with_name("broken-removal-form").exists():
                self.send(200, '<form><input name="other" value="changed"></form>')
            elif self.server.log_path.with_name("cross-origin-removal-form").exists():
                self.send(200, '<form action="https://other.invalid/remove/" method="post">'
                          '<input name="csrfmiddlewaretoken" value="test-csrf">'
                          '<input name="confirm" value="remove-now">'
                          f'<strong>{escape(invite["email"])}</strong></form>')
            else:
                self.send(200, f'<form action="{path}?popup=1" method="post">'
                          '<input type="hidden" name="csrfmiddlewaretoken" value="test-csrf">'
                          '<input type="hidden" name="confirm" value="remove-now">'
                          f'<strong>{escape(invite["email"])}</strong>'
                          '<button type="submit">Remove</button></form>')
        elif path == "/meeting/321/":
            self.send(200, "Meeting details")
        elif path == "/api/v1/meeting/321/":
            slow_detail = self.server.log_path.with_name("slow-detail").exists()
            if slow_detail:
                self.server.log_path.with_name("detail-started").touch()
                time.sleep(0.25)
            if self.server.log_path.with_name("malformed-detail-json").exists():
                self.send(200, '{"meeting":{"id":999}}', "application/json")
                return
            host = f"https://127.0.0.1:{self.server.server_port}"
            invites = ("unavailable" if self.server.log_path.with_name("unsupported-invites").exists()
                       else self.server.detail_invites)
            portal_guest_link = self.server.log_path.with_name("portal-guest-link").exists()
            wrong_moderator_pin = self.server.log_path.with_name("wrong-moderator-pin").exists()
            self.send(200, json.dumps({"meeting": {"id": 321,
                "pin": "0047", "lobby_pin": "0782",
                "description": "Agenda\nReview the next release.",
                "full_url": host + ("/meeting/321/" if portal_guest_link else "/webapp/#/?conference=guest"),
                "moderator_url": host + (("/webapp/#/?conference=room&pin=" + ("9999" if wrong_moderator_pin else "0782")) if portal_guest_link
                                          else "/webapp/#/?conference=moderator&pin=0782"),
                "room_name": "555123", "sip": "sip:555123@example.invalid",
                "invites": invites, "can_change": True,
                "url": ("/meeting/999/" if self.server.log_path.with_name("wrong-detail-url").exists()
                        else "" if portal_guest_link else "/meeting/321/")}}), "application/json")
            if slow_detail:
                self.server.log_path.with_name("detail-finished").touch()
        elif path == "/api/v1/meeting/322/":
            count_path = self.server.log_path.with_name("detail-322-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            self.send(404)
        elif path == "/api/v1/meeting/320/":
            host = f"https://127.0.0.1:{self.server.server_port}"
            self.send(200, json.dumps({"meeting": {"id": 320,
                "description": "Earlier agenda", "pin": "0047", "lobby_pin": "0782",
                "full_url": host + "/webapp/#/?conference=past&pin=0047",
                "moderator_url": host + "/webapp/#/?conference=past&pin=0782",
                "invites": [], "can_change": False, "url": "/meeting/320/"}}),
                "application/json")
        else:
            self.send(404)

    def do_POST(self):
        if not self.valid_headers():
            self.send(400, "Invalid request headers")
            return
        url = urlparse(self.path)
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode()
        form = parse_qs(body, keep_blank_values=True)
        if url.path == "/accounts/login/":
            count_path = self.server.log_path.with_name("login-post-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            origin = f"https://127.0.0.1:{self.server.server_port}"
            if (form.get("csrfmiddlewaretoken") != ["test-csrf"]
                    or "csrftoken=test-csrf" not in self.headers.get("Cookie", "")
                    or self.headers.get("Origin") != origin
                    or self.headers.get("Referer") != origin + "/accounts/login/?next=/"):
                self.send(403, "Invalid sign-in request")
                return
            if form.get("username") == ["creator@example.invalid"] and form.get("password") == ["local-password"]:
                self.send(302, headers=(("Location", "/"),
                                        ("Set-Cookie", "session=local-test; Path=/; Secure")))
            else:
                self.send(200, "Bad login")
            return
        if not self.authenticated():
            self.send(403)
            return
        if url.path == "/api/v1/meeting/book/":
            count_path = self.server.log_path.with_name("rest-book-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            try:
                request = json.loads(body)
            except json.JSONDecodeError:
                self.send(400, '{"detail":"Bad Request"}', "application/json")
                return
            if (not isinstance(request, dict)
                    or request.get("source", "missing") is not None
                    or request.get("send_emails") is not False
                    or request.get("recipients") != []
                    or request.get("moderators") != []
                    or not isinstance(request.get("title"), str)
                    or not request["title"].strip()):
                self.send(400, '{"detail":"Bad Request"}', "application/json")
                return
            self.server.rest_meeting = {"id": 901, "title": request["title"]}
            self.send(200, json.dumps({"status": "ok", "meeting": {
                "id": 901, "title": request["title"], "invites": []}}), "application/json")
            return
        if url.path == "/api/v1/meeting/901/unbook/":
            if self.server.rest_meeting is None or body != "{}":
                self.send(400, '{"detail":"Bad Request"}', "application/json")
                return
            self.server.rest_meeting = None
            self.send(204)
            return
        if url.path == "/book/":
            count_path = self.server.log_path.with_name("book-post-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            if self.server.log_path.with_name("ambiguous-booking-response").exists():
                self.send(200, "Booking result unknown")
                return
            if self.server.log_path.with_name("reject-booking-post").exists():
                self.send(400, "Portal form changed")
                return
            if (form.get("title") != ["Local journey"] or form.get("pin") != ["123456"]
                    or form.get("lobby_pin") != ["654321"] or form.get("use_lobby_pin") != ["on"]):
                self.send(400, "Invalid booking")
                return
            if self.server.log_path.with_name("ambiguous-booking-created").exists():
                self.server.meetings.append({"id": 323, "title": "Local journey",
                    "ts_start": self.server.start, "ts_stop": self.server.end,
                    "url": "/meeting/323/", "room_name": "reconciled"})
                self.send(200, "Booking result unknown")
                return
            self.server.meetings = [{"id": 321, "title": "Local journey",
                "ts_start": self.server.start, "ts_stop": self.server.end,
                "url": "/meeting/321/"}]
            self.send(302, headers=(("Location", "/meeting/321/invite/?wizard=1&wizard_current=2&wizard_total=3"),))
            return
        if url.path in ("/meeting/321/invite/", "/meeting/321/invite/moderator/",
                        "/meeting/323/invite/moderator/"):
            meeting_id = int(url.path.split("/")[2])
            role = "moderator" if url.path.endswith("/moderator/") else "participant"
            self.server.invitations[role] = form.get("emails", [""])[0].split()
            for email in self.server.invitations[role]:
                if not any(item["email"].casefold() == email.casefold()
                           for item in self.server.detail_invites):
                    self.server.detail_invites.append({"id": 19 + len(self.server.detail_invites),
                        "email": email, "is_moderator": role == "moderator"})
            self.server.log_path.write_text(json.dumps(self.server.invitations))
            self.send(302, headers=(("Location", f"/meeting/{meeting_id}/"),))
            return
        if re.fullmatch(r"/meeting/321/invite/\d+/remove/", url.path):
            count_path = self.server.log_path.with_name("remove-post-count")
            previous = int(count_path.read_text()) if count_path.exists() else 0
            count_path.write_text(str(previous + 1))
            if self.server.log_path.with_name("reject-removal-post").exists():
                self.send(503, "Temporarily unavailable")
                return
            invite_id = int(url.path.split("/")[4])
            if (form.get("csrfmiddlewaretoken") != ["test-csrf"]
                    or form.get("confirm") != ["remove-now"]
                    or not any(item["id"] == invite_id for item in self.server.detail_invites)):
                self.send(400, "Invalid removal")
                return
            if not self.server.log_path.with_name("pretend-remove-success").exists():
                self.server.detail_invites = [item for item in self.server.detail_invites
                                              if item["id"] != invite_id]
            if self.server.log_path.with_name("inline-remove-success").exists():
                self.send(200, "Removed")
                return
            self.send(302, headers=(("Location", "/meeting/321/"),))
            return
        self.send(404)


def main():
    certificate, key, log_path, start, end, version = sys.argv[1:]
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.meetings = []
    server.rest_meeting = None
    server.invitations = {}
    server.detail_invites = [
        {"id": 17, "email": "participant@example.invalid", "is_moderator": False},
        {"id": 18, "email": "creator@example.invalid", "is_moderator": True},
        {"id": 19, "email": "GUEST@example.invalid", "is_moderator": True},
    ]
    server.log_path = Path(log_path)
    server.start = start
    server.end = end
    server.version = version
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    print(server.server_port, flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
