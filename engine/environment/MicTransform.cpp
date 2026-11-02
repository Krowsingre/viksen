#include "engine/environment/MicTransform.h"

#include <algorithm>
#include <cmath>

namespace atmos {

void MicTransform::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;
    for (auto* p : {&cutoffHz, &echoSeconds, &feedback, &wet, &shiftHz, &shiftMix}) p->prepare(sampleRate, 0.05);
    shifter_.prepare(sampleRate);
    delay_.prepare(static_cast<int>(sampleRate * 2.0));
    lastCutoff_ = -1.0f;
    reset();
}

void MicTransform::reset() noexcept {
    tone_.reset();
    delay_.reset();
    shifter_.reset();
}

void MicTransform::process(const float* in, float* out, int numFrames) noexcept {
    cutoffHz.beginBlock(); echoSeconds.beginBlock(); feedback.beginBlock(); wet.beginBlock();
    shiftHz.beginBlock(); shiftMix.beginBlock();
    // Cutoff is updated once per block (cheap and click-free enough for a tone control).
    const float cutoff = cutoffHz.current();
    if (std::fabs(cutoff - lastCutoff_) > 0.5f) {
        tone_.setup(Biquad::Type::LowPass, sampleRate_, cutoff, 0.707);
        lastCutoff_ = cutoff;
    }
    const float maxDelay = static_cast<float>(delay_.maxDelay());
    for (int i = 0; i < numFrames; ++i) {
        cutoffHz.next();
        const float d = std::clamp(echoSeconds.next() * static_cast<float>(sampleRate_), 1.0f, maxDelay);
        const float fb = std::clamp(feedback.next(), 0.0f, 0.95f);
        const float w = std::clamp(wet.next(), 0.0f, 1.0f);
        shifter_.setShiftHz(shiftHz.next());
        const float m = std::clamp(shiftMix.next(), 0.0f, 1.0f);
        const float toned = tone_.process(in[i]);
        // Shifter always runs so its history stays continuous; the dry side is
        // taken from its internally delayed input, keeping both sides aligned.
        const float shifted = shifter_.process(toned);
        const float aligned = shifter_.lastDelayedInput();
        const float filtered = (1.0f - m) * aligned + m * shifted;
        const float echo = delay_.read(d - 1.0f);
        delay_.push(filtered + echo * fb);
        out[i] = (1.0f - w) * filtered + w * (filtered + echo) * 0.7f;
    }
}

} // namespace atmos
