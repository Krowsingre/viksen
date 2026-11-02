#include "core/worlds/TransitionFx.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace atmos::core {
namespace {
constexpr double kPi = 3.14159265358979323846;

float onePoleCoeff(double hz, double rate) { return static_cast<float>(1.0 - std::exp(-2.0 * kPi * hz / rate)); }

struct Comb {
    std::vector<float> buf;
    size_t i = 0;
    float store = 0, feedback = 0.8f, damp = 0.3f;
    float process(float x) {
        const float y = buf[i];
        store = y * (1.0f - damp) + store * damp;
        buf[i] = x + store * feedback;
        i = (i + 1) % buf.size();
        return y;
    }
};

struct Allpass {
    std::vector<float> buf;
    size_t i = 0;
    float process(float x) {
        const float b = buf[i];
        const float y = -x + b;
        buf[i] = x + b * 0.5f;
        i = (i + 1) % buf.size();
        return y;
    }
};
} // namespace

std::vector<float> renderTransitionTail(const float* in, int n, double rate, const TransitionShape& shape) {
    const float space = std::clamp(shape.space, 0.0f, 1.0f);
    const double tailSeconds = 3.0 + 3.0 * space;
    const int total = n + static_cast<int>(tailSeconds * rate);
    std::vector<float> wet(static_cast<size_t>(total) * 2, 0.0f);

    // Echo: dotted-eighth, ping-pong, darkening on every repeat.
    const double echoSec = std::clamp(shape.beatSeconds * 0.75, 0.12, 0.9);
    const size_t d = std::max<size_t>(1, static_cast<size_t>(echoSec * rate));
    std::vector<float> dl(d, 0.0f), dr(d, 0.0f);
    size_t di = 0;
    const float fb = 0.42f + 0.18f * space;
    const float lp = onePoleCoeff(2800.0, rate);
    float sl = 0, sr = 0;

    // Reverb: Freeverb-style combs and allpasses (tunings at 44.1 kHz, scaled).
    const double k = rate / 44100.0;
    const std::array<int, 4> combL = {1557, 1617, 1491, 1422}, combR = {1580, 1640, 1514, 1445};
    const std::array<int, 2> apL = {556, 441}, apR = {579, 464};
    std::array<Comb, 4> cl, cr;
    std::array<Allpass, 2> al, ar;
    const float roomFb = 0.80f + 0.12f * space;
    for (size_t c = 0; c < 4; ++c) {
        cl[c].buf.assign(static_cast<size_t>(combL[c] * k), 0.0f);
        cr[c].buf.assign(static_cast<size_t>(combR[c] * k), 0.0f);
        cl[c].feedback = cr[c].feedback = roomFb;
    }
    for (size_t a = 0; a < 2; ++a) {
        al[a].buf.assign(static_cast<size_t>(apL[a] * k), 0.0f);
        ar[a].buf.assign(static_cast<size_t>(apR[a] * k), 0.0f);
    }

    const int fadeStart = total - static_cast<int>(1.0 * rate);   // gentle end, never a cut
    for (int i = 0; i < total; ++i) {
        const float xl = i < n ? in[2 * i] : 0.0f;
        const float xr = i < n ? in[2 * i + 1] : 0.0f;
        // Echo (ping-pong: left feeds right and back).
        const float el = dl[di], er = dr[di];
        sl += lp * (er - sl);
        sr += lp * (el - sr);
        dl[di] = xl + sl * fb;
        dr[di] = xr + sr * fb;
        di = (di + 1) % d;
        // Reverb fed by the dry input and a little of the echo.
        const float inL = (xl + 0.3f * el) * 0.25f, inR = (xr + 0.3f * er) * 0.25f;
        float rl = 0, rr = 0;
        for (size_t c = 0; c < 4; ++c) { rl += cl[c].process(inL); rr += cr[c].process(inR); }
        for (size_t a = 0; a < 2; ++a) { rl = al[a].process(rl); rr = ar[a].process(rr); }
        float g = 1.0f;
        if (i > fadeStart) g = static_cast<float>(total - i) / static_cast<float>(total - fadeStart);
        wet[static_cast<size_t>(2 * i)] = g * (shape.echoMix * el + shape.reverbMix * rl);
        wet[static_cast<size_t>(2 * i + 1)] = g * (shape.echoMix * er + shape.reverbMix * rr);
    }
    return wet;
}

void submergeCrossfade(const float* out, float* in, int n, double rate) {
    float ol = 0, orr = 0, il = 0, ir = 0;
    for (int i = 0; i < n; ++i) {
        const double t = (i + 0.5) / std::max(1, n);
        // Outgoing closes 12 kHz -> 350 Hz; incoming opens 350 Hz -> 18 kHz (exponential sweeps).
        const float aOut = onePoleCoeff(12000.0 * std::pow(350.0 / 12000.0, t), rate);
        const float aIn = onePoleCoeff(350.0 * std::pow(18000.0 / 350.0, t), rate);
        ol += aOut * (out[2 * i] - ol);
        orr += aOut * (out[2 * i + 1] - orr);
        il += aIn * (in[2 * i] - il);
        ir += aIn * (in[2 * i + 1] - ir);
        const float gOut = static_cast<float>(std::cos(t * kPi / 2)), gIn = static_cast<float>(std::sin(t * kPi / 2));
        // Late in the fade the incoming part is fully open: blend to the unfiltered signal.
        const float open = static_cast<float>(std::clamp((t - 0.6) / 0.4, 0.0, 1.0));
        in[2 * i] = ol * gOut + gIn * (il * (1 - open) + in[2 * i] * open);
        in[2 * i + 1] = orr * gOut + gIn * (ir * (1 - open) + in[2 * i + 1] * open);
    }
}

} // namespace atmos::core
