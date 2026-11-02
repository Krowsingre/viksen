#pragma once
// Pre-allocated fractional delay line (linear interpolation).

#include <algorithm>
#include <vector>

namespace atmos {

class DelayLine {
public:
    void prepare(int maxDelaySamples) {
        buffer_.assign(static_cast<size_t>(std::max(2, maxDelaySamples + 2)), 0.0f);
        write_ = 0;
    }

    void reset() noexcept { std::fill(buffer_.begin(), buffer_.end(), 0.0f); }

    void push(float x) noexcept {
        buffer_[write_] = x;
        write_ = (write_ + 1) % buffer_.size();
    }

    // delay in samples, measured from the most recently pushed sample (0 = newest).
    float read(float delaySamples) const noexcept {
        const size_t n = buffer_.size();
        const float d = std::clamp(delaySamples, 0.0f, static_cast<float>(n - 2));
        const size_t whole = static_cast<size_t>(d);
        const float frac = d - static_cast<float>(whole);
        const size_t i0 = (write_ + n - 1 - whole) % n;
        const size_t i1 = (i0 + n - 1) % n;
        return buffer_[i0] + frac * (buffer_[i1] - buffer_[i0]);
    }

    int maxDelay() const noexcept { return static_cast<int>(buffer_.size()) - 2; }

private:
    std::vector<float> buffer_;
    size_t write_ = 0;
};

} // namespace atmos
