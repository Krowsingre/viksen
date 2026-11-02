// Linux is a development/CI convenience target only (not a product platform).
#include "platform/desktop/DesktopAudioDefaults.h"

namespace atmos::desktop {

juce::String initialiseAudioDevices(juce::AudioDeviceManager& manager, bool withInput) {
    auto setup = juce::AudioDeviceManager::AudioDeviceSetup{};
    setup.sampleRate = 48000.0;
    setup.bufferSize = 256;
    return manager.initialise(withInput ? 1 : 0, 2, nullptr, true, {}, &setup);
}

juce::String platformAudioApiDescription() { return "ALSA/JACK (development build)"; }

juce::String lowLatencyAdvice() { return "Linux builds are for development only."; }

} // namespace atmos::desktop
