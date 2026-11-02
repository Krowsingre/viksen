#pragma once
// RemoteIO audio unit host (iOS). One render callback pulls microphone input
// (when enabled) and renders the engine output in the same cycle, which is the
// lowest-latency full-duplex path available on iOS.
//
// The engine is not owned. Call start()/stop() from the main thread only; the
// AVAudioSession must be configured and active before start().

#include <AudioToolbox/AudioToolbox.h>

#include <atomic>
#include <string>
#include <vector>

namespace atmos { class AudioEngine; }

namespace atmos::ios {

class IOSAudioIO {
public:
    static constexpr int kMaxFramesPerSlice = 4096;

    explicit IOSAudioIO(AudioEngine& engine) : engine_(engine) {}
    ~IOSAudioIO() { stop(); }

    // withInput enables the mic bus. Changing it requires stop() + start().
    bool start(double sampleRate, bool withInput, std::string& error);
    void stop();
    bool isRunning() const noexcept { return unit_ != nullptr; }
    bool inputEnabled() const noexcept { return withInput_; }

private:
    static OSStatus renderCallback(void* refCon, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                                   UInt32 bus, UInt32 frames, AudioBufferList* ioData);

    AudioEngine& engine_;
    AudioUnit unit_ = nullptr;
    bool withInput_ = false;
    std::vector<float> inputBuffer_;
};

} // namespace atmos::ios
