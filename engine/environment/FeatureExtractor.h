#pragma once
// Local, real-time environmental feature extraction from the microphone signal.
// Produces control signals only; never stores audio.

#include <atomic>
#include <cstdint>

#include "engine/dsp/Biquad.h"

namespace atmos {

struct EnvironmentFeatures {
    float levelDb = -144.0f;     // smoothed RMS level
    float peak = 0.0f;           // block peak (linear)
    float lowLevelDb = -144.0f;  // RMS below ~150 Hz
    bool silent = true;          // relative silence
    uint32_t transientCount = 0; // monotonically increasing onset counter
};

class FeatureExtractor {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Audio thread.
    void process(const float* mono, int numFrames) noexcept;

    // Any thread (lock-free snapshot).
    EnvironmentFeatures snapshot() const noexcept;

    void setSilenceThresholdDb(float db) noexcept { silenceDb_.store(db, std::memory_order_relaxed); }
    void setTransientSensitivity(float ratio) noexcept { transientRatio_.store(ratio, std::memory_order_relaxed); }

private:
    double sampleRate_ = 48000.0;
    float fastCoeff_ = 0.0f, slowCoeff_ = 0.0f, levelCoeff_ = 0.0f;
    float fastEnv_ = 0.0f, slowEnv_ = 0.0f, meanSquare_ = 0.0f, lowMeanSquare_ = 0.0f;
    int holdoff_ = 0, holdoffSamples_ = 0;
    Biquad lowpass_;

    std::atomic<float> silenceDb_{-55.0f};
    std::atomic<float> transientRatio_{4.0f};

    std::atomic<float> outLevelDb_{-144.0f};
    std::atomic<float> outPeak_{0.0f};
    std::atomic<float> outLowDb_{-144.0f};
    std::atomic<bool> outSilent_{true};
    std::atomic<uint32_t> outTransients_{0};
};

} // namespace atmos
