#pragma once
// Adaptive arrangement: a Dimension's song is divided into sections (time ranges
// across all stems). The listener's state chooses which sections are acceptable;
// the stream jumps between them on the musical grid (every few bars), with a short
// crossfade, so different situations play different parts of the song — not just a
// re-balanced mix of the same part. With no reactive input it plays as composed.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "core/worlds/WorldConditions.h"

namespace atmos::core {

struct WorldSection {
    std::string id, title;
    double fromSeconds = 0, toSeconds = 0;
    ConditionSet when;     // the state this section belongs to (empty = a neutral bridge)
    ConditionSet prefer;   // optional preference (time of day, moon) among acceptable sections
    std::string label;     // why it is playing, for the UI ("You are moving")
};

// Which sections suit the current context. Control thread.
struct SectionChoice {
    uint64_t acceptable = 0;  // bit i = section i may play
    uint64_t preferred = 0;   // subset of acceptable favoured by `prefer`
    bool composed = true;     // no reactive section matched: play the song as written
};
SectionChoice chooseSections(const std::vector<WorldSection>& sections, const ConditionContext& c);

// Decides, at grid boundaries, where the stream continues. The feeder thread calls
// nextBoundary()/decide(); the control thread calls setChoice(). Lock-free between them.
class SectionPlanner {
public:
    static constexpr int kMaxSections = 64;

    // Frames at the stream rate. `gridFrames`: spacing of possible jump points.
    SectionPlanner(std::vector<std::pair<int64_t, int64_t>> sectionFrames, int64_t gridFrames,
                   int64_t loopStart, int64_t loopEnd, int64_t minStayFrames);

    void setChoice(const SectionChoice& c);
    // Multi-song Dimensions: at the end of the song (as composed) stop instead of
    // looping, so the next song can follow. decide() then returns kEnd.
    static constexpr int64_t kEnd = -1;
    void setStopAtSongEnd(bool on) { stopAtEnd_.store(on, std::memory_order_relaxed); }
    // First boundary strictly after `pos` (grid point, section end or loop end).
    int64_t nextBoundary(int64_t pos) const;
    // Where to continue when the stream reaches boundary `b` (b itself = carry on).
    int64_t decide(int64_t b);
    // Index of the section containing `frame`, or -1.
    int sectionAt(int64_t frame) const;
    int sectionCount() const { return static_cast<int>(sections_.size()); }

private:
    int pickTarget(int current, uint64_t acceptable, uint64_t preferred) const;

    std::vector<std::pair<int64_t, int64_t>> sections_;  // [start, end) sorted by start
    int64_t grid_, loopStart_, loopEnd_, minStay_;
    std::atomic<uint64_t> acceptable_{~0ull}, preferred_{0};
    std::atomic<bool> composed_{true};
    std::atomic<bool> stopAtEnd_{false};
    int64_t enteredAt_ = -1;   // feeder thread only: source frame where the current section was entered
    int lastSection_ = -1;
};

} // namespace atmos::core
