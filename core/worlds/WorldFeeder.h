#pragma once
// Background thread that keeps a WorldStreamPlayer's rings full: it decodes the
// same frame range of every stem (looping between the loop points) and writes
// it to the player. Decoding is a platform concern (AVFoundation on iOS); the
// feeder only sees the StemDecoder interface.

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "engine/audio/AudioClip.h"
#include "engine/audio/WorldStreamPlayer.h"
#include "core/worlds/WorldSections.h"
#include "core/worlds/TransitionFx.h"

namespace atmos::core {

class StemDecoder {
public:
    virtual ~StemDecoder() = default;
    // Output is interleaved stereo float at outputRate(). Mono sources are duplicated.
    virtual double outputRate() const = 0;
    virtual int64_t lengthFrames() const = 0;      // at outputRate()
    virtual bool seek(int64_t frame) = 0;
    virtual int read(float* interleavedLR, int frames) = 0; // frames actually read
};

// In-memory decoder (tests, small assets). The clip must be at the engine rate.
class ClipStemDecoder final : public StemDecoder {
public:
    explicit ClipStemDecoder(ClipPtr clip) : clip_(std::move(clip)) {}
    double outputRate() const override { return clip_->sampleRate; }
    int64_t lengthFrames() const override { return clip_->numFrames(); }
    bool seek(int64_t f) override { pos_ = std::clamp<int64_t>(f, 0, lengthFrames()); return true; }
    int read(float* lr, int frames) override;
private:
    ClipPtr clip_;
    int64_t pos_ = 0;
};

class WorldFeeder {
public:
    // `planner` (optional) chooses where the stream continues at grid boundaries
    // (adaptive sections); jumps crossfade over `crossfadeFrames`.
    WorldFeeder(WorldStreamPlayer& player, std::vector<std::unique_ptr<StemDecoder>> decoders,
                int64_t loopStart, int64_t loopEnd, std::shared_ptr<SectionPlanner> planner = nullptr,
                int64_t crossfadeFrames = 0);
    ~WorldFeeder();
    WorldFeeder(const WorldFeeder&) = delete;
    WorldFeeder& operator=(const WorldFeeder&) = delete;

    // Starts streaming from `frame`. The player becomes active once ~0.5 s is buffered.
    void start(int64_t frame);
    void stop();                                   // joins the thread; player deactivated
    bool running() const { return running_.load(); }
    // Source position of the audio currently being heard (approximate, frames).
    int64_t playbackFrame() const;
    int64_t loopStart() const { return loopStart_; }
    int64_t loopEnd() const { return loopEnd_; }
    double rate() const { return rate_; }
    uint64_t jumps() const { return jumps_.load(); }   // boundary jumps/loops made (diagnostics)
    // The song reached its end and the planner asked to stop (multi-song Dimensions);
    // true once the listener has heard the end.
    bool finishedPlaying() const;
    // Organic transitions on section jumps (submerge sweep + echo/reverb wash).
    // Call before start(); `space` can change any time (the moon sets it).
    void enableTransitionFx(double beatSeconds) { fxBeat_ = beatSeconds; fxOn_ = true; }
    void setTransitionSpace(float space) { fxSpace_.store(space); }
    // Whether the loop (song end back to its start) uses the same transition as a jump.
    // Off: a clean 8 ms crossfade at the loop point, no echo/reverb or sweep, and almost
    // nothing past the loop end is heard. Can change any time (owner's test switch).
    void setLoopFx(bool on) { loopFx_.store(on); }

private:
    void run();
    void noteSegment(uint64_t written, int64_t source);

    WorldStreamPlayer& player_;
    std::vector<std::unique_ptr<StemDecoder>> decoders_;
    int64_t loopStart_, loopEnd_;
    double rate_ = 48000.0;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> startFrame_{0};
    std::thread thread_;
    std::shared_ptr<SectionPlanner> planner_;
    int64_t crossfade_ = 0;
    std::atomic<uint64_t> jumps_{0};
    std::atomic<uint64_t> endWritten_{UINT64_MAX};   // frames written when the song ended
    bool fxOn_ = false;
    double fxBeat_ = 0.5;
    std::atomic<float> fxSpace_{0.5f};
    std::atomic<bool> loopFx_{true};
    // Map from frames written to the ring (= frames the player will play) to source
    // frames, one entry per contiguous run. Feeder appends, control thread reads.
    mutable std::mutex segMutex_;
    std::vector<std::pair<uint64_t, int64_t>> segments_;
};

} // namespace atmos::core
