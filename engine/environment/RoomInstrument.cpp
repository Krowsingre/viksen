#include "engine/environment/RoomInstrument.h"

#include <algorithm>
#include <cmath>

namespace atmos {
namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr int kDecimate = 4;
constexpr int kPitchWindow = 1024;     // at sr / 4: ~85 ms at 48 kHz
constexpr int kPitchHop = 256;         // ~21 ms
constexpr int kTicksPerBeat = 8;       // 32 ticks a bar, as in the studied dreams
constexpr int kTicksPerBar = 32;
constexpr double kIdleBpm = 100.0;     // the clock with no song
constexpr double kFadeSeconds = 0.8;   // effects fade in and out over about a second

double midiToHz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }
long floorMod(long a, long b) { const long m = a % b; return m < 0 ? m + b : m; }
float hp(float x, float& state, float& prev, float a) { state = a * (state + x - prev); prev = x; return state; }
float panL(float p) { return std::sqrt(0.5f * (1.0f - p)); }
float panR(float p) { return std::sqrt(0.5f * (1.0f + p)); }

const char* const kNames[RoomInstrument::kFxCount] = {"straight", "grains", "freqDream", "delayDream", "resonator", "glitch",
                                                     "spiral", "ripple", "groove", "memory", "tune"};
} // namespace

const char* RoomInstrument::fxName(int fx) noexcept { return fx >= 0 && fx < kFxCount ? kNames[fx] : ""; }

int RoomInstrument::fxFromName(const std::string& name) noexcept {
    for (int i = 0; i < kFxCount; ++i) if (name == kNames[i]) return i;
    return -1;
}

// One effect at a time, each for a number of bars, in the pacing of the studied dreams
// (sections of three bars; an effect holds for 9-24 bars).
std::vector<RoomInstrument::Scene> RoomInstrument::defaultScenes() {
    auto one = [](Fx f, int bars) { return Scene{static_cast<uint16_t>(1u << f), bars}; };
    return {one(kGrains, 12), one(kFreqDream, 9), one(kDelayDream, 24), one(kResonator, 9), one(kStraight, 12),
            one(kGlitch, 12), one(kRipple, 12), one(kGroove, 12), one(kSpiral, 9), one(kMemory, 12), one(kTune, 9)};
}

void RoomInstrument::setScenes(const std::vector<Scene>& scenes) noexcept {
    const int n = std::min<int>(static_cast<int>(scenes.size()), kMaxScenes);
    for (int i = 0; i < n; ++i)
        scenes_[static_cast<size_t>(i)].store(static_cast<uint32_t>(scenes[static_cast<size_t>(i)].fx) |
                                              (static_cast<uint32_t>(std::clamp(scenes[static_cast<size_t>(i)].bars, 1, 256)) << 16),
                                              std::memory_order_relaxed);
    sceneCount_.store(n, std::memory_order_release);
}

std::array<int, 3> RoomInstrument::resonatorNotes() const noexcept {
    return {combNotes_[0].load(std::memory_order_relaxed), combNotes_[1].load(std::memory_order_relaxed),
            combNotes_[2].load(std::memory_order_relaxed)};
}

void RoomInstrument::prepare(double sampleRate) {
    sr_ = sampleRate;
    ring_.assign(static_cast<size_t>(sampleRate * 20.5), 0.0f);
    hall_.prepare(sampleRate);
    fdL_.prepare(sampleRate);
    fdR_.prepare(sampleRate);
    ddPre_.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
    ddEcho_.assign(static_cast<size_t>(sampleRate * 3.0), 0.0f);
    ddShift_.prepare(sampleRate, 0.1);
    for (auto& c : combs_) c.line.assign(static_cast<size_t>(sampleRate / 30.0) + 4, 0.0f);
    resEchoA_.assign(static_cast<size_t>(sampleRate * 0.2), 0.0f);
    resEchoB_.assign(static_cast<size_t>(sampleRate * 1.1), 0.0f);
    for (int s = 0; s < 2; ++s) {
        glShift_[s].prepare(sampleRate, 0.1);
        glRep_[s].buf.assign(static_cast<size_t>(sampleRate * 2.05), 0.0f);
        spShift_[s].prepare(sampleRate);
        spShift_[s].setShiftHz(-100.0f);
        spLine_[s].assign(static_cast<size_t>(sampleRate * 0.25), 0.0f);
        rpShift_[s].prepare(sampleRate, 0.1);
    }
    grStereo_.assign(static_cast<size_t>(sampleRate * 0.01), 0.0f);
    pitchBuf_.assign(kPitchWindow, 0.0f);
    if (sceneCount_.load(std::memory_order_acquire) == 0) setScenes(defaultScenes());
    reset();
}

void RoomInstrument::reset() noexcept {
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    ringWrite_ = 0; recorded_ = 0;
    hall_.reset(); hallIn_ = {};
    hp200_ = hp200Prev_ = 0;
    for (auto& g : grains_) g.on = false;
    grainRate_ = 1.0; grainWait_ = 0;
    fdL_.reset(); fdR_.reset(); fdShiftL_ = {}; fdShiftR_ = {}; fdRightIn_ = -1; fdNext_ = 0; fdLow_ = false;
    std::fill(ddPre_.begin(), ddPre_.end(), 0.0f);
    std::fill(ddEcho_.begin(), ddEcho_.end(), 0.0f);
    ddPreW_ = ddEchoW_ = 0; ddShift_.reset(); ddFeedback_ = {}; ddFeedback_.set(0.4, 0); ddCount_ = 0; ddLp_ = {};
    for (auto& c : combs_) { std::fill(c.line.begin(), c.line.end(), 0.0f); c.w = 0; c.lp = 0; }
    combKey_ = 0;
    for (auto& f : resLp_) f = {};
    resLevelDb_ = -60.0f; resCutoff_ = 3000.0f; resHp1_ = resHp1Prev_ = resHp2_ = resHp2Prev_ = 0;
    std::fill(resEchoA_.begin(), resEchoA_.end(), 0.0f);
    std::fill(resEchoB_.begin(), resEchoB_.end(), 0.0f);
    resEchoW_ = 0; resEchoLp_ = {}; resPan_ = {}; resPan_.set(0.5, 0);
    for (int s = 0; s < 2; ++s) {
        glShift_[s].reset(); glPitch_[s] = {}; glLp_[s] = {};
        std::fill(glRep_[s].buf.begin(), glRep_[s].buf.end(), 0.0f);
        glRep_[s].length = 1; glRep_[s].pos = 0; glRep_[s].gateLeft = 0; glRep_[s].gate = 0;
        spShift_[s].reset(); std::fill(spLine_[s].begin(), spLine_[s].end(), 0.0f); spLp_[s] = {}; spHp_[s] = spHpPrev_[s] = 0;
        rpShift_[s].reset(); rpPitch_[s] = {};
    }
    spW_ = 0;
    rpCount_ = 0; rpSign_ = 1; rpRightIn_ = -1;
    grBack_ = sr_; grEvent_ = -1; grWait_ = 8; grMode_ = 4; grGate_ = {}; grGate_.set(1, 0); grPan_ = {};
    std::fill(grStereo_.begin(), grStereo_.end(), 0.0f); grStereoW_ = 0;
    grHp_ = grHpPrev_ = 0; grRumbleLp_ = {}; grRumbleHp_ = grRumbleHpPrev_ = 0;
    std::fill(pitchBuf_.begin(), pitchBuf_.end(), 0.0f);
    pitchWrite_ = decim_ = sinceAnalysis_ = 0; decimAcc_ = 0.0f;
    trackedHz_.store(0.0f, std::memory_order_relaxed);
    heardNote_ = -1; tuneOn_ = false;
    for (auto& g : tuneHz_) g = {};
    tunePhase_ = {}; tuneSaw_ = {}; for (auto& f : tuneLp_) f = {}; vibrato_ = 0;
    gain_.fill(0.0f); target_.fill(0.0f);
    scene_ = -1; started_ = false; ticked_ = false; sceneEnd_ = 0; lastTick_ = 0;
    memoryOut_.store(0.0f, std::memory_order_relaxed);
    activeOut_.store(0, std::memory_order_relaxed);
    sceneOut_.store(-1, std::memory_order_relaxed);
}

uint16_t RoomInstrument::scaleForKey(const std::string& keyName) {
    // Root: a letter, then optional sharp/flat (ASCII or the music symbols).
    static const int letters[] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
    std::string k = keyName;
    if (k.empty() || k[0] < 'A' || k[0] > 'G') return (1u << 9) | (1u << 0) | (1u << 2) | (1u << 4) | (1u << 7);
    int root = letters[k[0] - 'A'];
    size_t i = 1;
    if (k.compare(1, 1, "#") == 0) { root += 1; i = 2; }
    else if (k.compare(1, 1, "b") == 0) { root -= 1; i = 2; }
    else if (k.compare(1, 3, "\xE2\x99\xAF") == 0) { root += 1; i = 4; }   // ♯
    else if (k.compare(1, 3, "\xE2\x99\xAD") == 0) { root -= 1; i = 4; }   // ♭
    root = (root + 12) % 12;
    const bool minor = k.find("minor", i) != std::string::npos || k.find("min", i) != std::string::npos ||
                       (k.size() > i && k[i] == 'm');
    static const int majorPent[] = {0, 2, 4, 7, 9};
    static const int minorPent[] = {0, 3, 5, 7, 10};
    uint16_t mask = 0;
    for (int step : (minor ? minorPent : majorPent)) mask |= static_cast<uint16_t>(1u << ((root + step) % 12));
    return mask;
}

int RoomInstrument::rootForKey(const std::string& k) {
    static const int letters[] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
    if (k.empty() || k[0] < 'A' || k[0] > 'G') return 9;
    int root = letters[k[0] - 'A'];
    if (k.compare(1, 1, "#") == 0 || k.compare(1, 3, "\xE2\x99\xAF") == 0) root += 1;
    else if (k.compare(1, 1, "b") == 0 || k.compare(1, 3, "\xE2\x99\xAD") == 0) root -= 1;
    return (root + 12) % 12;
}

int RoomInstrument::nearestScaleNote(double m) const noexcept {
    const uint16_t mask = scale_.load(std::memory_order_relaxed);
    int best = -1; double bestD = 1e9;
    for (int n = static_cast<int>(std::floor(m)) - 6; n <= static_cast<int>(std::ceil(m)) + 6; ++n)
        if (mask & (1u << (((n % 12) + 12) % 12)))
            if (const double d = std::fabs(n - m); d < bestD) { bestD = d; best = n; }
    return best;
}

void RoomInstrument::trackPitch() noexcept {
    const double rate = sr_ / kDecimate;
    const int minLag = static_cast<int>(rate / 800.0), maxLag = static_cast<int>(rate / 80.0);
    const int n = kPitchWindow;
    auto at = [&](int i) { return pitchBuf_[static_cast<size_t>((pitchWrite_ + i) % n)]; };
    double e0 = 0;
    for (int i = 0; i < n; ++i) e0 += double(at(i)) * at(i);
    if (e0 < 1e-6 * n) { trackedHz_.store(0.0f, std::memory_order_relaxed); return; }
    // Correlation at every lag, then the FIRST peak within 10 % of the strongest: later
    // peaks are the same pitch two or three cycles on (octave errors).
    std::array<double, 512> r{};
    const int lags = std::min(maxLag, minLag + static_cast<int>(r.size()) - 1);
    double top = 0;
    for (int lag = minLag; lag <= lags; ++lag) {
        double c = 0, e1 = 0, e2 = 0;
        for (int i = 0; i + lag < n; ++i) { const float a = at(i), b = at(i + lag); c += a * b; e1 += a * a; e2 += b * b; }
        r[static_cast<size_t>(lag - minLag)] = c / std::sqrt(e1 * e2 + 1e-12);
        top = std::max(top, r[static_cast<size_t>(lag - minLag)]);
    }
    double best = 0; int bestLag = -1;
    std::array<double, 3> around{};
    for (int lag = minLag + 1; lag < lags; ++lag) {
        const double a = r[static_cast<size_t>(lag - minLag - 1)], b = r[static_cast<size_t>(lag - minLag)], c = r[static_cast<size_t>(lag - minLag + 1)];
        if (b > a && b >= c && b >= 0.9 * top) { best = b; bestLag = lag; around = {a, b, c}; break; }
    }
    if (bestLag < 0 || best < 0.7) { trackedHz_.store(0.0f, std::memory_order_relaxed); return; }
    // Parabolic interpolation around the peak.
    const double den = around[0] - 2 * around[1] + around[2];
    const double shift = std::fabs(den) > 1e-12 ? 0.5 * (around[0] - around[2]) / den : 0.0;
    trackedHz_.store(static_cast<float>(rate / (bestLag + std::clamp(shift, -0.5, 0.5))), std::memory_order_relaxed);
}

void RoomInstrument::setClock(double beat, double bpm) noexcept {
    clockBeat_.store(beat, std::memory_order_relaxed);
    clockBpm_.store(bpm, std::memory_order_relaxed);
    clockSeq_.fetch_add(1, std::memory_order_release);
}

void RoomInstrument::setScale(uint16_t mask, int root) noexcept {
    if (root < 0 || root > 11 || !(mask & (1u << root))) {
        root = 9;
        for (int pc = 0; pc < 12; ++pc) if (mask & (1u << pc)) { root = pc; break; }
    }
    scale_.store(mask, std::memory_order_relaxed);
    root_.store(root, std::memory_order_relaxed);
}

int RoomInstrument::scaleStep(int note, int steps) const noexcept {
    const uint16_t mask = scale_.load(std::memory_order_relaxed);
    if (mask == 0) return note + steps * 2;
    int n = note;
    for (int k = 0; k < steps; ++k) do { ++n; } while (!(mask & (1u << floorMod(n, 12))));
    return n;
}

float RoomInstrument::ringAt(double back) const noexcept {
    const int n = static_cast<int>(ring_.size());
    double pos = ringWrite_ - 1 - back;
    while (pos < 0) pos += n;
    const int i0 = static_cast<int>(pos) % n, i1 = (i0 + 1) % n;
    const float f = static_cast<float>(pos - std::floor(pos));
    return ring_[static_cast<size_t>(i0)] * (1 - f) + ring_[static_cast<size_t>(i1)] * f;
}

// ---------------------------------------------------------------- the arrangement

void RoomInstrument::nextScene(long tick) noexcept {
    const int count = sceneCount_.load(std::memory_order_acquire);
    const long barStart = tick - floorMod(tick, kTicksPerBar);
    if (count <= 0) { scene_ = -1; sceneEnd_ = barStart + kTicksPerBar; started_ = true; return; }
    scene_ = started_ ? (scene_ + 1) % count : std::uniform_int_distribution<int>(0, count - 1)(rng_);
    started_ = true;
    const uint32_t packed = scenes_[static_cast<size_t>(scene_)].load(std::memory_order_relaxed);
    sceneEnd_ = barStart + static_cast<long>(packed >> 16) * kTicksPerBar;
    if (packed & (1u << kRipple)) rpCount_ = 0;   // the ripple builds up from its start
}

void RoomInstrument::onTick(long tick) noexcept {
    const long inBar = floorMod(tick, kTicksPerBar);
    // Which effects play.
    uint16_t mask = 0;
    const int s = solo.load(std::memory_order_relaxed);
    if (s >= 0) mask = s < kFxCount ? static_cast<uint16_t>(1u << s) : 0;
    else {
        if (!started_ || tick >= sceneEnd_ || tick < sceneEnd_ - 256L * kTicksPerBar) nextScene(tick);
        if (scene_ >= 0) mask = static_cast<uint16_t>(scenes_[static_cast<size_t>(scene_)].load(std::memory_order_relaxed) & 0xffff);
    }
    for (int f = 0; f < kFxCount; ++f) target_[static_cast<size_t>(f)] = (mask >> f & 1) ? 1.0f : 0.0f;
    activeOut_.store(mask, std::memory_order_relaxed);
    sceneOut_.store(s >= 0 ? -1 : scene_, std::memory_order_relaxed);

    // Each effect re-picks its settings on the bar (or every few bars) and glides.
    if (inBar == 0) {
        grainRate_ = 0.6 + 2.4 * rnd();
        glAt_[0] = static_cast<int>(rnd() * 16) % 16;
        glAt_[1] = 16 + static_cast<int>(rnd() * 16) % 16;
    }
    if (floorMod(tick, 2 * kTicksPerBar) == 0) {
        fdLow_ = !fdLow_;
        fdNext_ = fdLow_ ? -500.0 : 0.0;
        fdShiftL_.set(fdNext_, sr_ * 8.5);
        fdRightIn_ = static_cast<int>(sr_ * 3.0);
        ddCount_ = (ddCount_ + 1) % 16;
        if (ddCount_ == 0) ddFeedback_.set(0.89, sr_ * 32.0);
        if (ddCount_ == 8) ddFeedback_.set(0.4, sr_ * 4.0);
        resPan_.set(0.9 * rnd(), sr_ * 1.0);
    }
    for (int side = 0; side < 2; ++side)
        if (inBar == glAt_[side]) {
            glPitch_[side].set(std::floor(rnd() * 48.0) - 24.0, ms(500.0 + 1500.0 * rnd()));
            Repeater& rp = glRep_[side];
            rp.length = std::clamp(static_cast<int>(ms(std::max(40.0, 2000.0 * rnd()))), 1, static_cast<int>(rp.buf.size()) - 1);
            rp.pos = 0;
            rp.gateLeft = rp.length;
        }
    // Ripple: faster and wider as it goes on.
    if (floorMod(tick, 16) == 0) rpCount_ = (rpCount_ + 1) % 24;
    {
        const int stage = rpCount_ < 7 ? 0 : rpCount_ < 15 ? 1 : rpCount_ < 19 ? 2 : 3;
        static const int rates[4] = {8, 4, 2, 1};
        static const double heights[4] = {4, 6, 9, 12};
        static const double glides[4] = {1000, 500, 300, 100};
        if (floorMod(tick, rates[stage]) == 0) {
            rpSign_ = -rpSign_;
            rpPitch_[0].set(rpSign_ * heights[stage], ms(glides[stage]));
            rpRightTarget_ = rpSign_ * heights[stage];
            rpRightGlide_ = ms(glides[stage]);
            rpRightIn_ = static_cast<int>(ms(500));
        }
    }
    // Groove: a cut to another moment of the last 20 s, every few ticks.
    if (floorMod(tick, 4 * kTicksPerBar) == 0) { static const int modes[3] = {2, 4, 8}; grMode_ = modes[static_cast<int>(rnd() * 3) % 3]; }
    if (--grWait_ <= 0) {
        static const int sets[3][3] = {{1, 2, 4}, {2, 4, 6}, {6, 8, 12}};
        const int m = grMode_ == 2 ? 0 : grMode_ == 4 ? 1 : 2;
        grWait_ = sets[m][static_cast<int>(rnd() * 3) % 3];
        grEvent_ = 0;
        grGate_.set(0.0, ms(8));
        grPan_.set(rnd() * 1.6 - 0.8, ms(50));
    }
    // Tune: the room's pitch, in the scale, on the bar.
    if (inBar == 0 && heardNote_ >= 0) {
        int n1 = heardNote_;
        while (n1 > 55) n1 -= 12;
        while (n1 < 43) n1 += 12;
        int notes[3] = {n1, scaleStep(n1, 2), scaleStep(n1, 4)};
        for (int k = 1; k < 3; ++k) { while (notes[k] > 67) notes[k] -= 12; while (notes[k] < 43) notes[k] += 12; }
        for (int k = 0; k < 3; ++k) tuneHz_[static_cast<size_t>(k)].set(midiToHz(notes[k]), tuneOn_ ? ms(50) : 0);
        tuneOn_ = true;
    }
}

// ---------------------------------------------------------------- the effects

float RoomInstrument::fxStraight(float x, float& send) noexcept {
    send += 0.4f * x;
    return 0.6f * x;
}

void RoomInstrument::fxGrains(float& l, float& r, float& send) noexcept {
    if (--grainWait_ <= 0) {
        grainWait_ = std::max(1, static_cast<int>(sr_ / grainRate_));
        const int length = static_cast<int>(sr_ * 1.0);
        const double back = 1.0 + rnd() * 0.4 * sr_;
        if (recorded_ > static_cast<int64_t>(back))
            for (auto& g : grains_)
                if (!g.on) {
                    const int n = static_cast<int>(ring_.size());
                    double read = ringWrite_ - back;
                    while (read < 0) read += n;
                    const float vol = 1.0f - 0.13f * rnd();
                    const float p = (rnd() * 2.0f - 1.0f) * 0.79f;
                    g = Grain{true, std::floor(read), length, length, vol * panL(p), vol * panR(p)};
                    break;
                }
    }
    float gl = 0, gr = 0;
    for (auto& g : grains_) {
        if (!g.on) continue;
        const float phase = static_cast<float>(g.length - g.left) / static_cast<float>(g.length);
        const float v = ring_[static_cast<size_t>(g.read)] * (0.5f - 0.5f * std::cos(static_cast<float>(kTwoPi) * phase));
        gl += v * g.gl;
        gr += v * g.gr;
        if (++g.read >= ring_.size()) g.read -= ring_.size();
        if (--g.left <= 0) g.on = false;
    }
    l += 0.8f * gl;
    r += 0.8f * gr;
    send += 0.5f * 0.8f * (gl + gr);
}

void RoomInstrument::fxFreqDream(float x, float& l, float& r, float& send) noexcept {
    if (fdRightIn_ > 0 && --fdRightIn_ == 0) fdShiftR_.set(fdNext_, sr_ * 4.0);
    fdL_.setShiftHz(static_cast<float>(fdShiftL_.next()));
    fdR_.setShiftHz(static_cast<float>(fdShiftR_.next()));
    const float sl = fdL_.process(x), sr = fdR_.process(x);
    l += 0.5f * sl;
    r += 0.5f * sr;
    send += 0.25f * (sl + sr);
}

void RoomInstrument::fxDelayDream(float x, float& l, float& r, float& send) noexcept {
    const double beat = 60.0 / (bpm_ > 0 ? bpm_ : kIdleBpm) * sr_;
    // The room 2/3 of a beat late and a semitone up, into an echo one beat long.
    const int np = static_cast<int>(ddPre_.size());
    ddPre_[static_cast<size_t>(ddPreW_)] = 0.6f * x;
    const int dPre = std::clamp(static_cast<int>(beat * 2.0 / 3.0), 1, np - 1);
    const float pre = ddPre_[static_cast<size_t>((ddPreW_ - dPre + np) % np)];
    if (++ddPreW_ >= np) ddPreW_ = 0;
    ddShift_.setRatio(1.0594630943592953);
    const float shifted = ddShift_.process(pre);
    const int ne = static_cast<int>(ddEcho_.size());
    const int dEcho = std::clamp(static_cast<int>(beat), 1, ne - 1);
    const float y = ddEcho_[static_cast<size_t>((ddEchoW_ - dEcho + ne) % ne)];
    const float fb = static_cast<float>(ddFeedback_.next());
    float v = shifted + fb * ddLp_.lp(y, 0.33f);
    if (std::fabs(v) < 1e-15f) v = 0.0f;
    ddEcho_[static_cast<size_t>(ddEchoW_)] = v;
    if (++ddEchoW_ >= ne) ddEchoW_ = 0;
    l += 1.3f * y;
    r += 1.05f * y;
    send += 0.6f * y;
}

void RoomInstrument::fxResonator(float x, float& l, float& r, float& send) noexcept {
    const float in = 0.3f * x;
    float sum = 0;
    for (auto& c : combs_) {
        const int n = static_cast<int>(c.line.size());
        double pos = c.w - c.delay;
        while (pos < 0) pos += n;
        const int i0 = static_cast<int>(pos) % n, i1 = (i0 + 1) % n;
        const float f = static_cast<float>(pos - std::floor(pos));
        const float y = c.line[static_cast<size_t>(i0)] * (1 - f) + c.line[static_cast<size_t>(i1)] * f;
        c.lp += 0.8f * (y - c.lp);
        float v = in + 0.99f * c.lp;
        if (std::fabs(v) < 1e-15f) v = 0.0f;
        c.line[static_cast<size_t>(c.w)] = v;
        if (++c.w >= n) c.w = 0;
        sum += y;
    }
    // Eight gentle low-passes, darker as the room gets louder, then the low rumble out.
    float m = 0.22f * sum;
    const float a = static_cast<float>(1.0 - std::exp(-kTwoPi * resCutoff_ / sr_));
    for (auto& p : resLp_) m = p.lp(m, a);
    const float hpA = static_cast<float>(std::exp(-kTwoPi * 80.0 / sr_));
    m = hp(hp(m, resHp1_, resHp1Prev_, hpA), resHp2_, resHp2Prev_, hpA);
    m = std::tanh(m);
    // A short ringing body and a long echo.
    const int na = static_cast<int>(resEchoA_.size()), nb = static_cast<int>(resEchoB_.size());
    const int dA = std::min(na - 1, static_cast<int>(ms(119)));
    const int dB = std::clamp(static_cast<int>(60.0 / (bpm_ > 0 ? bpm_ : kIdleBpm) * sr_ * 2.0 / 3.0), 1, nb - 1);
    const int wA = resEchoW_ % na, wB = resEchoW_ % nb;
    const float ea = resEchoA_[static_cast<size_t>((wA - dA + na) % na)];
    const float eb = resEchoB_[static_cast<size_t>((wB - dB + nb) % nb)];
    float va = m + 0.8f * resEchoLp_.lp(ea, static_cast<float>(1.0 - std::exp(-kTwoPi * 500.0 / sr_)));
    if (std::fabs(va) < 1e-15f) va = 0.0f;
    resEchoA_[static_cast<size_t>(wA)] = va;
    resEchoB_[static_cast<size_t>(wB)] = m;
    if (++resEchoW_ >= na * nb) resEchoW_ = 0;
    const float out = m + 0.3f * ea + 0.6f * eb;
    const float p = static_cast<float>(resPan_.next());
    l += out * (0.4f + 0.6f * p);
    r += out * (0.4f + 0.6f * (1.0f - p));
    send += 0.6f * out;
}

float RoomInstrument::repeat(Repeater& rp, float x) noexcept {
    // Catch `length` of the input (10 ms in and out), then repeat it, fading by 0.8 each time.
    const float target = rp.gateLeft > 0 ? 1.0f : 0.0f;
    const float step = static_cast<float>(1.0 / ms(10));
    rp.gate = target > rp.gate ? std::min(target, rp.gate + step) : std::max(target, rp.gate - step);
    if (rp.gateLeft > 0) --rp.gateLeft;
    const float y = rp.buf[static_cast<size_t>(rp.pos)];
    float v = rp.gate * x + (1.0f - rp.gate) * 0.8f * y;
    if (std::fabs(v) < 1e-15f) v = 0.0f;
    rp.buf[static_cast<size_t>(rp.pos)] = v;
    if (++rp.pos >= rp.length) rp.pos = 0;
    return y;
}

void RoomInstrument::fxGlitch(float x, float& l, float& r, float& send) noexcept {
    const float a = static_cast<float>(1.0 - std::exp(-kTwoPi * 3000.0 / sr_));
    for (int s = 0; s < 2; ++s) {
        glShift_[s].setRatio(std::exp2(glPitch_[s].next() / 12.0));
        const float out = glLp_[s].lp(repeat(glRep_[s], glShift_[s].process(x)), a);
        (s == 0 ? l : r) += out;
        send += 0.5f * out;
    }
}

void RoomInstrument::fxSpiral(float x, float& l, float& r, float& send) noexcept {
    // Two short echoes that are frequency-shifted down on every pass.
    const int n = static_cast<int>(spLine_[0].size());
    const float lpA = static_cast<float>(1.0 - std::exp(-kTwoPi * 3000.0 / sr_));
    const double delays[2] = {ms(100), ms(200)};
    const double hpHz[2] = {600, 800};
    for (int s = 0; s < 2; ++s) {
        const int d = std::clamp(static_cast<int>(delays[s]), 1, n - 1);
        const float y = spLine_[s][static_cast<size_t>((spW_ - d + n) % n)];
        const float f = hp(spLp_[s].lp(0.75f * y, lpA), spHp_[s], spHpPrev_[s], static_cast<float>(std::exp(-kTwoPi * hpHz[s] / sr_)));
        float v = spShift_[s].process(0.7f * x + f);
        if (std::fabs(v) < 1e-15f) v = 0.0f;
        spLine_[s][static_cast<size_t>(spW_)] = v;
        (s == 0 ? l : r) += 1.6f * f;
        send += 0.4f * f;
    }
    if (++spW_ >= n) spW_ = 0;
}

void RoomInstrument::fxRipple(float x, float& l, float& r, float& send) noexcept {
    if (rpRightIn_ > 0 && --rpRightIn_ == 0) rpPitch_[1].set(rpRightTarget_, rpRightGlide_);
    for (int s = 0; s < 2; ++s) {
        rpShift_[s].setRatio(std::exp2(rpPitch_[s].next() / 12.0));
        const float out = 0.7f * rpShift_[s].process(x);
        (s == 0 ? l : r) += out;
        send += 0.4f * out;
    }
}

void RoomInstrument::fxGroove(float x, float& l, float& r, float& send) noexcept {
    if (grEvent_ >= 0) {
        ++grEvent_;
        if (grEvent_ == static_cast<int>(ms(21))) {   // while the gate is shut: cut to another moment
            const double avail = std::min<double>(static_cast<double>(recorded_), sr_ * 20.0);
            grBack_ = avail > sr_ * 0.1 ? sr_ * 0.05 + rnd() * (avail - sr_ * 0.05) : sr_ * 0.05;
        }
        if (grEvent_ >= static_cast<int>(ms(30))) { grGate_.set(1.0, ms(30)); grEvent_ = -1; }
    }
    const float hpA = static_cast<float>(std::exp(-kTwoPi * 200.0 / sr_));
    const float y = hp(ringAt(grBack_), grHp_, grHpPrev_, hpA) * static_cast<float>(grGate_.next());
    const float p = static_cast<float>(grPan_.next());
    const int n = static_cast<int>(grStereo_.size());
    const int d = std::min(n - 1, static_cast<int>(ms(6)));
    const float right = grStereo_[static_cast<size_t>((grStereoW_ - d + n) % n)];
    grStereo_[static_cast<size_t>(grStereoW_)] = y * panR(p);
    if (++grStereoW_ >= n) grStereoW_ = 0;
    // The room's low rumble underneath.
    const float rumble = 0.3f * grRumbleLp_.lp(hp(3.0f * x, grRumbleHp_, grRumbleHpPrev_, static_cast<float>(std::exp(-kTwoPi * 80.0 / sr_))),
                                               static_cast<float>(1.0 - std::exp(-kTwoPi * 800.0 / sr_)));
    l += y * panL(p) + rumble;
    r += right + rumble;
    send += 0.3f * y;
}

void RoomInstrument::fxTune(float& l, float& r, float& send) noexcept {
    if (!tuneOn_) return;
    vibrato_ += kTwoPi * 4.0 / sr_;
    if (vibrato_ > kTwoPi) vibrato_ -= kTwoPi;
    static const float levels[3] = {0.3f, 0.15f, 0.2f};
    const float a = static_cast<float>(1.0 - std::exp(-kTwoPi * 800.0 / sr_));
    float m = 0;
    for (size_t k = 0; k < 3; ++k) {
        const double hz = tuneHz_[k].next() + 4.0 * std::sin(vibrato_);
        tuneSaw_[k] += hz / sr_;
        if (tuneSaw_[k] >= 1.0) tuneSaw_[k] -= 1.0;
        tunePhase_[k] += kTwoPi * hz / sr_;
        if (tunePhase_[k] > kTwoPi) tunePhase_[k] -= kTwoPi;
        const float saw = tuneLp_[k].lp(static_cast<float>(2.0 * tuneSaw_[k] - 1.0), a);
        m += levels[k] * (0.5f * saw + 0.5f * static_cast<float>(std::sin(tunePhase_[k])));
    }
    l += 0.35f * m;
    r += 0.35f * m;
    send += 0.2f * m;
}

// ---------------------------------------------------------------- the loop

void RoomInstrument::processAdd(const float* mono, float* outL, float* outR, int numFrames) noexcept {
    if (const uint32_t seq = clockSeq_.load(std::memory_order_acquire); seq != seenSeq_) {
        seenSeq_ = seq;
        const double b = clockBeat_.load(std::memory_order_relaxed);
        const double bpm = clockBpm_.load(std::memory_order_relaxed);
        const bool fresh = bpm_ <= 0;
        if (bpm > 0 && (fresh || std::fabs(b - beat_) > 0.25)) beat_ = b;   // a song started, or a jump
        else if (bpm > 0) beat_ += 0.1 * (b - beat_);                       // a gentle correction
        bpm_ = bpm;                                                          // 0: run on at the idle tempo
    }
    // The resonators' chord: the song's root, fifth and ninth, from octave 3.
    const uint32_t key = scale_.load(std::memory_order_relaxed) | (static_cast<uint32_t>(root_.load(std::memory_order_relaxed)) << 16) | (1u << 31);
    if (key != combKey_) {
        combKey_ = key;
        const int base = 48 + root_.load(std::memory_order_relaxed);
        const int notes[3] = {base, base + 7, base + 14};
        for (size_t c = 0; c < 3; ++c) {
            // The loop's low-pass delays a little; the loop is shortened to match.
            combs_[c].delay = std::clamp(sr_ / midiToHz(notes[c]) - 0.25, 2.0, static_cast<double>(combs_[c].line.size()) - 3.0);
            combNotes_[c].store(notes[c], std::memory_order_relaxed);
        }
    }
    // The room's level (for the resonator's darkness).
    {
        double e = 0;
        for (int i = 0; i < numFrames; ++i) e += double(mono[i]) * mono[i];
        const float db = 10.0f * std::log10(static_cast<float>(e / std::max(1, numFrames)) + 1e-12f);
        const float k = static_cast<float>(1.0 - std::exp(-numFrames / (0.42 * sr_)));
        resLevelDb_ += k * (db - resLevelDb_);
        const float hz = 5500.0f + (380.0f - 5500.0f) * std::clamp((resLevelDb_ + 40.0f) / 18.0f, 0.0f, 1.0f);
        resCutoff_ = std::clamp(hz, 80.0f, 3000.0f);
    }
    const double perFrame = (bpm_ > 0 ? bpm_ : kIdleBpm) / 60.0 / sr_;
    const float fadeK = static_cast<float>(1.0 - std::exp(-1.0 / (kFadeSeconds * sr_)));
    const float lv = std::clamp(level.load(std::memory_order_relaxed), 0.0f, 2.0f);
    const float dryLevel = std::clamp(dry.load(std::memory_order_relaxed), 0.0f, 1.0f);
    const float hp200A = static_cast<float>(std::exp(-kTwoPi * 200.0 / sr_));
    const float hallA = static_cast<float>(1.0 - std::exp(-kTwoPi * 2000.0 / sr_));
    const bool tuneWanted = target_[kTune] > 0 || gain_[kTune] > 1e-4f;
    for (int i = 0; i < numFrames; ++i) {
        const float x = mono[i];
        ring_[static_cast<size_t>(ringWrite_)] = x;
        if (++ringWrite_ >= static_cast<int>(ring_.size())) ringWrite_ = 0;
        ++recorded_;
        if (tuneWanted) {   // the room's pitch, for the tune
            decimAcc_ += x;
            if (++decim_ == kDecimate) {
                pitchBuf_[static_cast<size_t>(pitchWrite_)] = decimAcc_ / kDecimate;
                pitchWrite_ = (pitchWrite_ + 1) % kPitchWindow;
                decim_ = 0; decimAcc_ = 0.0f;
                if (++sinceAnalysis_ >= kPitchHop) {
                    sinceAnalysis_ = 0;
                    trackPitch();
                    const float hz = trackedHz_.load(std::memory_order_relaxed);
                    if (hz > 0) heardNote_ = nearestScaleNote(69.0 + 12.0 * std::log2(hz / 440.0));
                }
            }
        }
        if (const long tick = static_cast<long>(std::floor(beat_ * kTicksPerBeat)); tick != lastTick_ || !ticked_) {
            if (ticked_ && tick < lastTick_ - 1) sceneEnd_ = tick;   // the song jumped back: start the scene timing again
            ticked_ = true;
            lastTick_ = tick;
            onTick(tick);
        }
        const float xh = hp(x, hp200_, hp200Prev_, hp200A);
        float l = dryLevel * xh, r = dryLevel * xh, send = 0.3f * dryLevel * xh;
        for (size_t f = 0; f < kFxCount; ++f) gain_[f] += fadeK * (target_[f] - gain_[f]);
        auto on = [&](Fx f) { return gain_[static_cast<size_t>(f)] > 1e-4f; };
        float fl, fr, fs;
        auto add = [&](Fx f) { const float g = gain_[static_cast<size_t>(f)]; l += g * fl; r += g * fr; send += g * fs; };
        if (on(kStraight)) { fl = fr = fs = 0; const float y = fxStraight(x, fs); fl = fr = y; add(kStraight); }
        if (on(kGrains)) { fl = fr = fs = 0; fxGrains(fl, fr, fs); add(kGrains); }
        if (on(kFreqDream)) { fl = fr = fs = 0; fxFreqDream(x, fl, fr, fs); add(kFreqDream); }
        if (on(kDelayDream)) { fl = fr = fs = 0; fxDelayDream(x, fl, fr, fs); add(kDelayDream); }
        if (on(kResonator)) { fl = fr = fs = 0; fxResonator(x, fl, fr, fs); add(kResonator); }
        if (on(kGlitch)) { fl = fr = fs = 0; fxGlitch(xh, fl, fr, fs); add(kGlitch); }
        if (on(kSpiral)) { fl = fr = fs = 0; fxSpiral(xh, fl, fr, fs); add(kSpiral); }
        if (on(kRipple)) { fl = fr = fs = 0; fxRipple(x, fl, fr, fs); add(kRipple); }
        if (on(kGroove)) { fl = fr = fs = 0; fxGroove(x, fl, fr, fs); add(kGroove); }
        if (on(kTune)) { fl = fr = fs = 0; fxTune(fl, fr, fs); add(kTune); }
        // The hall: a large, dark room that every effect sends into.
        float hl = 0, hr = 0;
        hall_.process(hallIn_.lp(send, hallA), 0.92f, 0.5f, hl, hr);
        outL[i] += lv * (l + 2.5f * hl);
        outR[i] += lv * (r + 2.5f * hr);
        beat_ += perFrame;
        ++frame_;
    }
    memoryOut_.store(lv * gain_[kMemory], std::memory_order_relaxed);
}

} // namespace atmos
