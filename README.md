# Viamiora

Viamiora is an independent macOS app for HTTPS Mividas meeting portals. It offers native scheduling, meeting details, local Calendar and Contacts integration, and an embedded meeting view with a browser fallback. The portal address is chosen by the user.

## Build locally

Requires macOS, CMake 3.24+, Ninja, and Qt 6.11+ with Quick, Quick Controls, Network, Concurrent, WebView, Linguist Tools, and Test. CI uses the official Qt 6.12.1 macOS kit.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
open build/viamiora.app
```

For a stable local signature, put `VIAMIORA_SIGN_IDENTITY=<installed identity>` in the ignored `.env.local`. Homebrew Qt builds can require the host's macOS version. The official Qt 6.12.1 build targets macOS 14.4 or later. Local builds are not distributable packages.

## Project status

macOS is the only configured platform. The source and project artwork use [Apache-2.0](LICENSE). Qt remains separately licensed. Local packaging and launch tests pass. Developer ID signing, notarization, installed media tests, and automatic updates have not passed qualification.

- [Meeting journey and portal behavior](docs/spec/meeting-journey.md)
- [Platform delivery and release gates](docs/spec/platform-delivery.md)
- [macOS source qualification](.github/workflows/native-qualify.yml)
- [signed macOS candidate workflow](.github/workflows/macos-candidate.yml)
