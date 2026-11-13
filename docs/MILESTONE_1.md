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

---

# Milestone 1b: reactive foundations and iPhone polish

Design input: a private study of a reference app (notes in the private assets repository (research/)).
Nothing from that app is used; every item below is original code.

## Implemented
- `FrequencyShifter` (FIR Hilbert, about 2.6 ms latency), added to the mic chain. That makes
  **three live transformations**: tone filter, frequency shift and echo.
- `RoomMemory`: rolling RAM capture of the room and replays slowed or pitched with long fades and
  alternating pan, wiped whenever the mic stops.
- `ConditionGate` + `EnvironmentStates` (quiet, loud, still, moving, coarse loudness and
  activity levels), `MotionAnalyzer` (motion energy, free fall).
- `TickClock` (sample-accurate musical grid for future World cues).
- `WorldLifecycle` (fade in → active → ducked countdown → fade out; stay-forever override).
- `WorldConditions` with the **lunar calendar** (eight moon phases, lunar day), time of
  day, environment states, listening time, consecutive Worlds; unknown condition types
  are preserved but never satisfied.
- iPhone: opening screen from the chosen concept, violet theme, Core Motion adapter,
  environment state chips, moon phase and lunar day, frequency-shift and room-memory
  controls. Bundle id `com.krowsingre.viksen`; export-compliance key set.

## Tests (Linux)
45 tests pass (GCC Debug with warnings as errors; ASan+UBSan; engine suite under TSan; Clang
syntax check clean). Moon phases were checked against published new, first-quarter and
full-moon times. Desktop app rebuilt successfully against the changed engine.

## Not tested here
iPhone code (Swift/ObjC++) is still uncompiled in this environment. It is built by CI on
macOS, and audio, motion and microphone behaviour need a real iPhone.
