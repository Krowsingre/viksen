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

## TestFlight from GitHub (no Mac needed)

Workflow: `.github/workflows/ios-testflight.yml`. One-time setup:

1. App Store Connect → Users and Access → Integrations → App Store Connect API →
   Team Keys → **+**, role **Admin** (needed for cloud-managed signing). Download the
   `.p8` once; note the Key ID and Issuer ID.
2. GitHub → Settings → Secrets and variables → Actions → add `ASC_KEY_ID`,
   `ASC_ISSUER_ID`, `ASC_KEY_P8` (the whole `.p8` text).
3. Actions → "iPhone → TestFlight" → **Run workflow** (or push a tag `ios-v0.1.0`).
   The build number is the workflow run number.
4. After Apple finishes processing (usually 5–30 min), add yourself under TestFlight →
   Internal Testing and install via the TestFlight app on the iPhone.

## In-app purchases (StoreKit 2)

Create these **non-consumable** in-app purchases in App Store Connect (app → Monetization →
In-App Purchases), each with a price, display name, description and a review screenshot:

| Product ID | Price | Grants |
|---|---|---|
| `com.krowsingre.viksen.experience` | $9.99 | Experience |
| `com.krowsingre.viksen.studio` | $24.99 | Studio (includes Experience) |
| `com.krowsingre.viksen.studio.upgrade` | $14.99 | Studio; shown only to Experience owners |

What each tier unlocks is defined in one table: `Viksen/Store/Entitlements.swift`.
Purchases made in TestFlight builds are free sandbox purchases.
