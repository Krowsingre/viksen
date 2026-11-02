#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "engine/audio/AudioEngine.h"
#include "engine/rendering/OfflineRenderer.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;

namespace {
std::unique_ptr<AudioEngine> makeEngine(double rate = 48000, int block = 256) {
    auto e = std::make_unique<AudioEngine>();
    // Set gains before prepare() so smoothers start at their targets.
    e->setMasterGain(1.0f);
    e->setLimiterCeilingDb(0.0f);
    e->prepare({rate, block});
    return e;
}
} // namespace

TEST_CASE("engine/silent by default and mic ignored when disabled") {
    auto e = makeEngine();
    std::vector<float> in(256, 0.9f), l(256, 1.0f), r(256, 1.0f);
    const float* ins[1] = {in.data()};
    float* outs[2] = {l.data(), r.data()};
    for (int i = 0; i < 20; ++i) e->process(ins, 1, outs, 2, 256);
    CHECK(test::peakOf(l.data(), 256) == 0.0f);
    CHECK(!e->meters().micActive);
    CHECK(e->environmentFeatures().silent);
}

TEST_CASE("engine/plays a song bit-exactly at unity gain") {
    auto e = makeEngine();
    auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 4800, 440, 0.25f));
    REQUIRE(e->setLayer(0, clip, Bus::Music, 0));
    AudioBuffer out;
    REQUIRE(renderOffline(*e, {0, 4800, 256, {}}, out));
    double maxErr = 0;
    for (int i = 0; i < 4800; ++i) maxErr = std::max(maxErr, double(std::fabs(out.channel(0)[i] - clip->samples.channel(0)[i])));
    CHECK(maxErr < 1e-6);
}

TEST_CASE("engine/rejects clip at wrong sample rate") {
    auto e = makeEngine();
    auto clip = std::make_shared<const AudioClip>(test::makeSine(44100, 100, 440));
    CHECK(!e->setLayer(0, clip, Bus::Music, 0));
}

TEST_CASE("engine/stems keep leading silence and timeline offsets") {
    auto e = makeEngine();
    // Stem A: impulse at its own frame 1000 (leading silence preserved).
    // Stem B: impulse at frame 0 but placed at timeline frame 2500.
    auto a = std::make_shared<const AudioClip>(test::makeImpulse(48000, 4000, 1000, 0.5f));
    auto b = std::make_shared<const AudioClip>(test::makeImpulse(48000, 4000, 0, 0.25f));
    REQUIRE(e->setLayer(0, a, Bus::Music, 0));
    REQUIRE(e->setLayer(1, b, Bus::Music, 2500));
    AudioBuffer out;
    REQUIRE(renderOffline(*e, {0, 7000, 333, {}}, out)); // odd block size crosses boundaries
    CHECK_NEAR(out.channel(0)[1000], 0.5, 1e-6);
    CHECK_NEAR(out.channel(0)[2500], 0.25, 1e-6);
    int nonZero = 0;
    for (int i = 0; i < 7000; ++i) if (out.channel(0)[i] != 0.0f) ++nonZero;
    CHECK(nonZero == 2);
    CHECK(e->timelineLengthFrames() == 6500);
}

TEST_CASE("engine/phase relationship between stems is preserved") {
    auto e = makeEngine();
    auto s = test::makeSine(48000, 4800, 200, 0.25f);
    AudioClip inv = s;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < inv.numFrames(); ++i) inv.samples.channel(ch)[i] *= -1.0f;
    REQUIRE(e->setLayer(0, std::make_shared<const AudioClip>(s), Bus::Music, 0));
    REQUIRE(e->setLayer(1, std::make_shared<const AudioClip>(inv), Bus::Music, 0));
    AudioBuffer out;
    REQUIRE(renderOffline(*e, {0, 4800, 128, {}}, out));
    CHECK(test::peakOf(out.channel(0), 4800) < 1e-6f); // perfect cancellation => sample-aligned
}

TEST_CASE("engine/bus gains and mutes are independent and smooth") {
    auto e = makeEngine();
    auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 48000, 100, 0.5f));
    REQUIRE(e->setLayer(0, clip, Bus::Music, 0));
    REQUIRE(e->setLayer(1, clip, Bus::Atmosphere, 0));
    e->setBusGain(Bus::Atmosphere, 1.0f);
    e->setBusMuted(Bus::Music, true);
    AudioBuffer out;
    REQUIRE(renderOffline(*e, {0, 9600, 256, {}}, out));
    // After the ramp, only the atmosphere copy remains (unity).
    CHECK_NEAR(out.channel(0)[9000], clip->samples.channel(0)[9000], 1e-5);
    // Mute ramp is click-free: no sample-to-sample jump larger than the signal itself can produce.
    float maxJump = 0;
    for (int i = 1; i < 9600; ++i) maxJump = std::max(maxJump, std::fabs(out.channel(0)[i] - out.channel(0)[i - 1]));
    CHECK(maxJump < 0.05f);
}

TEST_CASE("engine/master limiter protects output") {
    auto e = makeEngine();
    e->setLimiterCeilingDb(-1.0f);
    auto loud = std::make_shared<const AudioClip>(test::makeSine(48000, 4800, 100, 1.0f));
    for (int s = 0; s < 4; ++s) REQUIRE(e->setLayer(s, loud, Bus::Music, 0));
    AudioBuffer out;
    REQUIRE(renderOffline(*e, {0, 4800, 256, {}}, out));
    CHECK(test::peakOf(out.channel(0), 4800) <= dbToGain(-1.0f) + 1e-6f);
    CHECK(e->meters().limiterGain < 0.5f);
}

TEST_CASE("engine/panic silences immediately and resumes with a ramp") {
    auto e = makeEngine();
    auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 48000, 100, 0.5f));
    REQUIRE(e->setLayer(0, clip, Bus::Music, 0));
    e->play();
    std::vector<float> l(256), r(256);
    float* outs[2] = {l.data(), r.data()};
    for (int i = 0; i < 10; ++i) e->process(nullptr, 0, outs, 2, 256);
    CHECK(test::peakOf(l.data(), 256) > 0.1f);
    e->panic();
    e->process(nullptr, 0, outs, 2, 256);
    CHECK(test::peakOf(l.data(), 256) == 0.0f);
    CHECK(e->meters().panicked);
    e->clearPanic();
    e->process(nullptr, 0, outs, 2, 256);
    CHECK(std::fabs(l[0]) < 0.01f); // starts from silence
}

TEST_CASE("engine/clips are retired and freed on the control thread") {
    auto e = makeEngine();
    std::weak_ptr<const AudioClip> weak;
    {
        auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 100, 440));
        weak = clip;
        REQUIRE(e->setLayer(0, clip, Bus::Music, 0));
    }
    std::vector<float> l(64), r(64);
    float* outs[2] = {l.data(), r.data()};
    e->process(nullptr, 0, outs, 2, 64);
    CHECK(!weak.expired());     // engine still owns it
    REQUIRE(e->clearLayer(0));
    e->process(nullptr, 0, outs, 2, 64); // audio thread retires
    CHECK(!weak.expired());     // not freed on the audio thread
    e->collectGarbage();
    CHECK(weak.expired());
}

TEST_CASE("engine/handles host blocks larger than maxBlockSize and mono output") {
    auto e = makeEngine(48000, 128);
    auto clip = std::make_shared<const AudioClip>(test::makeImpulse(48000, 2000, 700, 0.5f));
    REQUIRE(e->setLayer(0, clip, Bus::Music, 0));
    e->play();
    std::vector<float> mono(1000);
    float* outs[1] = {mono.data()};
    e->process(nullptr, 0, outs, 1, 1000);
    CHECK_NEAR(mono[700], 0.5, 1e-6);
    CHECK(e->meters().positionFrames == 1000);
}

TEST_CASE("engine/mic routing: raw and transformed buses are separable") {
    auto e = makeEngine();
    e->setMicInputEnabled(true);
    e->setBusGain(Bus::MicRaw, 1.0f);
    e->setBusMuted(Bus::MicTransformed, true);
    auto tone = test::makeSine(48000, 4800, 300, 0.3f, 1);
    std::vector<float> l(4800), r(4800);
    const float* ins[1] = {tone.samples.channel(0)};
    float* outs[2] = {l.data(), r.data()};
    e->process(ins, 1, outs, 2, 4800);
    CHECK(e->meters().micActive);
    // Raw monitor only: output equals input after the gain ramp.
    CHECK_NEAR(l[4000], tone.samples.channel(0)[4000], 1e-4);
    CHECK(e->environmentFeatures().levelDb > -20.0f);
}

TEST_CASE("engine/atmosphere bed works without music or mic and is deterministic") {
    auto render = [] {
        auto e = makeEngine();
        e->setAtmosphereBedEnabled(true);
        e->setBusGain(Bus::Atmosphere, 1.0f);
        AudioBuffer out;
        renderOffline(*e, {0, 48000, 512, {}}, out);
        return out;
    };
    AudioBuffer a = render(), b = render();
    CHECK(test::rmsOf(a.channel(0) + 4800, 43200) > 0.005);
    bool identical = true;
    for (int i = 0; i < 48000; ++i) identical &= a.channel(0)[i] == b.channel(0)[i];
    CHECK(identical);
    // Stereo channels are decorrelated, not identical.
    CHECK(a.channel(0)[30000] != a.channel(1)[30000]);
}

TEST_CASE("engine/feature extractor detects transients and silence") {
    FeatureExtractor fx;
    fx.prepare(48000);
    std::vector<float> quiet(4800, 0.0f);
    fx.process(quiet.data(), 4800);
    CHECK(fx.snapshot().silent);
    const uint32_t before = fx.snapshot().transientCount;
    std::vector<float> clap(4800, 0.0f);
    for (int i = 0; i < 200; ++i) clap[1000 + i] = 0.8f * ((i % 2) ? 1.0f : -1.0f);
    fx.process(clap.data(), 4800);
    CHECK(fx.snapshot().transientCount == before + 1);
}

TEST_CASE("engine/concurrent control and audio threads (stress)") {
    auto e = makeEngine(48000, 128);
    std::atomic<bool> stop{false};
    std::thread audio([&] {
        std::vector<float> in(128, 0.05f), l(128), r(128);
        const float* ins[1] = {in.data()};
        float* outs[2] = {l.data(), r.data()};
        while (!stop.load()) e->process(ins, 1, outs, 2, 128);
    });
    auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 2000, 440, 0.1f));
    e->play();
    e->setLooping(true);
    e->setMicInputEnabled(true);
    for (int i = 0; i < 3000; ++i) {
        const int slot = i % AudioEngine::kMaxLayers;
        if (i % 3 == 0) e->clearLayer(slot);
        else e->setLayer(slot, clip, static_cast<Bus>(i % kNumBuses), i % 500);
        e->setBusGain(static_cast<Bus>(i % kNumBuses), (i % 10) / 10.0f);
        e->setAtmosphereBedEnabled(i % 2 == 0);
        if (i % 50 == 0) e->collectGarbage();
        if (i % 500 == 0) e->seek(0);
    }
    // Make sure the audio thread really ran concurrently (schedulers may start it late).
    while (e->meters().blocksProcessed < 200) std::this_thread::yield();
    stop.store(true);
    audio.join();
    e->collectGarbage();
    CHECK(e->meters().blocksProcessed >= 200);
}
