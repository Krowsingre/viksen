#pragma once
// A World: built-in music (synchronised stems) plus rules that re-mix it in
// response to the listener's surroundings, motion and exercise. JSON on disk
// (world.json next to its audio files). See docs/WORLD_FORMAT.md.

#include <optional>
#include <string>
#include <vector>

#include "core/worlds/WorldConditions.h"

namespace atmos::core {

struct WorldStem {
    std::string id;
    std::string file;          // relative to the World folder
    std::string title;
    float baseGainDb = 0.0f;   // authored mix level (used when no rule applies)
    // Default position in 3D (headphone) mode. Azimuth: 0 front, +90 right;
    // elevation: +90 above; distance in metres; width: angle between the stem's
    // left and right channels.
    float azimuthDeg = 0.0f, elevationDeg = 0.0f, distanceM = 1.5f, widthDeg = 60.0f;
};

struct WorldRule {
    std::string stem;          // stem id
    ConditionSet when;
    float gainDb = 0.0f;       // target while `when` holds
    float otherwiseDb = -60.0f;// target while it does not
    std::string label;         // short explanation for the UI
};

struct WorldManifest {
    std::string id, name, artist, tier = "free";
    double tempo = 120.0;
    std::string key;
    double loopFromSeconds = 0.0;
    std::optional<double> loopToSeconds;  // nullopt = end of the stems
    float fadeSeconds = 4.0f;
    std::vector<WorldStem> stems;
    std::vector<WorldRule> rules;
    ConditionSet unlock;                  // when this World becomes available (empty = always)
    Json extra = Json::object();          // unknown keys, preserved

    int stemIndex(const std::string& id) const;
};

std::optional<WorldManifest> parseWorld(const Json& j, std::string* error = nullptr);
Json worldToJson(const WorldManifest& w);

} // namespace atmos::core
