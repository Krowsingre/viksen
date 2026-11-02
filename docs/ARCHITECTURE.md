# Architecture

```
apps/ios  (SwiftUI, iPhone)  ──► AtmosEngineBridge (ObjC++) ── IOSAudioIO (RemoteIO) ─┐
apps/ios  (SwiftUI, Mac)     ──► AtmosEngineBridge (ObjC++) ── MiniaudioHost ─────────┤
apps/windows (WinUI 3, C#)   ──► viksen_engine.dll (platform/capi, C ABI) ── MiniaudioHost ┤
apps/desktop (JUCE, legacy until the WinUI/Mac apps reach parity) ──► AudioHost ──────┤
                                                   ▼
                     core/session/LiveSession (settings, track, Dimensions, recording)
                     core/projects, core/worlds, core/assets, core/serialization
                                                   ▼
                     engine/  (real-time C++20, no platform/UI/product deps;
                               Steam Audio HRTF when compiled in)
platform/ios       AVAudioSession, RemoteIO, Core Motion, AVFoundation decode (decode also on Mac)
platform/desktop   MiniaudioHost: Core Audio / WASAPI device I/O and stem decoding
platform/capi      C interface for non-C++ apps (Windows)
```

One SwiftUI code base serves iPhone and Mac (`ViksenMac` target in `apps/ios/project.yml`,
same bundle id for universal purchase). iPhone-only pieces (AVAudioSession, RemoteIO,
accelerometer, Activity, Apple Watch link) are behind `TARGET_OS_IOS` / `#if os(iOS)`;
`Shared/Platform.swift` maps toolbar slots and title styles. The Windows app mirrors the
same sections and palette in WinUI 3.

Dependency rule: `engine` ← `core` ← `platform` ← `apps`. Nothing in `engine/` or `core/`
includes JUCE, Apple frameworks or Windows headers, or mentions the product name
(the engine namespace is `atmos`; the product name, bundle id and project extension
are CMake/Xcode settings in the app layer).

## Real-time engine (`engine/audio/AudioEngine`)

- `process(inputs, outputs, frames)` is called by the host's audio callback. It never
  allocates, locks or does I/O. Larger host blocks are split into `maxBlockSize` chunks.
- Control → audio: atomics for continuous values (smoothed per sample, 20 ms ramps) and a
  lock-free SPSC command queue for layer changes. Audio → control: a second SPSC queue
  returns retired clips, which `collectGarbage()` frees on the control thread.
- Signal flow per block: layers → their bus (music / atmosphere / mic raw / mic transformed);
  atmosphere bed → atmosphere; mic (only when enabled) → feature extractor, raw bus,
  transform chain → transformed bus; buses × smoothed gain/mute → master gain → safety
  limiter (instant attack, hard ceiling, NaN guard) → outputs.
- Panic silences output from the next block; resuming ramps up from zero.
- Clips must already be at the engine rate. `core/assets/ClipPreparation` resamples with an
  offline windowed-sinc so source pitch/tempo never change. Leading silence is never trimmed.
- `InterimStereoPanner` (ILD + ITD + head shadow) sits behind the `SpatialRenderer`
  interface. It is not HRTF and the UI must not present it as 3D audio.

## Sample-rate changes

Devices change rate on route changes (iOS) or device switches (desktop). `LiveSession`
keeps the user's settings and the native-rate decoded track, and rebuilds the engine
while the callback is stopped. iOS rebuilds from the bridge; desktop from
`audioDeviceAboutToStart`.

## Projects

See [PROJECT_SCHEMA.md](PROJECT_SCHEMA.md). Directory packages with an atomic manifest
save, a backup for crash recovery, copy-in asset import, relinking, and preservation of
every unknown key so projects round-trip across platforms and versions.
