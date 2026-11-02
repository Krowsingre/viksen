#include <chrono>
#include <filesystem>
#include <thread>

#include "core/assets/WavFile.h"
#include "platform/desktop/MiniaudioHost.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;

TEST_CASE("desktop/miniaudio host drives the engine (null device), stops cleanly") {
    core::LiveSession session(48000, 512);
    desktop::MiniaudioHost host(session);
    desktop::HostOptions o;
    o.nullBackend = true;
    std::string err;
    REQUIRE(host.start(o, err));
    CHECK(host.running());
    CHECK(host.backendName() == "Null");
    CHECK(host.sampleRate() > 0);
    const auto before = session.engine().meters().blocksProcessed;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(session.engine().meters().blocksProcessed > before);   // callback is calling the engine
    CHECK(session.sampleRate() == host.sampleRate());             // engine rebuilt for the device rate
    host.stop();
    CHECK(!host.running());
    // Restart with the microphone path.
    o.withInput = true;
    REQUIRE(host.start(o, err));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    host.stop();
}

TEST_CASE("desktop/miniaudio stem decoder streams a WAV exactly and seeks") {
    auto dir = test::tempDir("madec");
    auto clip = test::makeSine(48000, 9600, 440, 0.4f, 2, 100);
    REQUIRE(core::writeWav(dir / "s.wav", clip.samples, 48000, core::WavSampleFormat::Float32).ok);
    std::string err;
    auto d = desktop::makeMiniaudioStemDecoder((dir / "s.wav").string(), 48000, err);
    REQUIRE(d != nullptr);
    CHECK(d->lengthFrames() == clip.numFrames());
    std::vector<float> lr(2 * 4000);
    CHECK(d->read(lr.data(), 4000) == 4000);
    double maxErr = 0;
    for (int i = 0; i < 4000; ++i) maxErr = std::max(maxErr, double(std::fabs(lr[2 * i] - clip.samples.channel(0)[i])));
    CHECK(maxErr < 1e-6);                     // leading silence and samples intact
    CHECK(d->seek(5000));
    CHECK(d->read(lr.data(), 10) == 10);
    CHECK_NEAR(lr[0], clip.samples.channel(0)[5000], 1e-6);
    CHECK(desktop::makeMiniaudioStemDecoder((dir / "missing.wav").string(), 48000, err) == nullptr);
    d.reset();                                // Windows: the file must be closed before removal
    std::filesystem::remove_all(dir);
}
