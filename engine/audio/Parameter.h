#pragma once
// Thread-safe parameter: any thread writes the target atomically; the audio thread
// reads it once per block and smooths per sample.

#include <atomic>

#include "engine/dsp/Smoother.h"

namespace atmos {

class SmoothedParameter {
public:
    explicit SmoothedParameter(float initial = 0.0f) noexcept : target_(initial) { smoother_.reset(initial); }

    // Any thread.
    void set(float value) noexcept { target_.store(value, std::memory_order_relaxed); }
    float get() const noexcept { return target_.load(std::memory_order_relaxed); }

    // Audio thread only.
    void prepare(double sampleRate, double rampSeconds = 0.02) noexcept {
        smoother_.prepare(sampleRate, rampSeconds);
        smoother_.reset(get());
    }
    void resetTo(float value) noexcept { smoother_.reset(value); }
    void beginBlock() noexcept { smoother_.setTarget(get()); }
    float next() noexcept { return smoother_.next(); }
    bool isSmoothing() const noexcept { return smoother_.isSmoothing(); }
    float current() const noexcept { return smoother_.current(); }

private:
    std::atomic<float> target_;
    LinearSmoother smoother_;
};

} // namespace atmos
