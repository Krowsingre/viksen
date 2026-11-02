#pragma once
// Versioned project manifest (manifest.json inside a project package).
// See docs/PROJECT_SCHEMA.md. Every object keeps the JSON keys it does not
// understand in `extra`, and they are written back verbatim on save, so a project
// edited on one platform never loses data written by another (or by a newer build).

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/serialization/Json.h"
#include "engine/routing/Bus.h"

namespace atmos::core {

constexpr int kCurrentSchemaVersion = 1;
constexpr const char* kManifestFormatId = "atmos.project"; // product-neutral

struct MediaInfo {
    double sampleRate = 0.0;
    int channels = 0;
    int64_t frames = 0;
    Json extra = Json::object();
};

enum class SourceKind { Track, Stem, Sample, Recording, Texture };
enum class SourceStorage { Packaged, External };

struct SourceRef {
    std::string id;
    SourceKind kind = SourceKind::Track;
    SourceStorage storage = SourceStorage::Packaged;
    std::string path;            // packaged: relative to package root (forward slashes); external: absolute
    std::string displayName;
    std::string groupId;         // stems of one song share a group (and the full mix, if any)
    bool isReferenceMix = false; // full mix kept as A/B reference; not auto-played with its stems
    MediaInfo media;
    Json extra = Json::object();
};

struct Layer {
    std::string id;
    std::string sourceId;
    Bus bus = Bus::Music;
    int64_t timelineStartFrame = 0; // in session sample-rate frames; source frame 0 lands here
    float gainDb = 0.0f;
    bool muted = false;
    bool solo = false;
    Json extra = Json::object();
};

struct BusSettings {
    float gainDb = 0.0f;
    bool muted = false;
    Json extra = Json::object();
};

struct ProjectManifest {
    int schemaVersion = kCurrentSchemaVersion;
    std::string id;
    std::string name = "Untitled";
    std::string createdAt, modifiedAt;
    Json createdWith = Json::object(); // {"app":..., "version":..., "platform":...}
    double sampleRate = 48000.0;
    std::optional<double> bpm;         // manual tempo; never applied to sources implicitly
    std::vector<SourceRef> sources;
    std::vector<Layer> layers;
    BusSettings buses[kNumBuses];
    float masterGainDb = -3.0f;
    float limiterCeilingDb = -1.0f;
    Json extra = Json::object();        // unknown top-level keys (incl. future sections)
    Json sessionExtra = Json::object(); // unknown keys inside "session"
    Json busesExtra = Json::object();   // unknown buses inside "buses"
    Json masterExtra = Json::object();  // unknown keys inside "master"

    static ProjectManifest createNew(const std::string& name);
    const SourceRef* findSource(const std::string& id) const;
};

struct ValidationIssue {
    enum class Severity { Warning, Error } severity;
    std::string message;
};

struct LoadResult {
    bool ok = false;
    std::string error;
    bool migrated = false;
    int originalSchemaVersion = 0;
    bool newerThanSupported = false;   // opened a file from a newer build; unknown data is preserved
};

// Parse (with migration). Never throws.
LoadResult manifestFromJson(const Json& j, ProjectManifest& out);
Json manifestToJson(const ProjectManifest& m);

std::vector<ValidationIssue> validate(const ProjectManifest& m);

// Migrations: upgrade older documents in place to kCurrentSchemaVersion.
// Returns false (with error) if the document cannot be migrated.
bool migrateManifest(Json& j, int& fromVersion, std::string& error);

std::optional<Bus> busFromId(const std::string& s);

} // namespace atmos::core
