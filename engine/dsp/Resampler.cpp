#include "engine/dsp/Resampler.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace atmos {

AudioClip resampleClip(const AudioClip& source, double targetRate, int halfTaps) {
    AudioClip out;
    out.sampleRate = targetRate;
    if (source.sampleRate <= 0 || targetRate <= 0 || source.numFrames() == 0) {
        out.samples.setSize(source.numChannels(), 0);
        return out;
    }
    if (source.sampleRate == targetRate) {
        out.samples = source.samples;
        return out;
    }

    const double ratio = targetRate / source.sampleRate;
    const int outFrames = static_cast<int>(std::llround(source.numFrames() * ratio));
    out.samples.setSize(source.numChannels(), outFrames);

    // When downsampling, lower the cutoff to avoid aliasing.
    const double cutoff = std::min(1.0, ratio) * 0.97;
    const double pi = std::numbers::pi;
    const int inFrames = source.numFrames();

    for (int ch = 0; ch < source.numChannels(); ++ch) {
        const float* in = source.samples.channel(ch);
        float* dst = out.samples.channel(ch);
        for (int n = 0; n < outFrames; ++n) {
            const double t = n / ratio; // position in source frames
            const int centre = static_cast<int>(std::floor(t));
            double acc = 0.0, norm = 0.0;
            for (int k = centre - halfTaps + 1; k <= centre + halfTaps; ++k) {
                const double x = t - k;
                const double sincArg = pi * x * cutoff;
                const double sinc = std::fabs(x) < 1e-9 ? 1.0 : std::sin(sincArg) / sincArg;
                const double w = 0.42 + 0.5 * std::cos(pi * x / halfTaps) + 0.08 * std::cos(2 * pi * x / halfTaps);
                const double h = cutoff * sinc * (std::fabs(x) < halfTaps ? w : 0.0);
                norm += h;
                if (k >= 0 && k < inFrames) acc += in[k] * h;
            }
            dst[n] = static_cast<float>(norm != 0.0 ? acc / norm : acc); // unity DC gain
        }
    }
    return out;
}

} // namespace atmos
