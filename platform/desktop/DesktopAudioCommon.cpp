#include "platform/desktop/DesktopAudioDefaults.h"

namespace atmos::desktop {

juce::String setInputEnabled(juce::AudioDeviceManager& manager, bool enabled) {
    auto setup = manager.getAudioDeviceSetup();
    setup.inputChannels.clear();
    setup.useDefaultInputChannels = false;
    if (enabled) setup.inputChannels.setRange(0, 1, true); // first input (mono environment capture)
    return manager.setAudioDeviceSetup(setup, true);
}

} // namespace atmos::desktop
