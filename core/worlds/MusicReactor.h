#pragma once
// Room instrument, phase 2 (owner, 2026-11-12): the moment drives the music continuously
// and fast, instead of only through section jumps every 12-24 s.
//
// One "energy" (0..1) is taken from whatever the listener has on: the room's loudness (above
// its own learned background level, so every microphone and room reads alike) and
// busyness (sudden sounds per second) from the microphone, movement, and workout
// intensity. Any strong cue lifts it; it rises in about a third of a second and settles
// over a couple of seconds. From it:
// - a low-pass filter on the music: gently darkened (5 kHz) when everything is
//   calm (a song may close it further, e.g. "filterClosedHz": 1800 to submerge it), fully open from half energy upward;
// - stem levels: "energy" stems (drums, percussion, bass, guitar, brass) come up with the
//   energy, up to `depthDb` (10 dB) below their mixed level when calm; "calm" stems
//   (strings, synth, keyboard, woodwinds) recede a little at full energy; others stay.
// With no cue available (no microphone, no motion, no workout data), or with adaptive
// music off, everything stays neutral: the song plays as mixed.
// A song can adjust this under "react": {"depthDb", "filterClosedHz", "stems": {id: role}}.
// Control thread.

#include <optional>
#include <string>
#include <vector>

#include "core/serialization/Json.h"

namespace atmos::core {

struct ReactorInputs {
    bool adaptive = true;
    bool micActive = false;
    float roomLevelDb = -144.0f;    // unconditioned microphone level
    float onsetsPerSecond = 0.0f;   // sudden sounds in the room
    bool motionAvailable = false;
    int activity = 0;               // 0 (still) .. 4 (vigorous)
    std::optional<double> intensity;   // workout intensity 0..1
};

class MusicReactor {
public:
    enum class Role { Neutral, Energy, Calm };

    // Stem ids in mix order; `react` is the song's optional "react" object.
    void configure(const std::vector<std::string>& stemIds, const Json& react);
    void update(double dt, const ReactorInputs& in);

    double energy() const { return energy_; }
    bool engaged() const { return engaged_; }
    float cutoffHz() const;                     // 20000 = open
    float stemGain(int stem) const;              // linear, applied on top of the song's rules
    Role role(int stem) const { return stem >= 0 && stem < static_cast<int>(roles_.size()) ? roles_[static_cast<size_t>(stem)] : Role::Neutral; }

    static Role defaultRole(const std::string& stemId);

private:
    std::vector<Role> roles_;
    double depthDb_ = 10.0;
    double closedHz_ = 5000.0;
    double energy_ = 0.5;      // smoothed; neutral when nothing is engaged
    bool engaged_ = false;
    double neutralBlend_ = 1.0;   // 1 = fully neutral (fades in and out over ~1 s)
    double floorDb_ = -60.0;      // the room's background level, learned
    bool floorKnown_ = false;
public:
    double roomFloorDb() const { return floorDb_; }
};

} // namespace atmos::core
