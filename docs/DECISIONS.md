# Decisions and items needing product-owner approval

| # | Decision | Status |
|---|----------|--------|
| 1 | **JUCE 8 for the desktop UI and devices.** JUCE 8 is AGPLv3 or commercial. A closed-source release needs a commercial JUCE licence (or the desktop app must be AGPL). The engine and core do not depend on JUCE, so this can be swapped. | **Needs approval** |
| 2 | Shared engine is plain C++20 with no framework dependency. JUCE is used only in `apps/desktop` and `platform/desktop`. | Implemented |
| 3 | iPhone: SwiftUI + ObjC++ bridge + RemoteIO (not AVAudioEngine) for full-duplex in one callback. iOS 17 minimum (Observation, AVAudioApplication). | Implemented, untested on device |
| 4 | JSON: nlohmann/json 3.11.3 (MIT), vendored in `third_party/`. | Implemented |
| 5 | Engine namespace `atmos`, manifest format id `atmos.project`. The product name lives only in app settings. | Implemented |
| 6 | HRTF renderer: candidates are libmysofa (BSD) + SOFA datasets (check each dataset's licence) or Steam Audio (Apache-2.0). Until then, an interim ILD/ITD panner that is labelled as not-HRTF. | **Needs approval (Phase 3)** |
| 7 | Windows AAC/M4A decoding via a Media Foundation adapter (OS codec, no licence cost). | Planned |
| 8 | ASIO is disabled (needs a Steinberg licence agreement). WASAPI low-latency shared mode is the default. | Needs decision later |
| 9 | Library database: SQLite (public domain). | Planned (Phase 2) |
| 10 | Mic session mode on iOS: `.measurement` (no AGC/voice processing) with A2DP output allowed and default-to-speaker. Live monitoring over the speaker shows a feedback warning; raw monitoring is off by default. | Implemented |
| 11 | Bundle id `com.viksen.app`, project extension `.viksen`. Both are provisional with the name. | **Needs approval** |
| 12 | Linux desktop build exists only for development/CI and is not a product target. | Implemented |
