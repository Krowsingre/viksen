# Windows platform layer

- `WindowsAudioDefaults.cpp` — prefers JUCE's "Windows Audio (Low Latency Mode)" WASAPI type, falls back to shared WASAPI; requests 48 kHz / 256 frames.
- Windows microphone privacy (Settings > Privacy & security > Microphone > "Let desktop apps access your microphone") can silently deliver zeros. The app reports "no input" only when the device has no input channels; detecting the privacy block is a follow-up.
- AAC/M4A import needs a Media Foundation decoder adapter (not implemented yet). WAV, AIFF, FLAC, Ogg and MP3 work through JUCE.
- ASIO is disabled (`JUCE_ASIO=0`): it requires the Steinberg ASIO SDK licence.

## Build (Visual Studio 2022, x64)

```bash
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DATMOS_BUILD_DESKTOP=ON
```

```bash
cmake --build build-win --config Release --target AtmosDesktop
```

Packaging for Windows 11 (MSIX or an installer) and code signing are planned for Phase 8.
