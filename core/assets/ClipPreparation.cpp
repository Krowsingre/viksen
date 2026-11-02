#include "core/assets/ClipPreparation.h"

#include <cmath>
#include <memory>

#include "core/assets/WavFile.h"
#include "engine/dsp/Resampler.h"

namespace atmos::core {

ClipPtr prepareClipForEngine(AudioClip decoded, double rate) {
    if (std::fabs(decoded.sampleRate - rate) < 1e-6) {
        decoded.sampleRate = rate;
        return std::make_shared<const AudioClip>(std::move(decoded));
    }
    return std::make_shared<const AudioClip>(resampleClip(decoded, rate));
}

ClipPtr loadWavForEngine(const std::filesystem::path& path, double rate, std::string* error) {
    AudioClip clip;
    const auto r = readWav(path, clip);
    if (!r.ok) {
        if (error) *error = r.error;
        return nullptr;
    }
    return prepareClipForEngine(std::move(clip), rate);
}

} // namespace atmos::core
