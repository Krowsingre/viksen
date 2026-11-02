#include "AudioHost.h"

#include "core/assets/ClipPreparation.h"
#include "platform/desktop/DesktopAudioDefaults.h"
#include "platform/desktop/DesktopMediaDecoder.h"

AudioHost::AudioHost() : session_(48000.0, 4096) { formats_.registerBasicFormats(); }

AudioHost::~AudioHost() {
    alive_->store(false);
    decodePool_.removeAllJobs(true, 5000);
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
}

juce::String AudioHost::start() {
    const auto err = atmos::desktop::initialiseAudioDevices(deviceManager_, false);
    deviceManager_.addAudioCallback(this);
    return err;
}

int AudioHost::xruns() const {
    auto* d = deviceManager_.getCurrentAudioDevice();
    return d ? d->getXRunCount() : 0;
}

// Called before callbacks start (JUCE invokes this from the thread restarting the
// device, the message thread for every restart this app initiates). No audio
// callback is running, so the engine can be rebuilt for a new rate/block size.
void AudioHost::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    const double rate = device->getCurrentSampleRate();
    const int block = juce::jmax(device->getCurrentBufferSizeSamples(), 512);
    session_.rebuildIfNeeded(rate, juce::jmax(block, 4096));
}

void AudioHost::audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                                 int numOutputs, int numSamples,
                                                 const juce::AudioIODeviceCallbackContext&) {
    session_.engine().process(inputs, numInputs, outputs, numOutputs, numSamples);
}

void AudioHost::audioDeviceError(const juce::String& message) { lastDeviceError_ = message; }

void AudioHost::loadTrack(const juce::File& file, std::function<void(bool, juce::String)> done) {
    const double rate = session_.sampleRate();
    auto alive = alive_;
    decodePool_.addJob([this, file, rate, alive, done = std::move(done)] {
        auto decoded = std::make_shared<atmos::AudioClip>();
        atmos::core::AudioFileInfo info;
        std::string error;
        const bool ok = atmos::desktop::decodeAudioFile(formats_, file, *decoded, info, error);
        atmos::ClipPtr prepared = ok ? atmos::core::prepareClipForEngine(*decoded, rate) : nullptr;
        juce::MessageManager::callAsync([this, alive, ok, decoded, prepared, rate, error, file, done] {
            if (!alive->load()) return;
            if (!ok) { done(false, error); return; }
            if (!session_.setTrack(decoded, prepared, rate)) { done(false, "The engine rejected this file."); return; }
            trackName_ = file.getFileName();
            done(true, {});
        });
    });
}

void AudioHost::unloadTrack() {
    session_.clearTrack();
    trackName_.clear();
}

void AudioHost::setMicEnabled(bool enabled, std::function<void(bool, juce::String)> done) {
    if (!enabled) {
        session_.setMicEnabled(false);
        atmos::desktop::setInputEnabled(deviceManager_, false); // releases the OS microphone
        done(true, {});
        return;
    }
    auto alive = alive_;
    juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [this, alive, done](bool granted) {
        juce::MessageManager::callAsync([this, alive, granted, done] {
            if (!alive->load()) return;
            if (!granted) { done(false, "Microphone permission was not granted."); return; }
            const auto err = atmos::desktop::setInputEnabled(deviceManager_, true);
            if (err.isNotEmpty()) { done(false, err); return; }
            auto* device = deviceManager_.getCurrentAudioDevice();
            if (!device || device->getActiveInputChannels().countNumberOfSetBits() == 0) {
                done(false, "No microphone input is available on the selected audio device.");
                return;
            }
            session_.setMicEnabled(true);
            done(true, {});
        });
    });
}
