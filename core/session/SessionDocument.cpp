#include "core/session/SessionDocument.h"

#include <algorithm>
#include <cmath>

#include "core/serialization/Ids.h"

namespace atmos::core {

namespace {
template <typename T>
T pick(const Json& j, const char* k, T fallback) {
    if (!j.is_object() || !j.contains(k)) return fallback;
    try { return j.at(k).get<T>(); } catch (...) { return fallback; }
}
} // namespace

Json liveSettingsToJson(const LiveSettings& s) {
    Json buses = Json::object();
    for (int b = 0; b < kNumBuses; ++b)
        buses[busId(static_cast<Bus>(b))] = {{"gain", s.busGain[b]}, {"muted", s.busMuted[b]}};
    return {
        {"buses", buses},
        {"masterGain", s.masterGain},
        {"limiterCeilingDb", s.limiterCeilingDb},
        {"bed", {{"enabled", s.bedEnabled}, {"level", s.bedLevel}, {"colourHz", s.bedColourHz}}},
        {"micTransform", {{"toneHz", s.micToneHz}, {"echoSeconds", s.micEchoSeconds}, {"feedback", s.micFeedback},
                          {"wet", s.micWet}, {"shiftHz", s.micShiftHz}, {"shiftMix", s.micShiftMix}}},
        {"roomMemory", {{"enabled", s.roomMemoryEnabled}, {"level", s.roomMemoryLevel}, {"pitch", s.roomMemoryPitch}}},
        {"looping", s.looping},
        // The microphone is deliberately NOT restored: it must always be switched on by the user.
    };
}

// Every value is clamped to its UI range: a damaged or hand-edited project can
// never restore an unsafe level (the safety limiter is the second line of defence).
LiveSettings liveSettingsFromJson(const Json& j, const LiveSettings& base) {
    LiveSettings s = base;
    if (!j.is_object()) return s;
    if (j.contains("buses") && j["buses"].is_object())
        for (int b = 0; b < kNumBuses; ++b) {
            const Json bj = j["buses"].value(busId(static_cast<Bus>(b)), Json::object());
            s.busGain[b] = std::clamp(pick(bj, "gain", s.busGain[b]), 0.0f, 4.0f);
            s.busMuted[b] = pick(bj, "muted", s.busMuted[b]);
        }
    s.masterGain = std::clamp(pick(j, "masterGain", s.masterGain), 0.0f, 1.0f);
    s.limiterCeilingDb = std::clamp(pick(j, "limiterCeilingDb", s.limiterCeilingDb), -24.0f, 0.0f);
    const Json bed = j.value("bed", Json::object());
    s.bedEnabled = pick(bed, "enabled", s.bedEnabled);
    s.bedLevel = std::clamp(pick(bed, "level", s.bedLevel), 0.0f, 0.6f);
    s.bedColourHz = std::clamp(pick(bed, "colourHz", s.bedColourHz), 100.0f, 8000.0f);
    const Json mt = j.value("micTransform", Json::object());
    s.micToneHz = std::clamp(pick(mt, "toneHz", s.micToneHz), 100.0f, 16000.0f);
    s.micEchoSeconds = std::clamp(pick(mt, "echoSeconds", s.micEchoSeconds), 0.02f, 1.9f);
    s.micFeedback = std::clamp(pick(mt, "feedback", s.micFeedback), 0.0f, 0.9f);
    s.micWet = std::clamp(pick(mt, "wet", s.micWet), 0.0f, 1.0f);
    s.micShiftHz = std::clamp(pick(mt, "shiftHz", s.micShiftHz), -2000.0f, 2000.0f);
    s.micShiftMix = std::clamp(pick(mt, "shiftMix", s.micShiftMix), 0.0f, 1.0f);
    const Json rm = j.value("roomMemory", Json::object());
    s.roomMemoryEnabled = pick(rm, "enabled", s.roomMemoryEnabled);
    s.roomMemoryLevel = std::clamp(pick(rm, "level", s.roomMemoryLevel), 0.0f, 1.0f);
    s.roomMemoryPitch = std::clamp(pick(rm, "pitch", s.roomMemoryPitch), -12.0f, 12.0f);
    s.looping = pick(j, "looping", s.looping);
    s.micEnabled = false; // never restored
    return s;
}

ProjectManifest makeSessionProject(const std::string& name, const LiveSettings& settings,
                                   const std::optional<SessionTrack>& track, const std::string& dimensionId,
                                   double sampleRate, const std::vector<SessionStem>& stems) {
    ProjectManifest m = ProjectManifest::createNew(name);
    m.sampleRate = sampleRate;
    for (int b = 0; b < kNumBuses; ++b) {
        const float g = settings.busGain[b];
        m.buses[b].gainDb = g > 0 ? 20.0f * std::log10(g) : -144.0f;
        m.buses[b].muted = settings.busMuted[b];
    }
    m.masterGainDb = settings.masterGain > 0 ? 20.0f * std::log10(settings.masterGain) : -144.0f;
    m.limiterCeilingDb = settings.limiterCeilingDb;
    if (track) {
        m.sources.push_back({"song", SourceKind::Track, SourceStorage::Packaged, track->packagedPath,
                             track->displayName, "", false, track->media, Json::object()});
        m.layers.push_back({"song-layer", "song", Bus::Music, 0, 0.0f, false, false, Json::object()});
    }
    for (size_t i = 0; i < stems.size(); ++i) {
        const std::string sid = "stem-" + std::to_string(i);
        // All stems share one group and start at frame 0: their own leading silence keeps them aligned.
        m.sources.push_back({sid, SourceKind::Stem, SourceStorage::Packaged, stems[i].file.packagedPath,
                             stems[i].file.displayName, "studio", false, stems[i].file.media, Json::object()});
        m.layers.push_back({sid + "-layer", sid, Bus::Music, 0, stems[i].gainDb, stems[i].muted, false, Json::object()});
    }
    m.extra["liveSession"] = liveSettingsToJson(settings);
    if (!dimensionId.empty()) m.extra["liveSession"]["dimension"] = dimensionId;
    return m;
}

SessionRestore readSessionProject(const ProjectManifest& m, const LiveSettings& base) {
    SessionRestore r;
    const Json live = m.extra.value("liveSession", Json::object());
    r.settings = liveSettingsFromJson(live, base);
    r.dimensionId = pick<std::string>(live, "dimension", "");
    for (const auto& l : m.layers) {
        if (l.bus != Bus::Music) continue;
        const SourceRef* s = m.findSource(l.sourceId);
        if (!s || s->storage != SourceStorage::Packaged) continue;
        if (s->kind == SourceKind::Stem)
            r.stems.push_back({SessionTrack{s->path, s->displayName, s->media}, std::clamp(l.gainDb, -60.0f, 6.0f), l.muted});
        else if (!r.track)
            r.track = SessionTrack{s->path, s->displayName, s->media};
    }
    return r;
}

} // namespace atmos::core
