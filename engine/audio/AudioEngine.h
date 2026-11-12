#pragma once
// Shared real-time audio engine. Platform hosts (iOS RemoteIO, JUCE device manager,
// offline renderer) call process() from their audio callback.
//
// Threading model:
//   * control thread: everything except process(). Calls must not race with each
//     other (call them from one thread, typically the UI/main thread).
//   * audio thread: process() only. It never allocates, locks, or does I/O.
//   Clips are handed over through a lock-free command queue and retired back
//   through a second queue; collectGarbage() frees them on the control thread.

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "engine/atmosphere/NoiseBed.h"
#include "engine/audio/AudioBuffer.h"
#include "engine/audio/CaptureTap.h"
#include "engine/audio/AudioClip.h"
#include "engine/audio/Parameter.h"
#include "engine/audio/SpscQueue.h"
#include "engine/audio/WorldStreamPlayer.h"
#include "engine/dsp/Limiter.h"
#include "engine/environment/FeatureExtractor.h"
#include "engine/environment/MicConditioner.h"
#include "engine/environment/MicTransform.h"
#include "engine/environment/RoomMemory.h"
#include "engine/routing/Bus.h"

namespace atmos {

struct EngineConfig {
    double sampleRate = 48000.0;
    int maxBlockSize = 1024;
};

struct EngineMeters {
    float outputPeak[2] = {0.0f, 0.0f};
    float busPeak[kNumBuses] = {};
    float limiterGain = 1.0f;     // 1 = no reduction
    int64_t positionFrames = 0;
    bool playing = false;
    bool micActive = false;       // engine is actually consuming mic input
    bool panicked = false;
    uint64_t blocksProcessed = 0;
    uint64_t underruns = 0;       // reported by the platform host
    uint64_t droppedCommands = 0;
};

class AudioEngine {
public:
    static constexpr int kMaxLayers = 64;

    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // ---- Lifecycle (control thread, audio callback must be stopped) ----
    void prepare(const EngineConfig& config);
    const EngineConfig& config() const noexcept { return config_; }
    bool isPrepared() const noexcept { return prepared_; }

    // ---- Layers (control thread) ----
    // Clip sample rate must equal the engine sample rate (resample on import).
    // startFrame positions the clip's first sample (including any leading silence)
    // on the timeline. Returns false on invalid slot/rate or full queue.
    bool setLayer(int slot, ClipPtr clip, Bus bus, int64_t startFrame = 0);
    bool clearLayer(int slot);
    void setLayerGain(int slot, float linearGain) noexcept;
    void setLayerMuted(int slot, bool muted) noexcept;
    int64_t timelineLengthFrames() const;   // end of the last loaded layer

    // ---- Buses / master (any thread) ----
    void setBusGain(Bus bus, float linearGain) noexcept;
    float busGain(Bus bus) const noexcept;
    void setBusMuted(Bus bus, bool muted) noexcept;
    bool busMuted(Bus bus) const noexcept;
    void setMasterGain(float linearGain) noexcept;
    float masterGain() const noexcept { return master_.get(); }

    // ---- Transport (any thread) ----
    void play() noexcept { playing_.store(true, std::memory_order_relaxed); }
    void pause() noexcept { playing_.store(false, std::memory_order_relaxed); }
    void seek(int64_t frame) noexcept { seekRequest_.store(frame < 0 ? 0 : frame, std::memory_order_relaxed); }
    void setLooping(bool loop) noexcept { looping_.store(loop, std::memory_order_relaxed); }

    // ---- Environment input (any thread) ----
    // When disabled the engine ignores input buffers entirely.
    void setMicInputEnabled(bool enabled) noexcept { micEnabled_.store(enabled, std::memory_order_relaxed); }
    bool micInputEnabled() const noexcept { return micEnabled_.load(std::memory_order_relaxed); }
    EnvironmentFeatures environmentFeatures() const noexcept { return features_.snapshot(); }
    MicTransform& micTransform() noexcept { return micTransform_; }
    // Replays recent seconds of the room (RAM only) into the transformed-mic bus.
    // Wiped automatically whenever the mic stops being consumed.
    RoomMemory& roomMemory() noexcept { return roomMemory_; }

    // ---- Explicit session recording tap ----
    CaptureTap& capture() noexcept { return capture_; }

    // ---- World music (streamed stems on the music bus) ----
    WorldStreamPlayer& worldPlayer() noexcept { return world_; }

    // ---- Atmosphere generator (any thread) ----
    void setAtmosphereBedEnabled(bool enabled) noexcept { bedEnabled_.store(enabled, std::memory_order_relaxed); }
    bool atmosphereBedEnabled() const noexcept { return bedEnabled_.load(std::memory_order_relaxed); }
    NoiseBed& atmosphereBed() noexcept { return bed_; }

    // ---- Safety (any thread) ----
    // Immediately silences output from the next block until clearPanic().
    void panic() noexcept { panic_.store(true, std::memory_order_relaxed); }
    void clearPanic() noexcept { panic_.store(false, std::memory_order_relaxed); }
    void setLimiterCeilingDb(float db) noexcept { limiterCeilingDb_.store(db, std::memory_order_relaxed); }

    // ---- Housekeeping ----
    void collectGarbage();                       // control thread; call periodically
    void reportUnderrun() noexcept { underruns_.fetch_add(1, std::memory_order_relaxed); }
    EngineMeters meters() const noexcept;

    // ---- Audio thread ----
    // inputs may be null / numInputs 0. Outputs beyond channel 1 are zeroed;
    // with one output channel, the stereo mix is folded to mono.
    void process(const float* const* inputs, int numInputs,
                 float* const* outputs, int numOutputs, int numFrames) noexcept;

private:
    struct Command {
        enum class Type : uint8_t { SetLayer, ClearLayer } type;
        int slot;
        const AudioClip* clip;
        Bus bus;
        int64_t startFrame;
    };

    struct LayerState {               // audio-thread owned
        const AudioClip* clip = nullptr;
        Bus bus = Bus::Music;
        int64_t startFrame = 0;
        SmoothedParameter gain{1.0f};
        std::atomic<bool> muted{false};
    };

    struct BusState {
        SmoothedParameter gain{0.0f};
        std::atomic<bool> muted{false};
    };

    void processBlock(const float* const* inputs, int numInputs,
                      float* const* outputs, int numOutputs, int numFrames) noexcept;
    void applyCommands() noexcept;
    void renderLayers(int numFrames, int64_t position) noexcept;
    void renderMic(const float* const* inputs, int numInputs, int numFrames) noexcept;
    void sumToMaster(int numFrames) noexcept;
    void writeOutputs(float* const* outputs, int numOutputs, int numFrames) noexcept;

    EngineConfig config_;
    bool prepared_ = false;

    std::array<LayerState, kMaxLayers> layers_;
    std::array<BusState, kNumBuses> buses_;
    SmoothedParameter master_{0.7f};
    SafetyLimiter limiter_;
    NoiseBed bed_;
    FeatureExtractor features_;
    MicConditioner micConditioner_;   // level control + limiter before the room becomes sound
    MicTransform micTransform_;
    RoomMemory roomMemory_;
    WorldStreamPlayer world_;
    CaptureTap capture_;
    AudioBuffer captureMix_;        // partial mixes for capture modes other than FullMix
    SafetyLimiter captureLimiter_;
    bool micWasActive_ = false;

    std::array<AudioBuffer, kNumBuses> busBuffers_;
    AudioBuffer mix_;
    std::vector<float> micMono_, micWet_, micWetR_;
    // Room blend ducking: the room steps back under loud music (0 = never, 1 = deeply).
    std::atomic<float> roomDuckDepth_{0.0f};   // the room leads: no ducking unless a song asks
    float duckEnvDb_ = -90.0f;
public:
    void setRoomDuckDepth(float d) noexcept { roomDuckDepth_.store(d, std::memory_order_relaxed); }
    float roomDuckDepth() const noexcept { return roomDuckDepth_.load(std::memory_order_relaxed); }
private:

    SpscQueue<Command, 256> commands_;
    SpscQueue<const AudioClip*, 512> retired_;

    // Control-thread bookkeeping: keeps clips alive while the audio thread may use them.
    mutable std::mutex ownedMutex_;
    std::vector<ClipPtr> owned_;
    std::array<int64_t, kMaxLayers> layerEnds_{};

    std::atomic<bool> playing_{false};
    std::atomic<bool> looping_{false};
    std::atomic<int64_t> seekRequest_{-1};
    std::atomic<bool> micEnabled_{false};
    std::atomic<bool> bedEnabled_{false};
    std::atomic<bool> panic_{false};
    std::atomic<float> limiterCeilingDb_{-1.0f};
    float appliedCeilingDb_ = -1.0f;

    int64_t position_ = 0;            // audio thread
    std::atomic<int64_t> publishedPosition_{0};
    std::atomic<int64_t> timelineEnd_{0};
    std::array<std::atomic<float>, 2> outPeak_{};
    std::array<std::atomic<float>, kNumBuses> busPeak_{};
    std::atomic<float> limiterGain_{1.0f};
    std::atomic<bool> micActive_{false};
    std::atomic<uint64_t> blocks_{0};
    std::atomic<uint64_t> underruns_{0};
    std::atomic<uint64_t> droppedCommands_{0};
};

} // namespace atmos
