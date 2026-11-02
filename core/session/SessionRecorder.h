#pragma once
// Explicit session recording: drains the engine's CaptureTap on a background
// thread into a 24-bit WAV file. Starting and stopping are always user actions.

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

#include "core/assets/WavStreamWriter.h"
#include "engine/audio/CaptureTap.h"

namespace atmos::core {

class SessionRecorder {
public:
    ~SessionRecorder() { stop(); }
    bool start(CaptureTap& tap, CaptureMode mode, const std::filesystem::path& file, double sampleRate,
               std::string* error = nullptr);
    // Stops capture, writes everything still buffered, finalises the file.
    // Returns the recorded length in seconds (0 if nothing was recording).
    double stop();
    bool recording() const { return running_.load(); }
    double seconds() const;
    uint64_t droppedBlocks() const { return tap_ ? tap_->overruns() : 0; }
    std::filesystem::path file() const { return path_; }

private:
    void run();
    CaptureTap* tap_ = nullptr;
    WavStreamWriter writer_;
    std::filesystem::path path_;
    double rate_ = 48000.0;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> frames_{0};
    std::thread thread_;
};

} // namespace atmos::core
