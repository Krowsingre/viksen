#include "engine/spatial/BinauralFactory.h"

#include "engine/spatial/InterimStereoPanner.h"
#include "engine/spatial/SteamAudioRenderer.h"

namespace atmos {

std::unique_ptr<SpatialRenderer> makeBinauralRenderer(double sampleRate, int maxBlockSize) {
#if defined(ATMOS_WITH_STEAM_AUDIO)
    auto steam = std::make_unique<SteamAudioRenderer>();
    steam->prepare(sampleRate, maxBlockSize);
    if (steam->isBinauralHrtf()) return steam;
#endif
    auto interim = std::make_unique<InterimStereoPanner>();
    interim->prepare(sampleRate, maxBlockSize);
    return interim;
}

bool binauralHrtfCompiledIn() noexcept {
#if defined(ATMOS_WITH_STEAM_AUDIO)
    return true;
#else
    return false;
#endif
}

} // namespace atmos
