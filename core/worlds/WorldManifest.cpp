#include "core/worlds/WorldManifest.h"

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
        }
        for (const auto& s : j.value("stems", Json::array())) {
            WorldStem st;
            st.id = s.value("id", std::string{});
            st.file = s.value("file", std::string{});
            st.title = s.value("title", st.id);
            st.baseGainDb = s.value("baseGainDb", 0.0f);
            if (st.id.empty() || st.file.empty()) return fail("stem needs id and file");
            if (st.file.find("..") != std::string::npos || st.file.front() == '/') return fail("stem file escapes the World folder");
            w.stems.push_back(st);
        }
        if (w.id.empty() || w.stems.empty()) return fail("World needs an id and at least one stem");
        if (w.stems.size() > 8) return fail("at most 8 stems");
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
        if (j.contains("unlock")) {
            auto u = parseConditions(j["unlock"], error);
            if (!u) return std::nullopt;
            w.unlock = *u;
        }
        w.extra = j;
        for (const char* k : {"format", "schemaVersion", "id", "name", "artist", "tier", "fadeSeconds", "music", "stems", "rules", "unlock"})
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
    j["music"] = {{"tempo", w.tempo}, {"key", w.key}, {"loop", loop}};
    Json stems = Json::array();
    for (const auto& s : w.stems) stems.push_back({{"id", s.id}, {"file", s.file}, {"title", s.title}, {"baseGainDb", s.baseGainDb}});
    j["stems"] = stems;
    Json rules = Json::array();
    for (const auto& r : w.rules)
        rules.push_back({{"stem", r.stem}, {"when", conditionsToJson(r.when)}, {"gainDb", r.gainDb},
                         {"otherwiseDb", r.otherwiseDb}, {"label", r.label}});
    j["rules"] = rules;
    if (!w.unlock.all.empty() || w.unlock.hasUnknown()) j["unlock"] = conditionsToJson(w.unlock);
    return j;
}

} // namespace atmos::core
