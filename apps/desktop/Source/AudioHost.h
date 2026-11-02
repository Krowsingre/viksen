#pragma once
// Desktop audio host: owns the JUCE device manager and the shared LiveSession,
// and drives the engine from the device callback. Message thread only, except
// the AudioIODeviceCallback methods.

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>
#include <memory>

#include "core/session/LiveSession.h"

class AudioHost final : private juce::AudioIODeviceCallback {
public:
    AudioHost();
    ~AudioHost() override;

    juce::String start();                 // opens devices (output only until the mic is enabled)
    juce::AudioDeviceManager& devices() noexcept { return deviceManager_; }
    atmos::core::LiveSession& session() noexcept { return session_; }
    atmos::AudioEngine& engine() noexcept { return session_.engine(); }

    // Decodes on a background thread; callback on the message thread.
    void loadTrack(const juce::File& file, std::function<void(bool ok, juce::String error)> done);
    void unloadTrack();
    juce::String trackName() const { return trackName_; }
    juce::String supportedWildcard() { return formats_.getWildcardForAllFormats(); }

    // Requests OS permission if needed (macOS), opens the input and enables the
    // environment path. Callback on the message thread.
    void setMicEnabled(bool enabled, std::function<void(bool ok, juce::String error)> done);
    bool micEnabled() const noexcept { return session_.settings().micEnabled; }

    juce::String lastDeviceError() const { return lastDeviceError_; }
    int xruns() const;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                          int numOutputs, int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override {}
    void audioDeviceError(const juce::String& message) override;

    juce::AudioDeviceManager deviceManager_;
    juce::AudioFormatManager formats_;
    juce::ThreadPool decodePool_{1};
    atmos::core::LiveSession session_;
    juce::String trackName_;
    juce::String lastDeviceError_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};
