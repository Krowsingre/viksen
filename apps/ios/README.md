# iPhone app

SwiftUI interface over the shared C++ engine via an Objective-C++ bridge.

```
apps/ios/
  project.yml                 XcodeGen spec (the .xcodeproj is generated, not committed)
  Viksen/App                  App entry + tab navigation (Experience first)
  Viksen/Bridge               AtmosEngineBridge (ObjC++) — the only Swift ↔ C++ boundary
  Viksen/Engine               EngineController (UI state), MediaLibrary (imported copies)
  Viksen/Features/*           Experience, Studio, Worlds, Library, Settings
platform/ios/                 AVAudioSession, RemoteIO host, AVFoundation decoder
```

## Build

Requires macOS with Xcode 15.3+ (iOS 17 SDK) and [XcodeGen](https://github.com/yonaskolb/XcodeGen).

```bash
brew install xcodegen
```

```bash
cd apps/ios && xcodegen generate && open Viksen.xcodeproj
```

Command-line simulator build (what CI runs):

```bash
cd apps/ios && xcodegen generate && xcodebuild -project Viksen.xcodeproj -scheme Viksen -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build
```

Bundle ID: `com.krowsingre.viksen`. To run on a device, choose your team in Xcode (Signing & Capabilities) or add `DEVELOPMENT_TEAM` to `project.yml`.
Microphone input and real latency can only be evaluated on a physical iPhone.

## What works in this milestone

- Start/stop audio through RemoteIO (full-duplex in one callback when the mic is on).
- Import a song from Files (copied into the app library; the original is untouched),
  decode via AVFoundation (WAV/AIFF/FLAC/MP3/AAC/ALAC), resample to the hardware rate, play/pause/seek/loop.
- Atmosphere bed (synthesized), independent of music and mic.
- Opening screen (wordmark, violet light wave, Get Started, About).
- Microphone with explicit permission, red MIC LIVE indicator, raw monitor (off by default),
  three live transformations (tone filter, frequency shift, echo), live loudness/transient readout.
- Room memory: replays the last seconds of the surroundings, slowed and faded (RAM only,
  erased when the mic turns off).
- Environment states (Quiet / Loud, plus Still / Moving with optional motion sensing via
  Core Motion; no permission prompt needed for the accelerometer).
- Lunar calendar: current moon phase and lunar day shown in Experience.
- Independent levels for music / atmosphere / raw mic / transformed mic / master,
  safety limiter ceiling, "Stop all sound" panic button.
- Interruptions, headphone unplug (pauses and silences raw monitoring),
  sample-rate changes on route change, media-services reset.

Studio and Worlds tabs state plainly that they are not built yet and contain no controls.
