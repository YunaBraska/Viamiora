#!/usr/bin/env python3
"""Start a packaged app through LaunchServices and verify its QML imports."""

import argparse
import subprocess
import tempfile
import time
from pathlib import Path


def process_id(executable: Path) -> int | None:
    result = subprocess.run(["ps", "-axo", "pid=,command="], check=True,
                            text=True, capture_output=True)
    for line in result.stdout.splitlines():
        columns = line.strip().split(maxsplit=1)
        if len(columns) == 2 and columns[1] == str(executable):
            return int(columns[0])
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args()
    bundle = args.bundle.resolve()
    executable = bundle / "Contents/MacOS/viamiora"
    if not executable.is_file():
        parser.error("bundle has no executable")
    if process_id(executable):
        parser.error("this bundle is already running")
    with tempfile.TemporaryDirectory(prefix="viamiora-smoke-") as directory:
        log = Path(directory) / "app.stderr"
        subprocess.run(["open", "-n", "-j", "-g", "-a", str(bundle),
                        "--stderr", str(log), "--env", "QML_IMPORT_TRACE=1"], check=True)
        pid = None
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                pid = process_id(executable)
                output = log.read_text(errors="replace") if log.exists() else ""
                if pid and "Viamiora/Main.qml" in output and "QtQuick/Controls/Basic/qmldir" in output:
                    if any(term in output for term in ("Library not loaded", "module is not installed",
                                                       "plugin cannot be loaded")):
                        raise RuntimeError("packaged QML or Qt plugin failed to load")
                    print("Packaged Viamiora opened with its bundled Basic controls.")
                    return 0
                if not pid and output:
                    raise RuntimeError("packaged app exited: " + output[-1000:])
                time.sleep(0.25)
            raise RuntimeError("packaged app did not reach the QML view: " + output[-1000:])
        finally:
            if pid and process_id(executable) == pid:
                subprocess.run(["kill", "-TERM", str(pid)], check=False)


if __name__ == "__main__":
    raise SystemExit(main())
