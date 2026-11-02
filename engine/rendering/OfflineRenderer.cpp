#include "engine/rendering/OfflineRenderer.h"

#include <algorithm>

namespace atmos {

bool renderOffline(AudioEngine& engine, const OfflineRenderOptions& o, AudioBuffer& out) {
    const int64_t total = o.numFrames > 0 ? o.numFrames
                                          : std::max<int64_t>(0, engine.timelineLengthFrames() - o.startFrame);
    out.setSize(2, static_cast<int>(total));
    const bool micWas = engine.micInputEnabled();
    engine.setMicInputEnabled(false);
    engine.seek(o.startFrame);
    engine.play();

    const int block = std::clamp(o.blockSize, 16, engine.config().maxBlockSize);
    bool cancelled = false;
    for (int64_t done = 0; done < total; done += block) {
        const int n = static_cast<int>(std::min<int64_t>(block, total - done));
        float* outs[2] = {out.channel(0) + done, out.channel(1) + done};
        engine.process(nullptr, 0, outs, 2, n);
        if (o.progress && !o.progress(static_cast<double>(done + n) / static_cast<double>(total))) {
            cancelled = true;
            break;
        }
    }
    engine.pause();
    engine.setMicInputEnabled(micWas);
    engine.collectGarbage();
    return !cancelled;
}

} // namespace atmos
