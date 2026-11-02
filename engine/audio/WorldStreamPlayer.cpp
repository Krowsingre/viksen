#include "engine/audio/WorldStreamPlayer.h"

#include <algorithm>
#include <cmath>

#include "engine/spatial/BinauralFactory.h"

namespace atmos {

void WorldStreamPlayer::prepare(double sampleRate, int maxBlockSize, int ringFrames) {
    sampleRate_ = sampleRate;
    scratchL_.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    scratchR_.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    mixL_.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    spatialOut_.setSize(2, maxBlockSize);
    spatialMix_.prepare(sampleRate, 0.15);
    spatialMix_.resetTo(spatialOn_.load() ? 1.0f : 0.0f);
    int cap = 1024;
    while (cap < ringFrames) cap <<= 1;
    capacity_ = cap;
    for (auto& s : stems_) {
        s.ring.assign(static_cast<size_t>(cap) * 2, 0.0f);
        s.written.store(0);
        s.read.store(0);
        s.flushTo.store(0);
        s.gain.prepare(sampleRate, 0.05);
        s.peak.store(0.0f);
        s.left = makeBinauralRenderer(sampleRate, maxBlockSize);
        s.right = makeBinauralRenderer(sampleRate, maxBlockSize);
    }
    hrtf_ = stems_[0].left->isBinauralHrtf();
    spatialLatency_ = stems_[0].left->latencySamples();
    flushRequests_.store(0);
    flushesSeen_ = 0;
    underruns_.store(0);
    played_.store(0);
}

void WorldStreamPlayer::setStemGain(int stem, float g) noexcept {
    if (stem >= 0 && stem < kMaxStems) stems_[static_cast<size_t>(stem)].gain.set(std::max(0.0f, g));
}

void WorldStreamPlayer::setStemPosition(int stem, float az, float el, float dist, float width) noexcept {
    if (stem < 0 || stem >= kMaxStems) return;
    Stem& s = stems_[static_cast<size_t>(stem)];
    s.az.store(az, std::memory_order_relaxed);
    s.el.store(el, std::memory_order_relaxed);
    s.dist.store(dist, std::memory_order_relaxed);
    s.width.store(width, std::memory_order_relaxed);
}

float WorldStreamPlayer::stemPeak(int stem) const noexcept {
    return stem >= 0 && stem < kMaxStems ? stems_[static_cast<size_t>(stem)].peak.load(std::memory_order_relaxed) : 0.0f;
}

int WorldStreamPlayer::writableFrames() const noexcept {
    const int n = stemCount_.load();
    if (n == 0 || capacity_ == 0) return 0;
    uint64_t freeMin = static_cast<uint64_t>(capacity_);
    for (int i = 0; i < n; ++i) {
        const Stem& s = stems_[static_cast<size_t>(i)];
        const uint64_t used = s.written.load(std::memory_order_relaxed) - s.read.load(std::memory_order_acquire);
        freeMin = std::min(freeMin, static_cast<uint64_t>(capacity_) - std::min<uint64_t>(used, static_cast<uint64_t>(capacity_)));
    }
    return static_cast<int>(freeMin);
}

int WorldStreamPlayer::readableFrames() const noexcept {
    const int n = stemCount_.load();
    if (n == 0) return 0;
    uint64_t m = UINT64_MAX;
    for (int i = 0; i < n; ++i) {
        const Stem& s = stems_[static_cast<size_t>(i)];
        m = std::min(m, s.written.load(std::memory_order_acquire) - s.read.load(std::memory_order_acquire));
    }
    return static_cast<int>(std::min<uint64_t>(m, static_cast<uint64_t>(capacity_)));
}

void WorldStreamPlayer::write(int stem, const float* lr, int frames) noexcept {
    if (stem < 0 || stem >= kMaxStems || frames <= 0) return;
    Stem& s = stems_[static_cast<size_t>(stem)];
    uint64_t w = s.written.load(std::memory_order_relaxed);
    const uint64_t mask = static_cast<uint64_t>(capacity_) - 1;
    for (int i = 0; i < frames; ++i, ++w) {
        const size_t idx = static_cast<size_t>(w & mask) * 2;
        s.ring[idx] = lr[2 * i];
        s.ring[idx + 1] = lr[2 * i + 1];
    }
    s.written.store(w, std::memory_order_release);
}

void WorldStreamPlayer::requestFlush() noexcept {
    const int n = stemCount_.load();
    for (int i = 0; i < n; ++i) {
        Stem& s = stems_[static_cast<size_t>(i)];
        s.flushTo.store(s.written.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    flushRequests_.fetch_add(1, std::memory_order_release);
}

void WorldStreamPlayer::processAdd(AudioBuffer& out, int numFrames) noexcept {
    const int n = stemCount_.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) stems_[static_cast<size_t>(i)].gain.beginBlock();

    const uint32_t requests = flushRequests_.load(std::memory_order_acquire);
    if (requests != flushesSeen_) {
        for (int i = 0; i < n; ++i) {
            Stem& s = stems_[static_cast<size_t>(i)];
            s.read.store(s.flushTo.load(std::memory_order_relaxed), std::memory_order_release);
        }
        flushesSeen_ = requests;
        played_.store(0, std::memory_order_relaxed);
    }

    if (!active() || n == 0 || out.numChannels() < 2) {
        for (int i = 0; i < n; ++i) for (int f = 0; f < numFrames; ++f) stems_[static_cast<size_t>(i)].gain.next();
        spatialMix_.beginBlock();
        for (int f = 0; f < numFrames; ++f) spatialMix_.next();
        return;
    }

    // Lockstep: only play if every stem has the whole block.
    for (int i = 0; i < n; ++i) {
        const Stem& s = stems_[static_cast<size_t>(i)];
        if (s.written.load(std::memory_order_acquire) - s.read.load(std::memory_order_relaxed) < static_cast<uint64_t>(numFrames)) {
            underruns_.fetch_add(1, std::memory_order_relaxed);
            for (int k = 0; k < n; ++k) for (int f = 0; f < numFrames; ++f) stems_[static_cast<size_t>(k)].gain.next();
            spatialMix_.beginBlock();
            for (int f = 0; f < numFrames; ++f) spatialMix_.next();
            return;
        }
    }

    float* L = out.channel(0);
    float* R = out.channel(1);
    const uint64_t mask = static_cast<uint64_t>(capacity_) - 1;

    // 3D blend weight per frame (0 = plain stereo, 1 = binaural), smoothed so
    // switching modes is a short crossfade rather than a click.
    spatialMix_.beginBlock();
    float* w = mixL_.data();
    bool anySpatial = false, anyDirect = false;
    for (int f = 0; f < numFrames; ++f) {
        w[f] = spatialMix_.next();
        anySpatial |= w[f] > 0.0f;
        anyDirect |= w[f] < 1.0f;
    }
    if (anySpatial) spatialOut_.clear(numFrames);
    const float yaw = listenerYaw_.load(std::memory_order_relaxed);
    float* SL = scratchL_.data();
    float* SR = scratchR_.data();

    for (int i = 0; i < n; ++i) {
        Stem& s = stems_[static_cast<size_t>(i)];
        uint64_t r = s.read.load(std::memory_order_relaxed);
        float peak = 0.0f;
        for (int f = 0; f < numFrames; ++f, ++r) {
            const float g = s.gain.next();
            const size_t idx = static_cast<size_t>(r & mask) * 2;
            SL[f] = s.ring[idx] * g;
            SR[f] = s.ring[idx + 1] * g;
            peak = std::max(peak, std::max(std::fabs(SL[f]), std::fabs(SR[f])));
        }
        s.read.store(r, std::memory_order_release);
        s.peak.store(peak, std::memory_order_relaxed);
        if (anyDirect)
            for (int f = 0; f < numFrames; ++f) { L[f] += (1.0f - w[f]) * SL[f]; R[f] += (1.0f - w[f]) * SR[f]; }
        if (anySpatial) {
            // The stem's left/right channels become two sources either side of
            // its position, rotated against the listener's head direction.
            const float az = s.az.load(std::memory_order_relaxed) - yaw;
            const float half = 0.5f * s.width.load(std::memory_order_relaxed);
            const float el = s.el.load(std::memory_order_relaxed);
            const float dist = s.dist.load(std::memory_order_relaxed);
            s.left->setPosition({az - half, el, dist, 0.0f});
            s.right->setPosition({az + half, el, dist, 0.0f});
            s.left->processAdd(SL, spatialOut_, numFrames);
            s.right->processAdd(SR, spatialOut_, numFrames);
        }
    }
    if (anySpatial)
        for (int f = 0; f < numFrames; ++f) {
            L[f] += w[f] * spatialOut_.channel(0)[f];
            R[f] += w[f] * spatialOut_.channel(1)[f];
        }
    played_.fetch_add(static_cast<uint64_t>(numFrames), std::memory_order_relaxed);
}

} // namespace atmos
