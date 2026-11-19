#!/usr/bin/env python3
"""Fail a macOS release package with missing or host-only runtime dependencies."""

import argparse
import datetime
import gzip
import plistlib
import re
import subprocess
import sys
import zlib
from pathlib import Path


MACHO_MAGIC = {
    bytes.fromhex(value)
    for value in (
        "feedface", "cefaedfe", "feedfacf", "cffaedfe",
        "cafebabe", "bebafeca", "cafebabf", "bfbafeca",
    )
}
FORBIDDEN_QT = ("QtWebEngine", "QtQuickTimeline")


def command(*args: str) -> str:
    result = subprocess.run(args, check=False, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"{args[0]} failed for {args[-1]}: {result.stderr.strip()}")
    return result.stdout


def version(value: str) -> tuple[int, ...]:
    parts = [int(part) for part in value.split(".")]
    return tuple((parts + [0, 0, 0])[:3])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--require-signature", action="store_true")
    parser.add_argument("--require-distribution", action="store_true")
    parser.add_argument("--require-universal", action="store_true")
    args = parser.parse_args()

    bundle = args.bundle.resolve()
    contents = bundle / "Contents"
    try:
        with (contents / "Info.plist").open("rb") as stream:
            info = plistlib.load(stream)
    except (OSError, plistlib.InvalidFileException) as error:
        print(f"FAIL: cannot read Info.plist: {error}", file=sys.stderr)
        return 1
    executable_name = info.get("CFBundleExecutable", "")
    safe_executable = executable_name if isinstance(executable_name, str) else ""
    executable = contents / "MacOS" / safe_executable
    minimum = info.get("LSMinimumSystemVersion", "")
    errors = []
    if info.get("CFBundleIdentifier") != "berlin.yuna.viamiora":
        errors.append("bundle identifier is not berlin.yuna.viamiora")
    if not safe_executable or Path(safe_executable).name != safe_executable:
        errors.append("Info.plist has no valid CFBundleExecutable")
    display_version = info.get("CFBundleShortVersionString", "")
    try:
        build_version = display_version.removesuffix("-SNAPSHOT")
        year, month, day = (int(part) for part in build_version.split("."))
        datetime.date(year, month, day)
        if info.get("CFBundleVersion") != build_version:
            errors.append("bundle build version differs from its date version")
    except (ValueError, AttributeError):
        errors.append("app version is not a valid year.month.day date")
    valid_minimum = isinstance(minimum, str) and bool(re.fullmatch(r"\d+(?:\.\d+){1,2}", minimum))
    if not valid_minimum:
        errors.append("Info.plist has no numeric LSMinimumSystemVersion")
    for name, description in (
        ("LICENSE", "Apache-2.0 LICENSE"),
        ("THIRD_PARTY_NOTICES.txt", "third-party license notices"),
        ("licenses/GPL-3.0-only.txt", "Qt GPL license text"),
        ("licenses/LGPL-3.0-only.txt", "Qt LGPL license text"),
    ):
        resource = contents / "Resources" / name
        if not resource.is_file() or resource.stat().st_size == 0:
            errors.append(f"missing or empty {description} in bundle resources")
    if not executable.is_file():
        errors.append("main executable is missing")
    notice_path = contents / "Resources/THIRD_PARTY_NOTICES.txt"
    notice = notice_path.read_text(errors="replace") if notice_path.is_file() else ""
    qt_version = re.search(r"^Viamiora includes Qt (\d+\.\d+\.\d+)\b", notice)
    if not qt_version:
        errors.append("Qt version is missing from third-party notices")
    for module in ("qtbase", "qtdeclarative", "qtsvg", "qtwebview"):
        if not qt_version:
            break
        source = contents / "Resources/qt-sbom" / f"{module}-{qt_version.group(1)}.source.spdx.gz"
        try:
            with gzip.open(source, "rb") as stream:
                header = stream.read(4096)
                size = len(header)
                while block := stream.read(1024 * 1024):
                    size += len(block)
                    if size > 50 * 1024 * 1024:
                        raise ValueError("source attribution exceeds 50 MiB")
            if not header.startswith(b"SPDXVersion: SPDX-") or (
                f"DocumentName: {module}\n".encode() not in header
            ):
                raise ValueError("source attribution names another module")
        except (OSError, EOFError, ValueError, zlib.error) as error:
            errors.append(f"invalid Qt source attribution: {module}: {error}")

    inspected = 0
    forbidden_paths = set()
    for binary in contents.rglob("*"):
        relative = binary.relative_to(bundle)
        if any(forbidden in str(relative) for forbidden in FORBIDDEN_QT):
            forbidden_paths.add(str(relative))
        if binary.is_symlink():
            if not binary.resolve().is_relative_to(contents):
                errors.append(f"{relative} points outside the bundle")
            continue
        if not binary.is_file():
            continue
        with binary.open("rb") as stream:
            if stream.read(4) not in MACHO_MAGIC:
                continue
        inspected += 1
        try:
            if args.require_universal:
                architectures = set(command("lipo", "-archs", str(binary)).split())
                if not {"arm64", "x86_64"}.issubset(architectures):
                    errors.append(f"{relative} is missing an arm64 or x86_64 slice")
            build = command("xcrun", "vtool", "-show-build", str(binary))
            minimum_versions = re.findall(r"^\s*minos (\d+(?:\.\d+){1,2})$", build, re.MULTILINE)
            if not minimum_versions:
                errors.append(f"{relative} has no readable minimum macOS version")
            for required in minimum_versions:
                if valid_minimum and version(required) > version(minimum):
                    errors.append(f"{relative} requires macOS {required}, above {minimum}")
            libraries = command("otool", "-L", str(binary)).splitlines()
        except RuntimeError as error:
            errors.append(str(error))
            continue
        for line in libraries:
            if " (compatibility version " not in line:
                continue  # otool prints a heading for each architecture in a universal binary.
            dependency = line.strip().split(" (compatibility version", 1)[0]
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            if dependency.startswith("@executable_path/"):
                resolved = contents / "MacOS" / dependency.removeprefix("@executable_path/")
            elif dependency.startswith("@loader_path/"):
                resolved = binary.parent / dependency.removeprefix("@loader_path/")
            elif dependency.startswith("@rpath/"):
                resolved = contents / "Frameworks" / dependency.removeprefix("@rpath/")
            else:
                errors.append(f"{relative} links outside the bundle: {dependency}")
                continue
            if not resolved.exists() or not resolved.resolve().is_relative_to(contents):
                errors.append(f"{relative} cannot resolve {dependency} inside the bundle")

    for path in sorted(forbidden_paths):
        errors.append(f"unused or GPL-only Qt module is bundled: {path}")
    if not inspected:
        errors.append("bundle contains no Mach-O files")

    if args.require_signature:
        result = subprocess.run(
            ["codesign", "--verify", "--deep", "--strict", str(bundle)],
            check=False,
            text=True,
            capture_output=True,
        )
        if result.returncode:
            errors.append("code signature is invalid: " + result.stderr.strip())

    if args.require_distribution:
        if (contents / "Resources/LOCAL_TEST_ONLY").exists():
            errors.append("local test signing cannot be distributed")
        result = subprocess.run(["codesign", "-dv", "--verbose=4", str(bundle)],
                                check=False, text=True, capture_output=True)
        signature = result.stdout + result.stderr
        if result.returncode or "Authority=Developer ID Application:" not in signature or not re.search(
            r"TeamIdentifier=[A-Z0-9]{10}\b", signature
        ) or "flags=0x10000(runtime)" not in signature:
            errors.append("bundle lacks a hardened Developer ID Application signature")
        result = subprocess.run(["codesign", "-d", "--entitlements", ":-", str(bundle)],
                                check=False, capture_output=True)
        try:
            entitlements = plistlib.loads(result.stdout) if result.returncode == 0 else {}
        except plistlib.InvalidFileException:
            entitlements = {}
        for key in ("com.apple.security.device.audio-input", "com.apple.security.device.camera",
                    "com.apple.security.personal-information.addressbook",
                    "com.apple.security.personal-information.calendars",
                    "com.apple.security.cs.disable-library-validation"):
            if entitlements.get(key) is not True:
                errors.append(f"missing resource entitlement: {key}")
        if entitlements.get("com.apple.security.get-task-allow") is True:
            errors.append("distribution signature permits debugging")
        for key in ("NSCameraUsageDescription", "NSMicrophoneUsageDescription",
                    "NSContactsUsageDescription", "NSCalendarsFullAccessUsageDescription"):
            if not info.get(key):
                errors.append(f"missing privacy explanation: {key}")

    if errors:
        for error in errors[:25]:
            print(f"FAIL: {error}", file=sys.stderr)
        if len(errors) > 25:
            print(f"FAIL: {len(errors) - 25} additional errors", file=sys.stderr)
        return 1
    print(f"Checked {inspected} Mach-O files in {bundle}; minimum macOS {minimum}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
