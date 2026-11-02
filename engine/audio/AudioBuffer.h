#pragma once
// Non-interleaved, pre-allocated float audio buffer.
// Allocation happens only in setSize(); never call setSize() on the audio thread.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

namespace atmos {

class AudioBuffer {
public:
    AudioBuffer() = default;
    AudioBuffer(int numChannels, int numFrames) { setSize(numChannels, numFrames); }

    void setSize(int numChannels, int numFrames) {
        assert(numChannels >= 0 && numFrames >= 0);
        channels_ = numChannels;
        frames_ = numFrames;
        data_.assign(static_cast<size_t>(numChannels) * static_cast<size_t>(numFrames), 0.0f);
    }

    int numChannels() const noexcept { return channels_; }
    int numFrames() const noexcept { return frames_; }

    float* channel(int ch) noexcept {
        assert(ch >= 0 && ch < channels_);
        return data_.data() + static_cast<size_t>(ch) * static_cast<size_t>(frames_);
    }
    const float* channel(int ch) const noexcept {
        assert(ch >= 0 && ch < channels_);
        return data_.data() + static_cast<size_t>(ch) * static_cast<size_t>(frames_);
    }

    void clear() noexcept { std::fill(data_.begin(), data_.end(), 0.0f); }
    void clear(int numFramesToClear) noexcept {
        const int n = std::min(numFramesToClear, frames_);
        for (int ch = 0; ch < channels_; ++ch) std::fill(channel(ch), channel(ch) + n, 0.0f);
    }

private:
    int channels_ = 0;
    int frames_ = 0;
    std::vector<float> data_;
};

} // namespace atmos
