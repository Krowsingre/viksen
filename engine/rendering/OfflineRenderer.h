#pragma once
// Deterministic offline rendering of the engine (export path). Distinct from live
// session capture: no microphone input is consumed and the result is repeatable for
// a given project state and seed.

#include <cstdint>
#include <functional>

#include "engine/audio/AudioBuffer.h"
#include "engine/audio/AudioEngine.h"

namespace atmos {

struct OfflineRenderOptions {
    int64_t startFrame = 0;
    int64_t numFrames = 0;      // 0 = to the end of the timeline
    int blockSize = 512;
    // Optional progress callback in [0, 1]; return false to cancel.
    std::function<bool(double)> progress;
};

// Renders stereo output. The engine must be prepared and not driven by a live
// device callback at the same time. Returns false if cancelled.
bool renderOffline(AudioEngine& engine, const OfflineRenderOptions& options, AudioBuffer& out);

} // namespace atmos
