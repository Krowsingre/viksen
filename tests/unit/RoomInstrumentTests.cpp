#include <atomic>
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
// Only the named voice on.
void only(RoomInstrument& ri, std::atomic<float>& keep) {
    for (auto* l : {&ri.resonanceLevel, &ri.grainLevel, &ri.shiftEchoLevel, &ri.pitchEchoLevel, &ri.grooveLevel, &ri.tuneLevel})
        if (l != &keep) l->store(0.0f);
}
// Strength of one frequency (Goertzel), per sample.
double strength(const std::vector<float>& v, double hz, size_t from, size_t to) {
    const double w = 2 * 3.14159265358979 * hz / kSr, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < to && i < v.size(); ++i) { const double s0 = v[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / double(to - from);
}
// The period of a stretch of sound, by autocorrelation, as a frequency (lagMin..lagMax).
double pitchOf(const std::vector<float>& v, size_t from, size_t len, double loHz, double hiHz) {
    double best = -1; int bestLag = 0;
    for (int lag = static_cast<int>(kSr / hiHz); lag <= static_cast<int>(kSr / loHz); ++lag) {
        double c = 0, e1 = 0, e2 = 0;
        for (size_t i = from; i < from + len; ++i) { c += double(v[i]) * v[i + lag]; e1 += double(v[i]) * v[i]; e2 += double(v[i + lag]) * v[i + lag]; }
        const double r = c / std::sqrt(e1 * e2 + 1e-20);
        if (r > best) { best = r; bestLag = lag; }
    }
    return kSr / bestLag;
}
} // namespace

TEST_CASE("room/keys become pentatonic scales and roots") {
    CHECK(RoomInstrument::scaleForKey("G minor") == mask({7, 10, 0, 2, 5}));
    CHECK(RoomInstrument::scaleForKey("D\xE2\x99\xAD major") == mask({1, 3, 5, 8, 10}));
    CHECK(RoomInstrument::scaleForKey("C# minor") == mask({1, 4, 6, 8, 11}));
    CHECK(RoomInstrument::scaleForKey("") == mask({9, 0, 2, 4, 7}));   // unknown: A minor pentatonic
    CHECK(RoomInstrument::rootForKey("G minor") == 7);
    CHECK(RoomInstrument::rootForKey("D\xE2\x99\xAD major") == 1);
    CHECK(RoomInstrument::rootForKey("E\xE2\x99\xAD minor") == 3);
    CHECK(RoomInstrument::rootForKey("C# minor") == 1);
    CHECK(RoomInstrument::rootForKey("") == 9);
}

TEST_CASE("room/the tune voice follows the room's pitch in the scale") {
    RoomInstrument ri;
    ri.prepare(kSr);
    only(ri, ri.tuneLevel);
    ri.setScale(mask({9, 0, 2, 4, 7}), 9);
    auto out = run(ri, tone(220.0, 1.5));
    CHECK(std::fabs(ri.trackedHz() - 220.0f) < 3.0f);
    CHECK(ri.tuneNote() == 57);                       // A3
    CHECK(energyOf(out.l, out.l.size() / 2) > 1.0);   // and it sings
    // A pitch between scale notes snaps to the nearest one (A# -> A, in A minor pentatonic).
    RoomInstrument r2; r2.prepare(kSr); only(r2, r2.tuneLevel);
    r2.setScale(mask({9, 0, 2, 4, 7}), 9);
    run(r2, tone(233.08, 1.5));
    CHECK(r2.tuneNote() == 57);
}

TEST_CASE("room/silence stays silent with every voice on, with or without a beat") {
    for (double bpm : {0.0, 120.0}) {
        RoomInstrument ri;
        ri.prepare(kSr);
        ri.setClock(0.0, bpm);
        auto quiet = run(ri, std::vector<float>(static_cast<size_t>(kSr * 3), 0.0f));
        CHECK(energyOf(quiet.l) == 0.0);
        CHECK(energyOf(quiet.r) == 0.0);
    }
}

TEST_CASE("room/the groove slices the room only on a beat") {
    std::mt19937 rng(3); std::normal_distribution<float> n(0.0f, 0.05f);
    std::vector<float> noise(static_cast<size_t>(kSr * 4));
    for (auto& v : noise) v = n(rng);
    RoomInstrument a; a.prepare(kSr); only(a, a.grooveLevel);
    auto noBeat = run(a, noise);
    CHECK(energyOf(noBeat.l) < 1e-9);
    RoomInstrument b; b.prepare(kSr); only(b, b.grooveLevel);
    b.energy.store(1.0f);
    b.setClock(0.0, 120.0);
    auto groove = run(b, noise);
    CHECK(energyOf(groove.l, static_cast<size_t>(kSr * 2)) > 1.0);
}

TEST_CASE("room/a tap rings the resonators on the song's scale notes") {
    RoomInstrument ri;
    ri.prepare(kSr);
    only(ri, ri.resonanceLevel);
    ri.setScale(RoomInstrument::scaleForKey("A minor"), RoomInstrument::rootForKey("A minor"));
    std::vector<float> in(static_cast<size_t>(kSr * 2), 0.0f);
    const size_t at = static_cast<size_t>(kSr * 0.5);
    std::mt19937 rng(7); std::normal_distribution<float> n(0.0f, 0.3f);
    for (size_t i = 0; i < 480; ++i) in[at + i] = n(rng) * std::exp(-static_cast<float>(i) / 100.0f);   // a 10 ms tap
    auto out = run(ri, in);
    // Home chord, as A minor pentatonic steps: A2, A3, D4, G4.
    const auto notes = ri.resonatorNotes();
    CHECK(notes[0] == 45); CHECK(notes[1] == 57); CHECK(notes[2] == 62); CHECK(notes[3] == 67);
    CHECK(energyOf(out.l, 0, at) == 0.0);                 // nothing before the tap
    const size_t from = at + static_cast<size_t>(kSr * 0.2), to = at + static_cast<size_t>(kSr * 1.2);
    CHECK(energyOf(out.l, from, to) > 1e-3);              // still ringing 200 ms on
    // It rings at the chord's notes, not between them.
    for (int note : {57, 62, 67}) {
        const double hz = 440.0 * std::pow(2.0, (note - 69) / 12.0), off = hz * std::pow(2.0, 0.5 / 12.0);
        CHECK(strength(out.l, hz, from, to) > 4.0 * strength(out.l, off, from, to));
    }
    // With the song's beat the chord walks every two bars: bar 4 (beat 16) is degree 3.
    RoomInstrument b; b.prepare(kSr); only(b, b.resonanceLevel);
    b.setScale(RoomInstrument::scaleForKey("A minor"), 9);
    b.setClock(16.0, 120.0);
    run(b, std::vector<float>(512, 0.0f));
    const auto walked = b.resonatorNotes();
    CHECK(walked[0] == 52); CHECK(walked[1] == 64); CHECK(walked[2] == 69); CHECK(walked[3] == 74);
}

TEST_CASE("room/grains are made of the room's last second") {
    RoomInstrument ri;
    ri.prepare(kSr);
    only(ri, ri.grainLevel);
    std::vector<float> in(static_cast<size_t>(kSr * 4), 0.0f);
    std::mt19937 rng(5); std::normal_distribution<float> n(0.0f, 0.1f);
    const size_t start = static_cast<size_t>(kSr * 1), stop = static_cast<size_t>(kSr * 2);
    for (size_t i = start; i < stop; ++i) in[i] = n(rng);
    auto out = run(ri, in);
    CHECK(energyOf(out.l, 0, start) == 0.0);                                         // no room, no grains
    CHECK(energyOf(out.l, start + static_cast<size_t>(kSr * 0.5), stop) > 0.1);      // the room, scattered
    CHECK(energyOf(out.l, stop, stop + static_cast<size_t>(kSr * 0.3)) > 0.01);      // and a little after it
    CHECK(energyOf(out.l, stop + static_cast<size_t>(kSr * 1.3), in.size()) == 0.0); // gone once out of reach
}

TEST_CASE("room/the shifted echo repeats the room, an octave down, on the beat") {
    for (double bpm : {0.0, 120.0}) {
        RoomInstrument ri;
        ri.prepare(kSr);
        only(ri, ri.shiftEchoLevel);
        ri.setClock(0.0, bpm);
        const double delay = bpm > 0 ? 0.75 * 60.0 / bpm : 0.5;   // 3/4 beat, or 0.5 s
        std::vector<float> in(static_cast<size_t>(kSr * 2), 0.0f);
        const size_t at = static_cast<size_t>(kSr * 0.5), len = static_cast<size_t>(kSr * 0.3);
        for (size_t i = 0; i < len; ++i) in[at + i] = 0.3f * static_cast<float>(std::sin(2 * 3.14159265358979 * 440.0 * i / kSr));
        auto out = run(ri, in);
        const size_t echoAt = at + static_cast<size_t>(kSr * delay);
        CHECK(energyOf(out.l, 0, echoAt - 100) < 1e-9);          // nothing before the echo is due
        CHECK(energyOf(out.l, echoAt, echoAt + len) > 1.0);      // then the echo
        const size_t mid = echoAt + static_cast<size_t>(kSr * 0.05);
        CHECK(strength(out.l, 220.0, mid, mid + 9600) > 3.0 * strength(out.l, 440.0, mid, mid + 9600));   // an octave down
    }
}

TEST_CASE("room/a held note in the room is echoed in key on the 16ths") {
    // 455 Hz is between A4 (440) and A#4; in A minor pentatonic it is retuned to A4.
    RoomInstrument ri;
    ri.prepare(kSr);
    only(ri, ri.pitchEchoLevel);
    ri.setScale(RoomInstrument::scaleForKey("A minor"), 9);
    std::vector<float> in(static_cast<size_t>(kSr * 2), 0.0f);
    const size_t len = static_cast<size_t>(kSr * 0.5);
    for (size_t i = 0; i < len; ++i) in[i] = 0.3f * static_cast<float>(std::sin(2 * 3.14159265358979 * 455.0 * i / kSr));
    auto out = run(ri, in);
    CHECK(ri.pitchEchoes() == 1);
    size_t first = 0;
    while (first < out.l.size() && out.l[first] == 0.0f) ++first;
    REQUIRE(first < out.l.size());
    const double hz = pitchOf(out.l, first + 3000, 4096, 300.0, 600.0);
    CHECK(std::fabs(hz - 440.0) < 4.0);
    CHECK(energyOf(out.l, static_cast<size_t>(kSr * 1.5), out.l.size()) == 0.0);   // three repeats, then quiet

    // With the song's beat, each repeat starts exactly on a 16th (120 BPM: every 6000 samples).
    RoomInstrument b; b.prepare(kSr); only(b, b.pitchEchoLevel);
    b.setScale(RoomInstrument::scaleForKey("A minor"), 9);
    b.setClock(0.0, 120.0);
    std::vector<float> in2(static_cast<size_t>(kSr * 3), 0.0f);
    std::mt19937 rng(9); std::normal_distribution<float> n(0.0f, 0.001f);
    for (auto& v : in2) v = n(rng);                       // a quiet room
    for (size_t i = 0; i < len; ++i) in2[static_cast<size_t>(kSr) + i] += 0.3f * static_cast<float>(std::sin(2 * 3.14159265358979 * 455.0 * i / kSr));
    auto out2 = run(b, in2);
    CHECK(b.pitchEchoes() == 1);
    size_t on = 0;
    while (on < out2.l.size() && out2.l[on] == 0.0f) ++on;
    REQUIRE(on < out2.l.size());
    const size_t off = on % 6000;
    CHECK(std::min(off, 6000 - off) <= 2);
}
