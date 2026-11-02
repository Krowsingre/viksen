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
#include "engine/environment/MotionAnalyzer.h"
#include "engine/reactive/EnvironmentStates.h"

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
    float micShiftHz = -180.0f;
    float micShiftMix = 0.0f;
    bool roomMemoryEnabled = false;
    float roomMemoryLevel = 0.6f;
    float roomMemoryPitch = -12.0f;
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
    void setMicShiftHz(float hz);
    void setMicShiftMix(float mix);
    void setRoomMemoryEnabled(bool enabled);
    void setRoomMemoryLevel(float level);
    void setRoomMemoryPitch(float semitones);

    // Environment states (control thread). Call regularly, e.g. from the UI's meter timer.
    void updateEnvironment(double dtSeconds);
    // Accelerometer samples in g from the platform (e.g. Core Motion on iPhone).
    void addMotionSample(double ax, double ay, double az, double dtSeconds);
    void setMotionAvailable(bool available);
    // Head direction from head-tracking headphones (degrees, + = right). Not persisted.
    void setListenerYaw(float degrees);
    EnvironmentStateSnapshot environmentStates() const { return states_.snapshot(); }
    unsigned freeFallEvents() const { return motion_.freeFallEvents(); }

private:
    void applySettings();

    std::unique_ptr<AudioEngine> engine_;
    LiveSettings settings_;
    std::shared_ptr<const AudioClip> original_;
    EnvironmentStates states_;
    MotionAnalyzer motion_;
    float listenerYaw_ = 0.0f;
};

} // namespace atmos::core
