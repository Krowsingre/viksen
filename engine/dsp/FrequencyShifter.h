#pragma once
// Single-sideband frequency shifter: every partial moves by the same number of Hz,
// which makes harmonic sounds (voices, engines) inharmonic and "unreal".
// Analytic signal from a windowed-FIR Hilbert transformer (linear phase).
// Latency: kHalfTaps samples (127 samples, about 2.6 ms at 48 kHz).

#include <array>
#include <vector>

namespace atmos {

class FrequencyShifter {
public:
    static constexpr int kHalfTaps = 127;
    static constexpr int kTaps = 2 * kHalfTaps + 1;

    void prepare(double sampleRate);
    void reset() noexcept;
    void setShiftHz(float hz) noexcept { shiftHz_ = hz; }
    float process(float x) noexcept;
    int latencySamples() const noexcept { return kHalfTaps; }
    // The input sample delayed by latencySamples(), as used by the last process() call.
    float lastDelayedInput() const noexcept { return lastIn_; }

private:
    double sampleRate_ = 48000.0;
    float shiftHz_ = 0.0f;
    double phase_ = 0.0;
    std::array<float, kTaps> coeffs_{};
    std::vector<float> history_;   // doubled ring buffer for contiguous reads
    int pos_ = 0;
    float lastIn_ = 0.0f;
};

} // namespace atmos
