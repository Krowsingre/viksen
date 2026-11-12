#pragma once
// "Room memory": continuously keeps the last few seconds of environmental sound in
// RAM, split into slots, and replays completed slots later - slowed/pitched and
// with long fades and alternating pan - so an atmosphere grows out of the room.
//
// Privacy: audio lives only in pre-allocated RAM, is overwritten continuously, is
// never written to disk, and is wiped by forget() (called when the mic turns off).

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "engine/audio/AudioBuffer.h"
#include "engine/audio/Parameter.h"

namespace atmos {

class RoomMemory {
public:
    static constexpr int kSlots = 4;
    static constexpr int kVoices = 3;

    void prepare(double sampleRate, double slotSeconds = 5.0);

    // Any thread.
    SmoothedParameter level{0.6f};
    std::atomic<float> pitchSemitones{-12.0f};
    // The room blend's arrangement brings the memory in and out (0..1, smoothed by it).
    std::atomic<float> gate{1.0f};   // -12..+12, applied to the next voice started
    // Listener head direction (degrees, + = turned right). Each replayed memory has a
    // fixed direction in the room, so with head tracking the memories stay in place
    // while the head turns. 0 when no head tracking is available.
    std::atomic<float> listenerYawDeg{0.0f};
    void setEnabled(bool e) noexcept { enabled_.store(e, std::memory_order_relaxed); }
    bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }

    // Audio thread.
    void capture(const float* mono, int numFrames) noexcept;
    void renderAdd(AudioBuffer& out, int numFrames) noexcept;
    void forget() noexcept;                       // wipe memory and stop voices
    int activeVoices() const noexcept { return publishedVoices_.load(std::memory_order_relaxed); } // any thread

private:
    struct Voice {
        int slot = -1;
        double readPos = 0.0;
        double rate = 1.0;
        double lengthOut = 1.0;   // output samples this voice lasts
        double elapsed = 0.0;
        float azimuthDeg = 0.0f;  // direction in the room (+ = right of the initial front)
        float gainL = 0.0f, gainR = 0.0f; // last applied gains (ramped per block)
    };
    void startVoice(int slot) noexcept;
    void publishVoices() noexcept;

    double sampleRate_ = 48000.0;
    int slotFrames_ = 0;
    std::vector<float> memory_;   // kSlots * slotFrames_
    std::array<bool, kSlots> valid_{};
    int writeSlot_ = 0, writePos_ = 0, slotsCompleted_ = 0;
    std::array<Voice, kVoices> voices_{};
    int nextDirection_ = 0;
    std::atomic<bool> enabled_{false};
    bool wasEnabled_ = false;
    std::atomic<int> publishedVoices_{0};
};

} // namespace atmos
