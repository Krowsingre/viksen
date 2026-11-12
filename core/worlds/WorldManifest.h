#pragma once
// A World: built-in music (synchronised stems) plus rules that re-mix it in
// response to the listener's surroundings, motion and exercise. JSON on disk
// (world.json next to its audio files). See docs/WORLD_FORMAT.md.

#include <optional>
#include <string>
#include <vector>

#include "core/worlds/WorldConditions.h"
#include "core/worlds/WorldSections.h"

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
    // Musical grid for adaptive sections: bar 1 starts at barOffsetSeconds.
    int beatsPerBar = 4;
    double barOffsetSeconds = 0.0;
    int jumpEveryBars = 4;           // possible jump points (phrase length)
    double minStayBars = 8;          // a part plays at least this long before the state can move it
    double crossfadeBeats = 1.0;     // length of the crossfade when jumping
    std::vector<WorldSection> sections;
    // The lunar phase this song belongs to (a Dimension's eight lunar-phase songs), if any.
    std::optional<MoonPhaseName> lunarPhase;
    // When the song plays in a Dimension with a day track (owner, 2026-11-05): "day"
    // (default), "night" (every night that has no song for its own phase), the weather
    // songs "rain" (also thunderstorms, for now), "snow", "fog", "wind" and "cloudy" (day or
    // night), or "dawn" / "dusk" (owner, 2026-11-11).
    // "secret": a moon-event secret's hidden song (owner, 2026-11-11), with `moonEvent` set
    // to the event's id ("supermoon", "solarEclipse"...); it plays while that event is on.
    enum class Plays { Day, Night, Rain, Wind, Cloudy, Snow, Fog, Dawn, Dusk, Secret };
    Plays plays = Plays::Day;
    std::string moonEvent;
    std::vector<WorldStem> stems;
    std::vector<WorldRule> rules;
    ConditionSet unlock;                  // when this World becomes available (empty = always)
    Json extra = Json::object();          // unknown keys, preserved

    int stemIndex(const std::string& id) const;
};

std::optional<WorldManifest> parseWorld(const Json& j, std::string* error = nullptr);

// A Dimension holds one or more songs. Single-song world.json files are a Dimension
// with one song; multi-song files list them under "songs" (each with its own music,
// stems, sections and rules). Songs carry ids "<dimension>/<song>".
// One position in a Dimension's standard layout (owner, 2026-11-07): "day", "storm",
// "night-<phase>" for each of the eight phases, "secret-1".."secret-5", or any extra
// slot a non-standard Dimension adds. A slot with no stems yet waits to be filled and
// is not played; secrets are not played until the Secrets feature exists.
struct DimensionSlot {
    std::string slot, id, title;
    bool filled = false;      // has audio (stems) and is playable
};

struct DimensionManifest {
    std::string id, name, artist, tier = "free";
    std::string album;             // the owner's album the songs come from (shown, linked to buy)
    std::string albumLink;         // optional fixed Apple Music URL
    bool albumOnAppleMusic = false; // the album is live: the app may look up its Apple Music page
    ConditionSet unlock;
    std::vector<WorldManifest> songs;   // the playable songs (filled, non-secret slots)
    std::vector<DimensionSlot> slots;   // every slot, filled or waiting, in file order
    size_t totalStems() const { size_t n = 0; for (const auto& s : songs) n += s.stems.size(); return n; }
};
std::optional<DimensionManifest> parseDimension(const Json& j, std::string* error = nullptr);
Json worldToJson(const WorldManifest& w);

} // namespace atmos::core
