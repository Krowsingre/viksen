#pragma once
// Sample-accurate musical clock for Worlds: emits ticks at a fixed subdivision of
// the beat and wraps over a loop length, so cues can be placed on a grid
// ("on tick 0 of every 8-bar loop"). Audio thread; no allocation.

#include <atomic>
#include <cstdint>

namespace atmos {

struct TickEvent {
    int frameOffset;  // position inside the current block
    int tick;         // 0 .. loopTicks-1
};

class TickClock {
public:
    void prepare(double sampleRate) noexcept { sampleRate_ = sampleRate; reset(); }
    void reset() noexcept { framesToNext_ = 0.0; tick_ = -1; }

    // Any thread.
    void setTempo(double bpm, int ticksPerBeat = 8, int loopTicks = 256) noexcept {
        bpm_.store(bpm > 1.0 ? bpm : 1.0);
        ticksPerBeat_.store(ticksPerBeat > 0 ? ticksPerBeat : 1);
        loopTicks_.store(loopTicks > 0 ? loopTicks : 1);
    }

    // Writes up to maxEvents ticks falling in the next numFrames; returns the count.
    int advance(int numFrames, TickEvent* events, int maxEvents) noexcept {
        const double framesPerTick = sampleRate_ * 60.0 / (bpm_.load() * ticksPerBeat_.load());
        const int loop = loopTicks_.load();
        int count = 0;
        double t = framesToNext_;
        while (t < numFrames) {
            tick_ = (tick_ + 1) % loop;
            if (count < maxEvents) events[count++] = {static_cast<int>(t), tick_};
            t += framesPerTick;
        }
        framesToNext_ = t - numFrames;
        return count;
    }

    int currentTick() const noexcept { return tick_; }

private:
    double sampleRate_ = 48000.0;
    std::atomic<double> bpm_{120.0};
    std::atomic<int> ticksPerBeat_{8};
    std::atomic<int> loopTicks_{256};
    double framesToNext_ = 0.0;   // fractional, so long runs don't drift
    int tick_ = -1;
};

} // namespace atmos
