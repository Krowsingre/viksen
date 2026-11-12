#include "engine/audio/ClipSequencer.h"

#include <algorithm>
#include <cmath>

namespace atmos {

void ClipSequencer::prepare(double sampleRate) {
    sr_ = sampleRate;
    for (auto& v : voices_) v.on = false;
    lastStep_ = -1;
}

void ClipSequencer::setClock(double beat, double bpm) noexcept {
    clockBeat_.store(beat, std::memory_order_relaxed);
    clockBpm_.store(bpm, std::memory_order_relaxed);
    clockSeq_.fetch_add(1, std::memory_order_release);
}

void ClipSequencer::trigger(const SeqPattern& p) noexcept {
    if (!p.clip || p.clip->numFrames() <= 0) return;
    Voice* slot = nullptr;
    for (auto& v : voices_) if (!v.on) { slot = &v; break; }
    if (!slot) {   // all busy: replace the one furthest through its clip
        slot = &voices_[0];
        for (auto& v : voices_) if (v.pos > slot->pos) slot = &v;
    }
    const float pan = std::clamp(p.pan, -1.0f, 1.0f);
    *slot = Voice{p.clip.get(), 0, p.gain * std::sqrt(0.5f * (1 - pan)) * 1.41421356f, p.gain * std::sqrt(0.5f * (1 + pan)) * 1.41421356f, true};
}

void ClipSequencer::processAdd(AudioBuffer& music, int n) noexcept {
    if (stopVoices_.exchange(false)) for (auto& v : voices_) v.on = false;
    if (const uint32_t seq = clockSeq_.load(std::memory_order_acquire); seq != seenSeq_) {
        seenSeq_ = seq;
        const double b = clockBeat_.load(std::memory_order_relaxed);
        const bool fresh = bpm_ <= 0;   // the clock was stopped: take the position as it is
        bpm_ = clockBpm_.load(std::memory_order_relaxed);
        if (fresh || std::fabs(b - beat_) > 0.25 || bpm_ <= 0) { beat_ = b; lastStep_ = static_cast<long>(std::floor(b * 4.0)); }
        else beat_ += 0.1 * (b - beat_);
    }
    const float target = master.load(std::memory_order_relaxed);
    const float e = std::clamp(energy.load(std::memory_order_relaxed), 0.0f, 1.0f);
    const double perFrame = bpm_ / 60.0 / sr_;
    float* L = music.channel(0);
    float* R = music.channel(1);
    int active = 0;
    for (int i = 0; i < n; ++i) {
        // New 16th note: trigger the patterns that play on it.
        if (bpm_ > 0) {
            const long step = static_cast<long>(std::floor(beat_ * 4.0));
            if (step != lastStep_) {
                lastStep_ = step;
                for (const auto& s : slots_) {
                    const SeqPattern* p = s.load(std::memory_order_acquire);
                    if (!p || e < p->energyMin || e > p->energyMax) continue;
                    const int k = static_cast<int>(((step % p->length) + p->length) % p->length);
                    if (!p->steps[static_cast<size_t>(k)]) continue;
                    if (p->chance < 1.0f && std::uniform_real_distribution<float>(0, 1)(rng_) >= p->chance) continue;
                    trigger(*p);
                }
            }
            beat_ += perFrame;
        }
        masterNow_ += 0.0005f * (target - masterNow_);   // ~40 ms
        float l = 0, r = 0;
        for (auto& v : voices_) {
            if (!v.on) continue;
            const int64_t len = v.clip->numFrames();
            // 3 ms in and out, so a slice never clicks.
            const int64_t edge = static_cast<int64_t>(sr_ * 0.003);
            const float env = std::min({1.0f, static_cast<float>(v.pos) / edge, static_cast<float>(len - v.pos) / edge});
            const float* c0 = v.clip->samples.channel(0);
            const float* c1 = v.clip->samples.numChannels() > 1 ? v.clip->samples.channel(1) : c0;
            l += c0[v.pos] * v.gl * env;
            r += c1[v.pos] * v.gr * env;
            if (++v.pos >= len) v.on = false;
        }
        L[i] += l * masterNow_;
        R[i] += r * masterNow_;
    }
    for (auto& v : voices_) active += v.on ? 1 : 0;
    published_.store(active, std::memory_order_relaxed);
}

} // namespace atmos
