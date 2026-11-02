#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <random>
#include <string>

#include "engine/audio/AudioClip.h"

namespace atmos::test {

inline AudioClip makeSine(double rate, int frames, double freq, float amp = 0.5f, int channels = 2, int leadingSilence = 0) {
    AudioClip c;
    c.sampleRate = rate;
    c.samples.setSize(channels, frames + leadingSilence);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < frames; ++i)
            c.samples.channel(ch)[leadingSilence + i] =
                amp * static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * i / rate));
    return c;
}

inline AudioClip makeImpulse(double rate, int frames, int at, float value = 0.5f, int channels = 2) {
    AudioClip c;
    c.sampleRate = rate;
    c.samples.setSize(channels, frames);
    for (int ch = 0; ch < channels; ++ch) c.samples.channel(ch)[at] = value;
    return c;
}

inline float peakOf(const float* x, int n) {
    float p = 0.0f;
    for (int i = 0; i < n; ++i) p = std::max(p, std::fabs(x[i]));
    return p;
}

inline double rmsOf(const float* x, int n) {
    double s = 0;
    for (int i = 0; i < n; ++i) s += double(x[i]) * x[i];
    return n ? std::sqrt(s / n) : 0.0;
}

inline int firstNonZero(const float* x, int n, float thresh = 1e-6f) {
    for (int i = 0; i < n; ++i)
        if (std::fabs(x[i]) > thresh) return i;
    return -1;
}

inline std::filesystem::path tempDir(const std::string& name) {
    static std::mt19937 rng{std::random_device{}()};
    auto p = std::filesystem::temp_directory_path() / ("atmos-test-" + name + "-" + std::to_string(rng()));
    std::filesystem::create_directories(p);
    return p;
}

} // namespace atmos::test
