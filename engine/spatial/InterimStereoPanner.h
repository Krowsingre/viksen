#pragma once

#include <atomic>

#include "engine/audio/Parameter.h"
#include "engine/dsp/Biquad.h"
#include "engine/dsp/DelayLine.h"
#include "engine/spatial/SpatialRenderer.h"

namespace atmos {

// ILD + ITD + head-shadow approximation. See SpatialRenderer.h for limitations.
class InterimStereoPanner final : public SpatialRenderer {
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() noexcept override;
    void setPosition(const SpatialPosition& p) noexcept override;
    void processAdd(const float* mono, AudioBuffer& out, int numFrames) noexcept override;
    int latencySamples() const noexcept override { return 0; }
    bool isBinauralHrtf() const noexcept override { return false; }

    // Reference distance (m) inside which no further gain increase is applied.
    static constexpr float kReferenceDistance = 0.3f;

private:
    double sampleRate_ = 48000.0;
    SmoothedParameter panX_{0.0f};     // sin(azimuth): -1 left .. +1 right
    SmoothedParameter rearness_{0.0f}; // 0 front .. 1 behind
    SmoothedParameter distGain_{1.0f};
    DelayLine delayL_, delayR_;
    Biquad shadowL_, shadowR_;
    float maxItdSamples_ = 0.0f;
};

} // namespace atmos
