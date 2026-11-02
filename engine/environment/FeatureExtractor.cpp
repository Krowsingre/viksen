#include "engine/environment/FeatureExtractor.h"

#include <algorithm>
#include <cmath>

#include "engine/dsp/Smoother.h"

namespace atmos {

namespace {
float coeffForMs(double sampleRate, double ms) {
    return static_cast<float>(std::exp(-1.0 / std::max(1.0, sampleRate * ms / 1000.0)));
}
float msToDb(float meanSquare) { return meanSquare > 1e-14f ? 10.0f * std::log10(meanSquare) : -144.0f; }
} // namespace

void FeatureExtractor::prepare(double sampleRate) noexcept {
    sampleRate_ = sampleRate;
    fastCoeff_ = coeffForMs(sampleRate, 2.0);
    slowCoeff_ = coeffForMs(sampleRate, 250.0);
    levelCoeff_ = coeffForMs(sampleRate, 300.0);
    holdoffSamples_ = static_cast<int>(sampleRate * 0.08); // max ~12 onsets/second
    lowpass_.setup(Biquad::Type::LowPass, sampleRate, 150.0, 0.707);
    reset();
}

void FeatureExtractor::reset() noexcept {
    fastEnv_ = slowEnv_ = meanSquare_ = lowMeanSquare_ = 0.0f;
    holdoff_ = 0;
    lowpass_.reset();
}

void FeatureExtractor::process(const float* mono, int numFrames) noexcept {
    const float ratio = transientRatio_.load(std::memory_order_relaxed);
    const float floor = dbToGain(silenceDb_.load(std::memory_order_relaxed) + 10.0f);
    float peak = 0.0f;
    uint32_t transients = outTransients_.load(std::memory_order_relaxed);

    for (int i = 0; i < numFrames; ++i) {
        const float x = std::isfinite(mono[i]) ? mono[i] : 0.0f;
        const float a = std::fabs(x);
        peak = std::max(peak, a);
        fastEnv_ = a + fastCoeff_ * (fastEnv_ - a);
        slowEnv_ = a + slowCoeff_ * (slowEnv_ - a);
        meanSquare_ = x * x + levelCoeff_ * (meanSquare_ - x * x);
        const float lo = lowpass_.process(x);
        lowMeanSquare_ = lo * lo + levelCoeff_ * (lowMeanSquare_ - lo * lo);

        if (holdoff_ > 0) {
            --holdoff_;
        } else if (fastEnv_ > floor && fastEnv_ > ratio * slowEnv_) {
            ++transients;
            holdoff_ = holdoffSamples_;
        }
    }

    const float levelDb = msToDb(meanSquare_);
    outLevelDb_.store(levelDb, std::memory_order_relaxed);
    outPeak_.store(peak, std::memory_order_relaxed);
    outLowDb_.store(msToDb(lowMeanSquare_), std::memory_order_relaxed);
    outSilent_.store(levelDb < silenceDb_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    outTransients_.store(transients, std::memory_order_release);
}

EnvironmentFeatures FeatureExtractor::snapshot() const noexcept {
    EnvironmentFeatures f;
    f.transientCount = outTransients_.load(std::memory_order_acquire);
    f.levelDb = outLevelDb_.load(std::memory_order_relaxed);
    f.peak = outPeak_.load(std::memory_order_relaxed);
    f.lowLevelDb = outLowDb_.load(std::memory_order_relaxed);
    f.silent = outSilent_.load(std::memory_order_relaxed);
    return f;
}

} // namespace atmos
