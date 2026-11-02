// The C interface used by the Windows app, driven on miniaudio's null device.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "core/assets/WavFile.h"
#include "core/serialization/Json.h"
#include "platform/capi/ViksenEngine.h"
#include "tests/TestHarness.h"
#include "tests/TestUtil.h"

using namespace atmos;

TEST_CASE("desktop/C API: settings clamp, Dimension loads and plays, Studio stems, 3D, recording") {
    CHECK(vk_api_version() == VK_API_VERSION);
    vk_engine* e = vk_create_ex(VK_CREATE_NULL_AUDIO);
    REQUIRE(e != nullptr);

    // Settings: same clamping as projects (hearing safety).
    vk_set_param(e, VK_MASTER_GAIN, 5.0f);
    CHECK(vk_get_param(e, VK_MASTER_GAIN) == 1.0f);
    vk_set_param(e, VK_MIC_FEEDBACK, 2.0f);
    CHECK(vk_get_param(e, VK_MIC_FEEDBACK) <= 0.9f);
    vk_set_param(e, VK_BED_ENABLED, 1.0f);
    CHECK(vk_get_param(e, VK_BED_ENABLED) == 1.0f);
    vk_set_param(e, static_cast<vk_param>(999), 1.0f);       // ignored
    CHECK(vk_mic_enabled(e) == 0);                            // never on by itself

    // A Dimension folder with two WAV stems.
    auto dir = test::tempDir("capi");
    auto clip = test::makeSine(48000, 48000 * 6, 330, 0.3f);
    REQUIRE(core::writeWav(dir / "keys.wav", clip.samples, 48000, core::WavSampleFormat::Float32).ok);
    REQUIRE(core::writeWav(dir / "perc.wav", clip.samples, 48000, core::WavSampleFormat::Float32).ok);
    {
        std::ofstream(dir / "world.json") << R"({
          "format": "atmos.world", "schemaVersion": 1, "id": "demo", "name": "Demo", "tier": "free", "artist": "Krowsingre",
          "music": {"tempo": 110, "key": "G minor"}, "fadeSeconds": 0.5,
          "stems": [{"id": "keys", "file": "keys.wav", "title": "Keys"},
                    {"id": "perc", "file": "perc.wav", "title": "Percussion"}],
          "rules": []})";
    }
    const std::string folder = dir.string();
    const char* info = vk_dimension_info(e, folder.c_str());
    REQUIRE(info != nullptr);
    const auto ij = core::Json::parse(info);
    CHECK(ij["name"] == "Demo");
    CHECK(ij["tempo"] == 110.0);
    CHECK(ij["audioAvailable"] == true);
    CHECK(vk_dimension_info(e, (dir / "nope").string().c_str()) == nullptr);

    REQUIRE(vk_start_audio(e) == 1);
    CHECK(vk_audio_running(e) == 1);
    REQUIRE(vk_load_dimension(e, folder.c_str()) == 1);
    vk_world_start(e, 0);
    vk_meters m{};
    for (int i = 0; i < 40; ++i) {   // ~1.2 s of UI ticks
        vk_tick(e, -1.0, &m);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    auto st = core::Json::parse(vk_world_status(e));
    CHECK(st["loaded"] == true);
    CHECK(st["playing"] == true);
    CHECK(st["stems"].size() == 2);
    CHECK(st["spatial"] == true);
    CHECK(st["hrtf"] == (vk_binaural_hrtf() == 1));
    CHECK(m.lunar_day >= 1 && m.lunar_day <= 30);
    CHECK(std::string(m.moon_phase).size() > 0);
    CHECK(m.limiter_gain > 0.0f);

    vk_world_move_stem(e, 0, 450.0f, 0.0f, 40.0f);           // wrapped / clamped like the canvas
    st = core::Json::parse(vk_world_status(e));
    CHECK(std::abs(st["stems"][0]["azimuth"].get<double>() - 90.0) < 1e-3);
    vk_world_reset_positions(e);

    // Recording what you hear.
    const auto wav = dir / "take.wav";
    REQUIRE(vk_start_recording(e, wav.string().c_str(), VK_RECORD_FULL_MIX) == 1);
    CHECK(vk_recording(e) == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(vk_stop_recording(e) > 0.0);
    CHECK(std::filesystem::file_size(wav) > 44);
    CHECK(vk_start_recording(e, wav.string().c_str(), static_cast<vk_record_mode>(9)) == 0);

    // Studio: the user's own stems.
    const std::string a = (dir / "keys.wav").string(), b = (dir / "perc.wav").string();
    const char* paths[] = {a.c_str(), b.c_str()};
    REQUIRE(vk_load_studio_stems(e, paths, 2) == 1);
    CHECK(vk_load_studio_stems(e, paths, 0) == 0);
    CHECK(std::string(vk_last_error(e)).find("between 1 and 8") != std::string::npos);
    vk_world_start(e, 1);
    vk_world_set_stem(e, 1, -6.0f, 1);
    vk_tick(e, 0.5, &m);
    st = core::Json::parse(vk_world_status(e));
    CHECK(st["name"] == "Studio");
    CHECK(st["stems"][0]["title"] == "keys");
    vk_world_stop(e, 1);
    vk_world_unload(e);

    vk_panic(e);
    vk_clear_panic(e);
    vk_stop_audio(e);
    CHECK(vk_audio_running(e) == 0);
    vk_destroy(e);
    std::filesystem::remove_all(dir);
}
