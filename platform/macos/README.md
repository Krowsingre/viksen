# macOS platform layer

- `MacAudioDefaults.cpp` — Core Audio device policy for the JUCE desktop app.
- Microphone permission: JUCE `RuntimePermissions::request(recordAudio)` shows the TCC prompt.
  `MICROPHONE_PERMISSION_TEXT` in `apps/desktop/CMakeLists.txt` becomes `NSMicrophoneUsageDescription`.
- Hardened runtime and the `com.apple.security.device.audio-input` entitlement are enabled in CMake
  (both are required for notarization).

## Build (Apple Silicon native, optionally universal)

```bash
cmake -S . -B build-mac -G Xcode -DATMOS_BUILD_DESKTOP=ON "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
```

```bash
cmake --build build-mac --config Release --target AtmosDesktop
```

## Signing and notarization (planned, needs an Apple Developer account)

1. Sign with a Developer ID Application certificate:
   `codesign --force --options runtime --timestamp --entitlements <generated .entitlements> --sign "Developer ID Application: …" Viksen.app`
   (JUCE writes the entitlements file into the build tree.)
2. `ditto -c -k --keepParent Viksen.app Viksen.zip`
3. `xcrun notarytool submit Viksen.zip --keychain-profile <profile> --wait`
4. `xcrun stapler staple Viksen.app`

Minimum macOS: 12.0 (set in the root CMakeLists). Not yet verified on hardware.
