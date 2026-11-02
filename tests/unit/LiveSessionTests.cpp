#include "core/session/LiveSession.h"
#include "engine/rendering/OfflineRenderer.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

TEST_CASE("session/rebuild at a new sample rate keeps settings and re-resamples the track") {
    LiveSession s(48000, 512);
    s.setBusGain(Bus::Atmosphere, 0.9f);
    s.setBusMuted(Bus::MicTransformed, true);
    s.setMasterGain(1.0f);
    s.setLimiterCeilingDb(0.0f);
    s.setMicEchoSeconds(0.2f);
    auto original = std::make_shared<const AudioClip>(test::makeImpulse(44100, 44100, 4410, 0.5f));
    REQUIRE(s.setTrack(original));
    CHECK(s.hasTrack());
    CHECK(s.engine().timelineLengthFrames() == 48000);

    CHECK(s.rebuildIfNeeded(44100, 512));
    CHECK(!s.rebuildIfNeeded(44100, 256)); // smaller block fits: no rebuild
    CHECK(s.sampleRate() == 44100);
    CHECK_NEAR(s.engine().busGain(Bus::Atmosphere), 0.9, 1e-6);
    CHECK(s.engine().busMuted(Bus::MicTransformed));
    CHECK_NEAR(s.engine().micTransform().echoSeconds.get(), 0.2, 1e-6);
    CHECK(s.engine().timelineLengthFrames() == 44100);

    AudioBuffer out;
    REQUIRE(renderOffline(s.engine(), {0, 44100, 512, {}}, out));
    CHECK_NEAR(out.channel(0)[4410], 0.5, 1e-4); // native rate again: impulse back at its own frame
}

TEST_CASE("session/prepared clip at a stale rate is re-prepared") {
    LiveSession s(48000, 512);
    auto original = std::make_shared<const AudioClip>(test::makeSine(44100, 4410, 440));
    auto stale = std::make_shared<const AudioClip>(test::makeSine(96000, 9600, 440));
    REQUIRE(s.setTrack(original, stale, 96000));
    CHECK(s.engine().timelineLengthFrames() == 4800);
    s.clearTrack();
    CHECK(!s.hasTrack());
    CHECK(!s.setTrack(nullptr));
}
