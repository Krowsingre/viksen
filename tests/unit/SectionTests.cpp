// Adaptive sections: different situations play different parts of the song.

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <cmath>
#include <thread>

#include "core/session/LiveSession.h"
#include "core/worlds/WorldManifest.h"
#include "core/worlds/WorldSections.h"
#include "core/worlds/TransitionFx.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

namespace {
// 120 BPM, 4/4: one bar = 2 s. Song: bars 0-4 "calm", bars 4-8 "drive", bars 8-12 "calm2".
Json sectionWorld() {
    return Json::parse(R"({
      "format": "atmos.world", "schemaVersion": 1, "id": "sec", "name": "Sections", "tier": "free",
      "music": {"tempo": 120, "key": "A minor", "beatsPerBar": 4},
      "adaptive": {"jumpEveryBars": 1, "minStayBars": 1, "crossfadeBeats": 0.5},
      "fadeSeconds": 0.5,
      "stems": [{"id": "a", "file": "a.wav", "title": "A"}],
      "sections": [
        {"id": "calm", "title": "Calm", "fromBar": 0, "toBar": 4, "when": {"all": [{"type": "still"}]}, "label": "You are still"},
        {"id": "drive", "title": "Drive", "fromBar": 4, "toBar": 8, "when": {"all": [{"type": "moving"}]}, "label": "You are moving"},
        {"id": "calm2", "title": "Calm again", "fromBar": 8, "toBar": 12, "when": {"all": [{"type": "still"}]}, "label": "You are still"}
      ]})");
}
} // namespace

TEST_CASE("worlds/sections parse with bars and round-trip") {
    auto w = parseWorld(sectionWorld());
    REQUIRE(w.has_value());
    REQUIRE(w->sections.size() == 3);
    CHECK_NEAR(w->sections[1].fromSeconds, 8.0, 1e-9);
    CHECK_NEAR(w->sections[1].toSeconds, 16.0, 1e-9);
    CHECK(w->jumpEveryBars == 1);
    auto again = parseWorld(worldToJson(*w));
    REQUIRE(again.has_value());
    CHECK(again->sections.size() == 3);
    CHECK(again->sections[2].label == "You are still");
    Json bad = sectionWorld();
    bad["sections"][0]["toBar"] = 0;
    CHECK(!parseWorld(bad).has_value());
}

TEST_CASE("worlds/section choice: composed without sensors, matched sections otherwise") {
    auto w = parseWorld(sectionWorld());
    REQUIRE(w.has_value());
    ConditionContext c;                        // motion off: nothing evaluable
    auto ch = chooseSections(w->sections, c);
    CHECK(ch.composed);
    CHECK(ch.acceptable == 0b111);
    c.env.motionAvailable = true;
    c.env.moving = true;
    ch = chooseSections(w->sections, c);
    CHECK(!ch.composed);
    CHECK(ch.acceptable == 0b010);
}

TEST_CASE("worlds/planner jumps on the grid, flows in order, loops when composed") {
    const int64_t bar = 100;
    SectionPlanner p({{0, 4 * bar}, {4 * bar, 8 * bar}, {8 * bar, 12 * bar}}, bar, 0, 12 * bar, bar);
    CHECK(p.nextBoundary(0) == bar);
    CHECK(p.nextBoundary(350) == 4 * bar);
    // Composed: carry on, and back to the top at the end.
    CHECK(p.decide(bar) == bar);
    CHECK(p.decide(12 * bar) == 0);
    // Moving: from inside "calm" jump to "drive" at the next grid point.
    p.setChoice({0b010, 0, false});
    CHECK(p.decide(2 * bar) == 4 * bar);
    // Still in drive: carry on; at its end the next part does not suit -> repeat drive.
    CHECK(p.decide(5 * bar) == 5 * bar);
    CHECK(p.decide(8 * bar) == 4 * bar);
    // Still again: both calm parts suit; from drive go to the next suitable one in order.
    p.setChoice({0b101, 0, false});
    CHECK(p.decide(6 * bar) == 8 * bar);
    // calm2 ends at the song end -> cycles to calm (song order).
    CHECK(p.decide(12 * bar) == 0);
    // Preference wins among suitable parts.
    p.setChoice({0b101, 0b100, false});
    CHECK(p.decide(4 * bar) == 8 * bar);
}

TEST_CASE("session/adaptive sections: moving plays the drive part, with a crossfade, and reports why") {
    auto w = parseWorld(sectionWorld());
    REQUIRE(w.has_value());
    // Distinct content per section: 220 Hz in calm parts, 880 Hz in drive (bars 4-8 = 8-16 s).
    const double rate = 48000;   // stream lead ~2.7 s, as on device
    AudioClip clip;
    clip.sampleRate = rate;
    clip.samples.setSize(2, static_cast<int>(rate * 24));
    for (int i = 0; i < clip.numFrames(); ++i) {
        const double t = i / rate;
        const double f = (t >= 8 && t < 16) ? 880 : 220;
        const float v = static_cast<float>(0.3 * std::sin(2 * 3.14159265358979 * f * t));
        clip.samples.channel(0)[i] = v;
        clip.samples.channel(1)[i] = v;
    }
    auto shared = std::make_shared<const AudioClip>(clip);
    StemDecoderFactory factory = [&](double, std::string&) {
        std::vector<std::unique_ptr<StemDecoder>> v;
        v.push_back(std::make_unique<ClipStemDecoder>(shared));
        return v;
    };
    LiveSession s(rate, 256);
    s.setMasterGain(1.0f);
    s.setWorldSpatialEnabled(false);
    REQUIRE(s.loadWorld(*w, factory));
    ConditionContext ctx;
    ctx.env.motionAvailable = true;
    ctx.env.still = true;
    s.updateWorld(0, ctx);                     // choice before streaming starts
    s.startWorld(true);
    std::vector<float> l(256), r(256);
    float* outs[2] = {l.data(), r.data()};
    auto run = [&](double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * rate / 256); ++i) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            while (s.engine().worldPlayer().readableFrames() < 256 && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            s.updateWorld(256.0 / rate, ctx);
            s.engine().process(nullptr, 0, outs, 2, 256);
        }
    };
    run(3.0);
    auto st = s.worldStatus();
    CHECK(st.adaptive);
    CHECK(!st.composed);
    CHECK(st.sectionTitle == "Calm");
    // Start moving: within a couple of bars (plus the feeder's lead) the drive part plays.
    ctx.env.still = false;
    ctx.env.moving = true;
    run(9.0);
    st = s.worldStatus();
    CHECK(st.sectionTitle == "Drive");
    CHECK(st.sectionLabel == "You are moving");
    CHECK(st.positionSeconds >= 8.0 && st.positionSeconds < 16.0);
    CHECK(st.sectionJumps >= 1);
    // Zero crossings over the last block: ~880 Hz content, not 220 Hz.
    int crossings = 0;
    for (int i = 1; i < 256; ++i) crossings += (l[i - 1] < 0) != (l[i] < 0);
    CHECK(crossings >= 7);                     // 880 Hz -> ~9 per 256 frames at 48 kHz; 220 Hz -> ~2
    // Still again: back to a calm part.
    ctx.env.moving = false;
    ctx.env.still = true;
    run(9.0);
    st = s.worldStatus();
    CHECK(st.sectionTitle == "Calm again" || st.sectionTitle == "Calm");
    s.unloadWorld();
}

TEST_CASE("worlds/built-in Dimensions parse; sections stay inside their songs") {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(ATMOS_SOURCE_DIR) / "apps/ios/Viksen/Resources/Worlds";
    // Song lengths (seconds) of the built-in audio, which lives outside this repository.
    const std::map<std::string, double> lengths = {
        {"hinds-of-morrow", 561.4}, {"light-of-a-distant-sun", 425.97}, {"steel-steam-and-borrowed-dreams", 683.55},
        {"the-quiet-storm/the-silent-alley", 355.37},
        {"the-quiet-storm/in-different-times", 436.65},
        {"the-listening-sky/dance-of-the-dragonfly", 871.53},
        {"viksen/lunar-promise", 364.57}};   // the core theme: plays as composed (no sections yet)
    int songs = 0;
    for (const auto& dir : fs::directory_iterator(root)) {
        std::ifstream in(dir.path() / "world.json");
        REQUIRE(in.good());
        std::string err;
        auto d = parseDimension(Json::parse(in), &err);
        if (!d) std::printf("%s: %s\n", dir.path().filename().string().c_str(), err.c_str());
        REQUIRE(d.has_value());
        CHECK(d->id == dir.path().filename().string());
        CHECK(d->artist == "Krowsingre");
        for (const auto& w : d->songs) {
            CHECK((!w.sections.empty() || w.id == "viksen/lunar-promise"));
            CHECK(w.stems.size() <= 16);
            const auto len = lengths.find(w.id);
            if (len == lengths.end()) std::printf("unknown song %s\n", w.id.c_str());
            REQUIRE(len != lengths.end());
            double prevEnd = 0;
            for (const auto& s : w.sections) {
                CHECK(s.toSeconds <= len->second + 0.5);       // within the song
                CHECK(s.fromSeconds >= prevEnd - 1e-6);          // in order, no overlaps
                prevEnd = s.toSeconds;
                CHECK(s.when.all.empty() || !s.label.empty());   // every reactive part says why it plays
            }
            ++songs;
        }
    }
    CHECK(songs == 7);
}

namespace {
Json twoSongDimension() {
    return Json::parse(R"({
      "format": "atmos.world", "schemaVersion": 1, "id": "duo", "name": "Duo", "artist": "Krowsingre", "tier": "free",
      "songs": [
        {"id": "a", "title": "Song A", "music": {"tempo": 120, "key": "C major"},
         "adaptive": {"jumpEveryBars": 1, "minStayBars": 1, "crossfadeBeats": 0.5}, "fadeSeconds": 0.5,
         "stems": [{"id": "x", "file": "a-x.wav"}],
         "sections": [{"id": "calm", "title": "Calm", "fromBar": 0, "toBar": 4, "when": {"all": [{"type": "still"}]}, "label": "You are still"}]},
        {"id": "b", "title": "Song B", "music": {"tempo": 90, "key": "D minor"},
         "adaptive": {"jumpEveryBars": 1, "minStayBars": 1, "crossfadeBeats": 0.5}, "fadeSeconds": 0.5,
         "stems": [{"id": "x", "file": "b-x.wav"}, {"id": "y", "file": "b-y.wav"}],
         "sections": [{"id": "drive", "title": "Drive", "fromBar": 0, "toBar": 3, "when": {"all": [{"type": "moving"}]}, "label": "You are moving"}]}
      ]})");
}
} // namespace

TEST_CASE("worlds/multi-song Dimension parses; single-song files are one-song Dimensions") {
    auto d = parseDimension(twoSongDimension());
    REQUIRE(d.has_value());
    CHECK(d->songs.size() == 2);
    CHECK(d->songs[0].id == "duo/a");
    CHECK(d->songs[1].name == "Song B");
    CHECK(d->songs[1].artist == "Krowsingre");
    CHECK(d->songs[1].tempo == 90.0);
    CHECK(d->totalStems() == 3);
    auto single = parseDimension(sectionWorld());
    REQUIRE(single.has_value());
    CHECK(single->songs.size() == 1);
    Json bad = twoSongDimension();
    bad["songs"][1]["id"] = "a";
    CHECK(!parseDimension(bad).has_value());
}

TEST_CASE("session/multi-song Dimension: moving changes the song; a finished song hands over") {
    auto d = parseDimension(twoSongDimension());
    REQUIRE(d.has_value());
    const double rate = 48000;
    auto tone = [&](double f, double seconds) {
        return std::make_shared<const AudioClip>(test::makeSine(rate, static_cast<int>(rate * seconds), f, 0.3f));
    };
    std::map<std::string, std::shared_ptr<const AudioClip>> files = {
        {"a-x.wav", tone(220, 8)}, {"b-x.wav", tone(880, 8)}, {"b-y.wav", tone(660, 8)}};
    LiveSession::StemFileDecoder decode = [&](const std::string& f, double, std::string& err) -> std::unique_ptr<StemDecoder> {
        auto it = files.find(f);
        if (it == files.end()) { err = "missing " + f; return nullptr; }
        return std::make_unique<ClipStemDecoder>(it->second);
    };
    LiveSession s(rate, 256);
    s.setWorldSpatialEnabled(false);
    std::string err;
    REQUIRE(s.loadDimension(*d, decode, &err));
    ConditionContext ctx;
    ctx.env.motionAvailable = true;
    ctx.env.still = true;
    std::vector<float> l(256), r(256);
    float* outs[2] = {l.data(), r.data()};
    auto run = [&](double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * rate / 256); ++i) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
            while (s.worldStatus().playing && s.engine().worldPlayer().readableFrames() < 256 &&
                   std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            s.updateWorld(256.0 / rate, ctx);
            s.engine().process(nullptr, 0, outs, 2, 256);
        }
    };
    s.updateWorld(0, ctx);
    s.startWorld(true);
    run(2.0);
    auto st = s.worldStatus();
    CHECK(st.name == "Duo");
    CHECK(st.id == "duo");
    CHECK(st.songCount == 2);
    CHECK(st.songIndex == 0);
    CHECK(st.songTitle == "Song A");
    // Start moving: song A has no moving part, song B does -> after a hold and a fade, song B.
    ctx.env.still = false;
    ctx.env.moving = true;
    run(10.0);
    st = s.worldStatus();
    CHECK(st.songIndex == 1);
    CHECK(st.songTitle == "Song B");
    CHECK(st.sectionTitle == "Drive");
    CHECK(st.stems.size() == 2);
    CHECK(st.playing);
    // Senses off (composed): song B plays to its end (8 s), then song A follows.
    ctx.env.motionAvailable = false;
    ctx.env.moving = false;
    run(12.0);
    st = s.worldStatus();
    CHECK(st.songIndex == 0);
    CHECK(st.songSwitches >= 2);
    // Leaving during a song stops for good.
    s.stopWorld(true);
    run(1.0);
    CHECK(!s.worldStatus().playing);
    s.unloadWorld();
}

TEST_CASE("worlds/transition wash rings on, fades to silence, stays bounded; the moon lengthens it") {
    const double rate = 48000;
    const int n = 24000;                                  // a 0.5 s outgoing tail
    std::vector<float> in(static_cast<size_t>(n) * 2);
    for (int i = 0; i < n; ++i) in[2 * i] = in[2 * i + 1] = 0.5f * static_cast<float>(std::sin(2 * 3.14159265 * 330 * i / rate));
    TransitionShape dark, full;
    dark.beatSeconds = full.beatSeconds = 60.0 / 110;
    dark.space = 0.0f;
    full.space = 1.0f;
    const auto t0 = std::chrono::steady_clock::now();
    const auto a = renderTransitionTail(in.data(), n, rate, dark);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const auto b = renderTransitionTail(in.data(), n, rate, full);
    CHECK(b.size() > a.size());                           // full moon: longer
    float peak = 0, after = 0;
    for (float v : a) { CHECK(std::isfinite(v)); peak = std::max(peak, std::fabs(v)); }
    for (size_t i = static_cast<size_t>(n) * 2 + 48000 * 2; i < static_cast<size_t>(n) * 2 + 48000 * 3; ++i) after = std::max(after, std::fabs(a[i]));
    CHECK(peak < 1.5f);
    CHECK(after > 0.001f);                                // still ringing 1 s after the input stopped
    CHECK(std::fabs(a[a.size() - 2]) < 1e-4f);            // and ends in silence, not a cut
    std::printf("transition wash: %.1f ms for %zu frames\n", ms, a.size() / 2);
    CHECK(ms < 500);
    // The submerge crossfade starts as the outgoing part and ends as the incoming one.
    std::vector<float> out(in), inc(static_cast<size_t>(n) * 2, 0.25f);
    submergeCrossfade(out.data(), inc.data(), n, rate);
    CHECK(std::fabs(inc[static_cast<size_t>(2 * (n - 1))] - 0.25f) < 0.02f);
}

TEST_CASE("session/a crossfade longer than the stream buffer never stalls the stream") {
    // 60 BPM, 4-beat crossfade = 4 s at 48 kHz, longer than the ~2.7 s stream ring.
    Json j = sectionWorld();
    j["music"]["tempo"] = 60;
    j["adaptive"] = {{"jumpEveryBars", 1}, {"minStayBars", 1}, {"crossfadeBeats", 4}};
    j["sections"][0]["toBar"] = 3;
    j["sections"][1]["fromBar"] = 3;
    j["sections"][1]["toBar"] = 6;
    j["sections"][2]["fromBar"] = 6;
    j["sections"][2]["toBar"] = 9;
    auto w = parseWorld(j);
    REQUIRE(w.has_value());
    const double rate = 48000;
    auto clip = std::make_shared<const AudioClip>(test::makeSine(rate, static_cast<int>(rate * 40), 220, 0.3f));
    StemDecoderFactory factory = [&](double, std::string&) {
        std::vector<std::unique_ptr<StemDecoder>> v;
        v.push_back(std::make_unique<ClipStemDecoder>(clip));
        return v;
    };
    LiveSession s(rate, 256);
    s.setWorldSpatialEnabled(false);
    REQUIRE(s.loadWorld(*w, factory));
    ConditionContext ctx;
    ctx.env.motionAvailable = true;
    ctx.env.still = true;
    s.updateWorld(0, ctx);
    s.startWorld(true);
    std::vector<float> l(256), r(256);
    float* outs[2] = {l.data(), r.data()};
    uint64_t starved = 0;
    bool heardDrive = false;
    for (int i = 0; i < static_cast<int>(30 * rate / 256); ++i) {
        if (i == static_cast<int>(2 * rate / 256)) { ctx.env.still = false; ctx.env.moving = true; }   // force a jump
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
        while (s.engine().worldPlayer().readableFrames() < 256 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (s.engine().worldPlayer().readableFrames() < 256) ++starved;
        s.updateWorld(256.0 / rate, ctx);
        s.engine().process(nullptr, 0, outs, 2, 256);
        if (i % 64 == 0) heardDrive = heardDrive || s.worldStatus().sectionTitle == "Drive";
    }
    const auto st = s.worldStatus();
    CHECK(st.sectionJumps >= 1);
    CHECK(starved == 0);                       // the stream kept flowing through the long crossfade
    CHECK(heardDrive);                         // the jump to the moving part was heard (when it ends depends on timing)
    s.unloadWorld();
}

TEST_CASE("session/lunar Dimension: day track by day, tonight's lunar song after dark") {
    const double t = 20000.25;                                  // a fixed moment
    const std::string tonight = moonPhaseId(moonPhaseName(t));
    const std::string other = moonPhaseId(static_cast<MoonPhaseName>((static_cast<int>(moonPhaseName(t)) + 4) % 8));
    Json j = twoSongDimension();
    j["songs"][1]["lunarPhase"] = other;
    Json third = j["songs"][1];
    third["id"] = "c";
    third["title"] = "Song C";
    third["lunarPhase"] = tonight;
    j["songs"].push_back(third);
    auto d = parseDimension(j);
    REQUIRE(d.has_value());
    const double rate = 48000;
    auto clip = std::make_shared<const AudioClip>(test::makeSine(rate, static_cast<int>(rate * 30), 330, 0.3f));
    LiveSession::StemFileDecoder decode = [&](const std::string&, double, std::string&) -> std::unique_ptr<StemDecoder> {
        return std::make_unique<ClipStemDecoder>(clip);
    };
    LiveSession s(rate, 256);
    s.setWorldSpatialEnabled(false);
    ConditionContext ctx;
    ctx.daysSinceUnixEpochUtc = t;
    std::vector<float> l(256), r(256);
    float* outs[2] = {l.data(), r.data()};
    auto run = [&](double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * rate / 256); ++i) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
            while (s.worldStatus().playing && s.engine().worldPlayer().readableFrames() < 256 &&
                   std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            s.updateWorld(256.0 / rate, ctx);
            s.engine().process(nullptr, 0, outs, 2, 256);
        }
    };
    std::string err;
    REQUIRE(s.loadDimension(*d, decode, &err));
    s.updateWorld(0, ctx);
    s.startWorld(true);
    run(6.0);
    CHECK(s.worldStatus().songTitle == "Song A");               // day: the day track stays
    ctx.night = true;
    run(10.0);
    auto st = s.worldStatus();
    CHECK(st.songTitle == "Song C");                             // dusk: tonight's lunar song
    CHECK(st.songLunarPhase == tonight);
    ctx.night = false;
    run(10.0);
    CHECK(s.worldStatus().songTitle == "Song A");               // dawn: back to the day track
    s.unloadWorld();
    // Entering at night starts straight on tonight's song.
    ctx.night = true;
    s.updateWorld(0, ctx);
    REQUIRE(s.loadDimension(*d, decode, &err));
    CHECK(s.worldStatus().songTitle == "Song C");
    s.unloadWorld();
}
