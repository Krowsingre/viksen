#pragma once
// Minimal, dependency-free RIFF/WAVE reader & writer.
// Read: PCM 8/16/24/32-bit integer, IEEE float 32/64, WAVE_FORMAT_EXTENSIBLE.
// Write: PCM 16/24-bit or float 32-bit. Platform decoders (AVFoundation, JUCE)
// handle compressed formats; this exists so the core can import/export WAV and
// tests can run anywhere.

#include <cstdint>
#include <filesystem>
#include <string>

#include "engine/audio/AudioClip.h"

namespace atmos::core {

struct AudioFileInfo {
    double sampleRate = 0.0;
    int numChannels = 0;
    int64_t numFrames = 0;
    int bitsPerSample = 0;
    bool isFloat = false;
    double durationSeconds() const { return sampleRate > 0 ? static_cast<double>(numFrames) / sampleRate : 0.0; }
};

enum class WavSampleFormat { Int16, Int24, Float32 };

struct WavResult {
    bool ok = false;
    std::string error;
};

WavResult readWavInfo(const std::filesystem::path& path, AudioFileInfo& info);
WavResult readWav(const std::filesystem::path& path, AudioClip& clip, AudioFileInfo* info = nullptr);
WavResult writeWav(const std::filesystem::path& path, const AudioBuffer& audio, double sampleRate,
                   WavSampleFormat format = WavSampleFormat::Int24, int64_t numFrames = -1);

} // namespace atmos::core
