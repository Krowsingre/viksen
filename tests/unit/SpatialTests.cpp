#include <memory>
#include <vector>

#include "engine/audio/WorldStreamPlayer.h"
#include "engine/spatial/BinauralFactory.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;

namespace {
std::pair<double, double> renderEars(SpatialRenderer& r, float az, int block = 480, int total = 48000) {
    r.setPosition({az, 0.0f, 1.0f, 0.0f});
    auto noise = test::makeSine(48000, total, 700, 0.3f, 1);
    // Broadband-ish test signal: sum of a few sines (HRTF level differences are frequency dependent).
    for (int i = 0; i < total; ++i)
        noise.samples.channel(0)[i] += 0.2f * std::sin(i * 0.37f) + 0.15f * std::sin(i * 1.13f);
    AudioBuffer out(2, total);
    for (int i = 0; i + block <= total; i += block) {
        AudioBuffer b(2, block);
        r.processAdd(noise.samples.channel(0) + i, b, block);
        for (int ch = 0; ch < 2; ++ch) std::copy(b.channel(ch), b.channel(ch) + block, out.channel(ch) + i);
    }
    return {test::rmsOf(out.channel(0) + total / 2, total / 2), test::rmsOf(out.channel(1) + total / 2, total / 2)};
}
} // namespace

TEST_CASE("spatial/binaural renderer: Steam Audio HRTF when compiled in, sides are correct") {
    auto r = makeBinauralRenderer(48000, 1024);
    REQUIRE(r != nullptr);
    CHECK(r->isBinauralHrtf() == binauralHrtfCompiledIn());
    auto [l, rr] = renderEars(*r, 90.0f);
    CHECK(rr > 1.5 * l);                       // source on the right
    r->reset();
    auto [l2, r2] = renderEars(*r, -90.0f);
    CHECK(l2 > 1.5 * r2);                      // source on the left
    r->reset();
    auto [lf, rf] = renderEars(*r, 0.0f);
    // Front: nearly balanced. Steam Audio's default HRTF is a measured (slightly
    // asymmetric) head, about 2 dB at 0 degrees, so allow 3 dB.
    CHECK(std::fabs(20 * std::log10(lf / rf)) < 3.0);
}

TEST_CASE("spatial/binaural renderer: fixed latency and any host block size") {
    auto a = makeBinauralRenderer(48000, 1024);
    auto b = makeBinauralRenderer(48000, 1024);
    a->setPosition({30.0f, 10.0f, 1.0f, 0.0f});
    b->setPosition({30.0f, 10.0f, 1.0f, 0.0f});
    auto sig = test::makeImpulse(48000, 8192, 0, 1.0f, 1);
    AudioBuffer outA(2, 8192), outB(2, 8192);
    a->processAdd(sig.samples.channel(0), outA, 8192 - 7000); // one odd-sized block...
    {
        AudioBuffer tmp(2, 7000);
        a->processAdd(sig.samples.channel(0) + 1192, tmp, 7000);
        for (int ch = 0; ch < 2; ++ch) std::copy(tmp.channel(ch), tmp.channel(ch) + 7000, outA.channel(ch) + 1192);
    }
    for (int i = 0; i < 8192; i += 333) {                  // ...versus many small odd blocks
        const int n = std::min(333, 8192 - i);
        AudioBuffer tmp(2, n);
        b->processAdd(sig.samples.channel(0) + i, tmp, n);
        for (int ch = 0; ch < 2; ++ch) std::copy(tmp.channel(ch), tmp.channel(ch) + n, outB.channel(ch) + i);
    }
    double maxDiff = 0;
    for (int i = 0; i < 8192; ++i) maxDiff = std::max(maxDiff, double(std::fabs(outA.channel(0)[i] - outB.channel(0)[i])));
    CHECK(maxDiff < 1e-5);                                  // block size never changes the result
    const int onset = test::firstNonZero(outA.channel(0), 8192, 1e-6f);
    CHECK(onset >= a->latencySamples());
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 8192; ++i) CHECK(std::isfinite(outA.channel(ch)[i]));
}

TEST_CASE("worlds/3D stems: position, head rotation and click-free mode switch") {
    WorldStreamPlayer p;
    p.prepare(48000, 512, 1 << 16);
    p.setStemCount(1);
    p.setStemGain(0, 1.0f);
    p.setStemPosition(0, 90.0f, 0.0f, 1.0f, 0.0f);         // one stem, hard right
    p.setSpatialEnabled(true);
    p.setActive(true);
    auto tone = test::makeSine(48000, 48000 * 3, 500, 0.3f, 2);
    std::vector<float> lr(2 * 512);
    auto run = [&](int blocks, int offset) {
        AudioBuffer acc(2, blocks * 512);
        for (int k = 0; k < blocks; ++k) {
            for (int f = 0; f < 512; ++f) {
                lr[2 * f] = tone.samples.channel(0)[offset + k * 512 + f];
                lr[2 * f + 1] = tone.samples.channel(1)[offset + k * 512 + f];
            }
            p.write(0, lr.data(), 512);
            AudioBuffer out(2, 512);
            p.processAdd(out, 512);
            for (int ch = 0; ch < 2; ++ch) std::copy(out.channel(ch), out.channel(ch) + 512, acc.channel(ch) + k * 512);
        }
        return acc;
    };
    AudioBuffer a = run(40, 0);
    CHECK(test::rmsOf(a.channel(1) + 10240, 10240) > 1.5 * test::rmsOf(a.channel(0) + 10240, 10240));
    p.setListenerYaw(90.0f);                                // turn the head toward it: now in front
    AudioBuffer b = run(40, 40 * 512);
    const double lb = test::rmsOf(b.channel(0) + 10240, 10240), rb = test::rmsOf(b.channel(1) + 10240, 10240);
    CHECK(std::fabs(20 * std::log10(lb / rb)) < 3.0);
    p.setSpatialEnabled(false);                             // crossfade back to stereo
    AudioBuffer c = run(20, 80 * 512);
    float maxJump = 0;
    for (int i = 1; i < 20 * 512; ++i) maxJump = std::max(maxJump, std::fabs(c.channel(0)[i] - c.channel(0)[i - 1]));
    CHECK(maxJump < 0.1f);
    CHECK_NEAR(test::rmsOf(c.channel(0) + 8192, 2048), test::rmsOf(tone.samples.channel(0) + 88 * 512, 2048), 0.01);
}

TEST_CASE("spatial/Steam Audio HRTF gives realistic interaural cues at 90 degrees") {
    auto r = makeBinauralRenderer(48000, 1024);
    if (!r->isBinauralHrtf()) return; // interim panner build (e.g. no Steam Audio for this platform)
    r->setPosition({90.0f, 0.0f, 1.0f, 0.0f});
    auto imp = test::makeImpulse(48000, 4096, 0, 1.0f, 1);
    AudioBuffer out(2, 4096);
    for (int i = 0; i < 4096; i += 333) {
        const int n = std::min(333, 4096 - i);
        AudioBuffer b(2, n);
        r->processAdd(imp.samples.channel(0) + i, b, n);
        for (int ch = 0; ch < 2; ++ch) std::copy(b.channel(ch), b.channel(ch) + n, out.channel(ch) + i);
    }
    int pl = 0, pr = 0;
    double el = 0, er = 0;
    for (int i = 0; i < 4096; ++i) {
        if (std::fabs(out.channel(0)[i]) > std::fabs(out.channel(0)[pl])) pl = i;
        if (std::fabs(out.channel(1)[i]) > std::fabs(out.channel(1)[pr])) pr = i;
        el += double(out.channel(0)[i]) * out.channel(0)[i];
        er += double(out.channel(1)[i]) * out.channel(1)[i];
    }
    CHECK(10 * std::log10(er / el) > 10.0);   // broadband level difference (measured ~17 dB)
    CHECK(pl - pr >= 15 && pl - pr <= 40);    // far ear later: ~0.3-0.8 ms interaural delay
}
