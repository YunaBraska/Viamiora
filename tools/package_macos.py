#!/usr/bin/env python3
"""Build and check a self-contained macOS app from an official Qt SDK."""

import argparse
import gzip
import plistlib
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
QT_MODULES = ("qtbase", "qtdeclarative", "qtsvg", "qtwebview")


def run(*args: str) -> str:
    result = subprocess.run(args, check=False, text=True, capture_output=True, timeout=120)
    if result.returncode:
        raise RuntimeError(f"{Path(args[0]).name} failed: {result.stderr.strip() or result.stdout[-2000:]}")
    return result.stdout + result.stderr


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--qt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--identity", default="-", help="Codesign identity; '-' signs a local test package")
    parser.add_argument("--timestamp", action="store_true", help="Request an Apple timestamp for Developer ID signing")
    parser.add_argument("--local-test", action="store_true",
                        help="Permit a locally signed package to load its Qt frameworks")
    args = parser.parse_args()
    source = args.app.resolve()
    qt = args.qt.resolve()
    output = args.output.absolute()
    if not source.is_dir() or source.suffix != ".app" or not (source / "Contents/Info.plist").is_file():
        parser.error("--app must be a built .app bundle")
    if output.exists() or output == source or output.suffix != ".app":
        parser.error("--output must be a new .app path")
    if args.timestamp and args.identity == "-":
        parser.error("timestamping requires a Developer ID signing identity")
    if args.local_test and args.timestamp:
        parser.error("a local test package cannot be timestamped for distribution")
    if not args.local_test and (args.identity == "-" or not args.timestamp):
        parser.error("distribution requires a timestamped Developer ID identity; "
                     "use --local-test for a local smoke test")
    with (source / "Contents/Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    if info.get("CFBundleIdentifier") != "berlin.yuna.viamiora":
        parser.error("the source bundle has an unexpected identifier")
    version = run(str(qt / "bin/qtpaths"), "--qt-version").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        parser.error("cannot read the Qt version")
    linked_qt = re.findall(r"QtCore\.framework/[^\n]*current version (\d+\.\d+\.\d+)",
                           run("otool", "-L", str(source / "Contents/MacOS/viamiora")))
    if not linked_qt or any(linked != version for linked in linked_qt):
        parser.error("the app was built against a different Qt version; rebuild it with --qt")
    if not args.local_test and tuple(map(int, version.split("."))) < (6, 12, 1):
        parser.error("distribution requires Qt 6.12.1 or newer due to CVE-2026-116337")
    for module in QT_MODULES:
        if not (qt / f"sbom/{module}-{version}.source.spdx").is_file():
            parser.error(f"Qt source notice is missing: {module}")
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        shutil.copytree(source, output, symlinks=True)
        deploy = run(str(qt / "bin/macdeployqt"), str(output),
                     f"-qmldir={ROOT / 'qml'}", "-no-strip", "-no-codesign", "-verbose=1")
        omitted_sql_drivers = ("libmimerapi.dylib", "libiodbc.2.dylib", "libpq.5.dylib")
        unexpected = [line for line in deploy.splitlines() if line.startswith("ERROR:")
                      and not any(name in line for name in omitted_sql_drivers)]
        if unexpected:
            raise RuntimeError("macdeployqt reported an unexpected deployment error: " + unexpected[0])
        svg = qt / "plugins/imageformats/libqsvg.dylib"
        if not svg.is_file():
            raise RuntimeError("Qt SVG image plugin is missing")
        shutil.copy2(svg, output / "Contents/PlugIns/imageformats/libqsvg.dylib")
        run(sys.executable, str(ROOT / "tools/prune_macos_bundle.py"), str(output))

        resources = output / "Contents/Resources"
        licenses = resources / "licenses"
        licenses.mkdir()
        for name in ("GPL-3.0-only.txt", "LGPL-3.0-only.txt"):
            shutil.copy2(ROOT / "third_party/licenses" / name, licenses / name)
        sbom = resources / "qt-sbom"
        sbom.mkdir()
        for module in QT_MODULES:
            source_notice = qt / f"sbom/{module}-{version}.source.spdx"
            target_notice = sbom / f"{module}-{version}.source.spdx.gz"
            with source_notice.open("rb") as original, target_notice.open("wb") as target:
                with gzip.GzipFile(filename="", mode="wb", fileobj=target, mtime=0) as compressed:
                    shutil.copyfileobj(original, compressed)
        frameworks = sorted(path.stem for path in (output / "Contents/Frameworks").glob("Qt*.framework"))
        notices = (
            f"Viamiora includes Qt {version} by The Qt Company and contributors.\n"
            "The dynamically linked runtime modules are available under LGPL-3.0-only "
            "or their stated alternative licenses.\n"
            "https://www.qt.io/licensing/\n\n"
            "The GNU GPL and LGPL texts are in Resources/licenses/. The corresponding Qt "
            "source SBOMs, including third-party attribution and extracted license text, "
            "are in Resources/qt-sbom/ as gzip files.\n"
            f"Qt source: https://code.qt.io/cgit/qt/qt5.git/?h=v{version}\n\n"
            "Bundled Qt frameworks:\n" + "\n".join(frameworks) + "\n"
        )
        (resources / "THIRD_PARTY_NOTICES.txt").write_text(notices)
        if args.local_test:
            (resources / "LOCAL_TEST_ONLY").write_text("This package is for local smoke testing.\n")

        signature = ["codesign", "--force", "--sign", args.identity, "--options", "runtime"]
        if args.timestamp:
            signature.append("--timestamp")
        for framework in sorted((output / "Contents/Frameworks").glob("*.framework")):
            run(*signature, str(framework))
        for plugin in sorted((output / "Contents/PlugIns").rglob("*.dylib")):
            run(*signature, str(plugin))
        run(*signature, "--entitlements", str(ROOT / "src/Release.entitlements"), str(output))
        check = [sys.executable, str(ROOT / "tools/check_macos_bundle.py"),
                 str(output), "--require-signature"]
        if not args.local_test:
            check.append("--require-distribution")
        run(*check)
    except BaseException:
        shutil.rmtree(output, ignore_errors=True)
        raise
    print(f"Packaged {output} with Qt {version} ({len(frameworks)} frameworks).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
