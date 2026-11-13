#pragma once
// The room blend, built the way the studied reactive-audio dreams build it (owner,
// 2026-11-12; study notes in the private assets repository (research/) and docs/ROOM_INSTRUMENT.md). All code
// here is original; only the techniques and their musical settings were studied.
//
// The room is always the sound. Its effects are ARRANGED: one at a time, each for a number
// of bars, fading over about a second, while a little of the plain room stays underneath.
// Each effect re-picks its settings on the bar and glides between them, so the room keeps
// turning into something new. The clock is the song's beat (32 ticks a bar), or 100 BPM
// with no song.
//
// Effects (each the room itself):
// - straight:  the plain room, full range, into the hall.
// - grains:    one-second grains of the room, 0-0.4 s back, 0.6-3 a second (re-picked
//              every bar), spread wide.
// - freqDream: the room frequency-shifted, sliding between 0 and -500 Hz every two bars
//              (the right side follows the left three seconds later).
// - delayDream: the room a semitone up, echoed on the beat; the feedback swells over
//              16 bars, then falls back.
// - resonator: the room rings through three tuned resonators on the song's root, fifth
//              and ninth; the louder the room, the darker it rings; slow pan drift.
// - glitch:    once a bar on each side: the room's pitch glides to a new interval (up to
//              two octaves either way) and a 40 ms-2 s piece of it is caught and repeated.
// - spiral:    two short frequency-shifted feedback echoes: each repeat falls further.
// - ripple:    the room shifted up and down by an interval on the beat, faster and wider
//              as the scene goes on (8, 4, 2, then 1 tick; a third up to an octave).
// - groove:    a tape cut-up of the last 20 s of the room on the beat.
// - memory:    the room memory (slowed past moments, engine/environment/RoomMemory).
// - tune:      a soft three-voice synth on the room's pitches in the song's scale, changing
//              on the bar (the one app-made sound; kept for the owner to test).
//
// Nothing is stored beyond 20 s in memory, and it is wiped when the microphone stops.

#include <array>
#include <atomic>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "engine/dsp/FrequencyShifter.h"
#include "engine/dsp/PitchShifter.h"
#include "engine/environment/RoomReverb.h"

namespace atmos {

class RoomInstrument {
public:
    enum Fx { kStraight = 0, kGrains, kFreqDream, kDelayDream, kResonator, kGlitch, kSpiral, kRipple, kGroove, kMemory, kTune, kFxCount };
    static const char* fxName(int fx) noexcept;
    static int fxFromName(const std::string& name) noexcept;   // -1 if unknown

    struct Scene { uint16_t fx = 0; int bars = 12; };   // fx: bit i = Fx i
    static constexpr int kMaxScenes = 24;
    static std::vector<Scene> defaultScenes();

    void prepare(double sampleRate);
    void reset() noexcept;

    // Control thread.
    void setScenes(const std::vector<Scene>& scenes) noexcept;
    void setScale(uint16_t pitchClassMask, int root = -1) noexcept;
    void setClock(double beat, double bpm) noexcept;   // the song's position in beats; bpm 0 = no song
    std::atomic<float> level{1.0f};    // all effects
    std::atomic<float> dry{0.3f};      // the plain room always under the effects (high-passed)
    // Listening test: -1 = the arrangement; an Fx = that effect alone, held; kFxCount = none.
    std::atomic<int> solo{-1};

    // Audio thread: adds to out (stereo).
    void processAdd(const float* mono, float* outL, float* outR, int numFrames) noexcept;

    // How much of the room memory the arrangement wants now (0..1), for the engine.
    float memoryAmount() const noexcept { return memoryOut_.load(std::memory_order_relaxed); }

    static uint16_t scaleForKey(const std::string& key);
    static int rootForKey(const std::string& key);

    // For tests and the UI.
    int sceneIndex() const noexcept { return sceneOut_.load(std::memory_order_relaxed); }
    uint16_t activeFx() const noexcept { return activeOut_.load(std::memory_order_relaxed); }
    std::array<int, 3> resonatorNotes() const noexcept;
    float trackedHz() const noexcept { return trackedHz_.load(std::memory_order_relaxed); }

private:
    struct Glide {   // linear glide to a target over a time
        double value = 0, target = 0, step = 0;
        void set(double t, double samples) noexcept { target = t; step = samples > 1 ? (t - value) / samples : 0; if (samples <= 1) value = t; }
        double next() noexcept {
            if (step != 0) { value += step; if ((step > 0 && value >= target) || (step < 0 && value <= target)) { value = target; step = 0; } }
            return value;
        }
    };
    struct OnePole { float z = 0; float lp(float x, float a) noexcept { z += a * (x - z); return z; } };
    struct Grain { bool on = false; double read = 0; int left = 0, length = 1; float gl = 0, gr = 0; };
    struct Repeater {   // catches `length` of its input and repeats it, fading
        std::vector<float> buf; int length = 1, pos = 0, gateLeft = 0; float gate = 0;
    };

    void onTick(long tick) noexcept;
    void nextScene(long tick) noexcept;
    void trackPitch() noexcept;
    int nearestScaleNote(double midi) const noexcept;
    int scaleStep(int note, int steps) const noexcept;
    double ms(double m) const noexcept { return m * 0.001 * sr_; }
    float rnd() noexcept { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_); }
    float ringAt(double back) const noexcept;

    float fxStraight(float x, float& send) noexcept;
    void fxGrains(float& l, float& r, float& send) noexcept;
    void fxFreqDream(float x, float& l, float& r, float& send) noexcept;
    void fxDelayDream(float x, float& l, float& r, float& send) noexcept;
    void fxResonator(float x, float& l, float& r, float& send) noexcept;
    void fxGlitch(float x, float& l, float& r, float& send) noexcept;
    void fxSpiral(float x, float& l, float& r, float& send) noexcept;
    void fxRipple(float x, float& l, float& r, float& send) noexcept;
    void fxGroove(float x, float& l, float& r, float& send) noexcept;
    void fxTune(float& l, float& r, float& send) noexcept;
    float repeat(Repeater& rp, float x) noexcept;

    double sr_ = 48000.0;
    std::mt19937 rng_{0x5eed};
    std::atomic<uint16_t> scale_{(1u << 9) | (1u << 0) | (1u << 2) | (1u << 4) | (1u << 7)};
    std::atomic<int> root_{9};

    // Arrangement.
    std::array<std::atomic<uint32_t>, kMaxScenes> scenes_{};   // fx | bars << 16
    std::atomic<int> sceneCount_{0};
    int scene_ = -1;
    long sceneEnd_ = 0, lastTick_ = 0;
    bool started_ = false, ticked_ = false;
    std::array<float, kFxCount> gain_{}, target_{};
    std::atomic<int> sceneOut_{-1};
    std::atomic<uint16_t> activeOut_{0};
    std::atomic<float> memoryOut_{0.0f};

    // Clock.
    std::atomic<double> clockBeat_{0.0}, clockBpm_{0.0};
    std::atomic<uint32_t> clockSeq_{0};
    uint32_t seenSeq_ = 0;
    double beat_ = 0.0, bpm_ = 0.0;
    int64_t frame_ = 0;

    // Shared: the last 20 s of the room, the hall.
    std::vector<float> ring_;
    int ringWrite_ = 0;
    int64_t recorded_ = 0;
    RoomReverb hall_;
    OnePole hallIn_;
    float hp200_ = 0, hp200Prev_ = 0;   // the dry room's high-pass

    // grains
    std::array<Grain, 8> grains_{};
    double grainRate_ = 1.0;
    int grainWait_ = 0;
    // freqDream
    FrequencyShifter fdL_, fdR_;
    Glide fdShiftL_, fdShiftR_;
    int fdRightIn_ = -1;
    double fdNext_ = 0;
    bool fdLow_ = false;
    // delayDream
    std::vector<float> ddPre_, ddEcho_;
    int ddPreW_ = 0, ddEchoW_ = 0;
    PitchShifter ddShift_;
    Glide ddFeedback_;
    int ddCount_ = 0;
    OnePole ddLp_;
    // resonator
    struct Comb { std::vector<float> line; int w = 0; double delay = 200; float lp = 0; };
    std::array<Comb, 3> combs_{};
    std::array<std::atomic<int>, 3> combNotes_{};
    uint32_t combKey_ = 0;
    std::array<OnePole, 8> resLp_{};
    float resLevelDb_ = -60.0f, resCutoff_ = 3000.0f, resHp1_ = 0, resHp1Prev_ = 0, resHp2_ = 0, resHp2Prev_ = 0;
    std::vector<float> resEchoA_, resEchoB_;
    int resEchoW_ = 0;
    OnePole resEchoLp_;
    Glide resPan_;
    // glitch
    PitchShifter glShift_[2];
    Glide glPitch_[2];
    Repeater glRep_[2];
    int glAt_[2] = {0, 16};
    OnePole glLp_[2];
    // spiral
    FrequencyShifter spShift_[2];
    std::vector<float> spLine_[2];
    int spW_ = 0;
    OnePole spLp_[2];
    float spHp_[2] = {0, 0}, spHpPrev_[2] = {0, 0};
    // ripple
    PitchShifter rpShift_[2];
    Glide rpPitch_[2];
    int rpCount_ = 0, rpSign_ = 1, rpRightIn_ = -1;
    double rpRightTarget_ = 0, rpRightGlide_ = 0;
    // groove
    double grBack_ = 0;
    int grEvent_ = -1, grWait_ = 8, grMode_ = 4;
    Glide grGate_, grPan_;
    std::vector<float> grStereo_;
    int grStereoW_ = 0;
    float grHp_ = 0, grHpPrev_ = 0;
    OnePole grRumbleLp_;
    float grRumbleHp_ = 0, grRumbleHpPrev_ = 0;
    // tune
    std::vector<float> pitchBuf_;
    int pitchWrite_ = 0, decim_ = 0, sinceAnalysis_ = 0;
    float decimAcc_ = 0.0f;
    std::atomic<float> trackedHz_{0.0f};
    int heardNote_ = -1;
    std::array<Glide, 3> tuneHz_{};
    std::array<double, 3> tunePhase_{}, tuneSaw_{};
    std::array<OnePole, 3> tuneLp_{};
    double vibrato_ = 0;
    bool tuneOn_ = false;
};

} // namespace atmos
