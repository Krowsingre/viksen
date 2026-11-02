#pragma once
// HRTF binaural rendering with Steam Audio (Valve, Apache-2.0). Only compiled
// when ATMOS_WITH_STEAM_AUDIO is defined. Steam Audio processes fixed frames,
// so this renderer buffers internally and adds kFrameSize samples of latency
// (about 5 ms at 48 kHz). Use makeBinauralRenderer() rather than this directly.

#include "engine/spatial/SpatialRenderer.h"

#if defined(ATMOS_WITH_STEAM_AUDIO)

#include <atomic>
#include <memory>
#include <vector>

namespace atmos {

class SteamAudioRenderer final : public SpatialRenderer {
public:
    static constexpr int kFrameSize = 256;

    SteamAudioRenderer();
    ~SteamAudioRenderer() override;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setPosition(const SpatialPosition& p) noexcept override;
    void processAdd(const float* mono, AudioBuffer& out, int numFrames) noexcept override;
    int latencySamples() const noexcept override { return kFrameSize; }
    bool isBinauralHrtf() const noexcept override { return ok_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ok_ = false;
    std::atomic<float> az_{0.0f}, el_{0.0f}, dist_{1.0f}, spread_{0.0f};
};

} // namespace atmos

#endif
