#include "core/projects/ProjectManifest.h"

#include <cmath>
#include <set>

#include "core/serialization/Ids.h"

namespace atmos::core {

namespace {

const char* kindId(SourceKind k) {
    switch (k) {
        case SourceKind::Track: return "track";
        case SourceKind::Stem: return "stem";
        case SourceKind::Sample: return "sample";
        case SourceKind::Recording: return "recording";
        case SourceKind::Texture: return "texture";
    }
    return "track";
}
std::optional<SourceKind> kindFromId(const std::string& s) {
    if (s == "track") return SourceKind::Track;
    if (s == "stem") return SourceKind::Stem;
    if (s == "sample") return SourceKind::Sample;
    if (s == "recording") return SourceKind::Recording;
    if (s == "texture") return SourceKind::Texture;
    return std::nullopt;
}

// Removes the known keys from a copy of `j`, leaving only unknown ones.
Json unknownKeys(const Json& j, std::initializer_list<const char*> known) {
    Json rest = j.is_object() ? j : Json::object();
    for (const char* k : known) rest.erase(k);
    return rest;
}

// Writes known values on top of the preserved unknown keys.
Json withExtra(const Json& extra, Json known) {
    Json out = extra.is_object() ? extra : Json::object();
    for (auto it = known.begin(); it != known.end(); ++it) out[it.key()] = it.value();
    return out;
}

template <typename T>
T get(const Json& j, const char* key, T fallback) {
    if (!j.is_object()) return fallback;
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return fallback;
    try { return it->get<T>(); } catch (...) { return fallback; }
}

} // namespace

std::optional<Bus> busFromId(const std::string& s) {
    for (int b = 0; b < kNumBuses; ++b)
        if (s == busId(static_cast<Bus>(b))) return static_cast<Bus>(b);
    return std::nullopt;
}

ProjectManifest ProjectManifest::createNew(const std::string& name) {
    ProjectManifest m;
    m.id = makeUuid();
    m.name = name;
    m.createdAt = m.modifiedAt = utcTimestampNow();
    for (int b = 0; b < kNumBuses; ++b) {
        const float g = defaultBusGain(static_cast<Bus>(b));
        m.buses[b].gainDb = g > 0 ? 20.0f * std::log10(g) : -144.0f;
        m.buses[b].muted = g <= 0.0f;
    }
    return m;
}

const SourceRef* ProjectManifest::findSource(const std::string& sid) const {
    for (const auto& s : sources)
        if (s.id == sid) return &s;
    return nullptr;
}

// ------------------------------------------------------------------ migrations

bool migrateManifest(Json& j, int& fromVersion, std::string& error) {
    fromVersion = get<int>(j, "schemaVersion", -1);
    if (fromVersion < 0) {
        // Pre-release prototype documents (v0) used "version" and "tracks".
        if (j.contains("version") && j.contains("tracks")) fromVersion = 0;
        else { error = "missing schemaVersion"; return false; }
    }
    int v = fromVersion;
    if (v == 0) {
        // v0 -> v1: "tracks" (path list) became "sources" + "layers".
        Json sources = Json::array(), layers = Json::array();
        int n = 0;
        for (const auto& t : j["tracks"]) {
            const std::string sid = "src-" + std::to_string(n), lid = "layer-" + std::to_string(n);
            sources.push_back({{"id", sid}, {"kind", "track"}, {"storage", "external"},
                               {"path", t.is_string() ? t.get<std::string>() : get<std::string>(t, "path", "")}});
            layers.push_back({{"id", lid}, {"sourceId", sid}, {"bus", "music"}, {"timelineStartFrame", 0}});
            ++n;
        }
        j.erase("tracks");
        j.erase("version");
        j["sources"] = sources;
        j["layers"] = layers;
        j["schemaVersion"] = 1;
        v = 1;
    }
    // Future: if (v == 1) { ...; v = 2; }
    return true;
}

// ------------------------------------------------------------------ parse

static LoadResult parseManifest(const Json& input, ProjectManifest& m);

LoadResult manifestFromJson(const Json& input, ProjectManifest& m) {
    try {
        return parseManifest(input, m);
    } catch (const std::exception& e) {
        LoadResult r;
        r.error = std::string("malformed manifest: ") + e.what();
        return r;
    }
}

static LoadResult parseManifest(const Json& input, ProjectManifest& m) {
    LoadResult r;
    if (!input.is_object()) { r.error = "manifest is not a JSON object"; return r; }
    Json j = input;
    if (j.contains("format") && j["format"] != kManifestFormatId) { r.error = "not a project manifest"; return r; }

    int from = 0;
    if (!migrateManifest(j, from, r.error)) return r;
    r.originalSchemaVersion = from;
    r.migrated = from < kCurrentSchemaVersion;
    r.newerThanSupported = from > kCurrentSchemaVersion;

    auto section = [&](const char* key, Json fallback) {
        auto it = j.find(key);
        return it != j.end() && it->type() == fallback.type() ? *it : fallback;
    };
    m = ProjectManifest{};
    m.schemaVersion = std::max(kCurrentSchemaVersion, from); // never downgrade a newer document's version
    m.id = get<std::string>(j, "id", makeUuid());
    m.name = get<std::string>(j, "name", "Untitled");
    m.createdAt = get<std::string>(j, "createdAt", "");
    m.modifiedAt = get<std::string>(j, "modifiedAt", "");
    m.createdWith = section("createdWith", Json::object());

    const Json session = section("session", Json::object());
    m.sampleRate = get<double>(session, "sampleRate", 48000.0);
    if (session.contains("bpm") && session["bpm"].is_number()) m.bpm = session["bpm"].get<double>();

    for (const auto& s : section("sources", Json::array())) {
        SourceRef src;
        src.id = get<std::string>(s, "id", "");
        src.kind = kindFromId(get<std::string>(s, "kind", "track")).value_or(SourceKind::Track);
        src.storage = get<std::string>(s, "storage", "packaged") == "external" ? SourceStorage::External : SourceStorage::Packaged;
        src.path = get<std::string>(s, "path", "");
        src.displayName = get<std::string>(s, "displayName", "");
        src.groupId = get<std::string>(s, "groupId", "");
        src.isReferenceMix = get<bool>(s, "isReferenceMix", false);
        const Json media = s.is_object() && s.contains("media") && s["media"].is_object() ? s["media"] : Json::object();
        src.media.sampleRate = get<double>(media, "sampleRate", 0.0);
        src.media.channels = get<int>(media, "channels", 0);
        src.media.frames = get<int64_t>(media, "frames", 0);
        src.media.extra = unknownKeys(media, {"sampleRate", "channels", "frames"});
        src.extra = unknownKeys(s, {"id", "kind", "storage", "path", "displayName", "groupId", "isReferenceMix", "media"});
        m.sources.push_back(std::move(src));
    }

    for (const auto& l : section("layers", Json::array())) {
        Layer layer;
        layer.id = get<std::string>(l, "id", "");
        layer.sourceId = get<std::string>(l, "sourceId", "");
        layer.bus = busFromId(get<std::string>(l, "bus", "music")).value_or(Bus::Music);
        layer.timelineStartFrame = get<int64_t>(l, "timelineStartFrame", 0);
        layer.gainDb = get<float>(l, "gainDb", 0.0f);
        layer.muted = get<bool>(l, "muted", false);
        layer.solo = get<bool>(l, "solo", false);
        layer.extra = unknownKeys(l, {"id", "sourceId", "bus", "timelineStartFrame", "gainDb", "muted", "solo"});
        m.layers.push_back(std::move(layer));
    }

    const Json buses = section("buses", Json::object());
    Json unknownBuses = buses.is_object() ? buses : Json::object();
    for (int b = 0; b < kNumBuses; ++b) {
        const char* key = busId(static_cast<Bus>(b));
        const Json bj = buses.contains(key) && buses[key].is_object() ? buses[key] : Json::object();
        m.buses[b].gainDb = get<float>(bj, "gainDb", 0.0f);
        m.buses[b].muted = get<bool>(bj, "muted", false);
        m.buses[b].extra = unknownKeys(bj, {"gainDb", "muted"});
        unknownBuses.erase(key);
    }

    const Json master = section("master", Json::object());
    m.masterGainDb = get<float>(master, "gainDb", -3.0f);
    m.limiterCeilingDb = get<float>(master, "limiterCeilingDb", -1.0f);

    m.extra = unknownKeys(j, {"format", "schemaVersion", "id", "name", "createdAt", "modifiedAt", "createdWith",
                              "session", "sources", "layers", "buses", "master"});
    m.sessionExtra = unknownKeys(session, {"sampleRate", "bpm"});
    m.busesExtra = unknownBuses;
    m.masterExtra = unknownKeys(master, {"gainDb", "limiterCeilingDb"});

    r.ok = true;
    return r;
}

// ------------------------------------------------------------------ write

Json manifestToJson(const ProjectManifest& m) {
    const Json& extra = m.extra;
    const Json& sessionRest = m.sessionExtra;
    const Json& busesRest = m.busesExtra;
    const Json& masterRest = m.masterExtra;

    Json sources = Json::array();
    for (const auto& s : m.sources) {
        sources.push_back(withExtra(s.extra, {
            {"id", s.id}, {"kind", kindId(s.kind)},
            {"storage", s.storage == SourceStorage::External ? "external" : "packaged"},
            {"path", s.path}, {"displayName", s.displayName}, {"groupId", s.groupId},
            {"isReferenceMix", s.isReferenceMix},
            {"media", withExtra(s.media.extra, {{"sampleRate", s.media.sampleRate}, {"channels", s.media.channels},
                                                {"frames", s.media.frames}})}}));
    }
    Json layers = Json::array();
    for (const auto& l : m.layers) {
        layers.push_back(withExtra(l.extra, {{"id", l.id}, {"sourceId", l.sourceId}, {"bus", busId(l.bus)},
                                             {"timelineStartFrame", l.timelineStartFrame}, {"gainDb", l.gainDb},
                                             {"muted", l.muted}, {"solo", l.solo}}));
    }
    Json buses = busesRest.is_object() ? busesRest : Json::object();
    for (int b = 0; b < kNumBuses; ++b)
        buses[busId(static_cast<Bus>(b))] = withExtra(m.buses[b].extra, {{"gainDb", m.buses[b].gainDb}, {"muted", m.buses[b].muted}});

    Json session = withExtra(sessionRest, {{"sampleRate", m.sampleRate}});
    session["bpm"] = m.bpm ? Json(*m.bpm) : Json(nullptr);

    return withExtra(extra, {
        {"format", kManifestFormatId}, {"schemaVersion", m.schemaVersion}, {"id", m.id}, {"name", m.name},
        {"createdAt", m.createdAt}, {"modifiedAt", m.modifiedAt}, {"createdWith", m.createdWith},
        {"session", session}, {"sources", sources}, {"layers", layers}, {"buses", buses},
        {"master", withExtra(masterRest, {{"gainDb", m.masterGainDb}, {"limiterCeilingDb", m.limiterCeilingDb}})}});
}

// ------------------------------------------------------------------ validate

std::vector<ValidationIssue> validate(const ProjectManifest& m) {
    using S = ValidationIssue::Severity;
    std::vector<ValidationIssue> issues;
    if (m.sampleRate <= 0) issues.push_back({S::Error, "session sample rate must be positive"});
    if (m.bpm && (*m.bpm <= 0 || *m.bpm > 999)) issues.push_back({S::Warning, "tempo out of range"});
    std::set<std::string> ids;
    for (const auto& s : m.sources) {
        if (s.id.empty()) issues.push_back({S::Error, "source with empty id"});
        else if (!ids.insert(s.id).second) issues.push_back({S::Error, "duplicate source id: " + s.id});
        if (s.path.empty()) issues.push_back({S::Error, "source " + s.id + " has no path"});
        if (s.storage == SourceStorage::Packaged && (s.path.find("..") != std::string::npos || (!s.path.empty() && s.path[0] == '/')))
            issues.push_back({S::Error, "packaged source " + s.id + " path escapes the package"});
    }
    std::set<std::string> layerIds;
    for (const auto& l : m.layers) {
        if (l.id.empty()) issues.push_back({S::Error, "layer with empty id"});
        else if (!layerIds.insert(l.id).second) issues.push_back({S::Error, "duplicate layer id: " + l.id});
        if (!m.findSource(l.sourceId)) issues.push_back({S::Error, "layer " + l.id + " references missing source " + l.sourceId});
        if (l.timelineStartFrame < 0) issues.push_back({S::Error, "layer " + l.id + " starts before the timeline"});
    }
    // Hybrid workflow: warn about a reference mix playing together with its own stems.
    for (const auto& l : m.layers) {
        const SourceRef* s = m.findSource(l.sourceId);
        if (!s || !s->isReferenceMix || l.muted || s->groupId.empty()) continue;
        for (const auto& o : m.layers) {
            const SourceRef* os = m.findSource(o.sourceId);
            if (&o != &l && os && !o.muted && os->groupId == s->groupId && os->kind == SourceKind::Stem) {
                issues.push_back({S::Warning, "reference mix " + s->id + " is unmuted alongside its stems (double playback)"});
                break;
            }
        }
    }
    return issues;
}

} // namespace atmos::core
