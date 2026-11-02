#include "engine/environment/RoomMemory.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atmos {

void RoomMemory::prepare(double sampleRate, double slotSeconds) {
    sampleRate_ = sampleRate;
    slotFrames_ = std::max(1024, static_cast<int>(sampleRate * slotSeconds));
    memory_.assign(static_cast<size_t>(kSlots) * static_cast<size_t>(slotFrames_), 0.0f);
    level.prepare(sampleRate, 0.2);
    forget();
}

void RoomMemory::forget() noexcept {
    std::fill(memory_.begin(), memory_.end(), 0.0f);
    valid_.fill(false);
    for (auto& v : voices_) v.slot = -1;
    writeSlot_ = writePos_ = slotsCompleted_ = 0;
    publishVoices();
}

void RoomMemory::publishVoices() noexcept {
    publishedVoices_.store(static_cast<int>(std::count_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.slot >= 0; })),
                           std::memory_order_relaxed);
}

void RoomMemory::startVoice(int slot) noexcept {
    auto it = std::find_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.slot < 0; });
    if (it == voices_.end()) return; // all voices busy: skip this memory
    // Clamped to +-12 so a replay always finishes before its slot is overwritten.
    const float semis = std::clamp(pitchSemitones.load(std::memory_order_relaxed), -12.0f, 12.0f);;
    it->slot = slot;
    it->rate = std::pow(2.0, semis / 12.0);
    it->readPos = 0.0;
    it->elapsed = 0.0;
    it->lengthOut = slotFrames_ / it->rate;
    it->pan = nextPan_;
    nextPan_ = -nextPan_; // alternate sides so there is no long-term bias
}

void RoomMemory::capture(const float* mono, int numFrames) noexcept {
    const bool on = enabled();
    if (wasEnabled_ && !on) forget();
    wasEnabled_ = on;
    if (!on) return;
    float* mem = memory_.data();
    for (int i = 0; i < numFrames; ++i) {
        mem[static_cast<size_t>(writeSlot_) * slotFrames_ + writePos_] = mono[i];
        if (++writePos_ >= slotFrames_) {
            valid_[writeSlot_] = true;
            const int finished = writeSlot_;
            writePos_ = 0;
            writeSlot_ = (writeSlot_ + 1) % kSlots;
            valid_[writeSlot_] = false; // about to be overwritten
            ++slotsCompleted_;
            // Every completed slot schedules a replay of an older memory (not the
            // one just heard), once at least two memories exist.
            if (slotsCompleted_ >= 2) {
                const int older = (finished + kSlots - 1) % kSlots;
                bool inUse = false;
                for (const auto& v : voices_) inUse |= v.slot == older;
                if (valid_[older] && !inUse) startVoice(older);
            }
        }
    }
}

void RoomMemory::renderAdd(AudioBuffer& out, int numFrames) noexcept {
    level.beginBlock();
    if (out.numChannels() < 2) return;
    float* L = out.channel(0);
    float* R = out.channel(1);
    const double pi = std::numbers::pi;
    for (int i = 0; i < numFrames; ++i) {
        const float lvl = level.next();
        for (auto& v : voices_) {
            if (v.slot < 0) continue;
            if (v.slot == writeSlot_) { v.slot = -1; continue; } // being overwritten
            const float* src = memory_.data() + static_cast<size_t>(v.slot) * slotFrames_;
            const int i0 = static_cast<int>(v.readPos);
            if (i0 + 1 >= slotFrames_) { v.slot = -1; continue; }
            const float frac = static_cast<float>(v.readPos - i0);
            const float s = src[i0] + frac * (src[i0 + 1] - src[i0]);
            // Long raised-cosine envelope over the whole replay.
            const float env = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * pi * v.elapsed / v.lengthOut));
            const float theta = (v.pan + 1.0f) * 0.25f * static_cast<float>(pi);
            L[i] += s * env * lvl * std::cos(theta);
            R[i] += s * env * lvl * std::sin(theta);
            v.readPos += v.rate;
            v.elapsed += 1.0;
            if (v.elapsed >= v.lengthOut) v.slot = -1;
        }
    }
    publishVoices();
}

} // namespace atmos
