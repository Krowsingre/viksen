#include "engine/audio/CaptureTap.h"

#include <algorithm>

namespace atmos {

void CaptureTap::prepare(int ringFrames) {
    uint64_t cap = 1024;
    while (cap < static_cast<uint64_t>(ringFrames)) cap <<= 1;
    capacity_ = cap;
    ring_.assign(static_cast<size_t>(cap) * 2, 0.0f);
    written_.store(0);
    read_.store(0);
    overruns_.store(0);
}

void CaptureTap::push(const float* l, const float* r, int frames) noexcept {
    if (capacity_ == 0) return;
    uint64_t w = written_.load(std::memory_order_relaxed);
    const uint64_t used = w - read_.load(std::memory_order_acquire);
    if (used + static_cast<uint64_t>(frames) > capacity_) { // writer fell behind: drop the block, never block
        overruns_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const uint64_t mask = capacity_ - 1;
    for (int i = 0; i < frames; ++i, ++w) {
        const size_t idx = static_cast<size_t>(w & mask) * 2;
        ring_[idx] = l[i];
        ring_[idx + 1] = r[i];
    }
    written_.store(w, std::memory_order_release);
}

int CaptureTap::pop(float* out, int maxFrames) noexcept {
    uint64_t rd = read_.load(std::memory_order_relaxed);
    const uint64_t avail = written_.load(std::memory_order_acquire) - rd;
    const int n = static_cast<int>(std::min<uint64_t>(avail, static_cast<uint64_t>(maxFrames)));
    const uint64_t mask = capacity_ - 1;
    for (int i = 0; i < n; ++i, ++rd) {
        const size_t idx = static_cast<size_t>(rd & mask) * 2;
        out[2 * i] = ring_[idx];
        out[2 * i + 1] = ring_[idx + 1];
    }
    read_.store(rd, std::memory_order_release);
    return n;
}

} // namespace atmos
