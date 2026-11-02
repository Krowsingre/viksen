#include "core/worlds/WorldFeeder.h"

#include <algorithm>
#include <chrono>

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
                         int64_t loopStart, int64_t loopEnd)
    : player_(player), decoders_(std::move(decoders)) {
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

int64_t WorldFeeder::playbackFrame() const {
    const int64_t span = loopEnd_ - loopStart_;
    if (span <= 0) return loopStart_;
    const int64_t played = static_cast<int64_t>(player_.framesPlayed());
    return loopStart_ + ((startFrame_.load() - loopStart_ + played) % span);
}

void WorldFeeder::run() {
    constexpr int kChunk = 4096;
    std::vector<float> buf(static_cast<size_t>(kChunk) * 2);
    int64_t pos = startFrame_.load();
    int64_t buffered = 0;
    const int64_t activateAfter = static_cast<int64_t>(rate_ * 0.5);
    while (running_.load()) {
        if (player_.writableFrames() < kChunk) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        const int n = static_cast<int>(std::min<int64_t>(kChunk, loopEnd_ - pos));
        for (size_t s = 0; s < decoders_.size(); ++s) {
            const int got = decoders_[s]->read(buf.data(), n);
            if (got < n) std::fill(buf.begin() + 2 * std::max(0, got), buf.begin() + 2 * n, 0.0f);
            player_.write(static_cast<int>(s), buf.data(), n);
        }
        pos += n;
        buffered += n;
        if (pos >= loopEnd_) {
            pos = loopStart_;
            for (auto& d : decoders_) d->seek(pos);
        }
        if (!player_.active() && buffered >= activateAfter) player_.setActive(true);
    }
}

} // namespace atmos::core
