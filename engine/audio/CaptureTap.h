#pragma once
// Taps the engine output for explicit session recording. The audio thread
// pushes frames into a lock-free ring; a writer thread (core/session/
// SessionRecorder) drains it to disk. Nothing is captured unless a mode other
// than Off is set, and only while it is set.

#include <atomic>
#include <cstdint>
#include <vector>

namespace atmos {

enum class CaptureMode : uint8_t {
    Off = 0,
    FullMix = 1,        // exactly what the listener hears (after the safety limiter)
    WithoutMic = 2,     // music + atmosphere, no microphone buses
    AtmosphereOnly = 3, // atmosphere bus only
    MicOnly = 4,        // raw + transformed environment (explicit consent in the UI)
};

class CaptureTap {
public:
    void prepare(int ringFrames = 1 << 18); // ~5.5 s at 48 kHz
    void setMode(CaptureMode m) noexcept { mode_.store(static_cast<uint8_t>(m), std::memory_order_release); }
    CaptureMode mode() const noexcept { return static_cast<CaptureMode>(mode_.load(std::memory_order_acquire)); }

    // Audio thread.
    void push(const float* l, const float* r, int frames) noexcept;
    // Writer thread: copies up to maxFrames interleaved frames; returns count.
    int pop(float* interleaved, int maxFrames) noexcept;
    uint64_t overruns() const noexcept { return overruns_.load(std::memory_order_relaxed); }
    void resetCounters() noexcept { overruns_.store(0); }

private:
    std::vector<float> ring_;
    uint64_t capacity_ = 0;
    std::atomic<uint64_t> written_{0}, read_{0};
    std::atomic<uint8_t> mode_{0};
    std::atomic<uint64_t> overruns_{0};
};

} // namespace atmos
