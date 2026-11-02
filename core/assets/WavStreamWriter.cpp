#include "core/assets/WavStreamWriter.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace atmos::core {

namespace {
void put16(std::ofstream& f, uint16_t v) { char b[2] = {char(v & 0xFF), char(v >> 8)}; f.write(b, 2); }
void put32(std::ofstream& f, uint32_t v) {
    char b[4] = {char(v & 0xFF), char((v >> 8) & 0xFF), char((v >> 16) & 0xFF), char(v >> 24)};
    f.write(b, 4);
}
} // namespace

bool WavStreamWriter::open(const std::filesystem::path& path, double sampleRate, int channels, std::string* error) {
    close();
    channels_ = channels;
    frames_ = 0;
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_) { if (error) *error = "cannot create " + path.string(); return false; }
    const uint32_t rate = static_cast<uint32_t>(std::lround(sampleRate));
    file_.write("RIFF", 4); put32(file_, 36); file_.write("WAVE", 4);
    file_.write("fmt ", 4); put32(file_, 16); put16(file_, 1); put16(file_, static_cast<uint16_t>(channels));
    put32(file_, rate); put32(file_, rate * static_cast<uint32_t>(channels) * 3);
    put16(file_, static_cast<uint16_t>(channels * 3)); put16(file_, 24);
    file_.write("data", 4); put32(file_, 0);
    return static_cast<bool>(file_);
}

bool WavStreamWriter::write(const float* in, int frames) {
    if (!file_.is_open() || frames <= 0) return file_.is_open();
    const uint64_t bytesAfter = static_cast<uint64_t>(frames_ + frames) * static_cast<uint64_t>(channels_) * 3;
    if (bytesAfter > 0xFFFFFFFFull - 36) return false;
    std::vector<char> buf(static_cast<size_t>(frames) * static_cast<size_t>(channels_) * 3);
    for (size_t i = 0; i < static_cast<size_t>(frames) * static_cast<size_t>(channels_); ++i) {
        const float s = std::isfinite(in[i]) ? std::clamp(in[i], -1.0f, 1.0f) : 0.0f;
        const int32_t v = std::clamp<int32_t>(static_cast<int32_t>(std::lround(s * 8388607.0)), -8388608, 8388607);
        buf[3 * i] = char(v & 0xFF);
        buf[3 * i + 1] = char((v >> 8) & 0xFF);
        buf[3 * i + 2] = char((v >> 16) & 0xFF);
    }
    file_.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    frames_ += frames;
    return static_cast<bool>(file_);
}

bool WavStreamWriter::close() {
    if (!file_.is_open()) return true;
    const uint32_t dataBytes = static_cast<uint32_t>(frames_ * channels_ * 3);
    file_.seekp(4); put32(file_, 36 + dataBytes);
    file_.seekp(40); put32(file_, dataBytes);
    const bool ok = static_cast<bool>(file_);
    file_.close();
    return ok;
}

} // namespace atmos::core
