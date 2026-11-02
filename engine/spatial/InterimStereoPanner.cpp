#include "engine/spatial/InterimStereoPanner.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atmos {

void InterimStereoPanner::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;
    maxItdSamples_ = static_cast<float>(sampleRate * 0.00066); // ~0.66 ms max ITD
    delayL_.prepare(static_cast<int>(maxItdSamples_) + 4);
    delayR_.prepare(static_cast<int>(maxItdSamples_) + 4);
    for (auto* p : {&panX_, &rearness_, &distGain_}) p->prepare(sampleRate, 0.03);
    shadowL_.setup(Biquad::Type::LowPass, sampleRate, 18000.0, 0.707);
    shadowR_.setup(Biquad::Type::LowPass, sampleRate, 18000.0, 0.707);
    reset();
}

void InterimStereoPanner::reset() noexcept {
    delayL_.reset(); delayR_.reset(); shadowL_.reset(); shadowR_.reset();
}

void InterimStereoPanner::setPosition(const SpatialPosition& p) noexcept {
    const float az = p.azimuthDeg * static_cast<float>(std::numbers::pi) / 180.0f;
    const float x = std::sin(az) * (1.0f - std::clamp(p.spread, 0.0f, 1.0f));
    panX_.set(x);
    rearness_.set(std::clamp(-std::cos(az), 0.0f, 1.0f));
    distGain_.set(kReferenceDistance / std::max(kReferenceDistance, p.distanceM));
}

void InterimStereoPanner::processAdd(const float* mono, AudioBuffer& out, int numFrames) noexcept {
    if (out.numChannels() < 2) return;
    panX_.beginBlock(); rearness_.beginBlock(); distGain_.beginBlock();

    // Head shadow on the far ear: update per block from the block-start position.
    const float x0 = panX_.current();
    const double farCut = 18000.0 - 14000.0 * std::fabs(x0);
    const double rearCut = 1.0 - 0.4 * rearness_.current();
    shadowL_.setup(Biquad::Type::LowPass, sampleRate_, x0 > 0 ? farCut * rearCut : 18000.0 * rearCut, 0.707);
    shadowR_.setup(Biquad::Type::LowPass, sampleRate_, x0 < 0 ? farCut * rearCut : 18000.0 * rearCut, 0.707);

    float* L = out.channel(0);
    float* R = out.channel(1);
    const float halfPi = static_cast<float>(std::numbers::pi) * 0.5f;
    for (int i = 0; i < numFrames; ++i) {
        const float x = panX_.next();
        rearness_.next();
        const float g = distGain_.next();
        // Equal-power law over x in [-1, 1].
        const float theta = (x + 1.0f) * 0.5f * halfPi;
        const float gl = std::cos(theta) * g;
        const float gr = std::sin(theta) * g;
        // Far ear is delayed.
        const float itd = std::fabs(x) * maxItdSamples_;
        delayL_.push(mono[i]);
        delayR_.push(mono[i]);
        const float sl = delayL_.read(x > 0 ? itd : 0.0f);
        const float sr = delayR_.read(x < 0 ? itd : 0.0f);
        L[i] += shadowL_.process(sl) * gl;
        R[i] += shadowR_.process(sr) * gr;
    }
}

} // namespace atmos
