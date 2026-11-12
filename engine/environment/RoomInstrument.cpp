#include "engine/environment/RoomInstrument.h"

#include <algorithm>
#include <cmath>

namespace atmos {
namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr int kDecimate = 4;
constexpr int kPitchWindow = 1024;     // at sr / 4: ~85 ms at 48 kHz
constexpr int kPitchHop = 256;         // ~21 ms

double midiToHz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }

} // namespace

void RoomInstrument::prepare(double sampleRate) {
    sr_ = sampleRate;
    pitchBuf_.assign(kPitchWindow, 0.0f);
    ring_.assign(static_cast<size_t>(sampleRate * 4.0), 0.0f);
    reset();
}

void RoomInstrument::reset() noexcept {
    std::fill(pitchBuf_.begin(), pitchBuf_.end(), 0.0f);
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    pitchWrite_ = decim_ = sinceAnalysis_ = 0;
    decimAcc_ = 0.0f;
    trackedHz_.store(0.0f, std::memory_order_relaxed);
    candidateNote_ = -1; candidateHold_ = 0; heardRecently_ = 1 << 30;
    tuneNoteOut_.store(-1, std::memory_order_relaxed);
    tuneAmp_ = 0.0f;
    ringWrite_ = 0;
    for (auto& g : grains_) g.on = false;
    for (auto& b : bells_) b.on = false;
    fastEnv_ = slowEnv_ = 0.0f;
    refractory_ = 0;
    warmup_ = static_cast<int>(sr_ * 2.0);   // no hits in the first two seconds
    last16th_ = -1;
}

void RoomInstrument::setClock(double beat, double bpm) noexcept {
    clockBeat_.store(beat, std::memory_order_relaxed);
    clockBpm_.store(bpm, std::memory_order_relaxed);
    clockSeq_.fetch_add(1, std::memory_order_release);
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

int RoomInstrument::quantize(float hz) const noexcept {
    if (hz <= 0) return -1;
    double m = 69.0 + 12.0 * std::log2(hz / 440.0);
    while (m < 52) m += 12;   // sing it back between E3 and E5, whatever the source
    while (m > 76) m -= 12;
    const uint16_t mask = scale_.load(std::memory_order_relaxed);
    int best = -1; double bestD = 1e9;
    for (int n = static_cast<int>(std::floor(m)) - 6; n <= static_cast<int>(std::ceil(m)) + 6; ++n)
        if (mask & (1u << (((n % 12) + 12) % 12)))
            if (const double d = std::fabs(n - m); d < bestD) { bestD = d; best = n; }
    return best;
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
                for (auto& g : grains_)
                    if (!g.on) {
                        const float pan = std::uniform_real_distribution<float>(-0.7f, 0.7f)(rng_);
                        g = Grain{true, static_cast<double>((ringWrite_ - back + static_cast<int>(ring_.size())) % static_cast<int>(ring_.size())),
                                  static_cast<int>(sixteenthFrames * 0.9), static_cast<int>(sixteenthFrames * 0.9),
                                  std::sqrt(0.5f * (1 - pan)), std::sqrt(0.5f * (1 + pan))};
                        break;
                    }
        }
    }
    for (auto& g : grains_) {
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

void RoomInstrument::detectHit(float x) noexcept {
    const float a = std::fabs(x);
    fastEnv_ += (a > fastEnv_ ? 0.02f : 0.002f) * (a - fastEnv_);
    slowEnv_ += 0.00007f * (a - slowEnv_);   // ~300 ms
    if (warmup_ > 0) { --warmup_; return; }
    if (refractory_ > 0) { --refractory_; return; }
    if (fastEnv_ > 0.01f && fastEnv_ > 3.0f * slowEnv_ + 1e-4f) {
        refractory_ = static_cast<int>(sr_ * 0.18);
        startHit();
    }
}

void RoomInstrument::startHit() noexcept {
    if (hitsLevel.load(std::memory_order_relaxed) <= 0) return;
    if (bag_.empty()) {   // refill: every scale note across two octaves, shuffled
        const uint16_t mask = scale_.load(std::memory_order_relaxed);
        for (int n = 60; n < 84; ++n) if (mask & (1u << (n % 12))) bag_.push_back(n);
        std::shuffle(bag_.begin(), bag_.end(), rng_);
        if (bag_.empty()) return;
    }
    const int note = bag_.back();
    bag_.pop_back();
    Bell* slot = &bells_[0];
    for (auto& b : bells_) { if (!b.on) { slot = &b; break; } if (b.t > slot->t) slot = &b; }
    const float pan = std::uniform_real_distribution<float>(-0.8f, 0.8f)(rng_);
    *slot = Bell{true, 0, 0, midiToHz(note), 0, std::sqrt(0.5f * (1 - pan)), std::sqrt(0.5f * (1 + pan))};
}

void RoomInstrument::renderHits(float& l, float& r) noexcept {
    const float level = hitsLevel.load(std::memory_order_relaxed);
    for (auto& b : bells_) {
        if (!b.on) continue;
        // A soft FM bell: the brightness falls away faster than the tone.
        const float index = 3.5f * std::exp(-b.t / 0.25f);
        const float amp = std::exp(-b.t / 1.1f) * std::min(1.0f, b.t / 0.002f);
        b.phaseM += kTwoPi * b.hz * 3.5 / sr_;
        b.phaseC += kTwoPi * b.hz / sr_;
        const float x = 0.18f * level * amp * static_cast<float>(std::sin(b.phaseC + index * std::sin(b.phaseM)));
        l += x * b.panL;
        r += x * b.panR;
        b.t += static_cast<float>(1.0 / sr_);
        if (b.t > 5.0f) b.on = false;
    }
}

void RoomInstrument::processAdd(const float* mono, float* outL, float* outR, int numFrames) noexcept {
    // The beat clock: take the session's latest position, then run on at the tempo.
    if (const uint32_t seq = clockSeq_.load(std::memory_order_acquire); seq != seenSeq_) {
        seenSeq_ = seq;
        const double b = clockBeat_.load(std::memory_order_relaxed);
        bpm_ = clockBpm_.load(std::memory_order_relaxed);
        if (std::fabs(b - beat_) > 0.25 || bpm_ <= 0) beat_ = b;   // a jump (seek, new song)
        else beat_ += 0.1 * (b - beat_);                            // otherwise a gentle correction
    }
    const double beatPerFrame = bpm_ / 60.0 / sr_;
    for (int i = 0; i < numFrames; ++i) {
        const float x = mono[i];
        // Memory for the groove.
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
                const int note = quantize(trackedHz_.load(std::memory_order_relaxed));
                if (note >= 0) {
                    heardRecently_ = 0;
                    if (note == candidateNote_) candidateHold_ += kPitchHop;
                    else { candidateNote_ = note; candidateHold_ = 0; }
                    // A note held ~120 ms becomes the sung note.
                    if (candidateHold_ * kDecimate >= static_cast<int>(sr_ * 0.12) || tuneNoteOut_.load(std::memory_order_relaxed) < 0) {
                        tuneTargetHz_ = midiToHz(note);
                        if (tuneNoteOut_.load(std::memory_order_relaxed) < 0) tuneHz_ = tuneTargetHz_;
                        tuneNoteOut_.store(note, std::memory_order_relaxed);
                    }
                }
            }
        }
        if (heardRecently_ < (1 << 30)) ++heardRecently_;
        detectHit(x);
        float l = 0, r = 0;
        const float tone = renderTune();
        l += tone; r += tone;
        renderGroove(l, r);
        renderHits(l, r);
        outL[i] += l;
        outR[i] += r;
        beat_ += beatPerFrame;
    }
}

} // namespace atmos
