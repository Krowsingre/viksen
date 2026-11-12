#include <cmath>
#include <random>
#include <vector>

#include "engine/environment/RoomInstrument.h"
#include "tests/TestHarness.h"

using atmos::RoomInstrument;

namespace {
constexpr double kSr = 48000;
double energyOf(const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX) {
    double s = 0; to = std::min(to, v.size());
    for (size_t i = from; i < to; ++i) s += double(v[i]) * v[i];
    return s;
}
uint16_t mask(std::initializer_list<int> pcs) { uint16_t m = 0; for (int p : pcs) m |= static_cast<uint16_t>(1u << p); return m; }
struct Run { std::vector<float> l, r; };
Run run(RoomInstrument& ri, const std::vector<float>& in) {
    Run out{std::vector<float>(in.size(), 0.0f), std::vector<float>(in.size(), 0.0f)};
    for (size_t i = 0; i < in.size(); i += 256) {
        const int n = static_cast<int>(std::min<size_t>(256, in.size() - i));
        ri.processAdd(in.data() + i, out.l.data() + i, out.r.data() + i, n);
    }
    return out;
}
std::vector<float> tone(double hz, double seconds, float amp = 0.2f) {
    std::vector<float> v(static_cast<size_t>(kSr * seconds));
    for (size_t i = 0; i < v.size(); ++i) {
        const double p = 2 * 3.14159265358979 * hz * i / kSr;
        v[i] = amp * static_cast<float>(std::sin(p) + 0.5 * std::sin(2 * p) + 0.25 * std::sin(3 * p));   // a voice-like tone
    }
    return v;
}
} // namespace

TEST_CASE("room/keys become pentatonic scales") {
    CHECK(RoomInstrument::scaleForKey("G minor") == mask({7, 10, 0, 2, 5}));
    CHECK(RoomInstrument::scaleForKey("D\xE2\x99\xAD major") == mask({1, 3, 5, 8, 10}));
    CHECK(RoomInstrument::scaleForKey("C# minor") == mask({1, 4, 6, 8, 11}));
    CHECK(RoomInstrument::scaleForKey("") == mask({9, 0, 2, 4, 7}));   // unknown: A minor pentatonic
}

TEST_CASE("room/the room's pitch is followed and sung back in the scale") {
    RoomInstrument ri;
    ri.prepare(kSr);
    ri.grooveLevel.store(0); ri.hitsLevel.store(0);
    ri.setScale(mask({9, 0, 2, 4, 7}));
    auto out = run(ri, tone(220.0, 1.5));
    CHECK(std::fabs(ri.trackedHz() - 220.0f) < 3.0f);
    CHECK(ri.tuneNote() == 57);                       // A3
    CHECK(energyOf(out.l, out.l.size() / 2) > 1.0);   // and it sings
    // A pitch between scale notes snaps to the nearest one (A# -> A, in A minor pentatonic).
    RoomInstrument r2; r2.prepare(kSr); r2.grooveLevel.store(0); r2.hitsLevel.store(0);
    r2.setScale(mask({9, 0, 2, 4, 7}));
    run(r2, tone(233.08, 1.5));
    CHECK(r2.tuneNote() == 57);
}

TEST_CASE("room/silence stays silent; noise becomes rhythm only with a beat") {
    RoomInstrument ri;
    ri.prepare(kSr);
    auto quiet = run(ri, std::vector<float>(static_cast<size_t>(kSr * 2), 0.0f));
    CHECK(energyOf(quiet.l) == 0.0);
    // Steady noise, no song clock: no groove, no hits, no pitch.
    std::mt19937 rng(3); std::normal_distribution<float> n(0.0f, 0.05f);
    std::vector<float> noise(static_cast<size_t>(kSr * 4));
    for (auto& v : noise) v = n(rng);
    RoomInstrument a; a.prepare(kSr);
    auto noBeat = run(a, noise);
    CHECK(energyOf(noBeat.l, static_cast<size_t>(kSr * 2.5)) < 1e-3);
    // With the song's beat (120 BPM), slices of the room come back on the 16ths.
    RoomInstrument b; b.prepare(kSr); b.hitsLevel.store(0); b.tuneLevel.store(0);
    b.energy.store(1.0f);
    b.setClock(0.0, 120.0);
    auto groove = run(b, noise);
    CHECK(energyOf(groove.l, static_cast<size_t>(kSr * 2)) > 1.0);
}

TEST_CASE("room/a sudden sound is answered with a bell") {
    RoomInstrument ri;
    ri.prepare(kSr);
    ri.tuneLevel.store(0); ri.grooveLevel.store(0);
    std::vector<float> in(static_cast<size_t>(kSr * 4), 0.0f);
    std::mt19937 rng(7); std::normal_distribution<float> n(0.0f, 0.002f);
    for (auto& v : in) v = n(rng);                       // a quiet room
    const size_t at = static_cast<size_t>(kSr * 3);      // after the 2 s warm-up
    for (size_t i = 0; i < 2000; ++i) in[at + i] += 0.6f * std::exp(-static_cast<float>(i) / 300.0f) * (i % 2 ? 1.0f : -1.0f);   // a clap
    auto out = run(ri, in);
    CHECK(energyOf(out.l, 0, at) < 1e-6);                 // nothing before
    CHECK(energyOf(out.l, at, at + static_cast<size_t>(kSr * 0.5)) > 0.5);   // a bell after
}
