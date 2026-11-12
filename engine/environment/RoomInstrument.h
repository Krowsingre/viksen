#pragma once
// Room instrument, phase 3 (owner, 2026-11-12): the room becomes music, in the song's key
// and on its beat. Fed the conditioned microphone; adds stereo to the room's sound (heard
// only with the room blended in). Three voices, each with its own level (0 = off):
//
// - Tune: the room's pitch (a voice, a hum, an engine) is followed, snapped to the song's
//   scale and sung back by a soft, slightly detuned tone that glides between notes.
// - Groove: the last bar of the room is kept in a short delay; on the song's 16th notes,
//   short slices of it are replayed from earlier beats, so background noise becomes rhythm.
//   Busier moments (the music's energy) bring more slices.
// - Hits: a sudden sound (a door, a clap, a laugh) is answered with a tuned bell, its note
//   drawn from the scale without repeating until every note has sounded.
//
// Nothing is stored beyond a few seconds in memory; all of it stops with the microphone.
// The beat clock is set from the song's position by the session (control thread) and
// runs on between updates (audio thread).

#include <array>
#include <atomic>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace atmos {

class RoomInstrument {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    // Any thread.
    std::atomic<float> tuneLevel{0.35f};
    std::atomic<float> grooveLevel{0.4f};
    std::atomic<float> hitsLevel{0.45f};
    std::atomic<float> energy{0.5f};          // 0..1, the music's current energy
    void setScale(uint16_t pitchClassMask) noexcept { scale_.store(pitchClassMask, std::memory_order_relaxed); }
    // The song's position in beats now, and its tempo (0 = no song: the groove rests).
    void setClock(double beat, double bpm) noexcept;

    // Audio thread: adds to out (stereo).
    void processAdd(const float* mono, float* outL, float* outR, int numFrames) noexcept;

    // Scales from a key name ("G minor", "D♭ major", "C# minor"); pentatonic, which suits
    // anything played over the song. Unknown or empty: A minor pentatonic.
    static uint16_t scaleForKey(const std::string& key);
    // The tracked pitch (Hz, 0 = none) and the note being sung (MIDI, -1 = none), for tests.
    float trackedHz() const noexcept { return trackedHz_.load(std::memory_order_relaxed); }
    int tuneNote() const noexcept { return tuneNoteOut_.load(std::memory_order_relaxed); }

private:
    // Pitch tracking (decimated by 4).
    void trackPitch() noexcept;
    int quantize(float hz) const noexcept;
    float renderTune() noexcept;
    void renderGroove(float& l, float& r) noexcept;
    void detectHit(float x) noexcept;
    void startHit() noexcept;
    void renderHits(float& l, float& r) noexcept;

    double sr_ = 48000.0;
    std::atomic<uint16_t> scale_{(1u << 9) | (1u << 0) | (1u << 2) | (1u << 4) | (1u << 7)};   // A minor pentatonic
    std::mt19937 rng_{0x5eed};

    // Pitch tracking state.
    std::vector<float> pitchBuf_;   // 1024 samples at sr/4
    int pitchWrite_ = 0, decim_ = 0, sinceAnalysis_ = 0;
    float decimAcc_ = 0.0f;
    std::atomic<float> trackedHz_{0.0f};
    int candidateNote_ = -1, candidateHold_ = 0;   // samples a candidate has held (decimated)
    int heardRecently_ = 0;                          // samples since a confident pitch (full rate)
    // Tune voice.
    std::atomic<int> tuneNoteOut_{-1};
    double tuneHz_ = 220.0, tuneTargetHz_ = 220.0, phaseA_ = 0, phaseB_ = 0, vibrato_ = 0;
    float tuneAmp_ = 0.0f;

    // Beat clock.
    std::atomic<double> clockBeat_{0.0};
    std::atomic<double> clockBpm_{0.0};
    std::atomic<uint32_t> clockSeq_{0};
    uint32_t seenSeq_ = 0;
    double beat_ = 0.0, bpm_ = 0.0;
    long last16th_ = -1;
    // Groove.
    std::vector<float> ring_;   // 4 s of room
    int ringWrite_ = 0;
    struct Grain { bool on = false; double read = 0; int left = 0, length = 1; float panL = 0.7f, panR = 0.7f; };
    std::array<Grain, 4> grains_{};

    // Hits.
    float fastEnv_ = 0.0f, slowEnv_ = 0.0f;
    int refractory_ = 0, warmup_ = 0;
    std::vector<int> bag_;   // notes left before any repeats
    struct Bell { bool on = false; double phaseC = 0, phaseM = 0, hz = 440; float t = 0; float panL = 0.7f, panR = 0.7f; };
    std::array<Bell, 6> bells_{};
};

} // namespace atmos
