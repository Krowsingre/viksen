#pragma once
// Desktop audio host on miniaudio (public domain / MIT-0): WASAPI on Windows,
// Core Audio on macOS, ALSA/PulseAudio on Linux (development). Replaces JUCE's
// device layer. Drives a LiveSession's engine from the device callback.
// Control-thread API; not thread-safe against itself.

#include <memory>
#include <string>
#include <vector>

#include "core/session/LiveSession.h"
#include "core/worlds/WorldFeeder.h"

namespace atmos::desktop {

struct AudioDeviceInfo {
    std::string name;
    bool isDefault = false;
};

struct HostOptions {
    int playbackDevice = -1;        // index into playbackDevices(); -1 = system default
    int captureDevice = -1;         // index into captureDevices(); -1 = system default
    bool withInput = false;         // open the microphone (only after the user asks)
    unsigned periodFrames = 256;    // requested buffer size; the device may choose another
    bool nullBackend = false;       // tests: no hardware, the callback runs on a timer
};

class MiniaudioHost {
public:
    explicit MiniaudioHost(core::LiveSession& session);
    ~MiniaudioHost();
    MiniaudioHost(const MiniaudioHost&) = delete;
    MiniaudioHost& operator=(const MiniaudioHost&) = delete;

    std::vector<AudioDeviceInfo> playbackDevices();
    std::vector<AudioDeviceInfo> captureDevices();

    // Opens the device at its native rate and rebuilds the engine for it if needed.
    bool start(const HostOptions& options, std::string& error);
    void stop();
    bool running() const;
    double sampleRate() const;
    unsigned periodFrames() const;
    std::string backendName() const;
    std::string deviceName() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Streams a stem (WAV, FLAC, MP3) at a fixed output rate, for Studio and Dimensions.
// Returns nullptr and sets error if the file cannot be decoded.
std::unique_ptr<core::StemDecoder> makeMiniaudioStemDecoder(const std::string& path, double outputRate, std::string& error);

} // namespace atmos::desktop
