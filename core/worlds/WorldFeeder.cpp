#include "core/worlds/WorldFeeder.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace atmos::core {

int ClipStemDecoder::read(float* lr, int frames) {
    const int64_t n = std::min<int64_t>(frames, lengthFrames() - pos_);
    if (n <= 0) return 0;
    const float* l = clip_->samples.channel(0);
    const float* r = clip_->numChannels() > 1 ? clip_->samples.channel(1) : l;
    for (int64_t i = 0; i < n; ++i) { lr[2 * i] = l[pos_ + i]; lr[2 * i + 1] = r[pos_ + i]; }
    pos_ += n;
    return static_cast<int>(n);
}

WorldFeeder::WorldFeeder(WorldStreamPlayer& player, std::vector<std::unique_ptr<StemDecoder>> decoders,
                         int64_t loopStart, int64_t loopEnd, std::shared_ptr<SectionPlanner> planner,
                         int64_t crossfadeFrames)
    : player_(player), decoders_(std::move(decoders)), planner_(std::move(planner)),
      crossfade_(std::max<int64_t>(0, crossfadeFrames)) {
    // Stems may differ in length (e.g. a stem without a tail): play to the end of
    // the longest; shorter stems simply run out into silence.
    int64_t len = 0;
    for (auto& d : decoders_) len = std::max(len, d->lengthFrames());
    if (!decoders_.empty()) rate_ = decoders_.front()->outputRate();
    loopEnd_ = std::clamp<int64_t>(loopEnd <= 0 ? len : loopEnd, 1, std::max<int64_t>(1, len));
    loopStart_ = std::clamp<int64_t>(loopStart, 0, loopEnd_ - 1);
    player_.setStemCount(static_cast<int>(decoders_.size()));
}

WorldFeeder::~WorldFeeder() { stop(); }

void WorldFeeder::start(int64_t frame) {
    stop();
    frame = (frame < loopStart_ || frame >= loopEnd_) ? loopStart_ : frame;
    for (auto& d : decoders_) d->seek(frame);
    startFrame_.store(frame);
    {
        std::lock_guard<std::mutex> lock(segMutex_);
        segments_.assign(1, {0, frame});
    }
    endWritten_.store(UINT64_MAX);
    player_.setStemCount(static_cast<int>(decoders_.size()));
    player_.requestFlush();       // feeder thread is not running: safe producer-side call
    running_.store(true);
    thread_ = std::thread([this] { run(); });
}

void WorldFeeder::stop() {
    player_.setActive(false);
    running_.store(false);
    if (thread_.joinable()) thread_.join();
}

void WorldFeeder::noteSegment(uint64_t written, int64_t source) {
    std::lock_guard<std::mutex> lock(segMutex_);
    // Drop runs the player has long finished (keep the list short).
    const uint64_t played = player_.framesPlayed();
    while (segments_.size() > 2 && segments_[1].first <= played) segments_.erase(segments_.begin());
    segments_.emplace_back(written, source);
}

int64_t WorldFeeder::playbackFrame() const {
    const uint64_t played = player_.framesPlayed();
    std::lock_guard<std::mutex> lock(segMutex_);
    if (segments_.empty()) return startFrame_.load();
    auto seg = segments_.front();
    for (const auto& s : segments_) {
        if (s.first > played) break;
        seg = s;
    }
    return std::clamp<int64_t>(seg.second + static_cast<int64_t>(played - seg.first), loopStart_, loopEnd_ - 1);
}

bool WorldFeeder::finishedPlaying() const {
    const uint64_t end = endWritten_.load();
    return end != UINT64_MAX && player_.framesPlayed() >= end;
}

void WorldFeeder::run() {
    constexpr int kChunk = 4096;
    const size_t stems = decoders_.size();
    std::vector<float> buf(static_cast<size_t>(kChunk) * 2);
    // Crossfade scratch: the outgoing tail of every stem.
    std::vector<std::vector<float>> tail(stems, std::vector<float>(static_cast<size_t>(std::max<int64_t>(crossfade_, 1)) * 2));
    std::vector<float> incoming(static_cast<size_t>(std::max<int64_t>(crossfade_, 1)) * 2);
    int64_t pos = startFrame_.load();
    uint64_t written = 0;
    int64_t buffered = 0;
    const int64_t activateAfter = static_cast<int64_t>(rate_ * 0.5);
    auto boundaryAfter = [&](int64_t p) { return planner_ ? std::min(planner_->nextBoundary(p), loopEnd_) : loopEnd_; };
    int64_t boundary = boundaryAfter(pos);
    auto waitForRoom = [&](int frames) {
        while (running_.load() && player_.writableFrames() < frames) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return running_.load();
    };
    while (running_.load()) {
        if (!waitForRoom(kChunk)) break;
        const int n = static_cast<int>(std::min<int64_t>(kChunk, boundary - pos));
        for (size_t s = 0; s < stems; ++s) {
            const int got = decoders_[s]->read(buf.data(), n);
            if (got < n) std::fill(buf.begin() + 2 * std::max(0, got), buf.begin() + 2 * n, 0.0f);
            player_.write(static_cast<int>(s), buf.data(), n);
        }
        pos += n;
        written += static_cast<uint64_t>(n);
        buffered += n;
        if (!player_.active() && buffered >= activateAfter) player_.setActive(true);
        if (pos < boundary) continue;

        // ---- boundary: carry on, or jump (crossfaded) to another part of the song.
        const int64_t to = planner_ ? planner_->decide(pos) : (pos >= loopEnd_ ? loopStart_ : pos);
        if (to == SectionPlanner::kEnd) {
            // End of the song: keep the player fed with silence until the session moves on.
            endWritten_.store(written);
            std::fill(buf.begin(), buf.end(), 0.0f);
            while (running_.load()) {
                if (!waitForRoom(kChunk)) break;
                for (size_t s = 0; s < stems; ++s) player_.write(static_cast<int>(s), buf.data(), kChunk);
            }
            break;
        }
        if (to == pos && pos < loopEnd_) {
            boundary = boundaryAfter(pos);
            continue;
        }
        const int64_t target = std::clamp<int64_t>(to, loopStart_, loopEnd_ - 1);
        const int64_t xf = planner_ ? std::min<int64_t>(crossfade_, loopEnd_ - target) : 0;
        if (xf > 0 && waitForRoom(static_cast<int>(xf))) {
            // Equal-power crossfade: the outgoing part keeps playing past the boundary
            // (its natural tail) while the new part fades in.
            for (size_t s = 0; s < stems; ++s) {
                const int got = decoders_[s]->read(tail[s].data(), static_cast<int>(xf));
                std::fill(tail[s].begin() + 2 * std::max(0, got), tail[s].begin() + 2 * xf, 0.0f);
                decoders_[s]->seek(target);
                const int got2 = decoders_[s]->read(incoming.data(), static_cast<int>(xf));
                std::fill(incoming.begin() + 2 * std::max(0, got2), incoming.begin() + 2 * xf, 0.0f);
                for (int64_t i = 0; i < xf; ++i) {
                    const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(xf);
                    const float gOut = std::cos(t * 1.5707963f), gIn = std::sin(t * 1.5707963f);
                    for (int c = 0; c < 2; ++c)
                        incoming[static_cast<size_t>(2 * i + c)] = tail[s][static_cast<size_t>(2 * i + c)] * gOut +
                                                                   incoming[static_cast<size_t>(2 * i + c)] * gIn;
                }
                player_.write(static_cast<int>(s), incoming.data(), static_cast<int>(xf));
            }
            noteSegment(written, target);          // heard from here on: the new part
            written += static_cast<uint64_t>(xf);
            pos = target + xf;
        } else {
            for (auto& d : decoders_) d->seek(target);
            noteSegment(written, target);
            pos = target;
        }
        jumps_.fetch_add(1);
        boundary = boundaryAfter(pos);
    }
}

} // namespace atmos::core
