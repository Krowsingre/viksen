#pragma once
// Turns decoded media into an engine-ready clip: resampled (if needed) to the
// session rate so playback never alters source pitch or tempo. Original files are
// never modified; this works on in-memory copies.

#include <filesystem>
#include <string>

#include "engine/audio/AudioClip.h"

namespace atmos::core {

ClipPtr prepareClipForEngine(AudioClip decoded, double engineSampleRate);

// Convenience for WAV (platform decoders handle other formats).
ClipPtr loadWavForEngine(const std::filesystem::path& path, double engineSampleRate, std::string* error = nullptr);

} // namespace atmos::core
