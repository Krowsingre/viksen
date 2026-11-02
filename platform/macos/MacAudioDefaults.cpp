// macOS: Core Audio. Native Apple Silicon (arm64) and Intel via universal builds.
// Microphone access goes through TCC: JUCE's RuntimePermissions triggers the
// system prompt using NSMicrophoneUsageDescription; the hardened runtime needs
// the com.apple.security.device.audio-input entitlement (set in CMake).

#include "platform/desktop/DesktopAudioDefaults.h"

namespace atmos::desktop {

juce::String initialiseAudioDevices(juce::AudioDeviceManager& manager, bool withInput) {
    const juce::String err = manager.initialise(withInput ? 1 : 0, 2, nullptr, true);
    if (err.isNotEmpty()) return err;
    if (manager.getCurrentAudioDeviceType() != "CoreAudio") manager.setCurrentAudioDeviceType("CoreAudio", true);
    auto setup = manager.getAudioDeviceSetup();
    setup.sampleRate = 48000.0;
    setup.bufferSize = 256;
    // Built-in devices may already run at another rate shared with other apps;
    // the engine adapts to whatever the device actually uses.
    manager.setAudioDeviceSetup(setup, true);
    return {};
}

juce::String platformAudioApiDescription() { return "Core Audio"; }

juce::String lowLatencyAdvice() {
    return "For live monitoring use wired or USB headphones. AirPods and other Bluetooth headphones add "
           "100-300 ms, and macOS may switch them to a low-quality headset mode while the microphone is in use. "
           "Aggregate devices in Audio MIDI Setup can combine separate input and output interfaces.";
}

} // namespace atmos::desktop
