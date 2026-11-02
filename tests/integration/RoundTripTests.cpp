// End-to-end: import WAV stems -> project package -> reopen -> apply to engine ->
// offline render -> export WAV -> verify against the sources.
#include <map>

#include "core/assets/ClipPreparation.h"
#include "core/assets/WavFile.h"
#include "core/projects/ProjectPackage.h"
#include "core/projects/ProjectPlayback.h"
#include "engine/rendering/OfflineRenderer.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

TEST_CASE("integration/import stems, save, reopen, render, export") {
    auto work = test::tempDir("e2e");
    // Two 44.1 kHz stems with different leading silence, plus a reference mix.
    auto drums = test::makeImpulse(44100, 44100, 4410, 0.5f);   // hit at 100 ms
    auto pad = test::makeImpulse(44100, 44100, 22050, 0.25f);   // hit at 500 ms
    REQUIRE(writeWav(work / "drums.wav", drums.samples, 44100, WavSampleFormat::Float32).ok);
    REQUIRE(writeWav(work / "pad.wav", pad.samples, 44100, WavSampleFormat::Float32).ok);

    auto pkg = work / "Song.proj";
    auto m = ProjectManifest::createNew("E2E");
    for (auto name : {"drums", "pad"}) {
        std::string rel;
        REQUIRE(ProjectPackage::importAsset(pkg, work / (std::string(name) + ".wav"), rel).ok);
        AudioFileInfo info;
        REQUIRE(readWavInfo(pkg / rel, info).ok);
        m.sources.push_back({name, SourceKind::Stem, SourceStorage::Packaged, rel, name, "song", false,
                             {info.sampleRate, info.numChannels, info.numFrames, Json::object()}, Json::object()});
        m.layers.push_back({std::string("layer-") + name, name, Bus::Music, 0, 0.0f, false, false, Json::object()});
    }
    m.buses[static_cast<int>(Bus::Music)].gainDb = 0.0f;
    m.masterGainDb = 0.0f;
    m.limiterCeilingDb = 0.0f;
    REQUIRE(ProjectPackage::save(pkg, m).ok);

    ProjectManifest reopened;
    REQUIRE(ProjectPackage::load(pkg, reopened).ok);
    AudioEngine engine;
    engine.prepare({48000, 512});
    std::map<std::string, ClipPtr> clips;
    for (const auto& s : reopened.sources) clips[s.id] = loadWavForEngine(ProjectPackage::resolve(pkg, s), 48000);
    auto report = applyToEngine(reopened, clips, engine);
    CHECK(report.layersLoaded == 2);
    CHECK(report.warnings.empty());

    AudioBuffer render;
    REQUIRE(renderOffline(engine, {}, render));
    CHECK(render.numFrames() == 48000);
    // Impulses resampled to 48k land at 100 ms (4800) and 500 ms (24000).
    int peakA = 0, peakB = 0;
    for (int i = 0; i < 12000; ++i) if (std::fabs(render.channel(0)[i]) > std::fabs(render.channel(0)[peakA])) peakA = i;
    for (int i = 12000; i < 48000; ++i) if (std::fabs(render.channel(0)[i]) > std::fabs(render.channel(0)[peakB])) peakB = i;
    CHECK(std::abs(peakA - 4800) <= 1);
    CHECK(std::abs(peakB - 24000) <= 1);

    REQUIRE(writeWav(work / "render.wav", render, 48000, WavSampleFormat::Int24).ok);
    AudioFileInfo out;
    REQUIRE(readWavInfo(work / "render.wav", out).ok);
    CHECK(out.sampleRate == 48000 && out.numChannels == 2 && out.numFrames == 48000);
    std::filesystem::remove_all(work);
}
