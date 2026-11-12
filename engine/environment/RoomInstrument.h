#pragma once
// Room instrument (owner, 2026-11-12; reworked after the owner's listening and an audit of
// the studied reactive-audio design, docs/research/REFERENCE_STUDY.md): the room itself
// becomes the music. Every voice below is made from the room's own sound, in the song's
// key and on its beat; the only exception is "tune", kept for the owner to test.
//
// - Resonance: the room rings through four tuned resonators (comb filters) on notes of the
//   song's scale, re-chosen every two bars, so a tap or a word rings out in key.
// - Grains: the last second of the room, read back as small overlapping grains at random
//   places, a soft shimmering bed.
// - Shifted echo: the room pitch-shifted (an octave down by default) and echoed on the beat.
// - Pitch echo: when the room holds a pitch (a voice, a chime, a horn), that very sound is
//   echoed on the next 16th notes, retuned to the nearest note of the scale.
// - Groove: slices of the last bars of the room replayed on the song's 16th notes.
// - Tune (test): a soft tone that follows the room's pitch in the scale (app-made; may go).
// The plain room is kept by the room blend itself (MicTransform), softened.
//
// Nothing is stored beyond a few seconds in memory; it all stops with the microphone. The
// beat clock is set from the song's position by the session and runs on between updates.

#include <array>
#include <atomic>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "engine/dsp/PitchShifter.h"

namespace atmos {

class RoomInstrument {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    // Any thread. Levels 0..1 (0 = off).
    std::atomic<float> resonanceLevel{0.45f};
    std::atomic<float> grainLevel{0.3f};
    std::atomic<float> shiftEchoLevel{0.3f};
    std::atomic<float> pitchEchoLevel{0.4f};
    std::atomic<float> grooveLevel{0.4f};
    std::atomic<float> tuneLevel{0.35f};
    std::atomic<float> shiftEchoSemitones{-12.0f};
    std::atomic<float> energy{0.5f};          // 0..1, the music's current energy
    // The song's scale (pitch-class mask) and its root (0 = C .. 11 = B; -1 = the lowest
    // pitch class in the mask).
    void setScale(uint16_t pitchClassMask, int root = -1) noexcept;
    // The song's position in beats now, and its tempo (0 = no song).
    void setClock(double beat, double bpm) noexcept;

    // Audio thread: adds to out (stereo).
    void processAdd(const float* mono, float* outL, float* outR, int numFrames) noexcept;

    // Scales from a key name ("G minor", "D♭ major", "C# minor"); pentatonic, which suits
    // anything played over the song. Unknown or empty: A minor pentatonic.
    static uint16_t scaleForKey(const std::string& key);
    static int rootForKey(const std::string& key);   // 0..11; unknown: 9 (A)

    // For tests.
    float trackedHz() const noexcept { return trackedHz_.load(std::memory_order_relaxed); }
    int tuneNote() const noexcept { return tuneNoteOut_.load(std::memory_order_relaxed); }
    std::array<int, 4> resonatorNotes() const noexcept;
    int pitchEchoes() const noexcept { return pitchEchoCount_.load(std::memory_order_relaxed); }

private:
    void trackPitch() noexcept;
    int quantize(float hz) const noexcept;
    int nearestScaleNote(double midi) const noexcept;
    void chooseChord(long index) noexcept;
    float renderTune() noexcept;
    void renderResonance(float x, float& l, float& r) noexcept;
    void renderGrains(float& l, float& r) noexcept;
    void renderShiftEcho(float x, float& l, float& r) noexcept;
    void startPitchEcho(double detectedHz, int note) noexcept;
    void renderPitchEcho(float& l, float& r) noexcept;
    void renderGroove(float& l, float& r) noexcept;
    float ringAt(double back) const noexcept;   // the room `back` samples ago

    double sr_ = 48000.0;
    std::atomic<uint16_t> scale_{(1u << 9) | (1u << 0) | (1u << 2) | (1u << 4) | (1u << 7)};   // A minor pentatonic
    std::atomic<int> root_{9};
    std::mt19937 rng_{0x5eed};
    int64_t frame_ = 0;

    // The last 4 s of the room (groove, grains, pitch echo).
    std::vector<float> ring_;
    int ringWrite_ = 0;

    // Pitch tracking (decimated by 4).
    std::vector<float> pitchBuf_;
    int pitchWrite_ = 0, decim_ = 0, sinceAnalysis_ = 0;
    float decimAcc_ = 0.0f;
    std::atomic<float> trackedHz_{0.0f};
    int candidateNote_ = -1, candidateHold_ = 0;
    int heardRecently_ = 1 << 30;
    int lastEchoedNote_ = -1, sinceEcho_ = 1 << 30;

    // Tune voice (test).
    std::atomic<int> tuneNoteOut_{-1};
    double tuneHz_ = 220.0, tuneTargetHz_ = 220.0, phaseA_ = 0, phaseB_ = 0, vibrato_ = 0;
    float tuneAmp_ = 0.0f;

    // Resonators.
    struct Comb { std::vector<float> line; int write = 0; double delay = 200, target = 200; float lp = 0, g = 0.9f; float panL = 0.7f, panR = 0.7f; };
    std::array<Comb, 4> combs_{};
    std::array<std::atomic<int>, 4> combNotes_{};
    float resEnv_ = 0.0f;   // the resonators' level, so a held note on the chord can't build up
    long lastChordBar_ = -3;
    uint32_t chordKey_ = 0;   // the scale and root the chord was chosen for
    float hp_ = 0.0f, hpPrev_ = 0.0f, inLp_ = 0.0f;   // the resonators' input band (80 Hz - 3 kHz)
    float hpA_ = 0.99f, inLpA_ = 0.3f, grainLpA_ = 0.4f, echoLpA_ = 0.3f;
    double glideK_ = 0.0003;

    // Grains.
    struct Grain { bool on = false; double read = 0; int left = 0, length = 1; float panL = 0.7f, panR = 0.7f; };
    std::array<Grain, 8> grains_{};
    int nextGrain_ = 0;
    float grainLp_[2] = {0, 0};

    // Shifted echo.
    PitchShifter shifter_;
    std::vector<float> echo_;
    int echoWrite_ = 0;
    double echoDelay_ = 24000.0;
    float echoLp_ = 0.0f;

    // Pitch echo: a captured room sound, replayed retuned on the coming 16ths.
    std::vector<float> captured_;
    int capturedLen_ = 0;
    double captureRate_ = 1.0;
    struct Repeat { int64_t at = 0; float gain = 0; float pan = 0; bool on = false; };
    std::array<Repeat, 3> repeats_{};
    struct Playback { bool on = false; double pos = 0; float gain = 0, panL = 0.7f, panR = 0.7f; };
    std::array<Playback, 3> playbacks_{};
    std::atomic<int> pitchEchoCount_{0};

    // Beat clock and groove.
    std::atomic<double> clockBeat_{0.0};
    std::atomic<double> clockBpm_{0.0};
    std::atomic<uint32_t> clockSeq_{0};
    uint32_t seenSeq_ = 0;
    double beat_ = 0.0, bpm_ = 0.0;
    long last16th_ = -1;
    std::array<Grain, 4> slices_{};
};

} // namespace atmos
