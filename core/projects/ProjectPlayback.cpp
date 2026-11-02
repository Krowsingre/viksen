#include "core/projects/ProjectPlayback.h"

#include <algorithm>
#include <cmath>

#include "engine/dsp/Smoother.h"

namespace atmos::core {

ApplyReport applyToEngine(const ProjectManifest& m, const std::map<std::string, ClipPtr>& clips, AudioEngine& engine) {
    ApplyReport report;
    for (int slot = 0; slot < AudioEngine::kMaxLayers; ++slot) engine.clearLayer(slot);

    const bool anySolo = std::any_of(m.layers.begin(), m.layers.end(), [](const Layer& l) { return l.solo; });
    const double rateRatio = m.sampleRate > 0 ? engine.config().sampleRate / m.sampleRate : 1.0;

    int slot = 0;
    for (const auto& layer : m.layers) {
        if (slot >= AudioEngine::kMaxLayers) {
            report.warnings.push_back("too many layers; extra layers not loaded (" + layer.id + ")");
            continue;
        }
        auto it = clips.find(layer.sourceId);
        if (it == clips.end() || !it->second) {
            report.warnings.push_back("layer " + layer.id + ": source " + layer.sourceId + " unavailable");
            continue;
        }
        const int64_t start = static_cast<int64_t>(std::llround(static_cast<double>(layer.timelineStartFrame) * rateRatio));
        if (!engine.setLayer(slot, it->second, layer.bus, start)) {
            report.warnings.push_back("layer " + layer.id + ": engine rejected clip (sample-rate mismatch?)");
            continue;
        }
        engine.setLayerGain(slot, dbToGain(layer.gainDb));
        engine.setLayerMuted(slot, layer.muted || (anySolo && !layer.solo));
        ++slot;
        ++report.layersLoaded;
    }
    for (int b = 0; b < kNumBuses; ++b) {
        engine.setBusGain(static_cast<Bus>(b), m.buses[b].gainDb <= -144.0f ? 0.0f : dbToGain(m.buses[b].gainDb));
        engine.setBusMuted(static_cast<Bus>(b), m.buses[b].muted);
    }
    engine.setMasterGain(dbToGain(m.masterGainDb));
    engine.setLimiterCeilingDb(std::min(0.0f, m.limiterCeilingDb));
    return report;
}

} // namespace atmos::core
