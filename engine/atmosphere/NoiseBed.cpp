#include "engine/atmosphere/NoiseBed.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atmos {

float NoiseBed::Pink::next() noexcept {
    // xorshift64* -> uniform [-1, 1)
    state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
    const uint64_t r = state * 0x2545F4914F6CDD1Dull;
    const float white = static_cast<float>(static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0)) * 2.0f - 1.0f;
    // Paul Kellet's refined pink filter.
    b0 = 0.99886f * b0 + white * 0.0555179f;
    b1 = 0.99332f * b1 + white * 0.0750759f;
    b2 = 0.96900f * b2 + white * 0.1538520f;
    b3 = 0.86650f * b3 + white * 0.3104856f;
    b4 = 0.55000f * b4 + white * 0.5329522f;
    b5 = -0.7616f * b5 - white * 0.0168980f;
    const float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
    b6 = white * 0.115926f;
    return p * 0.11f;
}

void NoiseBed::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto* p : {&level, &colourHz, &driftDepth, &driftRateHz}) p->prepare(sampleRate, 0.1);
    for (auto& f : filter_) f.reset();
    lastColour_ = -1.0f;
    phase_ = 0.0;
}

void NoiseBed::setSeed(uint64_t seed) noexcept {
    pink_[0] = Pink{};
    pink_[1] = Pink{};
    pink_[0].state = seed ? seed : 1;
    pink_[1].state = (seed ^ 0xD1B54A32D192ED03ull) | 1;
    phase_ = 0.0;
}

void NoiseBed::processAdd(AudioBuffer& out, int numFrames) noexcept {
    level.beginBlock(); colourHz.beginBlock(); driftDepth.beginBlock(); driftRateHz.beginBlock();
    const float colour = colourHz.current();
    if (std::fabs(colour - lastColour_) > 0.5f) {
        for (auto& f : filter_) f.setup(Biquad::Type::LowPass, sampleRate_, colour, 0.6);
        lastColour_ = colour;
    }
    const int channels = std::min(2, out.numChannels());
    float* dst[2] = {channels > 0 ? out.channel(0) : nullptr, channels > 1 ? out.channel(1) : nullptr};
    const double twoPi = 2.0 * std::numbers::pi;
    for (int i = 0; i < numFrames; ++i) {
        colourHz.next();
        const float lvl = level.next();
        const float depth = std::clamp(driftDepth.next(), 0.0f, 1.0f);
        phase_ += driftRateHz.next() / sampleRate_;
        if (phase_ >= 1.0) phase_ -= 1.0;
        const float drift = 1.0f - depth * 0.5f * (1.0f + static_cast<float>(std::sin(twoPi * phase_)));
        for (int ch = 0; ch < 2; ++ch) {
            const float s = filter_[ch].process(pink_[ch].next()) * lvl * drift;
            if (dst[ch]) dst[ch][i] += s;
            else if (ch == 1 && dst[0]) dst[0][i] += s; // mono output: fold
        }
    }
}

} // namespace atmos
