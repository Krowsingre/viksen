# Viksen (working title)

Atmospheric & spatial audio studio and augmented auditory playground for iPhone,
Windows and macOS. Specification: product baseline provided by the product owner.

| Area | Path | Status |
|------|------|--------|
| Shared real-time engine (C++20) | `engine/` | Phase 1 implemented, tested |
| Projects, assets, live session | `core/` | Phase 1 implemented, tested |
| iPhone app (SwiftUI + ObjC++) | `apps/ios/` | Experience / Library / Settings working |
| Desktop app (JUCE: Windows, macOS) | `apps/desktop/` | Experience / Settings working |
| Platform adapters | `platform/{ios,windows,macos,desktop}` | |

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/DECISIONS.md](docs/DECISIONS.md) (items need approval),
[docs/PROJECT_SCHEMA.md](docs/PROJECT_SCHEMA.md) and [docs/MILESTONE_1.md](docs/MILESTONE_1.md).

## Build & test the engine (any OS, CMake ≥ 3.22, C++20 compiler)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

## Desktop app

```bash
cmake -S . -B build-desktop -DATMOS_BUILD_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build-desktop --config Release --target AtmosDesktop
```

JUCE is fetched automatically (or pass `-DATMOS_JUCE_DIR=/path/to/JUCE`). Platform notes:
[Windows](platform/windows/README.md), [macOS](platform/macos/README.md).

## iPhone app

See [apps/ios/README.md](apps/ios/README.md) (XcodeGen + Xcode 15.3+).

The apps' artwork (every image and app icon) lives in the private assets repository and is
not committed here. Run `tools/fetch_art.sh` once after cloning (and after artwork
changes) before building any app; the CI and TestFlight builds do it automatically.

## License

Copyright (c) 2026 Krowsingre Publishing. All rights reserved. This is proprietary
software: no one may use, copy, modify, distribute or sell anything in this repository
without written permission. See [LICENSE](LICENSE). Third-party components under
`third_party/` keep their own licenses.
