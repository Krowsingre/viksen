#include <filesystem>
#include <vector>

#include "core/assets/WavFile.h"
#include "core/session/LiveSession.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

namespace {
void runBlocks(LiveSession& s, const float* micIn, int blocks) {
    std::vector<float> l(512), r(512);
    float* outs[2] = {l.data(), r.data()};
    const float* ins[1] = {micIn};
    for (int b = 0; b < blocks; ++b) s.engine().process(micIn ? ins : nullptr, micIn ? 1 : 0, outs, 2, 512);
}
} // namespace

TEST_CASE("recording/full mix and partial modes capture the right buses, and nothing when off") {
    auto dir = test::tempDir("rec");
    LiveSession s(48000, 512);
    s.setMasterGain(1.0f);
    s.setLimiterCeilingDb(0.0f);
    s.setBedEnabled(true);                       // atmosphere present
    s.setBusGain(Bus::Atmosphere, 0.5f);
    s.setMicEnabled(true);
    s.setBusGain(Bus::MicRaw, 1.0f);             // mic present
    std::vector<float> mic(512, 0.2f);

    runBlocks(s, mic.data(), 20);                // not recording: nothing captured
    CHECK(!s.recording());

    REQUIRE(s.startRecording(dir / "full.wav", CaptureMode::FullMix));
    runBlocks(s, mic.data(), 200);               // ~2.1 s
    const double secs = s.stopRecording();
    CHECK_NEAR(secs, 200 * 512 / 48000.0, 0.02);
    AudioClip full;
    AudioFileInfo info;
    REQUIRE(readWav(dir / "full.wav", full, &info).ok);
    CHECK(info.sampleRate == 48000 && info.numChannels == 2 && info.bitsPerSample == 24);
    CHECK(info.numFrames == 200 * 512);
    CHECK(test::rmsOf(full.samples.channel(0) + 48000, 4800) > 0.1); // mic DC is in the full mix

    REQUIRE(s.startRecording(dir / "atmos.wav", CaptureMode::AtmosphereOnly));
    runBlocks(s, mic.data(), 100);
    s.stopRecording();
    AudioClip atmos;
    REQUIRE(readWav(dir / "atmos.wav", atmos).ok);
    // Atmosphere only: the bed (zero-mean noise), not the mic's constant 0.2 offset.
    double mean = 0;
    for (int i = 10000; i < 40000; ++i) mean += atmos.samples.channel(0)[i];
    mean /= 30000;
    CHECK(std::fabs(mean) < 0.02);
    CHECK(test::rmsOf(atmos.samples.channel(0) + 10000, 30000) > 0.001);

    REQUIRE(s.startRecording(dir / "mic.wav", CaptureMode::MicOnly));
    s.setBedEnabled(false);
    runBlocks(s, mic.data(), 100);
    s.stopRecording();
    AudioClip micRec;
    REQUIRE(readWav(dir / "mic.wav", micRec).ok);
    CHECK(test::rmsOf(micRec.samples.channel(0) + 30000, 10000) > 0.1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("recording/a device change finishes the file cleanly") {
    auto dir = test::tempDir("rec2");
    LiveSession s(48000, 512);
    s.setBedEnabled(true);
    s.setBusGain(Bus::Atmosphere, 1.0f);
    REQUIRE(s.startRecording(dir / "a.wav", CaptureMode::FullMix));
    runBlocks(s, nullptr, 50);
    s.rebuild(44100, 512);
    CHECK(!s.recording());
    CHECK(s.takeRecordingInterrupted());
    CHECK(!s.takeRecordingInterrupted());
    AudioFileInfo info;
    REQUIRE(readWavInfo(dir / "a.wav", info).ok);
    CHECK(info.numFrames == 50 * 512);
    std::filesystem::remove_all(dir);
}
