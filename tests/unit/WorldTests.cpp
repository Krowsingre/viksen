#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "core/session/LiveSession.h"
#include "core/worlds/WorldDirector.h"
#include "core/worlds/WorldFeeder.h"
#include "core/worlds/WorldManifest.h"
#include "engine/audio/WorldStreamPlayer.h"
#include "engine/dsp/Resampler.h"
#include "engine/spatial/BinauralFactory.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

namespace {
// Waits (briefly) until the feeder has buffered at least `frames` for every stem.
// Bounded by wall-clock time, not iterations: a 1 ms sleep lasts ~15 ms on Windows.
bool waitBuffered(WorldStreamPlayer& p, int frames) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < deadline) {
        if (p.active() && p.readableFrames() >= frames) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

Json demoWorldJson() {
    return Json::parse(R"({
      "format": "atmos.world", "schemaVersion": 1, "id": "demo", "name": "Demo", "tier": "free",
      "music": {"tempo": 110, "key": "G minor", "loop": {"fromSeconds": 0, "toSeconds": null}},
      "fadeSeconds": 2,
      "stems": [{"id": "keys", "file": "keys.m4a", "title": "Keys"},
                {"id": "perc", "file": "perc.m4a", "title": "Percussion", "baseGainDb": -6}],
      "rules": [{"stem": "perc", "when": {"all": [{"type": "moving"}]}, "gainDb": 0, "otherwiseDb": -60, "label": "You are moving"},
                {"stem": "keys", "when": {"all": [{"type": "quiet"}]}, "gainDb": 0, "otherwiseDb": -12, "label": "It is quiet"}],
      "futureField": {"x": 1}
    })");
}
} // namespace

TEST_CASE("worlds/stream player plays only when every stem has data, and flushes in sync") {
    WorldStreamPlayer p;
    p.prepare(48000, 512, 4096);
    p.setStemCount(2);
    p.setActive(true);
    std::vector<float> ones(2 * 1000, 0.25f), twos(2 * 1000, 0.5f);
    p.write(0, ones.data(), 1000);
    AudioBuffer out(2, 256);
    p.processAdd(out, 256);
    CHECK(test::peakOf(out.channel(0), 256) == 0.0f); // stem 1 empty -> silence
    CHECK(p.underruns() == 1);
    p.write(1, twos.data(), 1000);
    out.clear();
    p.processAdd(out, 256);
    CHECK_NEAR(out.channel(0)[100], 0.75, 1e-6);
    CHECK(p.framesPlayed() == 256);
    // Flush: new material written after the request is what plays next.
    p.requestFlush();
    std::vector<float> a(2 * 300, 0.1f), b(2 * 300, 0.2f);
    p.write(0, a.data(), 300);
    p.write(1, b.data(), 300);
    out.clear();
    p.processAdd(out, 256);
    CHECK_NEAR(out.channel(1)[10], 0.3, 1e-6);
    CHECK(p.framesPlayed() == 256);
}

TEST_CASE("worlds/manifest parses, validates and round-trips unknown keys") {
    auto w = parseWorld(demoWorldJson());
    REQUIRE(w.has_value());
    CHECK(w->tempo == 110 && w->key == "G minor" && w->stems.size() == 2 && w->rules.size() == 2);
    CHECK(worldToJson(*w)["futureField"]["x"] == 1);
    auto again = parseWorld(worldToJson(*w));
    REQUIRE(again.has_value());
    CHECK(again->stems[1].baseGainDb == -6.0f);

    CHECK(w->stems[0].distanceM == 1.5f);                      // default position
    Json spatial = demoWorldJson();
    spatial["stems"][1]["spatial"] = {{"azimuth", -120}, {"elevation", 200}, {"distance", 3}, {"width", 20}};
    auto ws = parseWorld(spatial);
    REQUIRE(ws.has_value());
    CHECK(ws->stems[1].azimuthDeg == -120.0f && ws->stems[1].elevationDeg == 90.0f); // clamped
    CHECK(worldToJson(*ws)["stems"][1]["spatial"]["distance"] == 3.0);

    Json bad = demoWorldJson();
    bad["rules"][0]["stem"] = "nope";
    std::string err;
    CHECK(!parseWorld(bad, &err).has_value());
    CHECK(err.find("unknown stem") != std::string::npos);
    bad = demoWorldJson();
    bad["stems"][0]["file"] = "../../secret.m4a";
    CHECK(!parseWorld(bad).has_value());
}

TEST_CASE("worlds/director: rules react only to available sensors, with timed fades") {
    auto w = parseWorld(demoWorldJson());
    REQUIRE(w.has_value());
    WorldDirector d(*w);
    ConditionContext c;                       // no mic, no motion: authored mix
    d.snapToTargets(c);
    CHECK_NEAR(d.currentDb(0), 0.0, 1e-6);
    CHECK_NEAR(d.currentDb(1), -6.0, 1e-6);
    c.env.motionAvailable = true;             // motion on, not moving: percussion out
    c.micActive = true;
    c.env.quiet = true;
    d.update(c, 1.0);                         // 2 s fade covers 60 dB -> 30 dB per second
    CHECK_NEAR(d.currentDb(1), -36.0, 1e-4);
    d.update(c, 5.0);
    CHECK(d.linearGain(1) == 0.0f);
    CHECK(d.reasons(0).size() == 1);
    c.env.moving = true;
    for (int i = 0; i < 30; ++i) d.update(c, 0.1);
    CHECK_NEAR(d.currentDb(1), 0.0, 1e-4);
    CHECK(d.reasons(1).front() == "You are moving");
}

TEST_CASE("worlds/feeder streams aligned stems through the engine and loops") {
    AudioEngine e;
    e.setMasterGain(1.0f);
    e.setLimiterCeilingDb(0.0f);
    e.prepare({48000, 512});
    auto a = std::make_shared<const AudioClip>(test::makeImpulse(48000, 20000, 1000, 0.5f));
    auto b = std::make_shared<const AudioClip>(test::makeImpulse(48000, 20000, 1000, -0.25f));
    std::vector<std::unique_ptr<StemDecoder>> dec;
    dec.push_back(std::make_unique<ClipStemDecoder>(a));
    dec.push_back(std::make_unique<ClipStemDecoder>(b));
    e.worldPlayer().setStemGain(0, 1.0f);
    e.worldPlayer().setStemGain(1, 1.0f);
    WorldFeeder f(e.worldPlayer(), std::move(dec), 0, 0);
    f.start(0);
    REQUIRE(waitBuffered(e.worldPlayer(), 512));

    std::vector<float> l(512), r(512);
    float* outs[2] = {l.data(), r.data()};
    std::vector<float> all;
    for (int blk = 0; blk < 60; ++blk) {                  // 30720 frames: wraps once (loop = 20000)
        REQUIRE(waitBuffered(e.worldPlayer(), 512));
        e.process(nullptr, 0, outs, 2, 512);
        all.insert(all.end(), l.begin(), l.end());
    }
    f.stop();
    // Both stems' impulses land on the same frame (sum 0.25), at 1000 and after the loop at 21000.
    CHECK_NEAR(all[1000], 0.25, 1e-5);
    CHECK_NEAR(all[21000], 0.25, 1e-5);
    int nonZero = 0;
    for (float v : all) if (std::fabs(v) > 1e-6f) ++nonZero;
    CHECK(nonZero == 2);
    CHECK(e.worldPlayer().underruns() == 0);
}

TEST_CASE("session/World fades in, reacts, survives a sample-rate change and fades out") {
    auto w = parseWorld(demoWorldJson());
    REQUIRE(w.has_value());
    auto keys = std::make_shared<const AudioClip>(test::makeSine(48000, 48000 * 10, 220, 0.3f));
    auto perc = std::make_shared<const AudioClip>(test::makeSine(48000, 48000 * 10, 880, 0.3f));
    StemDecoderFactory factory = [&](double rate, std::string&) {
        std::vector<std::unique_ptr<StemDecoder>> v;
        for (auto& c : {keys, perc})
            v.push_back(std::make_unique<ClipStemDecoder>(
                rate == c->sampleRate ? c : std::make_shared<const AudioClip>(resampleClip(*c, rate))));
        return v;
    };
    LiveSession s(48000, 512);
    s.setMasterGain(1.0f);
    std::string err;
    REQUIRE(s.loadWorld(*w, factory, &err));
    s.startWorld();
    ConditionContext ctx;
    std::vector<float> l(512), r(512);
    float* outs[2] = {l.data(), r.data()};
    auto run = [&](double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * s.sampleRate() / 512); ++i) {
            if (s.worldStatus().playing) waitBuffered(s.engine().worldPlayer(), 512);
            s.updateWorld(512.0 / s.sampleRate(), ctx);
            s.engine().process(nullptr, 0, outs, 2, 512);
        }
    };
    run(3.5);
    WorldStatus st = s.worldStatus();
    CHECK(st.playing && st.master > 0.99f);
    CHECK(st.stems.size() == 2);
    CHECK(test::peakOf(l.data(), 512) > 0.05f);

    s.rebuild(44100, 512);                     // device changed rate mid-World
    run(1.0);
    st = s.worldStatus();
    CHECK(st.playing);
    CHECK(test::peakOf(l.data(), 512) > 0.05f);

    s.setWorldStemPosition(0, 450.0f, 10.0f, 40.0f);           // canvas drag: wrapped and clamped
    st = s.worldStatus();
    CHECK_NEAR(st.stems[0].azimuthDeg, 90.0, 1e-4);
    CHECK_NEAR(st.stems[0].distanceM, 20.0, 1e-4);
    s.resetWorldStemPositions();
    CHECK_NEAR(s.worldStatus().stems[0].distanceM, 1.5, 1e-4); // authored default restored
    CHECK(st.spatial);
    CHECK(st.hrtf == binauralHrtfCompiledIn());

    s.stopWorld();
    run(3.0);
    st = s.worldStatus();
    CHECK(!st.playing);
    s.engine().process(nullptr, 0, outs, 2, 512);
    CHECK(test::peakOf(l.data(), 512) == 0.0f);
    s.unloadWorld();
    CHECK(!s.worldStatus().loaded);
}

TEST_CASE("session/Studio transport: quick start/stop, seek, levels, unequal stem lengths") {
    auto w = parseWorld(demoWorldJson());
    REQUIRE(w.has_value());
    w->rules.clear();
    auto longStem = std::make_shared<const AudioClip>(test::makeSine(48000, 48000 * 3, 220, 0.3f));
    auto shortStem = std::make_shared<const AudioClip>(test::makeSine(48000, 48000 * 1, 880, 0.3f));
    StemDecoderFactory factory = [&](double, std::string&) {
        std::vector<std::unique_ptr<StemDecoder>> v;
        v.push_back(std::make_unique<ClipStemDecoder>(longStem));
        v.push_back(std::make_unique<ClipStemDecoder>(shortStem));
        return v;
    };
    LiveSession s(48000, 512);
    REQUIRE(s.loadWorld(*w, factory));
    s.setWorldSpatialEnabled(false);
    s.startWorld(true);
    ConditionContext ctx;
    std::vector<float> l(512), r(512);
    float* outs[2] = {l.data(), r.data()};
    auto run = [&](double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 48000 / 512); ++i) {
            if (s.worldStatus().playing) waitBuffered(s.engine().worldPlayer(), 512);
            s.updateWorld(512.0 / 48000, ctx);
            s.engine().process(nullptr, 0, outs, 2, 512);
        }
    };
    run(0.5);
    CHECK(s.worldStatus().master > 0.99f);              // 150 ms fade-in done
    CHECK_NEAR(s.worldStatus().lengthSeconds, 3.0, 0.01); // longest stem, not the shortest
    s.setWorldStemLevel(1, 0.0f, true);                  // mute stem 2
    run(5.0);                                            // director fades (2 s demo fade)
    CHECK(s.worldStatus().stems[1].gainDb <= -59.0f);
    s.seekWorld(2.0);
    run(0.2);
    CHECK(s.worldStatus().positionSeconds >= 2.0 && s.worldStatus().positionSeconds < 2.6);
    s.stopWorld(true);
    run(0.4);
    CHECK(!s.worldStatus().playing);
    CHECK_NEAR(s.worldStatus().positionSeconds, 2.5, 0.3); // position kept for resume
}

TEST_CASE("worlds/loop effect switch: off gives a clean loop that never plays what lies past the loop end") {
    // Silence up to the loop end (40000), then a loud tone after it, like the leftovers at
    // the end of a song file. The loop effect plays some of that into its crossfade and wash;
    // with it off, only an 8 ms crossfade separates the loop end from the start.
    // `adaptive`: the song's first part does not suit the moment, so at the song end the
    // stream goes on to its second part instead of the very start (the loop all the same).
    auto run = [](bool loopFx, bool adaptive) -> double {
        AudioEngine e;
        e.setMasterGain(1.0f);
        e.setLimiterCeilingDb(0.0f);
        e.prepare({48000, 512});
        auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 48000, 440, 0.5f, 2, 40000));
        std::vector<std::unique_ptr<StemDecoder>> dec;
        dec.push_back(std::make_unique<ClipStemDecoder>(clip));
        e.worldPlayer().setStemGain(0, 1.0f);
        auto planner = adaptive
            ? std::make_shared<SectionPlanner>(std::vector<std::pair<int64_t, int64_t>>{{0, 20000}, {20000, 40000}}, 40000, 0, 40000, 0)
            : std::make_shared<SectionPlanner>(std::vector<std::pair<int64_t, int64_t>>{{0, 40000}}, 40000, 0, 40000, 0);
        if (adaptive) planner->setChoice(SectionChoice{0b10, 0, false});
        WorldFeeder f(e.worldPlayer(), std::move(dec), 0, 40000, planner, 24000);
        f.enableTransitionFx(0.5);
        f.setLoopFx(loopFx);
        f.start(0);
        std::vector<float> l(512), r(512);
        float* outs[2] = {l.data(), r.data()};
        std::vector<float> all;
        for (int blk = 0; blk < 150; ++blk) {               // 76800 frames: one loop and more
            if (!waitBuffered(e.worldPlayer(), 512)) return -1.0;
            e.process(nullptr, 0, outs, 2, 512);
            all.insert(all.end(), l.begin(), l.end());
        }
        f.stop();
        double energy = 0;
        for (size_t i = 40000 + 1000; i < 40000 + 18000; ++i) energy += double(all[i]) * all[i];
        return energy;
    };
    for (bool adaptive : {false, true}) {
        const double withFx = run(true, adaptive), clean = run(false, adaptive);
        CHECK(withFx > 10.0);        // the leftovers past the loop end are heard, washed
        CHECK(clean >= 0.0);         // streamed without stalling
        CHECK(clean < 1e-6);         // nothing after the 8 ms crossfade
    }
}
