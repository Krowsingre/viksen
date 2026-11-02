#pragma once
// Organic transitions between parts of a Dimension's song (feeder thread, not
// real-time). Inspired by how reactive-music apps blur scene changes: the outgoing
// part "submerges" (a closing low-pass) while the new part surfaces (an opening one),
// and the outgoing audio is thrown into a tempo-synced echo and a reverb wash that
// ring on under the new part.

#include <vector>

namespace atmos::core {

struct TransitionShape {
    double beatSeconds = 0.5;   // song tempo, for the echo time
    float space = 0.5f;         // 0 dry/short .. 1 long, spacious (set from the moon)
    float echoMix = 0.30f;
    float reverbMix = 0.40f;
};

// Wet tail of `in` (interleaved stereo, n frames at `rate`): echo + reverb that keeps
// ringing after the input ends. Returns interleaved stereo; length grows with `space`.
std::vector<float> renderTransitionTail(const float* in, int n, double rate, const TransitionShape& shape);

// Equal-power crossfade with the submerge/surface filter sweep. `out` and `in` are
// interleaved stereo, n frames; the result is written to `in`.
void submergeCrossfade(const float* out, float* in, int n, double rate);

} // namespace atmos::core
