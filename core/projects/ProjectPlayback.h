#pragma once
// Applies a project manifest to the engine: loads layers, bus and master settings.
// The caller supplies decoded clips (keyed by source id) because decoding is a
// platform concern (AVFoundation / JUCE / WAV reader).

#include <map>
#include <string>
#include <vector>

#include "core/projects/ProjectManifest.h"
#include "engine/audio/AudioEngine.h"

namespace atmos::core {

struct ApplyReport {
    int layersLoaded = 0;
    std::vector<std::string> warnings; // e.g. layer whose source failed to load
};

ApplyReport applyToEngine(const ProjectManifest& m, const std::map<std::string, ClipPtr>& clips, AudioEngine& engine);

} // namespace atmos::core
