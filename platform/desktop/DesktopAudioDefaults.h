#pragma once
// Per-OS audio device policy for the JUCE desktop app. Implemented once per
// platform (platform/windows, platform/macos, platform/desktop for Linux dev).

#include <juce_audio_devices/juce_audio_devices.h>

namespace atmos::desktop {

// Initialises the device manager with the platform's preferred driver type,
// 48 kHz and a low-latency buffer. `withInput` opens one input channel.
// Returns an empty string on success, otherwise the error.
juce::String initialiseAudioDevices(juce::AudioDeviceManager& manager, bool withInput);

// Opens or closes the input channels on the current device without changing the
// output configuration.
juce::String setInputEnabled(juce::AudioDeviceManager& manager, bool enabled);

// Human-readable name of the platform audio API family (for Settings).
juce::String platformAudioApiDescription();

// Platform guidance shown when live monitoring latency matters.
juce::String lowLatencyAdvice();

} // namespace atmos::desktop
