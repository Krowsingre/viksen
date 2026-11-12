#pragma once
// Moon-event secrets (owner, 2026-11-11): a Dimension's secret for an event is found
// after 3 minutes of audible listening to that Dimension while the event is on where the
// listener is (MoonEvents.h). Found once per Dimension and kept forever. The minutes need
// not be continuous; they reset once the event has been off for two days, so time from
// an earlier occurrence does not carry over. Control thread.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/progress/MoonEvents.h"

namespace atmos::core {

struct SecretDiscovery {
    std::string dimensionId;
    MoonEvent event;
};

class SecretTracker {
public:
    static constexpr double kQualifyingSeconds = 180.0;

    void markFound(const std::string& dimensionId, MoonEvent e) { found_.insert({dimensionId, static_cast<int>(e)}); }
    bool isFound(const std::string& dimensionId, MoonEvent e) const { return found_.count({dimensionId, static_cast<int>(e)}) > 0; }

    // Call about once a second with the events on now (activeMoonEvents). Returns the first
    // secret found by this tick, if any.
    std::optional<SecretDiscovery> update(double nowDaysUtc, double dtSeconds, const std::string& dimensionId,
                                          const std::vector<MoonEvent>& active, bool audible);

private:
    std::set<std::pair<std::string, int>> found_;
    struct Progress { double seconds = 0; double lastOnDays = -1e18; };
    std::map<std::pair<std::string, int>, Progress> progress_;
};

} // namespace atmos::core
