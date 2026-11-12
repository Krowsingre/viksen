#pragma once
// Real-time transformation of environmental (microphone) input.
// Chain: tone filter -> frequency shifter (blend) -> ping-pong feedback echo -> stereo
// reverb (room blend rebuild, owner 2026-11-05). The echo is set to the song's beat by
// the session, and the reverb's size follows the moon.
// Further transformations (granular delay, reverse buffer, pitch/formant) plug in here.
// Latency: the frequency shifter delays the whole transformed path by
// FrequencyShifter::kHalfTaps samples (about 2.6 ms at 48 kHz); the raw mic bus is not delayed.

#include "engine/audio/Parameter.h"
#include "engine/dsp/Biquad.h"
#include "engine/dsp/DelayLine.h"
#include "engine/dsp/FrequencyShifter.h"
#include "engine/environment/RoomReverb.h"

namespace atmos {

class MicTransform {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // Any thread.
    SmoothedParameter cutoffHz{1800.0f};   // low-pass tone ("submerged" when low)
    SmoothedParameter echoSeconds{0.38f};
    SmoothedParameter feedback{0.45f};     // clamped to < 0.95 for stability
    SmoothedParameter wet{0.6f};
    SmoothedParameter shiftHz{-180.0f};    // frequency shift amount (Hz, may be negative)
    SmoothedParameter shiftMix{0.0f};      // 0 = off, 1 = fully shifted
    SmoothedParameter reverbMix{0.35f};    // 0 = dry room, 1 = mostly reverb
    SmoothedParameter roomSize{0.6f};      // reverb decay 0..1 (the moon widens it)
    SmoothedParameter reverbDamp{0.4f};    // high-frequency absorption 0..1
    SmoothedParameter level{1.0f};         // output (0 when the room instrument makes the room blend)

    // Audio thread: mono in, stereo out (same length).
    void process(const float* in, float* outL, float* outR, int numFrames) noexcept;

private:
    double sampleRate_ = 48000.0;
    Biquad tone_;
    float lastCutoff_ = -1.0f;
    DelayLine delay_, delayR_;   // ping-pong: left echo, then right a beat later
    RoomReverb reverb_;
    FrequencyShifter shifter_;
};

} // namespace atmos
