#pragma once
// Offline (non-real-time) band-limited resampler: windowed-sinc, Blackman window.
// Used when importing media whose sample rate differs from the session rate so that
// playback never changes pitch/tempo of the source.

#include "engine/audio/AudioClip.h"

namespace atmos {

// Returns a new clip at targetRate. Preserves duration (to within one frame) and
// channel phase alignment: every channel is resampled with identical timing.
AudioClip resampleClip(const AudioClip& source, double targetRate, int halfTaps = 32);

} // namespace atmos
