#pragma once
// Creates the best available headphone renderer: Steam Audio HRTF binaural when
// the engine is built with ATMOS_WITH_STEAM_AUDIO (iPhone device builds, desktop),
// otherwise the interim ILD/ITD panner (e.g. the iOS Simulator).

#include <memory>

#include "engine/spatial/SpatialRenderer.h"

namespace atmos {

// Returns a prepared renderer. Falls back to the interim panner if Steam Audio
// cannot start (so a Dimension never goes silent).
std::unique_ptr<SpatialRenderer> makeBinauralRenderer(double sampleRate, int maxBlockSize);
bool binauralHrtfCompiledIn() noexcept;

} // namespace atmos
