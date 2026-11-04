#pragma once
// The listener's progression as an append-only log of discovery events (P-2,
// DECISIONS #62). Event ids are deterministic ("phase:<dimension>:<phase>"), so the same
// discovery on two devices is one event; merging is a union in which the earliest time
// wins, so a sync can never lose or duplicate a discovery. Progress (lit Phases, lunar
// journey, secret counts) is derived from the log, never stored separately.
// Dimension records stay separate; Global Secrets live in their own namespace.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/serialization/Json.h"
#include "core/worlds/WorldConditions.h"

namespace atmos::core {

enum class ProgressKind {
    PhaseDiscovered,          // dimension + lunar phase
    DimensionSecret,          // dimension + secret id
    GlobalSecret,             // secret id (no dimension)
    SongUnlocked,             // dimension + song id (e.g. a hidden song)
    MoodHeard,                // dimension + song id + mood ("rain", "night"...), for "heard in"
};

struct ProgressEvent {
    ProgressKind kind;
    std::string dimensionId;  // empty for Global Secrets
    std::string subject;      // phase id, secret id, song id, or "song|mood"
    double atDaysUtc = 0;     // when it happened (days since 1970-01-01 UTC)
    std::string id() const;   // deterministic identity
};

class ProgressLog {
public:
    static constexpr int kPhasesPerJourney = 8;
    static constexpr int kSecretsPerDimension = 5;

    // Adds an event; returns true if it is new. A repeat keeps the earliest time.
    bool add(const ProgressEvent& e);
    // Union with another device's log (earliest time wins). Returns how many were new here.
    int merge(const ProgressLog& other);

    // Derived progress.
    std::set<MoonPhaseName> discoveredPhases(const std::string& dimensionId) const;
    bool lunarJourneyComplete(const std::string& dimensionId) const;
    std::optional<double> lunarJourneyCompletedAt(const std::string& dimensionId) const;  // time of the 8th phase
    int dimensionSecretsFound(const std::string& dimensionId) const;   // 0..5
    bool allDimensionSecretsFound(const std::string& dimensionId) const;
    int globalSecretsFound() const;                                    // never a total
    bool has(const ProgressEvent& e) const { return events_.count(e.id()) > 0; }
    std::vector<ProgressEvent> events() const;                         // oldest first
    size_t size() const { return events_.size(); }

    Json toJson() const;
    static ProgressLog fromJson(const Json& j);   // unknown or malformed entries are skipped

private:
    std::map<std::string, ProgressEvent> events_;   // by id
};

const char* progressKindId(ProgressKind k);
std::optional<ProgressKind> progressKindFromId(const std::string& id);

} // namespace atmos::core
