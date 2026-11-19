#!/usr/bin/env python3
"""Check the packaged app through the same CLI used by release qualification."""

import plistlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


root = Path(__file__).resolve().parent.parent
bundle = Path(sys.argv[1]).resolve()
checker = root / "tools/check_macos_bundle.py"
with tempfile.TemporaryDirectory(prefix="viamiora-package-test-") as directory:
    broken = Path(directory) / "Viamiora.app"
    shutil.copytree(bundle, broken, symlinks=True)
    notices = broken / "Contents/Resources/qt-sbom"
    qtbase = next(notices.glob("qtbase-*.source.spdx.gz"))
    qtbase.write_bytes(qtbase.read_bytes()[:16])
    result = subprocess.run([sys.executable, str(checker), str(broken)],
                            capture_output=True, text=True, check=False)
    if result.returncode == 0 or "invalid Qt source attribution: qtbase" not in result.stderr:
        raise SystemExit(f"Corrupt Qt notice was accepted: {result.stderr}")
    info_path = broken / "Contents/Info.plist"
    with info_path.open("rb") as stream:
        info = plistlib.load(stream)
    info["CFBundleIdentifier"] = "example.invalid.other"
    with info_path.open("wb") as stream:
        plistlib.dump(info, stream)
    result = subprocess.run([sys.executable, str(checker), str(broken)],
                            capture_output=True, text=True, check=False)
    if result.returncode == 0 or "bundle identifier is not dev.yunabraska.viamiora" not in result.stderr:
        raise SystemExit(f"Wrong app identity was accepted: {result.stderr}")
    fake_qt = Path(directory) / "wrong-qt/bin"
    fake_qt.mkdir(parents=True)
    qtpaths = fake_qt / "qtpaths"
    qtpaths.write_text("#!/bin/sh\nprintf '6.12.99\\n'\n")
    qtpaths.chmod(0o755)
    result = subprocess.run([
        sys.executable, str(root / "tools/package_macos.py"),
        "--app", str(bundle), "--qt", str(fake_qt.parent),
        "--output", str(Path(directory) / "wrong-qt.app"), "--local-test",
    ], capture_output=True, text=True, check=False)
    if result.returncode == 0 or "different Qt version" not in result.stderr:
        raise SystemExit(f"Mismatched Qt build was accepted: {result.stderr}")
print("Corrupt attribution, wrong app identity, and mismatched Qt rejected.")
