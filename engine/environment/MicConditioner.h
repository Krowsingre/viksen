#pragma once
// Conditions the microphone before it becomes sound (room instrument, phase 1): an
// automatic level control brings the room to a steady, present level whatever the
// surroundings, then a peak limiter keeps sudden sounds (a door, a shout) safe.
// The level detection (quiet/loud) runs on the unconditioned signal, before this.
//
// - Level control: RMS follower (50 ms attack, 1.5 s release) toward kTargetDb, gain
//   between kMinGainDb and kMaxGainDb, changing at most ~6 dB a second upward so the
//   room swells in rather than pumping. Below kGateDb (near silence) the gain holds, so
//   hiss is never pulled up.
// - Limiter: instant attack, 80 ms release, ceiling kCeilingDb.
// Audio thread only.

#include <vector>

namespace atmos {

class MicConditioner {
public:
    static constexpr float kTargetDb = -20.0f;
    static constexpr float kMinGainDb = -12.0f;
    static constexpr float kMaxGainDb = 30.0f;
    static constexpr float kGateDb = -70.0f;
    static constexpr float kCeilingDb = -3.0f;

    void prepare(double sampleRate);
    void reset() noexcept;
    // In place.
    void process(float* mono, int numFrames) noexcept;
    float gainDb() const noexcept { return gainDb_; }

private:
    double sampleRate_ = 48000.0;
    float rms2_ = 0.0f;          // smoothed power
    float gainDb_ = 0.0f;        // level-control gain
    float limitGain_ = 1.0f;     // limiter gain
    float attack_ = 0, release_ = 0, limRelease_ = 0, upPerSample_ = 0, downPerSample_ = 0;
};

} // namespace atmos
