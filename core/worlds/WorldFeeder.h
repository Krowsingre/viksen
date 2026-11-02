#pragma once
// Background thread that keeps a WorldStreamPlayer's rings full: it decodes the
// same frame range of every stem (looping between the loop points) and writes
// it to the player. Decoding is a platform concern (AVFoundation on iOS); the
// feeder only sees the StemDecoder interface.

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "engine/audio/AudioClip.h"
#include "engine/audio/WorldStreamPlayer.h"

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
    WorldFeeder(WorldStreamPlayer& player, std::vector<std::unique_ptr<StemDecoder>> decoders,
                int64_t loopStart, int64_t loopEnd);
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

private:
    void run();

    WorldStreamPlayer& player_;
    std::vector<std::unique_ptr<StemDecoder>> decoders_;
    int64_t loopStart_, loopEnd_;
    double rate_ = 48000.0;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> startFrame_{0};
    std::thread thread_;
};

} // namespace atmos::core
