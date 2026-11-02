// C interface over LiveSession + MiniaudioHost. See ViksenEngine.h.

#include "platform/capi/ViksenEngine.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "core/session/LiveSession.h"
#include "core/session/SessionDocument.h"
#include "core/worlds/WorldConditions.h"
#include "core/worlds/WorldManifest.h"
#include "engine/spatial/BinauralFactory.h"
#include "platform/desktop/MiniaudioHost.h"

namespace fs = std::filesystem;
using namespace atmos;
using namespace atmos::core;

struct vk_engine {
    LiveSession session{48000.0, 8192};
    atmos::desktop::MiniaudioHost host{session};
    std::string error, text;
    std::chrono::steady_clock::time_point lastTick{};
    bool nullAudio = false;
};

namespace {
fs::path utf8Path(const char* s) { return fs::path(reinterpret_cast<const char8_t*>(s ? s : "")); }
std::string pathUtf8(const fs::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::optional<WorldManifest> readWorld(const fs::path& folder, std::string& error) {
    std::ifstream in(folder / "world.json", std::ios::binary);
    if (!in) { error = "world.json missing"; return std::nullopt; }
    const Json j = Json::parse(in, nullptr, false);
    if (j.is_discarded()) { error = "world.json is not valid JSON"; return std::nullopt; }
    return parseWorld(j, &error);
}

bool stemsPresent(const fs::path& folder, const WorldManifest& w) {
    std::error_code ec;
    for (const auto& s : w.stems)
        if (!fs::exists(folder / utf8Path(s.file.c_str()), ec)) return false;
    return true;
}

StemDecoderFactory factoryFor(std::vector<std::string> files) {
    return [files = std::move(files)](double rate, std::string& e) {
        std::vector<std::unique_ptr<StemDecoder>> v;
        for (const auto& f : files) {
            auto d = atmos::desktop::makeMiniaudioStemDecoder(f, rate, e);
            if (!d) return std::vector<std::unique_ptr<StemDecoder>>{};
            v.push_back(std::move(d));
        }
        return v;
    };
}

// Reads/writes one live setting. Writes go through the same clamping as projects.
float* fieldOf(LiveSettings& s, vk_param p, bool*& flag) {
    flag = nullptr;
    switch (p) {
        case VK_BUS_GAIN_MUSIC: case VK_BUS_GAIN_ATMOSPHERE: case VK_BUS_GAIN_MIC_RAW: case VK_BUS_GAIN_MIC_TRANSFORMED:
            return &s.busGain[p - VK_BUS_GAIN_MUSIC];
        case VK_BUS_MUTE_MUSIC: case VK_BUS_MUTE_ATMOSPHERE: case VK_BUS_MUTE_MIC_RAW: case VK_BUS_MUTE_MIC_TRANSFORMED:
            flag = &s.busMuted[p - VK_BUS_MUTE_MUSIC]; return nullptr;
        case VK_MASTER_GAIN: return &s.masterGain;
        case VK_LIMITER_CEILING_DB: return &s.limiterCeilingDb;
        case VK_BED_ENABLED: flag = &s.bedEnabled; return nullptr;
        case VK_BED_LEVEL: return &s.bedLevel;
        case VK_BED_COLOUR_HZ: return &s.bedColourHz;
        case VK_MIC_TONE_HZ: return &s.micToneHz;
        case VK_MIC_ECHO_SECONDS: return &s.micEchoSeconds;
        case VK_MIC_FEEDBACK: return &s.micFeedback;
        case VK_MIC_WET: return &s.micWet;
        case VK_MIC_SHIFT_HZ: return &s.micShiftHz;
        case VK_MIC_SHIFT_MIX: return &s.micShiftMix;
        case VK_ROOM_MEMORY_ENABLED: flag = &s.roomMemoryEnabled; return nullptr;
        case VK_ROOM_MEMORY_LEVEL: return &s.roomMemoryLevel;
        case VK_ROOM_MEMORY_PITCH: return &s.roomMemoryPitch;
        case VK_LOOPING: flag = &s.looping; return nullptr;
        default: return nullptr;
    }
}

const char* keep(vk_engine* e, std::string s) { e->text = std::move(s); return e->text.c_str(); }
int fail(vk_engine* e, std::string msg) { e->error = std::move(msg); return 0; }
} // namespace

extern "C" {

int vk_api_version(void) { return VK_API_VERSION; }

vk_engine* vk_create(void) { return vk_create_ex(0); }

vk_engine* vk_create_ex(int flags) {
    try {
        auto* e = new vk_engine();
        e->nullAudio = (flags & VK_CREATE_NULL_AUDIO) != 0;
        return e;
    } catch (...) {
        return nullptr;
    }
}

void vk_destroy(vk_engine* e) { delete e; }   // host stops the device before the session goes
const char* vk_last_error(vk_engine* e) { return e ? e->error.c_str() : "no engine"; }

int vk_start_audio(vk_engine* e) {
    if (e->host.running()) e->host.stop();
    atmos::desktop::HostOptions o;
    o.withInput = e->session.settings().micEnabled;
    o.nullBackend = e->nullAudio;
    std::string err;
    if (!e->host.start(o, err)) return fail(e, err);
    return 1;
}

void vk_stop_audio(vk_engine* e) {
    e->host.stop();
    e->session.engine().pause();
}

int vk_audio_running(vk_engine* e) { return e->host.running() ? 1 : 0; }
double vk_sample_rate(vk_engine* e) { return e->session.sampleRate(); }
const char* vk_device_name(vk_engine* e) { return keep(e, e->host.deviceName()); }

int vk_set_mic_enabled(vk_engine* e, int enabled) {
    e->session.setMicEnabled(enabled != 0);
    if (!enabled && !e->host.running()) return 1;
    if (vk_start_audio(e)) return 1;   // reopens the device with (or without) the input
    // Failed (no input device, permission off): leave the mic off and keep playback going.
    const std::string why = e->error;
    e->session.setMicEnabled(false);
    vk_start_audio(e);
    e->error = why;
    return 0;
}

int vk_mic_enabled(vk_engine* e) { return e->session.settings().micEnabled ? 1 : 0; }

void vk_panic(vk_engine* e) {
    e->session.engine().panic();
    e->session.engine().pause();
}
void vk_clear_panic(vk_engine* e) { e->session.engine().clearPanic(); }

void vk_set_param(vk_engine* e, vk_param p, float value) {
    LiveSettings s = e->session.settings();
    bool* flag = nullptr;
    float* f = fieldOf(s, p, flag);
    if (f) *f = value;
    else if (flag) *flag = value != 0.0f;
    else return;
    e->session.applyAllSettings(liveSettingsFromJson(liveSettingsToJson(s), s));
}

float vk_get_param(vk_engine* e, vk_param p) {
    LiveSettings s = e->session.settings();
    bool* flag = nullptr;
    float* f = fieldOf(s, p, flag);
    return f ? *f : flag ? (*flag ? 1.0f : 0.0f) : 0.0f;
}

void vk_set_listener_yaw(vk_engine* e, float degrees) { e->session.setListenerYaw(degrees); }

void vk_tick(vk_engine* e, double intensity, vk_meters* out) {
    AudioEngine& engine = e->session.engine();
    engine.collectGarbage();
    const auto now = std::chrono::steady_clock::now();
    const double dt = e->lastTick.time_since_epoch().count() == 0
                          ? 0.0 : std::min(0.5, std::chrono::duration<double>(now - e->lastTick).count());
    e->lastTick = now;
    e->session.updateEnvironment(dt);
    const EnvironmentStateSnapshot st = e->session.environmentStates();
    const bool running = e->host.running();
    const std::time_t t = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    const double days = static_cast<double>(t) / 86400.0;
    ConditionContext ctx;
    ctx.env = st;
    ctx.micActive = engine.meters().micActive && running;
    if (intensity >= 0) ctx.intensity = std::min(1.0, intensity);
    ctx.localHour = local.tm_hour;
    ctx.daysSinceUnixEpochUtc = days;
    e->session.updateWorld(dt, ctx);
    if (!out) return;
    const EngineMeters m = engine.meters();
    const EnvironmentFeatures f = engine.environmentFeatures();
    const double rate = e->session.sampleRate();
    *out = vk_meters{};
    out->output_peak[0] = m.outputPeak[0];
    out->output_peak[1] = m.outputPeak[1];
    for (int b = 0; b < 4; ++b) out->bus_peak[b] = m.busPeak[b];
    out->limiter_gain = m.limiterGain;
    out->position_seconds = m.positionFrames / rate;
    out->duration_seconds = engine.timelineLengthFrames() / rate;
    out->playing = m.playing;
    out->mic_active = m.micActive && running;
    out->panicked = m.panicked;
    out->underruns = m.underruns;
    out->environment_level_db = f.levelDb;
    out->quiet = st.quiet; out->loud = st.loud; out->still = st.still; out->moving = st.moving;
    out->loudness = st.loudness;
    out->room_memory_voices = engine.roomMemory().activeVoices();
    out->lunar_day = lunarDay(days);
    std::strncpy(out->moon_phase, moonPhaseId(moonPhaseName(days)), sizeof(out->moon_phase) - 1);
}

// ---------------------------------------------------------------- Dimensions

const char* vk_dimension_info(vk_engine* e, const char* folder) {
    const fs::path dir = utf8Path(folder);
    auto w = readWorld(dir, e->error);
    if (!w) return nullptr;
    const Json j = {{"id", w->id}, {"name", w->name}, {"artist", w->artist}, {"key", w->key}, {"tier", w->tier},
                    {"tempo", w->tempo}, {"stems", w->stems.size()}, {"audioAvailable", stemsPresent(dir, *w)}};
    return keep(e, j.dump());
}

int vk_load_dimension(vk_engine* e, const char* folder) {
    const fs::path dir = utf8Path(folder);
    auto w = readWorld(dir, e->error);
    if (!w) return 0;
    if (!stemsPresent(dir, *w)) return fail(e, "This build does not include this Dimension's music yet.");
    std::vector<std::string> files;
    for (const auto& s : w->stems) files.push_back(pathUtf8(dir / utf8Path(s.file.c_str())));
    std::string err;
    if (!e->session.loadWorld(*w, factoryFor(std::move(files)), &err)) return fail(e, err);
    return 1;
}

int vk_load_studio_stems(vk_engine* e, const char* const* paths, int count) {
    if (!paths || count < 1 || count > 8) return fail(e, "Choose between 1 and 8 stems.");
    WorldManifest w;
    w.id = "studio";
    w.name = "Studio";
    w.tier = "studio";
    w.fadeSeconds = 0.25f;   // mixer moves are near-immediate
    std::vector<std::string> files;
    const float n = static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        const fs::path p = utf8Path(paths[i]);
        WorldStem st;
        st.id = "stem" + std::to_string(i);
        st.file = pathUtf8(p.filename());
        st.title = pathUtf8(p.stem());
        // Spread across a frontal arc by default (same as iPhone); the canvas can move them.
        st.azimuthDeg = n > 1 ? -70.0f + 140.0f * static_cast<float>(i) / (n - 1) : 0.0f;
        st.distanceM = 1.6f;
        st.widthDeg = 40.0f;
        w.stems.push_back(st);
        files.push_back(paths[i]);
    }
    std::string err;
    if (!e->session.loadWorld(w, factoryFor(std::move(files)), &err)) return fail(e, err);
    return 1;
}

void vk_world_start(vk_engine* e, int quick) { e->session.startWorld(quick != 0); }
void vk_world_stop(vk_engine* e, int quick) { e->session.stopWorld(quick != 0); }
void vk_world_seek(vk_engine* e, double seconds) { e->session.seekWorld(seconds); }
void vk_world_unload(vk_engine* e) { e->session.unloadWorld(); }
void vk_world_set_stem(vk_engine* e, int stem, float gainDb, int muted) {
    e->session.setWorldStemLevel(stem, gainDb, muted != 0);
}
void vk_world_set_spatial(vk_engine* e, int enabled) { e->session.setWorldSpatialEnabled(enabled != 0); }
void vk_world_move_stem(vk_engine* e, int stem, float az, float el, float dist) {
    e->session.setWorldStemPosition(stem, az, el, dist);
}
void vk_world_reset_positions(vk_engine* e) { e->session.resetWorldStemPositions(); }
int vk_binaural_hrtf(void) { return binauralHrtfCompiledIn() ? 1 : 0; }

const char* vk_world_status(vk_engine* e) {
    const WorldStatus st = e->session.worldStatus();
    Json stems = Json::array();
    for (const auto& s : st.stems)
        stems.push_back({{"title", s.title}, {"azimuth", s.azimuthDeg}, {"elevation", s.elevationDeg},
                         {"distance", s.distanceM}, {"width", s.widthDeg}, {"gainDb", s.gainDb}, {"peak", s.peak},
                         {"reasons", s.reasons}});
    const Json j = {{"loaded", st.loaded}, {"playing", st.playing}, {"id", st.id}, {"name", st.name},
                    {"position", st.positionSeconds}, {"length", st.lengthSeconds}, {"master", st.master},
                    {"underruns", st.underruns}, {"spatial", st.spatial}, {"hrtf", st.hrtf}, {"stems", stems}};
    return keep(e, j.dump());
}

// ---------------------------------------------------------------- recording

int vk_start_recording(vk_engine* e, const char* path, vk_record_mode mode) {
    if (mode < VK_RECORD_FULL_MIX || mode > VK_RECORD_ENVIRONMENT_ONLY) return fail(e, "Unknown recording mode.");
    std::string err;
    if (!e->session.startRecording(utf8Path(path), static_cast<CaptureMode>(mode), &err)) return fail(e, err);
    return 1;
}
double vk_stop_recording(vk_engine* e) { return e->session.stopRecording(); }
int vk_recording(vk_engine* e) { return e->session.recording() ? 1 : 0; }
double vk_recording_seconds(vk_engine* e) { return e->session.recordingSeconds(); }

} // extern "C"
