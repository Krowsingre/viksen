#include "core/projects/ProjectPackage.h"
#include "core/session/SessionDocument.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"
#include "core/assets/WavFile.h"

using namespace atmos;
using namespace atmos::core;

TEST_CASE("session/live settings round-trip through a project package; mic never restored") {
    LiveSettings s;
    s.busGain[static_cast<int>(Bus::Atmosphere)] = 0.8f;
    s.busMuted[static_cast<int>(Bus::MicRaw)] = true;
    s.masterGain = 0.55f;
    s.bedEnabled = true;
    s.bedColourHz = 2400.0f;
    s.micShiftMix = 0.4f;
    s.roomMemoryEnabled = true;
    s.roomMemoryPitch = -5.0f;
    s.micEnabled = true;

    auto dir = test::tempDir("sessdoc");
    auto clip = test::makeSine(48000, 4800, 440, 0.3f);
    REQUIRE(writeWav(dir / "song.wav", clip.samples, 48000).ok);
    auto pkg = dir / "Evening.viksen";
    std::string rel;
    REQUIRE(ProjectPackage::importAsset(pkg, dir / "song.wav", rel).ok);
    auto m = makeSessionProject("Evening", s, SessionTrack{rel, "song.wav", {48000, 2, 4800, Json::object()}},
                                "hinds-of-morrow", 48000);
    REQUIRE(ProjectPackage::save(pkg, m).ok);

    ProjectManifest back;
    REQUIRE(ProjectPackage::load(pkg, back).ok);
    auto r = readSessionProject(back);
    CHECK_NEAR(r.settings.busGain[static_cast<int>(Bus::Atmosphere)], 0.8, 1e-6);
    CHECK(r.settings.busMuted[static_cast<int>(Bus::MicRaw)]);
    CHECK_NEAR(r.settings.masterGain, 0.55, 1e-6);
    CHECK(r.settings.bedEnabled && r.settings.bedColourHz == 2400.0f);
    CHECK(r.settings.roomMemoryEnabled && r.settings.roomMemoryPitch == -5.0f);
    CHECK(!r.settings.micEnabled);
    CHECK(r.dimensionId == "hinds-of-morrow");
    REQUIRE(r.track.has_value());
    CHECK(std::filesystem::exists(pkg / r.track->packagedPath));
    CHECK(back.masterGainDb < -5.0f);   // project-level master mirrors the live gain

    LiveSession live(48000, 512);
    live.setMicEnabled(true);
    live.applyAllSettings(r.settings);
    CHECK(live.settings().micEnabled);  // applying a project never switches the mic
    CHECK(live.settings().bedEnabled);
    std::filesystem::remove_all(dir);
}

TEST_CASE("session/garbage live settings fall back to defaults") {
    LiveSettings base;
    auto s = liveSettingsFromJson(Json::parse(R"({"masterGain": "loud", "buses": 7, "bed": {"level": 9}, "limiterCeilingDb": 12})"), base);
    CHECK(s.masterGain == base.masterGain);
    CHECK(s.bedLevel == 0.6f);              // clamped to the slider range (hearing safety)
    CHECK(s.limiterCeilingDb == 0.0f);      // never above 0 dBFS
}
