#include <vector>

#include "engine/audio/AudioBuffer.h"
#include "engine/audio/Parameter.h"
#include "engine/audio/SpscQueue.h"
#include "engine/dsp/Biquad.h"
#include "engine/dsp/Limiter.h"
#include "engine/dsp/Resampler.h"
#include "engine/spatial/InterimStereoPanner.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;

TEST_CASE("dsp/smoother reaches target without overshoot") {
    LinearSmoother s;
    s.prepare(48000, 0.01);
    s.reset(0.0f);
    s.setTarget(1.0f);
    float prev = 0.0f, maxStep = 0.0f;
    for (int i = 0; i < 480; ++i) {
        const float v = s.next();
        CHECK(v >= prev - 1e-7f);
        CHECK(v <= 1.0f + 1e-6f);
        maxStep = std::max(maxStep, v - prev);
        prev = v;
    }
    CHECK_NEAR(prev, 1.0, 1e-6);
    CHECK(!s.isSmoothing());
    CHECK(maxStep < 0.01f); // click-free: no jump larger than 1/480 + rounding
}

TEST_CASE("dsp/spsc queue preserves order and capacity") {
    SpscQueue<int, 8> q;
    for (int i = 0; i < 8; ++i) CHECK(q.push(i));
    CHECK(!q.push(99));
    for (int i = 0; i < 8; ++i) { int v = -1; CHECK(q.pop(v)); CHECK(v == i); }
    int v; CHECK(!q.pop(v));
}

TEST_CASE("dsp/limiter never exceeds ceiling and removes NaN") {
    SafetyLimiter lim;
    lim.prepare(48000, -1.0f);
    AudioBuffer b(2, 4800);
    for (int i = 0; i < 4800; ++i) {
        b.channel(0)[i] = 4.0f * std::sin(i * 0.05f);
        b.channel(1)[i] = (i % 97 == 0) ? 50.0f : 0.1f;
    }
    b.channel(0)[100] = std::nanf("");
    lim.process(b, 4800);
    const float ceiling = dbToGain(-1.0f);
    CHECK(test::peakOf(b.channel(0), 4800) <= ceiling + 1e-6f);
    CHECK(test::peakOf(b.channel(1), 4800) <= ceiling + 1e-6f);
    CHECK(std::isfinite(b.channel(0)[100]));
    CHECK(lim.lastGainReduction() < 0.5f);
}

TEST_CASE("dsp/limiter is transparent below ceiling") {
    SafetyLimiter lim;
    lim.prepare(48000, -1.0f);
    AudioBuffer b(2, 1000);
    for (int i = 0; i < 1000; ++i) b.channel(0)[i] = b.channel(1)[i] = 0.3f * std::sin(i * 0.1f);
    AudioBuffer ref = b;
    lim.process(b, 1000);
    for (int i = 0; i < 1000; ++i) CHECK_NEAR(b.channel(0)[i], ref.channel(0)[i], 1e-7);
}

TEST_CASE("dsp/biquad lowpass attenuates high frequencies") {
    Biquad lp;
    lp.setup(Biquad::Type::LowPass, 48000, 500, 0.707);
    auto hi = test::makeSine(48000, 48000, 8000, 0.5f, 1);
    std::vector<float> out(48000);
    for (int i = 0; i < 48000; ++i) out[i] = lp.process(hi.samples.channel(0)[i]);
    CHECK(test::rmsOf(out.data() + 1000, 47000) < 0.01 * test::rmsOf(hi.samples.channel(0), 48000) * 10);
}

TEST_CASE("dsp/resampler preserves duration, frequency and channel phase") {
    auto src = test::makeSine(44100, 44100, 1000.0, 0.5f, 2, /*leadingSilence*/ 441);
    auto out = resampleClip(src, 48000);
    CHECK(out.sampleRate == 48000);
    CHECK_NEAR(out.numFrames(), std::llround(src.numFrames() * 48000.0 / 44100.0), 1);
    // Leading silence scaled, not removed (441 frames @44.1k = 10 ms = 480 @48k).
    const int onset = test::firstNonZero(out.samples.channel(0), out.numFrames(), 1e-3f);
    CHECK(onset >= 470 && onset <= 490);
    // Channels stay identical (phase preserved).
    for (int i = 0; i < out.numFrames(); i += 37) CHECK_NEAR(out.samples.channel(0)[i], out.samples.channel(1)[i], 1e-7);
    // Amplitude preserved in the steady state.
    CHECK_NEAR(test::rmsOf(out.samples.channel(0) + 2000, 40000), 0.5 / std::sqrt(2.0), 0.01);
    // Zero crossings ~ 2 per cycle -> ~1000 Hz.
    int crossings = 0;
    for (int i = 2001; i < 2001 + 48000 / 2; ++i)
        if ((out.samples.channel(0)[i - 1] < 0) != (out.samples.channel(0)[i] < 0)) ++crossings;
    CHECK_NEAR(crossings, 1000, 4);
}

TEST_CASE("spatial/interim panner places right-side source in the right ear") {
    InterimStereoPanner p;
    p.prepare(48000, 512);
    p.setPosition({90.0f, 0.0f, 1.0f, 0.0f});
    auto src = test::makeSine(48000, 9600, 300, 0.5f, 1);
    AudioBuffer out(2, 9600);
    for (int i = 0; i < 9600; i += 480) {
        AudioBuffer block(2, 480);
        p.processAdd(src.samples.channel(0) + i, block, 480);
        for (int ch = 0; ch < 2; ++ch) std::copy(block.channel(ch), block.channel(ch) + 480, out.channel(ch) + i);
    }
    const double l = test::rmsOf(out.channel(0) + 4800, 4800), r = test::rmsOf(out.channel(1) + 4800, 4800);
    CHECK(r > 10 * l);
    CHECK(!p.isBinauralHrtf()); // must not be presented as HRTF
}

TEST_CASE("spatial/interim panner centre is balanced and distance attenuates") {
    auto run = [](float az, float dist) {
        InterimStereoPanner p;
        p.prepare(48000, 4800);
        p.setPosition({az, 0.0f, dist, 0.0f});
        auto src = test::makeSine(48000, 4800, 300, 0.5f, 1);
        AudioBuffer out(2, 4800);
        p.processAdd(src.samples.channel(0), out, 4800); // first block primes smoothers from 0
        out.clear();
        p.processAdd(src.samples.channel(0), out, 4800);
        return std::pair{test::rmsOf(out.channel(0), 4800), test::rmsOf(out.channel(1), 4800)};
    };
    auto [l, r] = run(0.0f, 1.0f);
    CHECK_NEAR(l, r, 1e-4);
    auto [lf, rf] = run(0.0f, 3.0f);
    CHECK(lf < l * 0.5);
    (void)rf;
}

#include "engine/environment/MicTransform.h"

TEST_CASE("dsp/room blend: echo on the beat ping-pongs, reverb is stereo and bounded") {
    const double sr = 48000;
    atmos::MicTransform m;
    m.prepare(sr, 512);
    m.cutoffHz.set(18000.0f);
    m.echoSeconds.set(0.5f);
    m.feedback.set(0.5f);
    m.wet.set(1.0f);
    m.reverbMix.set(0.0f);
    for (auto* p : {&m.cutoffHz, &m.echoSeconds, &m.feedback, &m.wet, &m.reverbMix}) p->resetTo(p->get());
    const int n = static_cast<int>(sr * 1.2);
    std::vector<float> in(n, 0.0f), l(n), r(n);
    in[100] = 1.0f;
    for (int off = 0; off < n; off += 512) {
        const int k = std::min(512, n - off);
        m.process(in.data() + off, l.data() + off, r.data() + off, k);
    }
    auto peakNear = [&](const std::vector<float>& x, int at) {
        float p = 0; for (int i = at - 400; i < at + 400; ++i) p = std::max(p, std::fabs(x[i])); return p; };
    const int beat = static_cast<int>(0.5 * sr);
    CHECK(peakNear(l, 100 + beat) > 0.1f);              // first echo on the left, one beat later
    CHECK(peakNear(r, 100 + beat) < 0.05f);              // ...not yet on the right
    CHECK(peakNear(r, 100 + 2 * beat) > 0.05f);          // the right answers a beat after
    // Reverb on: wet, stereo (left and right differ), finite and bounded.
    m.reverbMix.set(0.8f); m.reverbMix.resetTo(0.8f);
    m.roomSize.set(1.0f); m.roomSize.resetTo(1.0f);
    std::vector<float> noise(n);
    uint32_t seed = 1;
    for (auto& v : noise) { seed = seed * 1664525u + 1013904223u; v = (static_cast<float>(seed >> 8) / 16777216.0f - 0.5f) * 0.5f; }
    double diff = 0, maxAbs = 0;
    for (int off = 0; off < n; off += 512) {
        const int k = std::min(512, n - off);
        m.process(noise.data() + off, l.data() + off, r.data() + off, k);
    }
    for (int i = 0; i < n; ++i) {
        CHECK(std::isfinite(l[i]));
        diff += std::fabs(l[i] - r[i]);
        maxAbs = std::max<double>(maxAbs, std::max(std::fabs(l[i]), std::fabs(r[i])));
    }
    CHECK(diff / n > 1e-3);
    CHECK(maxAbs < 4.0);
}

#include <cmath>

#include "engine/environment/MicConditioner.h"

TEST_CASE("dsp/mic conditioner: a quiet room comes up, a loud one is held, silence is not pulled up") {
    const double sr = 48000;
    auto rmsDb = [](const std::vector<float>& v, size_t from) {
        double s = 0; for (size_t i = from; i < v.size(); ++i) s += double(v[i]) * v[i];
        return 10 * std::log10(s / double(v.size() - from) + 1e-12);
    };
    auto sine = [&](float amp, double seconds) {
        std::vector<float> v(static_cast<size_t>(sr * seconds));
        for (size_t i = 0; i < v.size(); ++i) v[i] = amp * static_cast<float>(std::sin(2 * 3.14159265 * 220 * i / sr));
        return v;
    };
    {   // A quiet room (about -50 dBFS) rises to about the target within a few seconds.
        atmos::MicConditioner c; c.prepare(sr);
        auto x = sine(0.0045f, 8.0);
        c.process(x.data(), static_cast<int>(x.size()));
        const double out = rmsDb(x, x.size() - static_cast<size_t>(sr));
        CHECK(out > atmos::MicConditioner::kTargetDb - 3);
        CHECK(out < atmos::MicConditioner::kTargetDb + 3);
    }
    {   // A sudden loud sound never exceeds the ceiling.
        atmos::MicConditioner c; c.prepare(sr);
        auto x = sine(0.01f, 3.0);
        auto bang = sine(0.99f, 0.5);
        x.insert(x.end(), bang.begin(), bang.end());
        c.process(x.data(), static_cast<int>(x.size()));
        float peak = 0; for (float v : x) peak = std::max(peak, std::fabs(v));
        CHECK(peak <= std::pow(10.0f, atmos::MicConditioner::kCeilingDb / 20.0f) + 1e-3f);
    }
    {   // Near silence (below the gate) is left where it is.
        atmos::MicConditioner c; c.prepare(sr);
        auto x = sine(0.0001f, 4.0);   // -83 dBFS
        c.process(x.data(), static_cast<int>(x.size()));
        CHECK(rmsDb(x, 0) < -75);
    }
}
