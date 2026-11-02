#pragma once
// Spatial rendering interface. A renderer takes a mono source and its position
// relative to the listener and produces a stereo (headphone) signal.
//
// IMPORTANT: The only implementation in Phase 1 is InterimStereoPanner, which uses
// interaural level/time differences and a head-shadow filter. It is NOT HRTF-based
// binaural rendering and must not be presented to users as full 3D positioning.
// An HRTF renderer (Phase 3) will implement this same interface.

#include "engine/audio/AudioBuffer.h"

namespace atmos {

struct SpatialPosition {
    float azimuthDeg = 0.0f;    // 0 = front, +90 = right, -90 = left, 180 = behind
    float elevationDeg = 0.0f;  // +90 = above (ignored by the interim panner)
    float distanceM = 1.0f;     // metres from the listener's head centre
    float spread = 0.0f;        // 0 = point source, 1 = fully diffuse
};

class SpatialRenderer {
public:
    virtual ~SpatialRenderer() = default;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() noexcept = 0;
    // Any thread.
    virtual void setPosition(const SpatialPosition& p) noexcept = 0;
    // Audio thread: mono in -> adds into the first two channels of out.
    virtual void processAdd(const float* mono, AudioBuffer& out, int numFrames) noexcept = 0;
    // Algorithmic latency in samples (for compensation reporting).
    virtual int latencySamples() const noexcept = 0;
    virtual bool isBinauralHrtf() const noexcept = 0;
};

} // namespace atmos
