#!/usr/bin/env python3
"""Render the synthetic, illustrative landing-page views with a local browser."""

import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests" / "showcase.html"
OUTPUT = ROOT / "docs" / "Page" / "assets" / "screenshots"
SIZE = (1200, 840)
VIEWS = ("overview", "schedule", "details")


def main():
    chrome = os.environ.get("CHROME_BIN") or shutil.which("google-chrome")
    if not chrome:
        mac_chrome = Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")
        if mac_chrome.exists():
            chrome = str(mac_chrome)
    if not chrome:
        raise SystemExit("Set CHROME_BIN to a local Chrome or Chromium executable")

    source = SOURCE.read_text(encoding="utf-8")
    for marker in ("Product planning", "Meeting PIN", "Moderator PIN", "example.invalid"):
        if marker not in source:
            raise SystemExit(f"Showcase fixture is missing {marker}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / "assets" / "app-icon.svg", OUTPUT.parent / "app-icon.svg")
    with tempfile.TemporaryDirectory(prefix="viamiora-showcase-") as profile:
        for view in VIEWS:
            target = OUTPUT / f"{view}.png"
            command = [chrome, "--headless=new", "--disable-gpu", "--no-first-run",
                       "--disable-extensions", "--hide-scrollbars",
                       "--force-device-scale-factor=1", f"--user-data-dir={profile}",
                       f"--window-size={SIZE[0]},{SIZE[1]}",
                       f"--screenshot={target}", f"{SOURCE.as_uri()}?view={view}"]
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            if result.returncode:
                raise SystemExit(f"Chrome failed to render {view}: {result.stderr[-1000:]}")
            image = target.read_bytes()
            if image[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", image[16:24]) != SIZE:
                raise SystemExit(f"Unexpected screenshot dimensions for {view}")
            print(target.relative_to(ROOT))


if __name__ == "__main__":
    main()
