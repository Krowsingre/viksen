#pragma once
// A light real-time pitch shifter for the room: two read taps sweep through a short delay
// line, each faded in and out with a sine window, half a window apart, so the sound is
// heard at `ratio` times its pitch (0.5 = an octave down, 1.5 = a fifth up) with no change
// in timing. Artefacts are a soft shimmer, which suits the room's dreamlike sounds.
// Audio thread only. Header-only.

#include <algorithm>
#include <cmath>
#include <vector>

namespace atmos {

class PitchShifter {
public:
    void prepare(double sampleRate, double windowSeconds = 0.05) {
        window_ = std::max(64, static_cast<int>(sampleRate * windowSeconds));
        buf_.assign(static_cast<size_t>(window_ * 2 + 4), 0.0f);
        write_ = 0;
        phase_ = 0.0;
    }
    void reset() noexcept { std::fill(buf_.begin(), buf_.end(), 0.0f); phase_ = 0.0; }
    void setRatio(double r) noexcept { ratio_ = std::clamp(r, 0.25, 4.0); }

    float process(float x) noexcept {
        const int n = static_cast<int>(buf_.size());
        buf_[static_cast<size_t>(write_)] = x;
        // The taps' delay changes by (1 - ratio) samples per sample, wrapping over the window.
        phase_ += (1.0 - ratio_) / window_;
        phase_ -= std::floor(phase_);
        float y = 0.0f;
        for (double p : {phase_, phase_ + 0.5 - std::floor(phase_ + 0.5)}) {
            const double delay = 1.0 + p * window_;
            double pos = write_ - delay;
            while (pos < 0) pos += n;
            const int i0 = static_cast<int>(pos), i1 = (i0 + 1) % n;
            const float frac = static_cast<float>(pos - i0);
            const float s = buf_[static_cast<size_t>(i0)] * (1 - frac) + buf_[static_cast<size_t>(i1)] * frac;
            y += s * static_cast<float>(std::sin(3.14159265358979 * p));   // sin windows of the two taps sum to ~1
        }
        if (++write_ >= n) write_ = 0;
        return y * 0.75f;   // keep the level close to the input's (the two windows overlap)
    }

private:
    std::vector<float> buf_;
    int window_ = 2400, write_ = 0;
    double phase_ = 0.0, ratio_ = 1.0;
};

} // namespace atmos
