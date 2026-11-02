#pragma once
// Streaming 24-bit PCM WAV writer for long recordings: the header is written
// first and patched with the final sizes on close(). Not for the audio thread.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace atmos::core {

class WavStreamWriter {
public:
    ~WavStreamWriter() { close(); }
    bool open(const std::filesystem::path& path, double sampleRate, int channels, std::string* error = nullptr);
    bool write(const float* interleaved, int frames);   // false when the 4 GiB RIFF limit is reached
    bool close();                                        // patches the header; safe to call twice
    int64_t framesWritten() const { return frames_; }
    bool isOpen() const { return file_.is_open(); }

private:
    std::ofstream file_;
    int channels_ = 2;
    int64_t frames_ = 0;
};

} // namespace atmos::core
