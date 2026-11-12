#include <cmath>

#include "core/worlds/MusicReactor.h"
#include "engine/audio/AudioEngine.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

namespace {
float db(float g) { return 20.0f * std::log10(g); }
void run(MusicReactor& r, const ReactorInputs& in, double seconds) {
    for (double t = 0; t < seconds; t += 1.0 / 30) r.update(1.0 / 30, in);
}
} // namespace

TEST_CASE("reactor/no cues or adaptive off: the song plays as mixed") {
    MusicReactor r;
    r.configure({"drums", "strings", "lead-vocals"}, Json::object());
    ReactorInputs none;
    run(r, none, 3);
    CHECK(!r.engaged());
    CHECK(r.cutoffHz() > 19900);
    for (int i = 0; i < 3; ++i) CHECK(std::fabs(db(r.stemGain(i))) < 0.1f);
    ReactorInputs off;
    off.micActive = true;
    off.roomLevelDb = -60;
    off.adaptive = false;
    run(r, off, 3);
    CHECK(r.cutoffHz() > 19900);
    CHECK(std::fabs(db(r.stemGain(0))) < 0.1f);
}

TEST_CASE("reactor/a calm room closes the music; a loud, busy one opens it fast") {
    MusicReactor r;
    r.configure({"drums", "strings", "lead-vocals"}, Json::object());
    ReactorInputs calm;
    calm.micActive = true;
    calm.roomLevelDb = -62;
    run(r, calm, 10);
    CHECK(r.engaged());
    CHECK(r.cutoffHz() < 5500);                 // gently darkened
    CHECK(db(r.stemGain(0)) < -8);              // drums down
    CHECK(std::fabs(db(r.stemGain(1))) < 0.5f); // strings stay
    CHECK(std::fabs(db(r.stemGain(2))) < 0.1f); // the voice is never moved
    ReactorInputs loud = calm;
    loud.roomLevelDb = -22;
    loud.onsetsPerSecond = 4;
    run(r, loud, 0.6);                          // within about half a second...
    CHECK(r.cutoffHz() > 10000);
    CHECK(db(r.stemGain(0)) > -4);              // ...the drums are most of the way up
    run(r, loud, 3);
    CHECK(r.cutoffHz() > 19900);
    CHECK(db(r.stemGain(1)) < -3);              // pads step back at full energy
    run(r, calm, 0.6);                          // and it settles slowly, not with a jolt
    CHECK(db(r.stemGain(0)) > -4);
}

TEST_CASE("reactor/movement or a workout lift the energy too; songs can set roles and depth") {
    MusicReactor r;
    Json react = {{"depthDb", 12.0}, {"stems", {{"lead-vocals", "energy"}, {"drums", "neutral"}}}};
    r.configure({"drums", "lead-vocals"}, react);
    ReactorInputs still;
    still.motionAvailable = true;
    still.activity = 0;
    run(r, still, 10);
    CHECK(db(r.stemGain(1)) < -10);   // the song made the voice an energy stem, 12 dB deep
    CHECK(std::fabs(db(r.stemGain(0))) < 0.1f);
    ReactorInputs workout = still;
    workout.intensity = 0.9;
    run(r, workout, 3);
    CHECK(db(r.stemGain(1)) > -2.5);
}

TEST_CASE("engine/the music filter cuts the highs when closed and is bypassed when open") {
    auto measure = [](float cutoff) {
        AudioEngine e;
        e.setMasterGain(1.0f);
        e.setLimiterCeilingDb(0.0f);
        e.prepare({48000, 512});
        e.setMusicCutoffHz(cutoff);
        auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 48000, 8000, 0.25f));
        // An 8 kHz tone on the music bus.
        std::vector<float> l(512), r(512);
        float* outs[2] = {l.data(), r.data()};
        e.setLayer(0, clip, Bus::Music);
        e.setLayerGain(0, 1.0f);
        e.play();
        double sum = 0;
        for (int b = 0; b < 60; ++b) {
            e.process(nullptr, 0, outs, 2, 512);
            if (b > 20) for (float v : l) sum += double(v) * v;
        }
        return sum;
    };
    const double open = measure(20000), closed = measure(1000);
    CHECK(open > 0);
    CHECK(closed < open * 0.01);   // 8 kHz is far above a 1 kHz cutoff: at least 20 dB down
}
