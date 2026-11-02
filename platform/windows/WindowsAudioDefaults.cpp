// Windows: WASAPI. Prefers JUCE's "Windows Audio (Low Latency Mode)" (IAudioClient3
// shared-mode low-latency periods) when available, then regular shared-mode WASAPI.
// Exclusive mode and ASIO are possible later; ASIO needs Steinberg's SDK licence.

#include "platform/desktop/DesktopAudioDefaults.h"

namespace atmos::desktop {

juce::String initialiseAudioDevices(juce::AudioDeviceManager& manager, bool withInput) {
    const juce::String err = manager.initialise(withInput ? 1 : 0, 2, nullptr, true);
    if (err.isNotEmpty()) return err;

    for (const char* preferred : {"Windows Audio (Low Latency Mode)", "Windows Audio"}) {
        for (auto* type : manager.getAvailableDeviceTypes()) {
            if (type->getTypeName() == preferred) {
                if (manager.getCurrentAudioDeviceType() != preferred) manager.setCurrentAudioDeviceType(preferred, true);
                auto setup = manager.getAudioDeviceSetup();
                setup.sampleRate = 48000.0;
                setup.bufferSize = 256;
                // Device may refuse; it then keeps its own rate/size, which the engine adapts to.
                manager.setAudioDeviceSetup(setup, true);
                return {};
            }
        }
    }
    return {};
}

juce::String platformAudioApiDescription() {
    return "WASAPI";
}

juce::String lowLatencyAdvice() {
    return "For live monitoring use wired headphones and \"Windows Audio (Low Latency Mode)\". "
           "Bluetooth headsets add 100-300 ms and often switch to a low-quality headset profile when the mic is on.";
}

} // namespace atmos::desktop
