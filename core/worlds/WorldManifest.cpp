#include "core/worlds/WorldManifest.h"

#include <algorithm>

#include "engine/audio/WorldStreamPlayer.h"

namespace atmos::core {

int WorldManifest::stemIndex(const std::string& sid) const {
    for (size_t i = 0; i < stems.size(); ++i)
        if (stems[i].id == sid) return static_cast<int>(i);
    return -1;
}

std::optional<WorldManifest> parseWorld(const Json& j, std::string* error) {
    auto fail = [&](const std::string& m) -> std::optional<WorldManifest> { if (error) *error = m; return std::nullopt; };
    try {
        if (!j.is_object() || j.value("format", std::string{}) != "atmos.world") return fail("not a World manifest");
        WorldManifest w;
        w.id = j.value("id", std::string{});
        w.name = j.value("name", std::string{});
        w.artist = j.value("artist", std::string{});
        w.tier = j.value("tier", std::string{"free"});
        w.fadeSeconds = j.value("fadeSeconds", 4.0f);
        if (j.contains("music") && j["music"].is_object()) {
            const Json& m = j["music"];
            w.tempo = m.value("tempo", 120.0);
            w.key = m.value("key", std::string{});
            if (m.contains("loop") && m["loop"].is_object()) {
                w.loopFromSeconds = m["loop"].value("fromSeconds", 0.0);
                if (m["loop"].contains("toSeconds") && m["loop"]["toSeconds"].is_number())
                    w.loopToSeconds = m["loop"]["toSeconds"].get<double>();
            }
            w.beatsPerBar = std::clamp(m.value("beatsPerBar", 4), 1, 16);
            w.barOffsetSeconds = std::max(0.0, m.value("barOffsetSeconds", 0.0));
        }
        for (const auto& s : j.value("stems", Json::array())) {
            WorldStem st;
            st.id = s.value("id", std::string{});
            st.file = s.value("file", std::string{});
            st.title = s.value("title", st.id);
            st.baseGainDb = s.value("baseGainDb", 0.0f);
            if (s.contains("spatial") && s["spatial"].is_object()) {
                const Json& sp = s["spatial"];
                st.azimuthDeg = sp.value("azimuth", 0.0f);
                st.elevationDeg = std::clamp(sp.value("elevation", 0.0f), -90.0f, 90.0f);
                st.distanceM = std::clamp(sp.value("distance", 1.5f), 0.25f, 20.0f);
                st.widthDeg = std::clamp(sp.value("width", 60.0f), 0.0f, 180.0f);
            }
            if (st.id.empty() || st.file.empty()) return fail("stem needs id and file");
            if (st.file.find("..") != std::string::npos || st.file.front() == '/') return fail("stem file escapes the World folder");
            w.stems.push_back(st);
        }
        if (w.id.empty() || w.stems.empty()) return fail("World needs an id and at least one stem");
        if (w.stems.size() > static_cast<size_t>(WorldStreamPlayer::kMaxStems)) return fail("at most 16 stems");
        for (const auto& r : j.value("rules", Json::array())) {
            WorldRule rule;
            rule.stem = r.value("stem", std::string{});
            if (w.stemIndex(rule.stem) < 0) return fail("rule refers to unknown stem " + rule.stem);
            auto when = parseConditions(r.value("when", Json(nullptr)), error);
            if (!when) return std::nullopt;
            rule.when = *when;
            rule.gainDb = r.value("gainDb", 0.0f);
            rule.otherwiseDb = r.value("otherwiseDb", -60.0f);
            rule.label = r.value("label", std::string{});
            w.rules.push_back(rule);
        }
        if (j.contains("adaptive") && j["adaptive"].is_object()) {
            const Json& a = j["adaptive"];
            w.jumpEveryBars = std::clamp(a.value("jumpEveryBars", 4), 1, 64);
            w.minStayBars = std::clamp(a.value("minStayBars", 8.0), 0.0, 256.0);
            w.crossfadeBeats = std::clamp(a.value("crossfadeBeats", 1.0), 0.0, 8.0);
        }
        const double barSeconds = 60.0 / std::max(1.0, w.tempo) * w.beatsPerBar;
        for (const auto& sj : j.value("sections", Json::array())) {
            WorldSection sec;
            sec.id = sj.value("id", std::string{});
            sec.title = sj.value("title", sec.id);
            sec.label = sj.value("label", std::string{});
            if (sj.contains("fromBar")) {
                sec.fromSeconds = w.barOffsetSeconds + sj["fromBar"].get<double>() * barSeconds;
                sec.toSeconds = w.barOffsetSeconds + sj.value("toBar", 0.0) * barSeconds;
            } else {
                sec.fromSeconds = sj.value("fromSeconds", 0.0);
                sec.toSeconds = sj.value("toSeconds", 0.0);
            }
            if (sec.id.empty() || sec.toSeconds <= sec.fromSeconds) return fail("section needs an id and a positive length");
            auto when = parseConditions(sj.value("when", Json(nullptr)), error);
            if (!when) return std::nullopt;
            sec.when = *when;
            auto pref = parseConditions(sj.value("prefer", Json(nullptr)), error);
            if (!pref) return std::nullopt;
            sec.prefer = *pref;
            w.sections.push_back(std::move(sec));
        }
        if (w.sections.size() > static_cast<size_t>(SectionPlanner::kMaxSections)) return fail("at most 64 sections");
        if (j.contains("unlock")) {
            auto u = parseConditions(j["unlock"], error);
            if (!u) return std::nullopt;
            w.unlock = *u;
        }
        w.extra = j;
        for (const char* k : {"format", "schemaVersion", "id", "name", "artist", "tier", "fadeSeconds", "music", "stems", "rules", "unlock", "adaptive", "sections"})
            w.extra.erase(k);
        return w;
    } catch (const std::exception& e) {
        return fail(std::string("malformed World: ") + e.what());
    }
}

Json worldToJson(const WorldManifest& w) {
    Json j = w.extra;
    j["format"] = "atmos.world";
    j["schemaVersion"] = 1;
    j["id"] = w.id; j["name"] = w.name; j["artist"] = w.artist; j["tier"] = w.tier;
    j["fadeSeconds"] = w.fadeSeconds;
    Json loop = {{"fromSeconds", w.loopFromSeconds}};
    loop["toSeconds"] = w.loopToSeconds ? Json(*w.loopToSeconds) : Json(nullptr);
    j["music"] = {{"tempo", w.tempo}, {"key", w.key}, {"loop", loop}, {"beatsPerBar", w.beatsPerBar},
                  {"barOffsetSeconds", w.barOffsetSeconds}};
    j["adaptive"] = {{"jumpEveryBars", w.jumpEveryBars}, {"minStayBars", w.minStayBars}, {"crossfadeBeats", w.crossfadeBeats}};
    Json sections = Json::array();
    for (const auto& sec : w.sections) {
        Json o = {{"id", sec.id}, {"title", sec.title}, {"fromSeconds", sec.fromSeconds}, {"toSeconds", sec.toSeconds},
                  {"label", sec.label}};
        if (!sec.when.all.empty() || sec.when.hasUnknown()) o["when"] = conditionsToJson(sec.when);
        if (!sec.prefer.all.empty() || sec.prefer.hasUnknown()) o["prefer"] = conditionsToJson(sec.prefer);
        sections.push_back(o);
    }
    if (!sections.empty()) j["sections"] = sections;
    Json stems = Json::array();
    for (const auto& s : w.stems)
        stems.push_back({{"id", s.id}, {"file", s.file}, {"title", s.title}, {"baseGainDb", s.baseGainDb},
                         {"spatial", {{"azimuth", s.azimuthDeg}, {"elevation", s.elevationDeg},
                                      {"distance", s.distanceM}, {"width", s.widthDeg}}}});
    j["stems"] = stems;
    Json rules = Json::array();
    for (const auto& r : w.rules)
        rules.push_back({{"stem", r.stem}, {"when", conditionsToJson(r.when)}, {"gainDb", r.gainDb},
                         {"otherwiseDb", r.otherwiseDb}, {"label", r.label}});
    j["rules"] = rules;
    if (!w.unlock.all.empty() || w.unlock.hasUnknown()) j["unlock"] = conditionsToJson(w.unlock);
    return j;
}

std::optional<DimensionManifest> parseDimension(const Json& j, std::string* error) {
    auto fail = [&](const std::string& m) -> std::optional<DimensionManifest> { if (error) *error = m; return std::nullopt; };
    if (!j.is_object() || j.value("format", std::string{}) != "atmos.world") return fail("not a Dimension manifest");
    DimensionManifest d;
    d.id = j.value("id", std::string{});
    d.name = j.value("name", std::string{});
    d.artist = j.value("artist", std::string{});
    d.tier = j.value("tier", std::string{"free"});
    if (d.id.empty()) return fail("Dimension needs an id");
    if (!j.contains("songs")) {
        auto w = parseWorld(j, error);
        if (!w) return std::nullopt;
        d.unlock = w->unlock;
        d.songs.push_back(std::move(*w));
        return d;
    }
    if (!j["songs"].is_array() || j["songs"].empty()) return fail("songs must be a non-empty list");
    if (j["songs"].size() > 8) return fail("at most 8 songs in a Dimension");
    if (j.contains("unlock")) {
        auto u = parseConditions(j["unlock"], error);
        if (!u) return std::nullopt;
        d.unlock = *u;
    }
    for (const auto& sj : j["songs"]) {
        if (!sj.is_object()) return fail("each song must be an object");
        Json song = sj;
        const std::string sid = sj.value("id", std::string{});
        if (sid.empty()) return fail("each song needs an id");
        song["format"] = "atmos.world";
        song["id"] = d.id + "/" + sid;
        song["name"] = sj.value("title", sid);
        if (!song.contains("artist")) song["artist"] = d.artist;
        song["tier"] = d.tier;
        song.erase("title");
        std::string err;
        auto w = parseWorld(song, &err);
        if (!w) return fail("song " + sid + ": " + err);
        for (const auto& other : d.songs)
            if (other.id == w->id) return fail("duplicate song id " + sid);
        d.songs.push_back(std::move(*w));
    }
    return d;
}

} // namespace atmos::core
