# Milestone 1 — Foundation (Phase 1) report

## Implemented
- Shared C++20 engine: four independent buses, layered sample-accurate playback that
  keeps leading silence and phase, smoothed parameters, safety limiter, panic, offline
  renderer, synthesized atmosphere bed, mic feature extraction (level, silence,
  transients, low-frequency energy), mic transform (tone + echo), interim stereo panner.
- Core: WAV read/write, offline resampler, project manifest v1 with migration,
  validation and unknown-key preservation, package save/load/backup recovery,
  asset import and relink, `LiveSession` shared by every host.
- iPhone app (SwiftUI): Experience (import/play song, atmosphere bed, mic with
  permission and indicator, transforms, levels, stop-all), Library (imported copies),
  Settings (route, reported latency, ceiling, privacy, appearance). Studio and Worlds
  show their status honestly and have no controls.
- Desktop app (JUCE; Windows, macOS): sidebar navigation, the same Experience
  features, drag-and-drop import, device settings, and a runtime log file.
- CI: engine tests on Linux/Windows/macOS, desktop builds on Windows/macOS, iOS simulator build.

## Tests run (in this environment, Linux x86-64)
- 34 unit/integration tests, GCC 13 Debug with warnings as errors: **all pass**.
- The same tests under AddressSanitizer + UBSan: **pass**. Engine tests (including a
  two-thread stress test) under ThreadSanitizer: **pass**.
- Clang 18 `-Wall -Wextra -Wpedantic -Wshadow -Werror` syntax check: **clean**.
- Benchmark: 32 layers + atmosphere bed + mic chain run at about 30× real time in a Debug build (one core).
- Desktop app compiled against JUCE 8.0.4 on Linux and launched under Xvfb (no audio device present): the UI renders.

## Not tested here
- **iOS**: not compiled (no Xcode in this environment). CI builds it for the simulator.
  Device audio, permissions, route changes, Bluetooth and latency need an iPhone.
- **Windows / macOS**: not compiled here. CI builds both. Real devices, WASAPI/Core Audio
  behaviour, macOS mic permission and latency are unmeasured.
- No real-world latency measurement on any platform.

## Known limitations
- Spatial panner is not HRTF. Nothing in the UI uses it yet.
- Only one song slot in the apps; projects (multi-layer) are engine/core-only so far.
- Session capture and export UI are not built (offline render and WAV export exist in core).
- Windows AAC/M4A decoding is not implemented. WAV export is limited to RIFF's 4 GB (RF64 is not implemented).
- Clips are decoded fully into memory (long files use a lot of RAM). Streaming is planned.

## Next milestone (Phase 2: Atmospheric Studio)
Multitrack layers in the UI, project save/open in both apps, basic effects per layer,
timeline, stereo export, SQLite library.
