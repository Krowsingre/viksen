#include <atomic>
#include <bit>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "engine/environment/RoomInstrument.h"
#include "tests/TestHarness.h"

using atmos::RoomInstrument;

namespace {
constexpr double kSr = 48000;
constexpr double kPi = 3.14159265358979;
double energyOf(const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX) {
    double s = 0; to = std::min(to, v.size());
    for (size_t i = from; i < to; ++i) s += double(v[i]) * v[i];
    return s;
}
uint16_t mask(std::initializer_list<int> pcs) { uint16_t m = 0; for (int p : pcs) m |= static_cast<uint16_t>(1u << p); return m; }
struct Run { std::vector<float> l, r; };
Run run(RoomInstrument& ri, const std::vector<float>& in, int block = 256) {
    Run out{std::vector<float>(in.size(), 0.0f), std::vector<float>(in.size(), 0.0f)};
    for (size_t i = 0; i < in.size(); i += static_cast<size_t>(block)) {
        const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), in.size() - i));
        ri.processAdd(in.data() + i, out.l.data() + i, out.r.data() + i, n);
    }
    return out;
}
std::vector<float> sine(double hz, double seconds, float amp, double from = 0, double total = -1) {
    std::vector<float> v(static_cast<size_t>(kSr * (total > 0 ? total : seconds)), 0.0f);
    const size_t a = static_cast<size_t>(kSr * from);
    for (size_t i = 0; i < static_cast<size_t>(kSr * seconds) && a + i < v.size(); ++i) v[a + i] = amp * static_cast<float>(std::sin(2 * kPi * hz * i / kSr));
    return v;
}
std::vector<float> noise(double seconds, float sd, unsigned seed = 3) {
    std::mt19937 rng(seed); std::normal_distribution<float> n(0.0f, sd);
    std::vector<float> v(static_cast<size_t>(kSr * seconds));
    for (auto& x : v) x = n(rng);
    return v;
}
double strength(const std::vector<float>& v, double hz, size_t from, size_t to) {
    const double w = 2 * kPi * hz / kSr, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < to && i < v.size(); ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / double(to - from);
}
// One effect alone, without the plain room under it.
void alone(RoomInstrument& ri, int fx) { ri.solo.store(fx); ri.dry.store(0.0f); }
} // namespace

TEST_CASE("room/keys become pentatonic scales and roots") {
    CHECK(RoomInstrument::scaleForKey("G minor") == mask({7, 10, 0, 2, 5}));
    CHECK(RoomInstrument::scaleForKey("D\xE2\x99\xAD major") == mask({1, 3, 5, 8, 10}));
    CHECK(RoomInstrument::scaleForKey("C# minor") == mask({1, 4, 6, 8, 11}));
    CHECK(RoomInstrument::scaleForKey("") == mask({9, 0, 2, 4, 7}));
    CHECK(RoomInstrument::rootForKey("G minor") == 7);
    CHECK(RoomInstrument::rootForKey("E\xE2\x99\xAD minor") == 3);
    CHECK(RoomInstrument::rootForKey("") == 9);
    CHECK(RoomInstrument::fxFromName("grains") == RoomInstrument::kGrains);
    CHECK(RoomInstrument::fxFromName("nope") == -1);
}

TEST_CASE("room/silence stays silent: every effect, the arrangement, with or without a song") {
    for (double bpm : {0.0, 120.0})
        for (int fx = -1; fx <= RoomInstrument::kFxCount; ++fx) {
            RoomInstrument ri; ri.prepare(kSr);
            ri.solo.store(fx);
            ri.setClock(0.0, bpm);
            auto out = run(ri, std::vector<float>(static_cast<size_t>(kSr * 3), 0.0f));
            CHECK(energyOf(out.l) == 0.0);
            CHECK(energyOf(out.r) == 0.0);
        }
}

TEST_CASE("room/the arrangement plays one effect at a time and moves on at its bar") {
    RoomInstrument ri; ri.prepare(kSr);
    ri.setScenes({{static_cast<uint16_t>(1u << RoomInstrument::kGrains), 1}, {static_cast<uint16_t>(1u << RoomInstrument::kFreqDream), 2}});
    ri.setClock(0.0, 120.0);                       // a bar = 2 s
    run(ri, std::vector<float>(256, 0.0f));
    const int first = ri.sceneIndex();
    REQUIRE((first == 0 || first == 1));
    CHECK(std::popcount(static_cast<unsigned>(ri.activeFx())) == 1);
    const double firstBars = first == 0 ? 1 : 2;
    // Just before the scene's end, still the same; just after, the next one.
    run(ri, std::vector<float>(static_cast<size_t>(kSr * (firstBars * 2.0 - 0.1)), 0.0f));
    CHECK(ri.sceneIndex() == first);
    run(ri, std::vector<float>(static_cast<size_t>(kSr * 0.2), 0.0f));
    CHECK(ri.sceneIndex() == (first + 1) % 2);
    CHECK(ri.activeFx() == (first == 0 ? (1u << RoomInstrument::kFreqDream) : (1u << RoomInstrument::kGrains)));
    // The default arrangement holds every room effect once.
    uint16_t all = 0;
    for (const auto& s : RoomInstrument::defaultScenes()) { CHECK(std::popcount(static_cast<unsigned>(s.fx)) == 1); all |= s.fx; }
    CHECK(all == (1u << RoomInstrument::kFxCount) - 1);
}

TEST_CASE("room/each effect is made from the room: sound in, sound out") {
    const auto in = noise(6.0, 0.05f);
    for (int fx = 0; fx < RoomInstrument::kFxCount; ++fx) {
        if (fx == RoomInstrument::kMemory || fx == RoomInstrument::kTune) continue;   // memory: the engine's; tune: pitches only
        RoomInstrument ri; ri.prepare(kSr);
        alone(ri, fx);
        ri.setClock(0.0, 120.0);
        auto out = run(ri, in);
        const double e = energyOf(out.l, static_cast<size_t>(kSr * 3)) + energyOf(out.r, static_cast<size_t>(kSr * 3));
        if (e <= 1.0) std::printf("    effect %s: energy %g\n", RoomInstrument::fxName(fx), e);
        CHECK(e > 1.0);
    }
    // Nothing but the plain room when "none" is chosen, and the plain room is high-passed.
    RoomInstrument none; none.prepare(kSr);
    none.solo.store(RoomInstrument::kFxCount);
    auto plain = run(none, sine(60.0, 2.0, 0.3f));
    CHECK(energyOf(plain.l, static_cast<size_t>(kSr)) < 0.15 * energyOf(sine(60.0, 1.0, 0.3f)));
}

TEST_CASE("room/the resonator rings on the song's root, fifth and ninth") {
    RoomInstrument ri; ri.prepare(kSr);
    alone(ri, RoomInstrument::kResonator);
    ri.setScale(RoomInstrument::scaleForKey("A minor"), RoomInstrument::rootForKey("A minor"));
    std::vector<float> in(static_cast<size_t>(kSr * 4), 0.0f);
    const size_t at = static_cast<size_t>(kSr * 1.5);   // after the fade-in
    std::mt19937 rng(7); std::normal_distribution<float> n(0.0f, 0.3f);
    for (size_t i = 0; i < 480; ++i) in[at + i] = n(rng) * std::exp(-static_cast<float>(i) / 100.0f);
    auto out = run(ri, in);
    const auto notes = ri.resonatorNotes();
    CHECK(notes[0] == 57); CHECK(notes[1] == 64); CHECK(notes[2] == 71);   // A3, E4, B4
    CHECK(energyOf(out.l, 0, at) == 0.0);
    const size_t from = at + static_cast<size_t>(kSr * 0.3), to = at + static_cast<size_t>(kSr * 1.3);
    CHECK(energyOf(out.l, from, to) > 1e-4);
    for (int note : {57, 64}) {
        const double hz = 440.0 * std::pow(2.0, (note - 69) / 12.0), off = hz * std::pow(2.0, 0.5 / 12.0);
        CHECK(strength(out.l, hz, from, to) > 3.0 * strength(out.l, off, from, to));
    }
}

TEST_CASE("room/the frequency dream slides the room down toward -500 Hz and back") {
    RoomInstrument ri; ri.prepare(kSr);
    alone(ri, RoomInstrument::kFreqDream);
    ri.setClock(0.0, 120.0);                       // turns every 2 bars (4 s); a full slide takes 8.5 s
    auto out = run(ri, sine(1000.0, 8.0, 0.2f));
    // Near 4 s it has slid about 235 Hz down (1000 Hz becomes ~765-795 Hz).
    const size_t a = static_cast<size_t>(kSr * 3.6), b = static_cast<size_t>(kSr * 4.0);
    CHECK(strength(out.l, 780.0, a, b) > 3.0 * strength(out.l, 1000.0, a, b));
    // Then it heads back up over 8.5 s: by 8 s it is about 125 Hz down (~875 Hz).
    const size_t c = static_cast<size_t>(kSr * 7.6), d = static_cast<size_t>(kSr * 8.0);
    CHECK(strength(out.l, 880.0, c, d) > 3.0 * strength(out.l, 780.0, c, d));
}

TEST_CASE("room/the delay dream echoes the room a semitone up, on the beat") {
    RoomInstrument ri; ri.prepare(kSr);
    alone(ri, RoomInstrument::kDelayDream);
    ri.setClock(0.0, 120.0);                       // a beat = 0.5 s
    auto out = run(ri, sine(440.0, 0.3, 0.3f, 1.5, 5.0));
    const size_t at = static_cast<size_t>(kSr * 1.5);
    const size_t due = at + static_cast<size_t>(kSr * (0.5 * 2.0 / 3.0 + 0.5));   // 2/3 beat + 1 beat
    CHECK(energyOf(out.l, at, due - 2000) < 1e-6);
    CHECK(energyOf(out.l, due, due + static_cast<size_t>(kSr * 0.3)) > 0.5);
    const size_t m = due + 4000;
    CHECK(strength(out.l, 466.16, m, m + 8000) > 2.0 * strength(out.l, 440.0, m, m + 8000));
}

TEST_CASE("room/the groove cuts back into the last 20 seconds") {
    RoomInstrument ri; ri.prepare(kSr);
    alone(ri, RoomInstrument::kGroove);
    ri.setClock(0.0, 120.0);
    auto in = noise(8.0, 0.1f);
    std::fill(in.begin() + static_cast<long>(kSr * 2), in.end(), 0.0f);   // the room falls silent after 2 s
    auto out = run(ri, in);
    // In the silence, the groove keeps playing pieces of what the room did.
    CHECK(energyOf(out.l, static_cast<size_t>(kSr * 4), static_cast<size_t>(kSr * 8)) > 1.0);
}

TEST_CASE("room/memory comes in only in its scene; tune follows the room's pitch") {
    RoomInstrument ri; ri.prepare(kSr);
    alone(ri, RoomInstrument::kMemory);
    run(ri, std::vector<float>(static_cast<size_t>(kSr * 4), 0.0f));
    CHECK(ri.memoryAmount() > 0.9f);
    ri.solo.store(RoomInstrument::kGrains);
    run(ri, std::vector<float>(static_cast<size_t>(kSr * 8), 0.0f));
    CHECK(ri.memoryAmount() < 0.01f);

    RoomInstrument t; t.prepare(kSr);
    alone(t, RoomInstrument::kTune);
    t.setScale(RoomInstrument::scaleForKey("A minor"), 9);
    t.setClock(0.0, 120.0);
    std::vector<float> in(static_cast<size_t>(kSr * 6));
    for (size_t i = 0; i < in.size(); ++i) {
        const double p = 2 * kPi * 220.0 * i / kSr;
        in[i] = 0.2f * static_cast<float>(std::sin(p) + 0.5 * std::sin(2 * p));
    }
    auto out = run(t, in);
    CHECK(std::fabs(t.trackedHz() - 220.0f) < 3.0f);
    CHECK(energyOf(out.l, static_cast<size_t>(kSr * 4)) > 1.0);
}
