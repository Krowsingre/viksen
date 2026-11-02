#pragma once
// Plays up to kMaxStems synchronized, streamed stereo stems (a World's music).
//
// A feeder thread (see core/worlds/WorldFeeder) decodes the same frame range of
// every stem and writes it into per-stem lock-free rings. The audio thread only
// consumes frames when EVERY stem has them, so stems can never drift apart; if
// the feeder falls behind, the block is silent and an underrun is counted.
//
// Flushing (seek / restart) is lock-free too: the feeder publishes, per stem,
// the ring position where the new material begins; the audio thread jumps every
// read position there at the start of its next block.

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "engine/audio/AudioBuffer.h"
#include "engine/audio/Parameter.h"

namespace atmos {

class WorldStreamPlayer {
public:
    static constexpr int kMaxStems = 8;

    // Control thread, with the audio callback stopped or the player inactive.
    void prepare(double sampleRate, int ringFrames = 1 << 17); // ~2.7 s at 48 kHz (1 MB per stem)
    void setStemCount(int n) noexcept { stemCount_.store(n < 0 ? 0 : (n > kMaxStems ? kMaxStems : n)); }
    int stemCount() const noexcept { return stemCount_.load(); }

    // Any thread.
    void setActive(bool a) noexcept { active_.store(a, std::memory_order_relaxed); }
    bool active() const noexcept { return active_.load(std::memory_order_relaxed); }
    void setStemGain(int stem, float linear) noexcept;
    float stemPeak(int stem) const noexcept;
    uint64_t underruns() const noexcept { return underruns_.load(std::memory_order_relaxed); }
    uint64_t framesPlayed() const noexcept { return played_.load(std::memory_order_relaxed); } // since last flush

    // Feeder thread (single producer).
    int writableFrames() const noexcept;                       // min free space over all stems
    int readableFrames() const noexcept;                       // min buffered over all stems (any thread)
    void write(int stem, const float* interleavedLR, int frames) noexcept; // caller writes the same count to every stem
    void requestFlush() noexcept;                              // new material starts at the current write positions

    // Audio thread: adds into the first two channels of `out`.
    void processAdd(AudioBuffer& out, int numFrames) noexcept;

private:
    struct Stem {
        std::vector<float> ring;               // interleaved stereo, capacity_ frames
        std::atomic<uint64_t> written{0};      // frames ever written (feeder)
        std::atomic<uint64_t> read{0};         // frames ever read (audio)
        std::atomic<uint64_t> flushTo{0};      // read position to jump to on flush
        SmoothedParameter gain{1.0f};
        std::atomic<float> peak{0.0f};
    };

    std::array<Stem, kMaxStems> stems_;
    int capacity_ = 0;                         // frames, power of two
    double sampleRate_ = 48000.0;
    std::atomic<int> stemCount_{0};
    std::atomic<bool> active_{false};
    std::atomic<uint32_t> flushRequests_{0};
    uint32_t flushesSeen_ = 0;                 // audio thread
    std::atomic<uint64_t> underruns_{0};
    std::atomic<uint64_t> played_{0};
};

} // namespace atmos
