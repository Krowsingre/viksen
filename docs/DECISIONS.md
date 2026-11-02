# Decisions and items needing product-owner approval

| # | Decision | Status |
|---|----------|--------|
| 1 | **Replace JUCE (AGPLv3 / paid commercial licence).** Owner: swapping is fine if there is a better option. Proposed licence-free stack: miniaudio (public domain or MIT-0) for WASAPI/Core Audio device I/O and WAV/FLAC/MP3 decoding, OS codecs for AAC/M4A (Media Foundation, AudioToolbox); macOS UI in SwiftUI sharing the iPhone app's views and ObjC++ bridge; Windows UI in WinUI 3 (C++/WinRT, Windows App SDK, MIT). The JUCE app stays until the replacement reaches parity. | **Approved**: each OS has its own native UI with a matching look; iPhone is the priority |
| 2 | Shared engine is plain C++20 with no framework dependency. JUCE is used only in `apps/desktop` and `platform/desktop`. | Implemented |
| 3 | iPhone: SwiftUI + ObjC++ bridge + RemoteIO (not AVAudioEngine) for full-duplex in one callback. iOS 17 minimum (Observation, AVAudioApplication). | Implemented, untested on device |
| 4 | JSON: nlohmann/json 3.11.3 (MIT), vendored in `third_party/`. | Implemented |
| 5 | Engine namespace `atmos`, manifest format id `atmos.project`. The product name lives only in app settings. | Implemented |
| 6 | HRTF renderer (no licence fee required). **Recommended: Steam Audio** (Apache-2.0 since v4.5.2; Windows, macOS, iOS arm64; HRTF binaural, SOFA loading, distance, occlusion, reflections). Obligations: ship its licence/NOTICE text. Fallback: our own convolution renderer + libmysofa (BSD-3) + SADIE II HRTFs (Apache-2.0, cite the dataset). Avoid: 3D Tune-In (GPL), OpenAL Soft (LGPL, awkward on iOS), Resonance Audio (archived), ARI database (CC BY-SA). Interim ILD/ITD panner until Phase 3. | **Approved** (Phase 3); verify licence files at integration |
| 7 | Windows AAC/M4A decoding via a Media Foundation adapter (OS codec, no licence cost). | Planned |
| 8 | ASIO is disabled (needs a Steinberg licence agreement). WASAPI low-latency shared mode is the default. | Deferred by owner |
| 9 | Library database: SQLite (public domain). | Planned (Phase 2) |
| 10 | Mic session mode on iOS: `.measurement` (no AGC/voice processing) with A2DP output allowed and default-to-speaker. Live monitoring over the speaker shows a feedback warning; raw monitoring is off by default. | Implemented |
| 11 | Bundle id `com.krowsingre.viksen` (registered by the owner), project extension `.viksen`. A future SwiftUI Mac app reuses the same bundle id for universal purchase. | Approved by owner |
| 12 | Linux desktop build exists only for development/CI and is not a product target. | Implemented |
| 13 | Wherever dates matter, the app uses the **lunar calendar** (moon phase, lunar day), computed on-device. No solar-calendar features. | Owner directive |
| 14 | **iPhone is the primary platform**; desktop follows. | Owner directive |
| 15 | Launch flow: splash (placeholder until designed) → **Home tab** styled after the owner-selected concept (wave, ring, wordmark, Get Started, destinations). Tabs: Home, Experience, Studio, Worlds, Library; Settings from the gear on Home. | Implemented; splash design pending |
| 16 | Apple team `JNJX9FYHC2`, automatic signing. | Configured |
| 17 | Business model: paid app. | Owner directive; price to be decided |
| 18 | iOS context inputs: **time** (time of day; dates via the lunar calendar), **weather** (opt-in; needs location + network, WeatherKit), **activity** (Core Motion activity + pedometer cadence), **head tracking** (AirPods via CMHeadphoneMotionManager, feeding the Steam Audio renderer), **Health / exercise intensity** (HealthKit; live heart rate needs an Apple Watch companion or a Bluetooth heart-rate strap). | Owner directive; open questions in chat |
| 19 | Proposed tiers: Free (a few Worlds, basic augmented audio, limited spatial) · Experience $9.99 · Studio $24.99 · World packs $1.99–4.99. One app with in-app purchases (StoreKit 2), universal purchase with the Mac app. | Proposed by owner; under discussion |
| 20 | Desktop (Windows/macOS) emphasises Studio; Experience features kept wherever practical. | Owner directive |
