#pragma once
// Room instrument, phase 4 (owner, 2026-11-12): music built on the beat. Short clips
// (loops and one-shots) are re-triggered on the song's 16th notes by step patterns, each
// within an energy range, so the music can answer the moment on any beat instead of only
// at section jumps. Clips play into the music bus, on top of the streamed stems.
//
// Thread model: the control thread owns the patterns (and their audio) and publishes them
// through atomic pointers; it keeps replaced patterns alive for a while (retire()) so the
// audio thread never sees freed memory. The beat clock is set from the song's position,
// as for the room instrument.

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <random>

#include "engine/audio/AudioBuffer.h"
#include "engine/audio/AudioClip.h"

namespace atmos {

struct SeqPattern {
    ClipPtr clip;                 // at the engine rate
    std::array<bool, 32> steps{}; // 16th notes over two bars
    int length = 16;              // 16 or 32 steps
    float gain = 1.0f;            // linear
    float pan = 0.0f;             // -1 .. 1
    float energyMin = 0.0f, energyMax = 1.0f;
    float chance = 1.0f;          // probability per step
};

class ClipSequencer {
public:
    static constexpr int kSlots = 16;
    static constexpr int kVoices = 24;

    void prepare(double sampleRate);

    // Control thread.
    void setPattern(int slot, const SeqPattern* p) noexcept { if (slot >= 0 && slot < kSlots) slots_[slot].store(p, std::memory_order_release); }
    void clearPatterns() noexcept { for (auto& s : slots_) s.store(nullptr, std::memory_order_release); stopVoices_.store(true); }
    void setClock(double beat, double bpm) noexcept;
    std::atomic<float> energy{0.5f};
    std::atomic<float> master{0.0f};   // follows the song's fade in/out

    // Audio thread: adds into the (stereo) music bus.
    void processAdd(AudioBuffer& music, int numFrames) noexcept;
    int activeVoices() const noexcept { return published_.load(std::memory_order_relaxed); }

private:
    struct Voice { const AudioClip* clip = nullptr; int64_t pos = 0; float gl = 0, gr = 0; bool on = false; };
    void trigger(const SeqPattern& p) noexcept;

    double sr_ = 48000.0;
    std::array<std::atomic<const SeqPattern*>, kSlots> slots_{};
    std::array<Voice, kVoices> voices_{};
    std::atomic<bool> stopVoices_{false};
    std::atomic<int> published_{0};
    std::mt19937 rng_{0xbea7};
    // Clock.
    std::atomic<double> clockBeat_{0.0}, clockBpm_{0.0};
    std::atomic<uint32_t> clockSeq_{0};
    uint32_t seenSeq_ = 0;
    double beat_ = 0.0, bpm_ = 0.0;
    long lastStep_ = -1;
    float masterNow_ = 0.0f;
};

} // namespace atmos
