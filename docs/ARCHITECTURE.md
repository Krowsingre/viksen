# Architecture

```
apps/ios (SwiftUI) ──► AtmosEngineBridge (ObjC++) ─┐
apps/desktop (JUCE UI) ──► AudioHost ──────────────┤
                                                   ▼
                     core/session/LiveSession (settings, track, rebuilds)
                     core/projects, core/assets, core/serialization
                                                   ▼
                     engine/  (real-time C++20, no platform/UI/product deps)
platform/ios       AVAudioSession, RemoteIO, AVFoundation decode
platform/windows   WASAPI policy          platform/macos   Core Audio policy
platform/desktop   JUCE decode + input toggling (shared Win/macOS), Linux dev fallback
```

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
