#include "core/worlds/WorldSections.h"

#include <algorithm>

namespace atmos::core {

SectionChoice chooseSections(const std::vector<WorldSection>& sections, const ConditionContext& c) {
    SectionChoice r;
    const size_t n = std::min<size_t>(sections.size(), SectionPlanner::kMaxSections);
    uint64_t matched = 0;
    for (size_t i = 0; i < n; ++i) {
        const auto& s = sections[i];
        if (s.when.all.empty() || s.when.hasUnknown() || !s.when.inputsAvailable(c)) continue;
        if (s.when.evaluate(c)) matched |= 1ull << i;
    }
    if (matched == 0) {
        r.acceptable = n >= 64 ? ~0ull : ((1ull << n) - 1);   // as composed
        r.composed = true;
    } else {
        r.acceptable = matched;
        r.composed = false;
    }
    for (size_t i = 0; i < n; ++i)
        if ((r.acceptable >> i & 1) && !sections[i].prefer.all.empty() && !sections[i].prefer.hasUnknown() &&
            sections[i].prefer.evaluate(c))
            r.preferred |= 1ull << i;
    return r;
}

SectionPlanner::SectionPlanner(std::vector<std::pair<int64_t, int64_t>> sectionFrames, int64_t gridFrames,
                               int64_t loopStart, int64_t loopEnd, int64_t minStayFrames)
    : sections_(std::move(sectionFrames)), grid_(std::max<int64_t>(1, gridFrames)), loopStart_(loopStart),
      loopEnd_(loopEnd), minStay_(minStayFrames) {
    for (auto& s : sections_) {
        s.first = std::clamp(s.first, loopStart_, loopEnd_);
        s.second = std::clamp(s.second, s.first, loopEnd_);
    }
    std::stable_sort(sections_.begin(), sections_.end());
    sections_.erase(std::remove_if(sections_.begin(), sections_.end(), [](auto& s) { return s.second <= s.first; }),
                    sections_.end());
    if (sections_.size() > static_cast<size_t>(kMaxSections)) sections_.resize(kMaxSections);
}

void SectionPlanner::setChoice(const SectionChoice& c) {
    acceptable_.store(c.acceptable, std::memory_order_relaxed);
    preferred_.store(c.preferred, std::memory_order_relaxed);
    composed_.store(c.composed, std::memory_order_relaxed);
}

int SectionPlanner::sectionAt(int64_t f) const {
    for (int i = 0; i < static_cast<int>(sections_.size()); ++i)
        if (f >= sections_[static_cast<size_t>(i)].first && f < sections_[static_cast<size_t>(i)].second) return i;
    return -1;
}

int64_t SectionPlanner::nextBoundary(int64_t pos) const {
    int64_t best = loopEnd_;
    // Grid points are measured from the song start (bar 1 at loopStart).
    const int64_t g = loopStart_ + ((pos - loopStart_) / grid_ + 1) * grid_;
    if (g > pos) best = std::min(best, g);
    for (const auto& s : sections_) {
        if (s.first > pos) best = std::min(best, s.first);
        if (s.second > pos) best = std::min(best, s.second);
    }
    return std::max(best, pos + 1);
}

// Next acceptable section after `current` in song order (cycling); preferred ones first.
int SectionPlanner::pickTarget(int current, uint64_t acceptable, uint64_t preferred) const {
    const int n = static_cast<int>(sections_.size());
    for (const uint64_t pool : {preferred & acceptable, acceptable}) {
        if (pool == 0) continue;
        for (int k = 1; k <= n; ++k) {
            const int i = ((current < 0 ? -1 : current) + k + n) % n;
            if (pool >> i & 1) return i;
        }
    }
    return -1;
}

int64_t SectionPlanner::decide(int64_t b) {
    const int cur = sectionAt(b - 1);
    if (cur != lastSection_) { lastSection_ = cur; enteredAt_ = cur >= 0 ? sections_[static_cast<size_t>(cur)].first : b; }
    const uint64_t acc = acceptable_.load(std::memory_order_relaxed);
    const uint64_t pref = preferred_.load(std::memory_order_relaxed);
    const bool composed = composed_.load(std::memory_order_relaxed);
    auto enter = [&](int i) {
        lastSection_ = i;
        enteredAt_ = sections_[static_cast<size_t>(i)].first;
        return sections_[static_cast<size_t>(i)].first;
    };

    const bool atSongEnd = b >= loopEnd_;
    const bool atSectionEnd = atSongEnd || (cur >= 0 && b >= sections_[static_cast<size_t>(cur)].second);
    if (sections_.empty() || composed) {
        if (!atSongEnd) return b;
        lastSection_ = -1;
        if (stopAtEnd_.load(std::memory_order_relaxed)) return kEnd;
        return loopStart_;                     // as composed: play through, then from the top
    }
    const bool curOk = cur >= 0 && (acc >> cur & 1);
    if (atSectionEnd) {
        const int next = atSongEnd ? -1 : sectionAt(b);
        // A preferred section waiting elsewhere wins; otherwise flow on in song order when the
        // next part also suits, else move to another suitable part (or repeat this one).
        const bool prefElsewhere = (pref & acc & ~(cur >= 0 ? 1ull << cur : 0)) != 0 && !(next >= 0 && (pref >> next & 1));
        if (!prefElsewhere && next >= 0 && (acc >> next & 1)) return b;
        const int t = pickTarget(cur, acc, pref);
        if (t >= 0) return enter(t);
        return atSongEnd ? loopStart_ : b;
    }
    // Mid-section grid point: leave only if this part no longer suits and it has played a while.
    if (curOk || b - enteredAt_ < minStay_) return b;
    const int t = pickTarget(cur, acc, pref);
    return t >= 0 && t != cur ? enter(t) : b;
}

} // namespace atmos::core
