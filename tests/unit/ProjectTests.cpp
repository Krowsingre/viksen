#include <fstream>

#include "core/assets/ClipPreparation.h"
#include "core/assets/WavFile.h"
#include "core/projects/ProjectManifest.h"
#include "core/projects/ProjectPackage.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

TEST_CASE("wav/round trip 24-bit and float preserves samples and rate") {
    auto dir = test::tempDir("wav");
    auto clip = test::makeSine(44100, 2000, 523.25, 0.7f, 2, 300);
    for (auto fmt : {WavSampleFormat::Int16, WavSampleFormat::Int24, WavSampleFormat::Float32}) {
        auto path = dir / "x.wav";
        REQUIRE(writeWav(path, clip.samples, clip.sampleRate, fmt).ok);
        AudioClip back;
        AudioFileInfo info;
        auto r = readWav(path, back, &info);
        REQUIRE(r.ok);
        CHECK(info.sampleRate == 44100);
        CHECK(info.numChannels == 2);
        CHECK(back.numFrames() == clip.numFrames());
        const double tol = fmt == WavSampleFormat::Int16 ? 2.0 / 32768 : fmt == WavSampleFormat::Int24 ? 2.0 / 8388608 : 0.0;
        double maxErr = 0;
        for (int i = 0; i < clip.numFrames(); ++i)
            maxErr = std::max(maxErr, double(std::fabs(back.samples.channel(1)[i] - clip.samples.channel(1)[i])));
        CHECK(maxErr <= tol);
        CHECK(test::firstNonZero(back.samples.channel(0), back.numFrames(), 1e-4f) >= 300); // leading silence intact
    }
    std::filesystem::remove_all(dir);
}

TEST_CASE("wav/rejects garbage") {
    auto dir = test::tempDir("wavbad");
    { std::ofstream f(dir / "bad.wav", std::ios::binary); f << "RIFF....WAVEjunkjunkjunk"; }
    AudioClip c;
    CHECK(!readWav(dir / "bad.wav", c).ok);
    CHECK(!readWav(dir / "missing.wav", c).ok);
    std::filesystem::remove_all(dir);
}

TEST_CASE("clip preparation resamples to the engine rate") {
    auto clip = loadWavForEngine("does-not-exist.wav", 48000);
    CHECK(clip == nullptr);
    auto prepared = prepareClipForEngine(test::makeSine(44100, 44100, 440), 48000);
    CHECK(prepared->sampleRate == 48000);
    CHECK(prepared->numFrames() == 48000);
}

TEST_CASE("project/manifest round trip preserves known and unknown data") {
    auto m = ProjectManifest::createNew("Night Drive");
    m.bpm = 92.5;
    m.sources.push_back({"s1", SourceKind::Track, SourceStorage::Packaged, "assets/audio/song.wav", "Song", "g1", true,
                         {48000, 2, 480000, Json{{"codec", "pcm"}}}, Json{{"futureSourceField", 7}}});
    m.layers.push_back({"l1", "s1", Bus::Music, 1234, -2.5f, false, false, Json{{"automation", {{"lane", "x"}}}}});
    m.extra["worldsV2"] = {{"something", true}};
    m.sessionExtra["timeSignature"] = "7/8";
    m.busesExtra["sidechain"] = {{"gainDb", -3}};

    const Json j = manifestToJson(m);
    ProjectManifest back;
    auto r = manifestFromJson(j, back);
    REQUIRE(r.ok);
    CHECK(!r.migrated);
    CHECK(back.name == "Night Drive");
    CHECK(back.bpm && *back.bpm == 92.5);
    REQUIRE(back.sources.size() == 1);
    CHECK(back.sources[0].isReferenceMix);
    CHECK(back.sources[0].media.frames == 480000);
    REQUIRE(back.layers.size() == 1);
    CHECK(back.layers[0].timelineStartFrame == 1234);
    CHECK_NEAR(back.layers[0].gainDb, -2.5, 1e-6);
    // Byte-identical JSON after a second round trip => nothing dropped or reordered semantically.
    CHECK(manifestToJson(back) == j);
    CHECK(j["worldsV2"]["something"] == true);
    CHECK(j["session"]["timeSignature"] == "7/8");
    CHECK(j["buses"]["sidechain"]["gainDb"] == -3);
    CHECK(j["sources"][0]["futureSourceField"] == 7);
    CHECK(j["sources"][0]["media"]["codec"] == "pcm");
    CHECK(j["layers"][0]["automation"]["lane"] == "x");
}

TEST_CASE("project/newer schema is opened and its version is not downgraded") {
    auto m = ProjectManifest::createNew("Future");
    Json j = manifestToJson(m);
    j["schemaVersion"] = kCurrentSchemaVersion + 3;
    j["hologram"] = {{"x", 1}};
    ProjectManifest back;
    auto r = manifestFromJson(j, back);
    REQUIRE(r.ok);
    CHECK(r.newerThanSupported);
    const Json out = manifestToJson(back);
    CHECK(out["schemaVersion"] == kCurrentSchemaVersion + 3);
    CHECK(out["hologram"]["x"] == 1);
}

TEST_CASE("project/migrates v0 prototype documents") {
    Json v0 = {{"version", 0}, {"name", "Old"}, {"tracks", {"/music/a.wav", {{"path", "/music/b.wav"}}}}};
    ProjectManifest m;
    auto r = manifestFromJson(v0, m);
    REQUIRE(r.ok);
    CHECK(r.migrated);
    CHECK(r.originalSchemaVersion == 0);
    REQUIRE(m.sources.size() == 2);
    CHECK(m.sources[1].path == "/music/b.wav");
    CHECK(m.sources[1].storage == SourceStorage::External);
    CHECK(m.layers.size() == 2);
    CHECK(validate(m).empty());
}

TEST_CASE("project/malformed manifests fail without throwing") {
    ProjectManifest m;
    CHECK(!manifestFromJson(Json::array(), m).ok);
    CHECK(!manifestFromJson(Json{{"format", "something.else"}, {"schemaVersion", 1}}, m).ok);
    CHECK(!manifestFromJson(Json{{"name", "no version"}}, m).ok);
    auto r = manifestFromJson(Json{{"schemaVersion", 1}, {"sources", "oops"}, {"buses", 5}, {"layers", {{{"id", 3}}}}}, m);
    CHECK(r.ok); // tolerated: wrong-typed sections are ignored, not fatal
}

TEST_CASE("project/validation catches broken references and double playback") {
    auto m = ProjectManifest::createNew("V");
    m.sources.push_back({"mix", SourceKind::Track, SourceStorage::Packaged, "assets/audio/mix.wav", "", "song", true, {}, Json::object()});
    m.sources.push_back({"drums", SourceKind::Stem, SourceStorage::Packaged, "assets/audio/drums.wav", "", "song", false, {}, Json::object()});
    m.sources.push_back({"evil", SourceKind::Sample, SourceStorage::Packaged, "../../etc/passwd", "", "", false, {}, Json::object()});
    m.layers.push_back({"l1", "mix", Bus::Music, 0, 0, false, false, Json::object()});
    m.layers.push_back({"l2", "drums", Bus::Music, 0, 0, false, false, Json::object()});
    m.layers.push_back({"l3", "ghost", Bus::Music, 0, 0, false, false, Json::object()});
    auto issues = validate(m);
    auto has = [&](const std::string& needle) {
        for (auto& i : issues) if (i.message.find(needle) != std::string::npos) return true;
        return false;
    };
    CHECK(has("missing source ghost"));
    CHECK(has("double playback"));
    CHECK(has("escapes the package"));
}

TEST_CASE("project/package save, reopen, original media untouched, backup recovery") {
    auto work = test::tempDir("pkg");
    auto original = work / "song.wav";
    auto clip = test::makeSine(48000, 4800, 440, 0.5f);
    REQUIRE(writeWav(original, clip.samples, 48000).ok);
    const auto originalSize = std::filesystem::file_size(original);
    const auto originalTime = std::filesystem::last_write_time(original);

    auto pkg = work / "Test.proj";
    std::string rel;
    REQUIRE(ProjectPackage::importAsset(pkg, original, rel).ok);
    CHECK(rel == "assets/audio/song.wav");
    std::string rel2;
    REQUIRE(ProjectPackage::importAsset(pkg, original, rel2).ok);
    CHECK(rel2 == "assets/audio/song-1.wav");

    auto m = ProjectManifest::createNew("Pkg");
    m.sources.push_back({"s1", SourceKind::Track, SourceStorage::Packaged, rel, "song", "", false, {48000, 2, 4800, Json::object()}, Json::object()});
    m.sources.push_back({"s2", SourceKind::Texture, SourceStorage::External, (work / "nope.wav").generic_string(), "", "", false, {}, Json::object()});
    m.layers.push_back({"l1", "s1", Bus::Music, 0, 0, false, false, Json::object()});
    REQUIRE(ProjectPackage::save(pkg, m).ok);
    REQUIRE(ProjectPackage::save(pkg, m).ok); // second save creates a backup
    CHECK(std::filesystem::exists(pkg / ProjectPackage::kBackupName));
    for (auto sub : {"layers", "automation", "worlds", "sessions", "thumbnails"}) CHECK(std::filesystem::is_directory(pkg / sub));

    ProjectManifest back;
    REQUIRE(ProjectPackage::load(pkg, back).ok);
    CHECK(back.id == m.id);
    auto missing = ProjectPackage::findMissingAssets(pkg, back);
    REQUIRE(missing.size() == 1);
    CHECK(missing[0].sourceId == "s2");
    CHECK(ProjectPackage::relink(back, "s2", original, SourceStorage::External));
    CHECK(ProjectPackage::findMissingAssets(pkg, back).empty());

    CHECK(std::filesystem::file_size(original) == originalSize);
    CHECK(std::filesystem::last_write_time(original) == originalTime);

    // Corrupt the primary manifest: load recovers from backup.
    { std::ofstream f(pkg / ProjectPackage::kManifestName, std::ios::trunc); f << "{ not json"; }
    ProjectManifest recovered;
    auto r = ProjectPackage::load(pkg, recovered);
    CHECK(r.ok);
    CHECK(r.error.find("recovered") != std::string::npos);
    CHECK(recovered.id == m.id);
    std::filesystem::remove_all(work);
}
