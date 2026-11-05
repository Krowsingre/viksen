#pragma once
// A small stereo reverb (Freeverb topology: parallel damped combs into series allpasses,
// right channel slightly detuned for width) for the room blend. Real-time safe after
// prepare(). Mono in, stereo wet out.

#include <cmath>
#include <vector>

namespace atmos {

class RoomReverb {
public:
    void prepare(double sampleRate) {
        const double s = sampleRate / 44100.0;
        static constexpr int kComb[kCombs] = {1116, 1188, 1277, 1356, 1422, 1491};
        static constexpr int kAll[kAllpasses] = {556, 441, 341};
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < kCombs; ++i) comb_[c][i].prepare(static_cast<int>((kComb[i] + c * kSpread) * s));
            for (int i = 0; i < kAllpasses; ++i) all_[c][i].prepare(static_cast<int>((kAll[i] + c * kSpread) * s));
        }
    }
    void reset() noexcept {
        for (auto& ch : comb_) for (auto& f : ch) f.reset();
        for (auto& ch : all_) for (auto& f : ch) f.reset();
    }
    // size 0..1 (decay), damp 0..1 (high-frequency absorption).
    void process(float in, float size, float damp, float& outL, float& outR) noexcept {
        const float fb = 0.70f + 0.28f * size;
        const float x = in * 0.03f;
        float o[2] = {0.0f, 0.0f};
        for (int c = 0; c < 2; ++c) {
            for (auto& f : comb_[c]) o[c] += f.process(x, fb, damp);
            for (auto& a : all_[c]) o[c] = a.process(o[c]);
        }
        outL = o[0];
        outR = o[1];
    }

private:
    static constexpr int kCombs = 6, kAllpasses = 3, kSpread = 23;
    struct Comb {
        std::vector<float> buf; size_t pos = 0; float store = 0.0f;
        void prepare(int n) { buf.assign(static_cast<size_t>(std::max(1, n)), 0.0f); pos = 0; store = 0.0f; }
        void reset() noexcept { std::fill(buf.begin(), buf.end(), 0.0f); store = 0.0f; }
        float process(float x, float fb, float damp) noexcept {
            const float y = buf[pos];
            store = y * (1.0f - damp) + store * damp;
            buf[pos] = x + store * fb;
            if (++pos >= buf.size()) pos = 0;
            return y;
        }
    };
    struct Allpass {
        std::vector<float> buf; size_t pos = 0;
        void prepare(int n) { buf.assign(static_cast<size_t>(std::max(1, n)), 0.0f); pos = 0; }
        void reset() noexcept { std::fill(buf.begin(), buf.end(), 0.0f); }
        float process(float x) noexcept {
            const float b = buf[pos];
            buf[pos] = x + b * 0.5f;
            if (++pos >= buf.size()) pos = 0;
            return b - x;
        }
    };
    Comb comb_[2][kCombs];
    Allpass all_[2][kAllpasses];
};

} // namespace atmos
