#include "engine/environment/RoomInstrument.h"

#include <algorithm>
#include <cmath>

namespace atmos {
namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr int kDecimate = 4;
constexpr int kPitchWindow = 1024;     // at sr / 4: ~85 ms at 48 kHz
constexpr int kPitchHop = 256;         // ~21 ms
constexpr double kCaptureSeconds = 0.25;   // the pitch echo's piece of the room
constexpr double kRingSeconds = 2.5;       // resonators: time to fall 60 dB
constexpr float kDamp = 0.6f;              // resonators: loop low-pass (one pole)
constexpr double kDampDelay = (1.0 - kDamp) / kDamp;   // its delay in samples, at low notes

double midiToHz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }

} // namespace

void RoomInstrument::prepare(double sampleRate) {
    sr_ = sampleRate;
    pitchBuf_.assign(kPitchWindow, 0.0f);
    ring_.assign(static_cast<size_t>(sampleRate * 4.0), 0.0f);
    for (auto& c : combs_) c.line.assign(static_cast<size_t>(sampleRate / 30.0) + 4, 0.0f);   // down to ~30 Hz
    echo_.assign(static_cast<size_t>(sampleRate * 2.0), 0.0f);
    captured_.assign(static_cast<size_t>(sampleRate * kCaptureSeconds) + 2, 0.0f);
    shifter_.prepare(sampleRate);
    hpA_ = static_cast<float>(std::exp(-kTwoPi * 80.0 / sampleRate));
    inLpA_ = static_cast<float>(1.0 - std::exp(-kTwoPi * 3000.0 / sampleRate));
    grainLpA_ = static_cast<float>(1.0 - std::exp(-kTwoPi * 4000.0 / sampleRate));
    echoLpA_ = static_cast<float>(1.0 - std::exp(-kTwoPi * 3000.0 / sampleRate));
    glideK_ = 1.0 - std::exp(-1.0 / (0.08 * sampleRate));   // 80 ms
    reset();
}

void RoomInstrument::reset() noexcept {
    std::fill(pitchBuf_.begin(), pitchBuf_.end(), 0.0f);
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    std::fill(echo_.begin(), echo_.end(), 0.0f);
    std::fill(captured_.begin(), captured_.end(), 0.0f);
    for (auto& c : combs_) { std::fill(c.line.begin(), c.line.end(), 0.0f); c.lp = 0; }
    shifter_.reset();
    pitchWrite_ = decim_ = sinceAnalysis_ = 0;
    decimAcc_ = 0.0f;
    trackedHz_.store(0.0f, std::memory_order_relaxed);
    candidateNote_ = -1; candidateHold_ = 0; heardRecently_ = 1 << 30;
    lastEchoedNote_ = -1; sinceEcho_ = 1 << 30;
    tuneNoteOut_.store(-1, std::memory_order_relaxed);
    tuneAmp_ = 0.0f;
    ringWrite_ = 0;
    echoWrite_ = 0; echoLp_ = 0.0f;
    hp_ = hpPrev_ = inLp_ = 0.0f; resEnv_ = 0.0f;
    grainLp_[0] = grainLp_[1] = 0.0f;
    nextGrain_ = 0;
    for (auto& g : grains_) g.on = false;
    for (auto& g : slices_) g.on = false;
    for (auto& rp : repeats_) rp.on = false;
    for (auto& pb : playbacks_) pb.on = false;
    capturedLen_ = 0;
    last16th_ = -1;
    lastChordBar_ = -3;   // choose the chord again on the next block, without a glide
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

std::array<int, 4> RoomInstrument::resonatorNotes() const noexcept {
    return {combNotes_[0].load(std::memory_order_relaxed), combNotes_[1].load(std::memory_order_relaxed),
            combNotes_[2].load(std::memory_order_relaxed), combNotes_[3].load(std::memory_order_relaxed)};
}

int RoomInstrument::rootForKey(const std::string& k) {
    static const int letters[] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
    if (k.empty() || k[0] < 'A' || k[0] > 'G') return 9;
    int root = letters[k[0] - 'A'];
    if (k.compare(1, 1, "#") == 0 || k.compare(1, 3, "\xE2\x99\xAF") == 0) root += 1;
    else if (k.compare(1, 1, "b") == 0 || k.compare(1, 3, "\xE2\x99\xAD") == 0) root -= 1;
    return (root + 12) % 12;
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

int RoomInstrument::nearestScaleNote(double m) const noexcept {
    const uint16_t mask = scale_.load(std::memory_order_relaxed);
    int best = -1; double bestD = 1e9;
    for (int n = static_cast<int>(std::floor(m)) - 6; n <= static_cast<int>(std::ceil(m)) + 6; ++n)
        if (mask & (1u << (((n % 12) + 12) % 12)))
            if (const double d = std::fabs(n - m); d < bestD) { bestD = d; best = n; }
    return best;
}

int RoomInstrument::quantize(float hz) const noexcept {
    if (hz <= 0) return -1;
    double m = 69.0 + 12.0 * std::log2(hz / 440.0);
    while (m < 52) m += 12;   // sing it back between E3 and E5, whatever the source
    while (m > 76) m -= 12;
    return nearestScaleNote(m);
}

// Normalised autocorrelation over the last window, 80-800 Hz.
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

float RoomInstrument::renderTune() noexcept {
    const float level = tuneLevel.load(std::memory_order_relaxed);
    const bool singing = heardRecently_ < static_cast<int>(sr_ * 0.35) && level > 0;
    const float target = singing ? level : 0.0f;
    const float k = static_cast<float>(1.0 - std::exp(-1.0 / ((singing ? 0.25 : 1.2) * sr_)));   // soft attack, long release
    tuneAmp_ += k * (target - tuneAmp_);
    tuneHz_ += (1.0 - std::exp(-1.0 / (0.06 * sr_))) * (tuneTargetHz_ - tuneHz_);           // 60 ms glide
    if (tuneAmp_ < 1e-5f) return 0.0f;
    vibrato_ += kTwoPi * 5.0 / sr_;
    const double hz = tuneHz_ * (1.0 + 0.003 * std::sin(vibrato_));
    phaseA_ += kTwoPi * hz / sr_;
    phaseB_ += kTwoPi * hz * 1.004 / sr_;   // a second, slightly sharp voice for width
    if (phaseA_ > kTwoPi) phaseA_ -= kTwoPi;
    if (phaseB_ > kTwoPi) phaseB_ -= kTwoPi;
    auto tone = [](double p) { return std::sin(p) + 0.3 * std::sin(2 * p) + 0.12 * std::sin(3 * p); };
    return static_cast<float>(0.25 * tuneAmp_ * (tone(phaseA_) + tone(phaseB_)));
}

float RoomInstrument::ringAt(double back) const noexcept {
    const int n = static_cast<int>(ring_.size());
    double pos = ringWrite_ - 1 - back;
    while (pos < 0) pos += n;
    const int i0 = static_cast<int>(pos) % n, i1 = (i0 + 1) % n;
    const float f = static_cast<float>(pos - std::floor(pos));
    return ring_[static_cast<size_t>(i0)] * (1 - f) + ring_[static_cast<size_t>(i1)] * f;
}

// Groove: slices of the last bars of the room on the song's 16th notes.
void RoomInstrument::renderGroove(float& l, float& r) noexcept {
    const float level = grooveLevel.load(std::memory_order_relaxed);
    if (bpm_ <= 0 || level <= 0) return;
    const long sixteenth = static_cast<long>(std::floor(beat_ * 4.0));
    if (sixteenth != last16th_) {
        last16th_ = sixteenth;
        const double p = 0.2 + 0.5 * std::clamp(energy.load(std::memory_order_relaxed), 0.0f, 1.0f);
        if (std::uniform_real_distribution<double>(0, 1)(rng_) < p) {
            const int sixteenthFrames = std::max(1, static_cast<int>(sr_ * 60.0 / bpm_ / 4.0));
            const int back = std::uniform_int_distribution<int>(1, 16)(rng_) * sixteenthFrames;
            if (back < static_cast<int>(ring_.size()) - 1)
                for (auto& g : slices_)
                    if (!g.on) {
                        const float pan = std::uniform_real_distribution<float>(-0.7f, 0.7f)(rng_);
                        g = Grain{true, static_cast<double>((ringWrite_ - back + static_cast<int>(ring_.size())) % static_cast<int>(ring_.size())),
                                  static_cast<int>(sixteenthFrames * 0.9), static_cast<int>(sixteenthFrames * 0.9),
                                  std::sqrt(0.5f * (1 - pan)), std::sqrt(0.5f * (1 + pan))};
                        break;
                    }
        }
    }
    for (auto& g : slices_) {
        if (!g.on) continue;
        const int done = g.length - g.left;
        const float env = std::min(1.0f, done / static_cast<float>(sr_ * 0.003)) * std::exp(-3.0f * done / static_cast<float>(g.length));
        const float x = ring_[static_cast<size_t>(g.read)] * env * level;
        l += x * g.panL;
        r += x * g.panR;
        g.read += 1;
        if (g.read >= ring_.size()) g.read -= ring_.size();
        if (--g.left <= 0) g.on = false;
    }
}

// Resonance: four notes of the song's scale, re-chosen every two bars. A slow walk that
// mostly rests on the home chord: degrees 0, 0, 3, 2 of the scale, as triads in scale steps,
// with the root an octave below.
void RoomInstrument::chooseChord(long index) noexcept {
    const uint16_t mask = scale_.load(std::memory_order_relaxed);
    const int root = root_.load(std::memory_order_relaxed);
    std::array<int, 12> steps{};
    int count = 0;
    for (int s = 0; s < 12; ++s) if (mask & (1u << ((root + s) % 12))) steps[static_cast<size_t>(count++)] = s;
    if (count == 0) return;
    auto degree = [&](int d) {   // any scale degree, as MIDI, from the root in octave 3
        const int oct = d >= 0 ? d / count : -((-d + count - 1) / count);
        return 48 + root + 12 * oct + steps[static_cast<size_t>(d - oct * count)];
    };
    static const int walk[] = {0, 0, 3, 2};
    const int base = index < 0 ? 0 : walk[index % 4];
    const int notes[4] = {degree(base - count), degree(base), degree(base + 2), degree(base + 4)};
    static const float pans[4] = {0.0f, -0.6f, 0.6f, -0.2f};
    for (size_t c = 0; c < 4; ++c) {
        auto& comb = combs_[c];
        const double hz = midiToHz(notes[c]);
        comb.target = std::clamp(sr_ / hz - kDampDelay, 2.0, static_cast<double>(comb.line.size()) - 3.0);
        if (lastChordBar_ == -3) comb.delay = comb.target;   // first chord: no glide
        comb.g = static_cast<float>(std::pow(10.0, -3.0 * (comb.target + kDampDelay) / (sr_ * kRingSeconds)));
        comb.panL = std::sqrt(0.5f * (1 - pans[c]));
        comb.panR = std::sqrt(0.5f * (1 + pans[c]));
        combNotes_[c].store(notes[c], std::memory_order_relaxed);
    }
    lastChordBar_ = index;
}

void RoomInstrument::renderResonance(float x, float& l, float& r) noexcept {
    // The room's band that rings well: high-passed at 80 Hz, low-passed at 3 kHz.
    hp_ = hpA_ * (hp_ + x - hpPrev_);
    hpPrev_ = x;
    inLp_ += inLpA_ * (hp_ - inLp_);
    const float level = resonanceLevel.load(std::memory_order_relaxed);
    if (level <= 0) return;
    const float in = 0.5f * inLp_;
    float sl = 0, sr = 0;
    for (auto& c : combs_) {
        const int n = static_cast<int>(c.line.size());
        c.delay += glideK_ * (c.target - c.delay);
        double pos = c.write - c.delay;
        while (pos < 0) pos += n;
        const int i0 = static_cast<int>(pos) % n, i1 = (i0 + 1) % n;
        const float f = static_cast<float>(pos - std::floor(pos));
        const float y = c.line[static_cast<size_t>(i0)] * (1 - f) + c.line[static_cast<size_t>(i1)] * f;
        c.lp += kDamp * (y - c.lp);   // the ring darkens as it fades, like a string
        float v = in + c.g * c.lp;
        if (std::fabs(v) < 1e-15f) v = 0.0f;
        c.line[static_cast<size_t>(c.write)] = v;
        if (++c.write >= n) c.write = 0;
        sl += y * c.panL;
        sr += y * c.panR;
    }
    // A tap rings out fully; a long note right on a resonator is held down to the same level.
    const float peak = std::max(std::fabs(sl), std::fabs(sr));
    resEnv_ += (peak > resEnv_ ? 0.01f : 0.00005f) * (peak - resEnv_);   // ~2 ms up, ~400 ms down
    const float gain = level * 0.9f * std::min(1.0f, 0.35f / (resEnv_ + 1e-9f));
    l += 0.5f * std::tanh(2.0f * gain * sl);
    r += 0.5f * std::tanh(2.0f * gain * sr);
}

// Grains: 60-200 ms of the last second of the room, at random places, ~12 a second.
void RoomInstrument::renderGrains(float& l, float& r) noexcept {
    const float level = grainLevel.load(std::memory_order_relaxed);
    if (level <= 0) return;
    if (--nextGrain_ <= 0) {
        nextGrain_ = static_cast<int>(sr_ / 12.0 * std::uniform_real_distribution<double>(0.5, 1.5)(rng_));
        for (auto& g : grains_)
            if (!g.on) {
                const int length = static_cast<int>(sr_ * std::uniform_real_distribution<double>(0.06, 0.2)(rng_));
                const double back = sr_ * std::uniform_real_distribution<double>(0.2, 1.0)(rng_) + length;
                const int n = static_cast<int>(ring_.size());
                double read = ringWrite_ - back;
                while (read < 0) read += n;
                const float pan = std::uniform_real_distribution<float>(-0.8f, 0.8f)(rng_);
                g = Grain{true, std::floor(read), length, length, std::sqrt(0.5f * (1 - pan)), std::sqrt(0.5f * (1 + pan))};
                break;
            }
    }
    float gl = 0, gr = 0;
    for (auto& g : grains_) {
        if (!g.on) continue;
        const float phase = static_cast<float>(g.length - g.left) / static_cast<float>(g.length);
        const float x = ring_[static_cast<size_t>(g.read)] * (0.5f - 0.5f * std::cos(static_cast<float>(kTwoPi) * phase));
        gl += x * g.panL;
        gr += x * g.panR;
        g.read += 1;
        if (g.read >= ring_.size()) g.read -= ring_.size();
        if (--g.left <= 0) g.on = false;
    }
    grainLp_[0] += grainLpA_ * (gl - grainLp_[0]);
    grainLp_[1] += grainLpA_ * (gr - grainLp_[1]);
    l += 1.5f * level * grainLp_[0];
    r += 1.5f * level * grainLp_[1];
}

// Shifted echo: the room, pitch-shifted, repeating every 3/4 of a beat (0.5 s with no song).
void RoomInstrument::renderShiftEcho(float x, float& l, float& r) noexcept {
    const float level = shiftEchoLevel.load(std::memory_order_relaxed);
    if (level <= 0) return;
    const int n = static_cast<int>(echo_.size());
    const double target = std::clamp(bpm_ > 0 ? 0.75 * 60.0 / bpm_ * sr_ : 0.5 * sr_, 0.05 * sr_, n - 3.0);
    echoDelay_ += glideK_ * (target - echoDelay_);
    double pos = echoWrite_ - echoDelay_;
    while (pos < 0) pos += n;
    const int i0 = static_cast<int>(pos) % n, i1 = (i0 + 1) % n;
    const float f = static_cast<float>(pos - std::floor(pos));
    const float y = echo_[static_cast<size_t>(i0)] * (1 - f) + echo_[static_cast<size_t>(i1)] * f;
    echoLp_ += echoLpA_ * (y - echoLp_);   // each repeat a little darker
    float v = shifter_.process(x) + 0.45f * echoLp_;
    if (std::fabs(v) < 1e-15f) v = 0.0f;
    echo_[static_cast<size_t>(echoWrite_)] = v;
    if (++echoWrite_ >= n) echoWrite_ = 0;
    l += 1.4f * level * y;          // the echo itself on the left,
    r += 1.4f * level * echoLp_;    // its darker shadow on the right
}

// Pitch echo: the room's own held note, retuned into the scale, on the next 16ths.
void RoomInstrument::startPitchEcho(double detectedHz, int note) noexcept {
    if (pitchEchoLevel.load(std::memory_order_relaxed) <= 0 || detectedHz <= 0 || note < 0) return;
    for (const auto& p : playbacks_) if (p.on) return;   // still sounding: let it finish
    for (const auto& rp : repeats_) if (rp.on) return;
    capturedLen_ = static_cast<int>(captured_.size()) - 2;
    for (int i = 0; i < capturedLen_; ++i) captured_[static_cast<size_t>(i)] = ringAt(capturedLen_ - 1 - i);
    captureRate_ = std::clamp(midiToHz(note) / detectedHz, 0.5, 2.0);
    static const int offsets[3] = {1, 3, 6};             // 16ths ahead
    static const float gains[3] = {1.0f, 0.6f, 0.35f};
    static const float pans[3] = {-0.5f, 0.5f, -0.2f};
    const double perFrame = bpm_ / 60.0 / sr_;
    const long step = static_cast<long>(std::floor(beat_ * 4.0));
    for (size_t k = 0; k < 3; ++k) {
        const double wait = bpm_ > 0 ? ((step + offsets[k]) / 4.0 - beat_) / perFrame
                                     : offsets[k] * 0.125 * sr_;   // with no song: as at 120 bpm
        repeats_[k] = Repeat{frame_ + static_cast<int64_t>(std::max(0.0, wait)), gains[k], pans[k], true};
    }
    pitchEchoCount_.fetch_add(1, std::memory_order_relaxed);
}

void RoomInstrument::renderPitchEcho(float& l, float& r) noexcept {
    for (auto& rp : repeats_) {
        if (!rp.on || frame_ < rp.at) continue;
        rp.on = false;
        for (auto& p : playbacks_)
            if (!p.on) { p = Playback{true, 0.0, rp.gain, std::sqrt(0.5f * (1 - rp.pan)), std::sqrt(0.5f * (1 + rp.pan))}; break; }
    }
    const float level = pitchEchoLevel.load(std::memory_order_relaxed);
    const double last = capturedLen_ - 1;
    for (auto& p : playbacks_) {
        if (!p.on) continue;
        if (p.pos >= last) { p.on = false; continue; }
        const int i0 = static_cast<int>(p.pos);
        const float f = static_cast<float>(p.pos - i0);
        const float s = captured_[static_cast<size_t>(i0)] * (1 - f) + captured_[static_cast<size_t>(i0 + 1)] * f;
        const float env = 0.5f - 0.5f * std::cos(static_cast<float>(kTwoPi * p.pos / last));
        const float y = 1.6f * level * p.gain * env * s;
        l += y * p.panL;
        r += y * p.panR;
        p.pos += captureRate_;
    }
}

void RoomInstrument::processAdd(const float* mono, float* outL, float* outR, int numFrames) noexcept {
    // The beat clock: take the session's latest position, then run on at the tempo.
    if (const uint32_t seq = clockSeq_.load(std::memory_order_acquire); seq != seenSeq_) {
        seenSeq_ = seq;
        const double b = clockBeat_.load(std::memory_order_relaxed);
        const bool fresh = bpm_ <= 0;   // the clock was stopped: take the position as it is
        bpm_ = clockBpm_.load(std::memory_order_relaxed);
        if (fresh || std::fabs(b - beat_) > 0.25 || bpm_ <= 0) beat_ = b;   // a jump (seek, new song)
        else beat_ += 0.1 * (b - beat_);                            // otherwise a gentle correction
    }
    // The resonators' chord: every two bars, or whenever the key changes.
    const uint32_t key = scale_.load(std::memory_order_relaxed) | (static_cast<uint32_t>(root_.load(std::memory_order_relaxed)) << 16);
    const long chordIndex = bpm_ > 0 ? static_cast<long>(std::floor(beat_ / 8.0)) : -1;
    if (chordIndex != lastChordBar_ || key != chordKey_) { chordKey_ = key; chooseChord(chordIndex); }
    shifter_.setRatio(std::pow(2.0, std::clamp(shiftEchoSemitones.load(std::memory_order_relaxed), -24.0f, 24.0f) / 12.0));

    const double beatPerFrame = bpm_ / 60.0 / sr_;
    const int confirm = static_cast<int>(sr_ * 0.12);
    for (int i = 0; i < numFrames; ++i) {
        const float x = mono[i];
        ring_[static_cast<size_t>(ringWrite_)] = x;
        if (++ringWrite_ >= static_cast<int>(ring_.size())) ringWrite_ = 0;
        // Pitch: decimate (box average), analyse every hop.
        decimAcc_ += x;
        if (++decim_ == kDecimate) {
            pitchBuf_[static_cast<size_t>(pitchWrite_)] = decimAcc_ / kDecimate;
            pitchWrite_ = (pitchWrite_ + 1) % kPitchWindow;
            decim_ = 0; decimAcc_ = 0.0f;
            if (++sinceAnalysis_ >= kPitchHop) {
                sinceAnalysis_ = 0;
                trackPitch();
                const float hz = trackedHz_.load(std::memory_order_relaxed);
                const int note = quantize(hz);
                if (note >= 0) {
                    heardRecently_ = 0;
                    if (note == candidateNote_) candidateHold_ += kPitchHop;
                    else { candidateNote_ = note; candidateHold_ = 0; }
                    // A note held ~120 ms is a real note: the tune sings it, the pitch echo repeats it.
                    const bool held = candidateHold_ * kDecimate >= confirm;
                    if (held || tuneNoteOut_.load(std::memory_order_relaxed) < 0) {
                        tuneTargetHz_ = midiToHz(note);
                        if (tuneNoteOut_.load(std::memory_order_relaxed) < 0) tuneHz_ = tuneTargetHz_;
                        tuneNoteOut_.store(note, std::memory_order_relaxed);
                    }
                    if (held && sinceEcho_ > static_cast<int>(sr_ * 0.4) &&
                        (note != lastEchoedNote_ || sinceEcho_ > static_cast<int>(sr_ * 1.5))) {
                        const int before = pitchEchoCount_.load(std::memory_order_relaxed);
                        startPitchEcho(hz, nearestScaleNote(69.0 + 12.0 * std::log2(hz / 440.0)));
                        if (pitchEchoCount_.load(std::memory_order_relaxed) != before) { lastEchoedNote_ = note; sinceEcho_ = 0; }
                    }
                }
            }
        }
        if (heardRecently_ < (1 << 30)) ++heardRecently_;
        if (sinceEcho_ < (1 << 30)) ++sinceEcho_;
        float l = 0, r = 0;
        const float tone = renderTune();
        l += tone; r += tone;
        renderResonance(x, l, r);
        renderGrains(l, r);
        renderShiftEcho(x, l, r);
        renderPitchEcho(l, r);
        renderGroove(l, r);
        outL[i] += l;
        outR[i] += r;
        beat_ += beatPerFrame;
        ++frame_;
    }
}

} // namespace atmos
