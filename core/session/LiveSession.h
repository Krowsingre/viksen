#pragma once
// Control-side owner of a live AudioEngine, shared by every platform host
// (iOS bridge, desktop app). It mirrors user settings so the engine can be
// rebuilt when the device sample rate or block size changes (route change,
// device switch) without losing state, and keeps the original decoded track
// so it can be re-resampled for the new rate.
//
// Threading: all methods on one control thread. rebuild() must only be called
// while no audio callback is running.

#include <functional>
#include <optional>
#include <memory>
#include <string>
#include <vector>

#include "engine/audio/AudioEngine.h"
#include "engine/environment/MotionAnalyzer.h"
#include "engine/reactive/EnvironmentStates.h"
#include "core/worlds/WorldDirector.h"
#include "core/worlds/WorldFeeder.h"
#include "core/worlds/WorldLifecycle.h"
#include "core/session/SessionRecorder.h"

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

// Creates decoders for every stem of a World, producing audio at `rate`.
// Returns an empty vector (and sets error) on failure.
using StemDecoderFactory =
    std::function<std::vector<std::unique_ptr<StemDecoder>>(double rate, std::string& error)>;

struct WorldStemStatus {
    std::string id, title;
    float azimuthDeg = 0, elevationDeg = 0, distanceM = 1.5f, widthDeg = 60;
    float gainDb = -60.0f;
    float peak = 0.0f;
    std::vector<std::string> reasons;
};

struct WorldStatus {
    bool loaded = false;
    bool playing = false;           // fading in, active or fading out
    std::string id, name;
    double positionSeconds = 0.0, lengthSeconds = 0.0;
    float master = 0.0f;
    uint64_t underruns = 0;
    bool spatial = false;           // 3D headphone mode on
    bool hrtf = false;              // true: Steam Audio HRTF; false: interim panner
    bool adaptive = false;          // the Dimension has adaptive sections
    bool composed = true;           // no reactive input matched: playing as written
    std::string sectionTitle;       // part of the song being heard now
    std::string sectionLabel;       // why it is playing ("You are moving")
    std::string songTitle;          // the song being heard (Dimensions can hold several)
    std::string songLunarPhase;     // the lunar phase the playing song belongs to ("" if none)
    int songIndex = 0, songCount = 1;
    uint64_t songSwitches = 0;
    uint64_t sectionJumps = 0;
    std::vector<WorldStemStatus> stems;
};

class LiveSession {
public:
    static constexpr double kStopFadeSeconds = 1.0;   // the listener pressed stop
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
    // Replaces all settings at once (e.g. opening a project). Keeps the mic state.
    void applyAllSettings(const LiveSettings& s);
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
    // Whether the loop point uses the jump transition (echo/reverb wash and sweep) or a
    // clean short crossfade. On by default; applies at once to the playing Dimension.
    void setLoopTransitionFx(bool on);
    bool loopTransitionFx() const { return loopFx_; }
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

    // ---- Worlds (built-in music re-mixed by the surroundings) ----
    bool loadWorld(const WorldManifest& world, StemDecoderFactory factory, std::string* error = nullptr);
    // A Dimension with one or more songs; `decode` opens one stem file (relative to the
    // Dimension folder) at the engine rate.
    using StemFileDecoder = std::function<std::unique_ptr<StemDecoder>(const std::string& file, double rate, std::string& error)>;
    bool loadDimension(const DimensionManifest& dimension, StemFileDecoder decode, std::string* error = nullptr);
    void unloadWorld();
    void startWorld(bool quickFade = false);  // fades in (quick: 150 ms, for transport play)
    void stopWorld(bool quickFade = false);   // fades out, then stops streaming (position kept)
    void seekWorld(double seconds);           // jump; keeps playing if it was playing
    // Mixer (Studio): user level per stem, used when no rule drives the stem.
    void setWorldStemLevel(int stem, float gainDb, bool muted);
    // Call regularly (e.g. 30 Hz): advances rule fades and the lifecycle.
    void updateWorld(double dtSeconds, const ConditionContext& context);
    WorldStatus worldStatus() const;
    // 3D headphone rendering of the World's stems (kept across rebuilds).
    void setWorldSpatialEnabled(bool on);
    bool worldSpatialEnabled() const { return worldSpatial_; }
    // Moves a stem (spatial canvas). Kept until the World is unloaded.
    void setWorldStemPosition(int stem, float azimuthDeg, float elevationDeg, float distanceM);
    // Restores every stem to the position authored in world.json (no interruption).
    void resetWorldStemPositions();
    ~LiveSession();

    // ---- Explicit session recording (24-bit WAV) ----
    bool startRecording(const std::filesystem::path& file, CaptureMode mode, std::string* error = nullptr);
    double stopRecording();                    // seconds recorded
    bool recording() const { return recorder_.recording(); }
    double recordingSeconds() const { return recorder_.seconds(); }
    uint64_t recordingDroppedBlocks() const { return recorder_.droppedBlocks(); }
    // True once after a device change ended a recording (the file is complete).
    bool takeRecordingInterrupted() { bool v = recordingInterrupted_; recordingInterrupted_ = false; return v; }

private:
    void applySettings();
    void stopFeeder();
    bool startFeeder(int64_t fromFrame, std::string* error);

    std::unique_ptr<AudioEngine> engine_;
    LiveSettings settings_;
    std::shared_ptr<const AudioClip> original_;
    EnvironmentStates states_;
    MotionAnalyzer motion_;
    float listenerYaw_ = 0.0f;

    std::optional<WorldManifest> world_;
    std::optional<WorldManifest> worldAuthored_;   // as loaded, for "reset layout"
    StemDecoderFactory worldFactory_;
    std::unique_ptr<WorldFeeder> feeder_;
    std::shared_ptr<SectionPlanner> planner_;
    SectionChoice lastChoice_;
    float transitionSpace_ = 0.5f;
    bool loopFx_ = true;
    // Multi-song Dimensions.
    std::optional<DimensionManifest> dimension_;
    StemFileDecoder fileDecoder_;
    int song_ = 0, switchTo_ = -1, candidate_ = -1;
    ConditionContext lastContext_;     // the most recent conditions (to start a Dimension on the right song)
    bool haveContext_ = false;
    int lunarTarget(const ConditionContext& c) const;   // -1: the Dimension has no lunar songs
    bool hasLunarSongs() const;
    void applyRoomBlend(const WorldManifest& w);
    double candidateFor_ = 0, switchFromSeconds_ = 0;
    uint64_t songSwitches_ = 0;
    bool loadSong(const WorldManifest& world, StemDecoderFactory factory, std::string* error);
    StemDecoderFactory songFactory(int song) const;
    void updateSongChoice(double dt, const ConditionContext& context);
    void beginSongSwitch(int song, const ConditionContext& context, bool immediate);
    void finishSongSwitch();
    std::unique_ptr<WorldDirector> director_;
    std::unique_ptr<WorldLifecycle> lifecycle_;
    int64_t resumeFrame_ = 0;
    bool worldSpatial_ = true;
    SessionRecorder recorder_;
    bool recordingInterrupted_ = false;
    void applyWorldSpatial();
};

} // namespace atmos::core
