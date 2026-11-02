#pragma once
// The independent source buses required by the hybrid model. Each bus has its own
// gain, mute, and (later) processing chain; they are summed only at the master.

#include <cstdint>

namespace atmos {

enum class Bus : uint8_t {
    Music = 0,          // imported songs / stems (source audio, untouched)
    Atmosphere = 1,     // authored atmospheric layers and generators
    MicRaw = 2,         // direct environmental monitoring (off by default: feedback risk)
    MicTransformed = 3, // processed environmental input
    Count = 4
};

constexpr int kNumBuses = static_cast<int>(Bus::Count);

constexpr const char* busId(Bus b) noexcept {
    switch (b) {
        case Bus::Music: return "music";
        case Bus::Atmosphere: return "atmosphere";
        case Bus::MicRaw: return "micRaw";
        case Bus::MicTransformed: return "micTransformed";
        case Bus::Count: break;
    }
    return "unknown";
}

// Conservative defaults (linear gain).
constexpr float defaultBusGain(Bus b) noexcept {
    switch (b) {
        case Bus::Music: return 1.0f;
        case Bus::Atmosphere: return 0.5f;
        case Bus::MicRaw: return 0.0f;
        case Bus::MicTransformed: return 0.5f;
        case Bus::Count: break;
    }
    return 0.0f;
}

} // namespace atmos
