#include "core/session/SessionRecorder.h"

#include <chrono>
#include <vector>

namespace atmos::core {

bool SessionRecorder::start(CaptureTap& tap, CaptureMode mode, const std::filesystem::path& file, double sampleRate,
                            std::string* error) {
    stop();
    if (mode == CaptureMode::Off) { if (error) *error = "no capture mode"; return false; }
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (!writer_.open(file, sampleRate, 2, error)) return false;
    tap_ = &tap;
    path_ = file;
    rate_ = sampleRate;
    frames_.store(0);
    float drain[2048];
    while (tap.pop(drain, 1024) > 0) {}   // discard anything left from an earlier session
    tap.resetCounters();
    running_.store(true);
    thread_ = std::thread([this] { run(); });
    tap.setMode(mode);                    // capture begins only now
    return true;
}

void SessionRecorder::run() {
    std::vector<float> buf(8192 * 2);
    while (true) {
        const bool more = running_.load();
        int n;
        while ((n = tap_->pop(buf.data(), 8192)) > 0) {
            if (!writer_.write(buf.data(), n)) { running_.store(false); break; }  // 4 GiB limit
            frames_.fetch_add(n);
        }
        if (!more) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

double SessionRecorder::stop() {
    if (!tap_) return 0.0;
    tap_->setMode(CaptureMode::Off);
    running_.store(false);
    if (thread_.joinable()) thread_.join();   // the thread drains the remainder before exiting
    writer_.close();
    tap_ = nullptr;
    return seconds();
}

double SessionRecorder::seconds() const { return rate_ > 0 ? static_cast<double>(frames_.load()) / rate_ : 0.0; }

} // namespace atmos::core
