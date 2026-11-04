#include "core/progress/ProgressLog.h"

#include <algorithm>

namespace atmos::core {
namespace {
constexpr const char* kKindIds[] = {"phase", "secret", "globalSecret", "song", "mood"};
}

const char* progressKindId(ProgressKind k) { return kKindIds[static_cast<int>(k)]; }

std::optional<ProgressKind> progressKindFromId(const std::string& id) {
    for (int i = 0; i < 5; ++i)
        if (id == kKindIds[i]) return static_cast<ProgressKind>(i);
    return std::nullopt;
}

std::string ProgressEvent::id() const {
    return std::string(progressKindId(kind)) + ":" + (kind == ProgressKind::GlobalSecret ? "" : dimensionId) + ":" + subject;
}

bool ProgressLog::add(const ProgressEvent& e) {
    if (e.subject.empty() || (e.kind != ProgressKind::GlobalSecret && e.dimensionId.empty())) return false;
    ProgressEvent clean = e;
    if (clean.kind == ProgressKind::GlobalSecret) clean.dimensionId.clear();   // global namespace
    const auto [it, inserted] = events_.emplace(clean.id(), clean);
    if (!inserted) it->second.atDaysUtc = std::min(it->second.atDaysUtc, clean.atDaysUtc);
    return inserted;
}

int ProgressLog::merge(const ProgressLog& other) {
    int added = 0;
    for (const auto& [id, e] : other.events_) added += add(e) ? 1 : 0;
    return added;
}

std::set<MoonPhaseName> ProgressLog::discoveredPhases(const std::string& dim) const {
    std::set<MoonPhaseName> out;
    for (const auto& [id, e] : events_)
        if (e.kind == ProgressKind::PhaseDiscovered && e.dimensionId == dim)
            if (auto p = moonPhaseFromId(e.subject)) out.insert(*p);
    return out;
}

bool ProgressLog::lunarJourneyComplete(const std::string& dim) const {
    return static_cast<int>(discoveredPhases(dim).size()) == kPhasesPerJourney;
}

std::optional<double> ProgressLog::lunarJourneyCompletedAt(const std::string& dim) const {
    if (!lunarJourneyComplete(dim)) return std::nullopt;
    double latest = 0;
    for (const auto& [id, e] : events_)
        if (e.kind == ProgressKind::PhaseDiscovered && e.dimensionId == dim) latest = std::max(latest, e.atDaysUtc);
    return latest;
}

int ProgressLog::dimensionSecretsFound(const std::string& dim) const {
    int n = 0;
    for (const auto& [id, e] : events_) n += (e.kind == ProgressKind::DimensionSecret && e.dimensionId == dim) ? 1 : 0;
    return std::min(n, kSecretsPerDimension);
}

bool ProgressLog::allDimensionSecretsFound(const std::string& dim) const {
    return dimensionSecretsFound(dim) == kSecretsPerDimension;
}

int ProgressLog::globalSecretsFound() const {
    int n = 0;
    for (const auto& [id, e] : events_) n += e.kind == ProgressKind::GlobalSecret ? 1 : 0;
    return n;
}

std::vector<ProgressEvent> ProgressLog::events() const {
    std::vector<ProgressEvent> v;
    for (const auto& [id, e] : events_) v.push_back(e);
    std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.atDaysUtc < b.atDaysUtc; });
    return v;
}

Json ProgressLog::toJson() const {
    Json list = Json::array();
    for (const auto& e : events())
        list.push_back({{"kind", progressKindId(e.kind)}, {"dimension", e.dimensionId}, {"subject", e.subject},
                        {"at", e.atDaysUtc}});
    return {{"format", "viksen.progress"}, {"version", 1}, {"events", list}};
}

ProgressLog ProgressLog::fromJson(const Json& j) {
    ProgressLog log;
    if (!j.is_object() || !j.contains("events") || !j["events"].is_array()) return log;
    for (const auto& x : j["events"]) {
        if (!x.is_object()) continue;
        const auto kind = progressKindFromId(x.value("kind", std::string{}));
        if (!kind) continue;
        ProgressEvent e{*kind, x.value("dimension", std::string{}), x.value("subject", std::string{}),
                        x.value("at", 0.0)};
        if (*kind == ProgressKind::PhaseDiscovered && !moonPhaseFromId(e.subject)) continue;
        log.add(e);
    }
    return log;
}

} // namespace atmos::core
