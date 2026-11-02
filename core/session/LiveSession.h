#pragma once
// Control-side owner of a live AudioEngine, shared by every platform host
// (iOS bridge, desktop app). It mirrors user settings so the engine can be
// rebuilt when the device sample rate or block size changes (route change,
// device switch) without losing state, and keeps the original decoded track
// so it can be re-resampled for the new rate.
//
// Threading: all methods on one control thread. rebuild() must only be called
// while no audio callback is running.

#include <memory>

#include "engine/audio/AudioEngine.h"

namespace atmos::core {

struct LiveSettings {
    float busGain[kNumBuses] = {defaultBusGain(Bus::Music), defaultBusGain(Bus::Atmosphere),
                                defaultBusGain(Bus::MicRaw), defaultBusGain(Bus::MicTransformed)};
    bool busMuted[kNumBuses] = {};
    float masterGain = 0.7f;
    float limiterCeilingDb = -1.0f;
    bool bedEnabled = false;
    float bedLevel = 0.25f;
    float bedColourHz = 900.0f;
    float micToneHz = 1800.0f;
    float micEchoSeconds = 0.38f;
    float micFeedback = 0.45f;
    float micWet = 0.6f;
    bool micEnabled = false;
    bool looping = false;
};

class LiveSession {
public:
    static constexpr int kTrackSlot = 0;

    explicit LiveSession(double sampleRate = 48000.0, int maxBlockSize = 4096);

    AudioEngine& engine() noexcept { return *engine_; }
    double sampleRate() const noexcept { return engine_->config().sampleRate; }
    int maxBlockSize() const noexcept { return engine_->config().maxBlockSize; }

    // Recreates the engine at a new configuration and restores settings and track.
    // The previous engine is destroyed: hosts must stop callbacks first.
    void rebuild(double sampleRate, int maxBlockSize);
    // Rebuilds only if the configuration actually differs. Returns true if rebuilt.
    bool rebuildIfNeeded(double sampleRate, int maxBlockSize);

    // `original` is the decoded file at its native rate. `prepared` may be a clip
    // already resampled to `preparedRate` off-thread; it is used if that rate is
    // still current, otherwise the original is resampled here.
    bool setTrack(std::shared_ptr<const AudioClip> original, ClipPtr prepared = nullptr, double preparedRate = 0.0);
    void clearTrack();
    bool hasTrack() const noexcept { return original_ != nullptr; }
    double trackDurationSeconds() const noexcept;

    const LiveSettings& settings() const noexcept { return settings_; }
    void setBusGain(Bus bus, float gain);
    void setBusMuted(Bus bus, bool muted);
    void setMasterGain(float gain);
    void setLimiterCeilingDb(float db);
    void setBedEnabled(bool enabled);
    void setBedLevel(float level);
    void setBedColourHz(float hz);
    void setMicToneHz(float hz);
    void setMicEchoSeconds(float seconds);
    void setMicFeedback(float feedback);
    void setMicWet(float wet);
    void setMicEnabled(bool enabled);
    void setLooping(bool looping);

private:
    void applySettings();

    std::unique_ptr<AudioEngine> engine_;
    LiveSettings settings_;
    std::shared_ptr<const AudioClip> original_;
};

} // namespace atmos::core
