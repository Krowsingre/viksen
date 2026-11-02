#pragma once
// Saves and restores a live session (Experience state) as a regular project
// manifest, so it opens with the same package format on every platform.
// The listener's live settings live under the manifest key "liveSession";
// the song (if any) is a normal source + layer on the music bus.

#include <optional>
#include <string>

#include "core/projects/ProjectManifest.h"
#include "core/session/LiveSession.h"

namespace atmos::core {

Json liveSettingsToJson(const LiveSettings& s);
// Missing or wrong-typed keys keep the values from `base`.
LiveSettings liveSettingsFromJson(const Json& j, const LiveSettings& base = {});

struct SessionTrack {
    std::string packagedPath;   // relative path inside the package (assets/audio/...)
    std::string displayName;
    MediaInfo media;
};

ProjectManifest makeSessionProject(const std::string& name, const LiveSettings& settings,
                                   const std::optional<SessionTrack>& track, const std::string& dimensionId,
                                   double sampleRate);

struct SessionRestore {
    LiveSettings settings;
    std::optional<SessionTrack> track;
    std::string dimensionId;
};
SessionRestore readSessionProject(const ProjectManifest& m, const LiveSettings& base = {});

} // namespace atmos::core
