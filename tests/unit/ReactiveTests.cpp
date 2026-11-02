#include <cmath>
#include <numbers>
#include <vector>

#include "core/session/LiveSession.h"
#include "core/worlds/WorldConditions.h"
#include "core/worlds/WorldLifecycle.h"
#include "engine/automation/TickClock.h"
#include "engine/dsp/FrequencyShifter.h"
#include "engine/environment/MotionAnalyzer.h"
#include "engine/environment/RoomMemory.h"
#include "engine/reactive/ConditionGate.h"
#include "engine/reactive/EnvironmentStates.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;
using namespace atmos::core;

namespace {
// Magnitude of one frequency in a signal (single-bin DFT).
double toneMagnitude(const float* x, int n, double freq, double rate) {
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / (n - 1)); // Hann
        re += x[i] * w * std::cos(2 * std::numbers::pi * freq * i / rate);
        im -= x[i] * w * std::sin(2 * std::numbers::pi * freq * i / rate);
    }
    return std::sqrt(re * re + im * im) / n;
}
} // namespace

TEST_CASE("dsp/frequency shifter moves a tone by the shift amount") {
    for (double shift : {150.0, -250.0}) {
        FrequencyShifter fs;
        fs.prepare(48000);
        fs.setShiftHz(static_cast<float>(shift));
        auto tone = test::makeSine(48000, 48000, 1000.0, 0.5f, 1);
        std::vector<float> out(48000);
        for (int i = 0; i < 48000; ++i) out[i] = fs.process(tone.samples.channel(0)[i]);
        const float* steady = out.data() + 4800;
        const double target = toneMagnitude(steady, 38400, 1000.0 + shift, 48000);
        const double original = toneMagnitude(steady, 38400, 1000.0, 48000);
        const double mirror = toneMagnitude(steady, 38400, 1000.0 - shift, 48000);
        CHECK(target > 0.1);
        CHECK(original < target * 0.02);  // carrier suppressed by > 34 dB
        CHECK(mirror < target * 0.02);    // opposite sideband suppressed
    }
}

TEST_CASE("environment/room memory replays the room pitched down, then forgets") {
    RoomMemory rm;
    rm.prepare(48000, 0.25); // short slots for the test
    rm.setEnabled(true);
    rm.pitchSemitones.store(-12.0f);
    auto tone = test::makeSine(48000, 48000, 800.0, 0.5f, 1);
    AudioBuffer out(2, 48000);
    for (int i = 0; i < 48000; i += 480) {
        AudioBuffer block(2, 480);
        rm.capture(tone.samples.channel(0) + i, 480);
        rm.renderAdd(block, 480);
        for (int ch = 0; ch < 2; ++ch) std::copy(block.channel(ch), block.channel(ch) + 480, out.channel(ch) + i);
    }
    // Nothing plays until at least two memories exist (0.5 s).
    CHECK(test::peakOf(out.channel(0), 24000 - 480) == 0.0f);
    std::vector<float> mono(24000);
    for (int i = 0; i < 24000; ++i) mono[i] = out.channel(0)[24000 + i] + out.channel(1)[24000 + i];
    CHECK(toneMagnitude(mono.data(), 24000, 400.0, 48000) > 5 * toneMagnitude(mono.data(), 24000, 800.0, 48000));
    CHECK(rm.activeVoices() > 0);

    rm.setEnabled(false);
    rm.capture(tone.samples.channel(0), 480); // disabling wipes memory and voices
    CHECK(rm.activeVoices() == 0);
    AudioBuffer after(2, 4800);
    rm.renderAdd(after, 4800);
    CHECK(test::peakOf(after.channel(0), 4800) == 0.0f);
}

TEST_CASE("engine/turning the mic off wipes room memory") {
    AudioEngine e;
    e.prepare({48000, 512});
    e.setMicInputEnabled(true);
    e.roomMemory().setEnabled(true);
    std::vector<float> in(512, 0.3f), l(512), r(512);
    const float* ins[1] = {in.data()};
    float* outs[2] = {l.data(), r.data()};
    for (int i = 0; i < 1500; ++i) e.process(ins, 1, outs, 2, 512); // ~16 s
    CHECK(e.roomMemory().activeVoices() > 0);
    e.setMicInputEnabled(false);
    e.process(ins, 1, outs, 2, 512);
    CHECK(e.roomMemory().activeVoices() == 0);
}

TEST_CASE("reactive/condition gate needs hold time and has hysteresis") {
    ConditionGate g(ConditionGate::Direction::Below, -50.0f, -45.0f, 1.0, 0.5);
    for (int i = 0; i < 9; ++i) g.update(-60.0f, 0.1);
    CHECK(!g.state());                  // 0.9 s: not yet
    CHECK(g.update(-60.0f, 0.2));       // passes 1.0 s
    CHECK(g.state());
    for (int i = 0; i < 20; ++i) g.update(-47.0f, 0.1); // between thresholds: stays on
    CHECK(g.state());
    g.update(-40.0f, 0.3);
    g.update(-60.0f, 0.1);              // brief return resets the pending switch
    g.update(-40.0f, 0.3);
    CHECK(g.state());
    g.update(-40.0f, 0.3);
    CHECK(!g.state());
}

TEST_CASE("reactive/environment states from mic level and motion") {
    EnvironmentStates s;
    EnvironmentFeatures quiet; quiet.levelDb = -70.0f;
    EnvironmentFeatures loud; loud.levelDb = -20.0f;
    for (int i = 0; i < 30; ++i) s.update(quiet, true, 0.1);
    CHECK(s.snapshot().quiet && !s.snapshot().loud && s.snapshot().loudness == 0);
    for (int i = 0; i < 30; ++i) s.update(loud, true, 0.1);
    CHECK(!s.snapshot().quiet && s.snapshot().loud && s.snapshot().loudness == 4);
    s.update(loud, false, 0.1); // mic off: no claims
    CHECK(!s.snapshot().loud);

    CHECK(!s.snapshot().still); // motion not available yet
    s.setMotionAvailable(true);
    MotionAnalyzer m;
    for (int i = 0; i < 200; ++i) { m.addSample(0, 0, -1.0, 1.0 / 30); s.updateMotion(m.energy(), 1.0 / 30); }
    CHECK(s.snapshot().still && !s.snapshot().moving);
    for (int i = 0; i < 200; ++i) {
        const double bump = 0.3 * std::sin(i * 1.7);
        m.addSample(0.2 * std::cos(i * 0.9), 0, -1.0 + bump, 1.0 / 30);
        s.updateMotion(m.energy(), 1.0 / 30);
    }
    CHECK(s.snapshot().moving && !s.snapshot().still && s.snapshot().activity >= 3);
}

TEST_CASE("reactive/motion analyzer detects free fall once") {
    MotionAnalyzer m;
    for (int i = 0; i < 30; ++i) m.addSample(0, 0, -1, 0.01);
    m.addSample(0, 0, 0.05, 0.05); // single jolt: not a fall
    CHECK(m.freeFallEvents() == 0);
    for (int i = 0; i < 30; ++i) m.addSample(0.02, 0, 0.03, 0.01);
    CHECK(m.freeFallEvents() == 1);
}

TEST_CASE("automation/tick clock is sample-accurate and wraps") {
    TickClock c;
    c.prepare(48000);
    c.setTempo(120.0, 8, 16); // 32nd notes at 120 BPM = 3000 frames per tick
    TickEvent ev[64];
    int total = 0, lastTick = -1;
    long long frame = 0;
    std::vector<long long> positions;
    for (int b = 0; b < 200; ++b) {
        const int n = c.advance(333, ev, 64);
        for (int i = 0; i < n; ++i) { positions.push_back(frame + ev[i].frameOffset); lastTick = ev[i].tick; }
        total += n;
        frame += 333;
    }
    CHECK(total == static_cast<int>((200 * 333 - 1) / 3000) + 1);
    CHECK(positions[0] == 0 && positions[5] == 15000);
    CHECK(lastTick == (total - 1) % 16);
}

TEST_CASE("worlds/lifecycle ducks, recovers, counts down and fades out") {
    WorldLifecycle w({1.0, 0.3, 0.5, 4.0, 1.0});
    for (int i = 0; i < 12; ++i) w.update(true, 0.1);
    CHECK(w.state() == WorldLifecycle::State::Active && w.gain() == 1.0);
    for (int i = 0; i < 20; ++i) w.update(false, 0.1);
    CHECK(w.state() == WorldLifecycle::State::Fading);
    CHECK_NEAR(w.gain(), 0.3, 1e-9);
    CHECK_NEAR(w.countdownRemaining(), 2.1, 1e-6); // fading began on the first 'lost' update
    w.update(true, 0.1); // conditions return
    CHECK(w.state() == WorldLifecycle::State::Active);
    for (int i = 0; i < 43; ++i) w.update(false, 0.1); // 4 s countdown, then fading out
    CHECK(w.state() == WorldLifecycle::State::Ending);
    CHECK(w.gain() < 0.3 && w.gain() > 0.0);
    for (int i = 0; i < 15; ++i) w.update(false, 0.1);
    CHECK(w.finished() && w.gain() == 0.0);

    WorldLifecycle stay({0.1, 0.3, 0.5, 1.0, 1.0});
    stay.setStayForever(true);
    for (int i = 0; i < 100; ++i) stay.update(false, 0.1);
    CHECK(stay.state() == WorldLifecycle::State::Active);
    stay.requestExit();
    for (int i = 0; i < 20; ++i) stay.update(false, 0.1);
    CHECK(stay.finished());
}

TEST_CASE("worlds/lunar calendar matches published moon phases") {
    // Published UTC times: new moon 2024-01-11 11:57, first quarter 2024-01-18 03:53,
    // full moons 2024-09-18 02:34 and 2025-01-13 22:27.
    CHECK(moonPhaseName(19733.4979) == MoonPhaseName::New);
    CHECK(lunarDay(19733.4979 + 0.6) <= 2);
    CHECK(moonPhaseName(19740.1618) == MoonPhaseName::FirstQuarter);
    CHECK(moonPhaseName(19984.1069) == MoonPhaseName::Full);
    CHECK(isFullMoon(19984.1069));
    CHECK(moonPhaseName(20101.9354) == MoonPhaseName::Full);
    CHECK(!isFullMoon(20101.9354 + 7.0));
    const int d = lunarDay(19984.1069);
    CHECK(d >= 14 && d <= 17);
    CHECK(std::string(moonPhaseId(MoonPhaseName::WaningGibbous)) == "waningGibbous");
}

TEST_CASE("worlds/conditions parse, evaluate, round-trip and refuse unknown types") {
    Json j = Json::parse(R"({"all":[{"type":"moonPhase","phase":"full"},{"type":"timeWindow","from":23,"to":5},
                                    {"type":"quiet"},{"type":"lunarDay","from":28,"to":2}]})");
    auto set = parseConditions(j);
    REQUIRE(set.has_value());
    CHECK(set->all.size() == 4);
    ConditionContext c;
    c.daysSinceUnixEpochUtc = 19984.1069; // full moon
    c.localHour = 1;
    c.env.quiet = true;
    CHECK(!set->evaluate(c));             // lunar day ~15 is outside 28..2
    set->all.pop_back();
    CHECK(set->evaluate(c));
    c.localHour = 14;
    CHECK(!set->evaluate(c));             // outside 23:00-05:00
    CHECK(conditionsToJson(*set)["all"][0]["phase"] == "full");

    auto future = parseConditions(Json::parse(R"({"all":[{"type":"weather","is":"rain"},{"type":"moonPhase","phase":"blue"}]})"));
    REQUIRE(future.has_value());
    CHECK(future->hasUnknown());
    CHECK(!future->evaluate(c));          // never a free unlock
    CHECK(conditionsToJson(*future)["all"].size() == 2); // preserved
    CHECK(!parseConditions(Json::array()).has_value());
}

TEST_CASE("session/environment states and new mic settings survive a rebuild") {
    LiveSession s(48000, 512);
    s.setMicShiftMix(0.7f);
    s.setRoomMemoryEnabled(true);
    s.setRoomMemoryPitch(-7.0f);
    s.rebuild(44100, 512);
    CHECK_NEAR(s.engine().micTransform().shiftMix.get(), 0.7, 1e-6);
    CHECK(s.engine().roomMemory().enabled());
    CHECK_NEAR(s.engine().roomMemory().pitchSemitones.load(), -7.0, 1e-6);
    s.setMotionAvailable(true);
    for (int i = 0; i < 150; ++i) s.addMotionSample(0, 0, -1, 1.0 / 30);
    CHECK(s.environmentStates().still);
}
