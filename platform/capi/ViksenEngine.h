#pragma once
/* Plain C interface to the Viksen engine for apps that are not C++ (the Windows
 * WinUI 3 app calls it through P/Invoke). One engine handle per app; every call
 * is made from the UI thread. Strings are UTF-8. Returned strings stay valid
 * until the next call on the same handle. */

#include <stdint.h>

#if defined(_WIN32)
#  if defined(VIKSEN_ENGINE_BUILD)
#    define VK_API __declspec(dllexport)
#  else
#    define VK_API __declspec(dllimport)
#  endif
#else
#  define VK_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define VK_API_VERSION 1

typedef struct vk_engine vk_engine;

/* Live settings, read and written with vk_get_param / vk_set_param. Values are
 * clamped to the same safe ranges as the iPhone app. */
typedef enum vk_param {
    VK_BUS_GAIN_MUSIC = 0, VK_BUS_GAIN_ATMOSPHERE = 1, VK_BUS_GAIN_MIC_RAW = 2, VK_BUS_GAIN_MIC_TRANSFORMED = 3,
    VK_BUS_MUTE_MUSIC = 4, VK_BUS_MUTE_ATMOSPHERE = 5, VK_BUS_MUTE_MIC_RAW = 6, VK_BUS_MUTE_MIC_TRANSFORMED = 7,
    VK_MASTER_GAIN = 8, VK_LIMITER_CEILING_DB = 9,
    VK_BED_ENABLED = 10, VK_BED_LEVEL = 11, VK_BED_COLOUR_HZ = 12,
    VK_MIC_TONE_HZ = 13, VK_MIC_ECHO_SECONDS = 14, VK_MIC_FEEDBACK = 15, VK_MIC_WET = 16,
    VK_MIC_SHIFT_HZ = 17, VK_MIC_SHIFT_MIX = 18,
    VK_ROOM_MEMORY_ENABLED = 19, VK_ROOM_MEMORY_LEVEL = 20, VK_ROOM_MEMORY_PITCH = 21,
    VK_LOOPING = 22,
    VK_PARAM_COUNT = 23
} vk_param;

typedef enum vk_record_mode {
    VK_RECORD_FULL_MIX = 1, VK_RECORD_WITHOUT_MIC = 2, VK_RECORD_ATMOSPHERE_ONLY = 3, VK_RECORD_ENVIRONMENT_ONLY = 4
} vk_record_mode;

/* Blittable meter snapshot (same layout in C#). */
typedef struct vk_meters {
    float output_peak[2];
    float bus_peak[4];
    float limiter_gain;
    double position_seconds;
    double duration_seconds;
    int32_t playing;
    int32_t mic_active;
    int32_t panicked;
    uint64_t underruns;
    float environment_level_db;
    int32_t quiet, loud, still, moving;
    int32_t loudness;           /* 0..4 */
    int32_t room_memory_voices;
    int32_t lunar_day;          /* 1..30 */
    char moon_phase[24];        /* "new", "waxingCrescent", ... */
} vk_meters;

VK_API int vk_api_version(void);
VK_API vk_engine* vk_create(void);
/* Tests and headless checks: VK_CREATE_NULL_AUDIO runs the engine on a timer, no hardware. */
#define VK_CREATE_NULL_AUDIO 1
VK_API vk_engine* vk_create_ex(int flags);
VK_API void vk_destroy(vk_engine* e);
VK_API const char* vk_last_error(vk_engine* e);

/* ---- audio device (system default output; the microphone only when enabled) ---- */
VK_API int vk_start_audio(vk_engine* e);               /* 1 = ok, 0 = see vk_last_error */
VK_API void vk_stop_audio(vk_engine* e);
VK_API int vk_audio_running(vk_engine* e);
VK_API double vk_sample_rate(vk_engine* e);
VK_API const char* vk_device_name(vk_engine* e);
VK_API int vk_set_mic_enabled(vk_engine* e, int enabled); /* restarts the device; never on by itself */
VK_API int vk_mic_enabled(vk_engine* e);
VK_API void vk_panic(vk_engine* e);
VK_API void vk_clear_panic(vk_engine* e);

VK_API void vk_set_param(vk_engine* e, vk_param p, float value);
VK_API float vk_get_param(vk_engine* e, vk_param p);
VK_API void vk_set_listener_yaw(vk_engine* e, float degrees);

/* Call ~30 times a second: advances Dimensions and returns meters.
 * intensity: exercise intensity 0..1, or negative when unknown. */
VK_API void vk_tick(vk_engine* e, double intensity, vk_meters* out);

/* ---- Dimensions (a folder with world.json and its stems) ---- */
/* JSON summary {"id","name","artist","key","tier","tempo","stems","audioAvailable"} or NULL. */
VK_API const char* vk_dimension_info(vk_engine* e, const char* folder);
VK_API int vk_load_dimension(vk_engine* e, const char* folder);
/* Studio: 1..8 of the user's own stems, streamed in sync. */
VK_API int vk_load_studio_stems(vk_engine* e, const char* const* paths, int count);
VK_API void vk_world_start(vk_engine* e, int quick);
VK_API void vk_world_stop(vk_engine* e, int quick);
VK_API void vk_world_seek(vk_engine* e, double seconds);
VK_API void vk_world_unload(vk_engine* e);
VK_API void vk_world_set_stem(vk_engine* e, int stem, float gain_db, int muted);
VK_API void vk_world_set_spatial(vk_engine* e, int enabled);
VK_API void vk_world_move_stem(vk_engine* e, int stem, float azimuth, float elevation, float distance);
VK_API void vk_world_reset_positions(vk_engine* e);
VK_API int vk_binaural_hrtf(void);                     /* 1 when Steam Audio is compiled in */
/* JSON {"loaded","playing","id","name","position","length","master","spatial","hrtf",
 *       "stems":[{"title","azimuth","elevation","distance","width","gainDb","peak","reasons":[..]}]} */
VK_API const char* vk_world_status(vk_engine* e);

/* ---- explicit recording (24-bit WAV) ---- */
VK_API int vk_start_recording(vk_engine* e, const char* path, vk_record_mode mode);
VK_API double vk_stop_recording(vk_engine* e);
VK_API int vk_recording(vk_engine* e);
VK_API double vk_recording_seconds(vk_engine* e);

#ifdef __cplusplus
}
#endif
