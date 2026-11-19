#!/usr/bin/env python3
"""Keep the Qt Quick modules used by the macOS Basic-style application."""

import argparse
import re
import shutil
import subprocess
from pathlib import Path


QML_MODULES = {
    "QML", "QtQml", "QtQml/Models", "QtQml/WorkerScript",
    "QtQuick", "QtQuick/Controls", "QtQuick/Controls/Basic",
    "QtQuick/Controls/Basic/impl", "QtQuick/Controls/impl",
    "QtQuick/Effects", "QtQuick/Layouts", "QtQuick/Shapes",
    "QtQuick/Templates", "QtQuick/Window", "QtWebView",
}
RUNTIME_PLUGINS = {
    "imageformats": {"libqgif.dylib", "libqico.dylib", "libqjpeg.dylib", "libqsvg.dylib"},
    "networkinformation": {"libqapplenetworkinformation.dylib"},
    "platforms": {"libqcocoa.dylib"},
    "tls": {"libqcertonlybackend.dylib", "libqsecuretransportbackend.dylib"},
    "webview": {"libqtwebview_darwin.dylib"},
}
FRAMEWORK_NAME = re.compile(r"(?:^|/)Qt([A-Za-z0-9]+)\.framework/")


def linked_frameworks(binary: Path) -> set[str]:
    output = subprocess.check_output(["otool", "-L", str(binary)], text=True)
    return {"Qt" + match for match in FRAMEWORK_NAME.findall(output)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args()
    bundle = args.bundle.resolve()
    if bundle.suffix != ".app" or not bundle.is_dir():
        parser.error("expected an existing .app bundle")
    contents = bundle / "Contents"
    qml = contents / "Resources/qml"
    plugins = contents / "PlugIns"
    frameworks = contents / "Frameworks"
    if not all(path.is_dir() for path in (qml, plugins, frameworks)):
        parser.error("bundle must be deployed with macdeployqt and QML imports first")

    modules = sorted((path.parent for path in qml.rglob("qmldir")),
                     key=lambda path: len(path.parts), reverse=True)
    for module in modules:
        relative = module.relative_to(qml).as_posix()
        if relative not in QML_MODULES and not any(
            allowed.startswith(relative + "/") for allowed in QML_MODULES
        ):
            shutil.rmtree(module)
    required_qml_plugins = {
        path.resolve().name for path in qml.rglob("*.dylib")
        if path.resolve().is_relative_to(plugins)
    }
    required_qml_plugins.add("libqtwebviewquickplugin.dylib")
    for plugin in plugins.rglob("*.dylib"):
        allowed = (required_qml_plugins if plugin.parent.name == "quick"
                   else RUNTIME_PLUGINS.get(plugin.parent.name, set()))
        if plugin.name not in allowed:
            plugin.unlink()
    for directory in sorted(plugins.rglob("*"), key=lambda path: len(path.parts), reverse=True):
        if directory.is_dir() and not any(directory.iterdir()):
            directory.rmdir()

    required = set()
    pending = [contents / "MacOS/viamiora", *plugins.rglob("*.dylib")]
    while pending:
        for name in linked_frameworks(pending.pop()):
            if name in required:
                continue
            binary = frameworks / f"{name}.framework/Versions/A/{name}"
            if not binary.is_file():
                raise RuntimeError(f"required framework is missing: {name}")
            required.add(name)
            pending.append(binary)
    for framework in frameworks.glob("Qt*.framework"):
        if framework.stem not in required:
            shutil.rmtree(framework)
    print(f"Kept {len(required)} Qt frameworks and {len(list(plugins.rglob('*.dylib')))} plugins.")


if __name__ == "__main__":
    main()
