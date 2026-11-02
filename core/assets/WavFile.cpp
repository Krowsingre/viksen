#include "core/assets/WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

namespace atmos::core {

namespace {

constexpr uint16_t kFormatPcm = 1;
constexpr uint16_t kFormatFloat = 3;
constexpr uint16_t kFormatExtensible = 0xFFFE;

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

struct Parsed {
    AudioFileInfo info;
    uint16_t format = 0;
    std::streamoff dataOffset = 0;
    uint64_t dataBytes = 0;
};

WavResult fail(const std::string& msg) { return {false, msg}; }

WavResult parseHeader(std::ifstream& f, Parsed& p) {
    uint8_t riff[12];
    if (!f.read(reinterpret_cast<char*>(riff), 12)) return fail("file too short");
    if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) return fail("not a RIFF/WAVE file");

    bool haveFmt = false, haveData = false;
    uint16_t blockAlign = 0;
    while (!haveData) {
        uint8_t hdr[8];
        if (!f.read(reinterpret_cast<char*>(hdr), 8)) break;
        const uint32_t size = le32(hdr + 4);
        const std::streamoff bodyStart = f.tellg();
        if (std::memcmp(hdr, "fmt ", 4) == 0) {
            if (size < 16) return fail("fmt chunk too small");
            std::vector<uint8_t> fmt(size);
            if (!f.read(reinterpret_cast<char*>(fmt.data()), size)) return fail("truncated fmt chunk");
            p.format = le16(&fmt[0]);
            p.info.numChannels = le16(&fmt[2]);
            p.info.sampleRate = le32(&fmt[4]);
            blockAlign = le16(&fmt[12]);
            p.info.bitsPerSample = le16(&fmt[14]);
            if (p.format == kFormatExtensible) {
                if (size < 40) return fail("extensible fmt chunk too small");
                p.format = le16(&fmt[24]); // first two bytes of the sub-format GUID
            }
            haveFmt = true;
        } else if (std::memcmp(hdr, "data", 4) == 0) {
            if (!haveFmt) return fail("data chunk before fmt chunk");
            p.dataOffset = bodyStart;
            f.seekg(0, std::ios::end);
            const std::streamoff fileEnd = f.tellg();
            // Tolerate files whose data size is wrong/0xFFFFFFFF (streamed writers).
            p.dataBytes = std::min<uint64_t>(size, static_cast<uint64_t>(fileEnd - bodyStart));
            haveData = true;
            break;
        }
        f.seekg(bodyStart + static_cast<std::streamoff>(size + (size & 1)));
    }
    if (!haveFmt) return fail("missing fmt chunk");
    if (!haveData) return fail("missing data chunk");
    if (p.info.numChannels <= 0) return fail("invalid channel count");
    if (p.info.sampleRate <= 0) return fail("invalid sample rate");
    if (p.format == kFormatPcm) {
        if (p.info.bitsPerSample != 8 && p.info.bitsPerSample != 16 && p.info.bitsPerSample != 24 && p.info.bitsPerSample != 32)
            return fail("unsupported PCM bit depth");
    } else if (p.format == kFormatFloat) {
        if (p.info.bitsPerSample != 32 && p.info.bitsPerSample != 64) return fail("unsupported float bit depth");
        p.info.isFloat = true;
    } else {
        return fail("unsupported WAV encoding (only PCM and IEEE float)");
    }
    const int bytesPerFrame = p.info.numChannels * (p.info.bitsPerSample / 8);
    if (blockAlign != bytesPerFrame) return fail("inconsistent block alignment");
    p.info.numFrames = static_cast<int64_t>(p.dataBytes / static_cast<uint64_t>(bytesPerFrame));
    return {true, {}};
}

float decodeSample(const uint8_t* s, int bits, bool isFloat) {
    if (isFloat) {
        if (bits == 32) { float v; std::memcpy(&v, s, 4); return v; }
        double v; std::memcpy(&v, s, 8); return static_cast<float>(v);
    }
    switch (bits) {
        case 8: return (static_cast<int>(s[0]) - 128) / 128.0f;
        case 16: return static_cast<int16_t>(le16(s)) / 32768.0f;
        case 24: {
            int32_t v = static_cast<int32_t>(s[0] | (s[1] << 8) | (s[2] << 16));
            if (v & 0x800000) v |= ~0xFFFFFF;
            return static_cast<float>(v) / 8388608.0f;
        }
        default: return static_cast<float>(static_cast<int32_t>(le32(s)) / 2147483648.0);
    }
}

} // namespace

WavResult readWavInfo(const std::filesystem::path& path, AudioFileInfo& info) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return fail("cannot open file");
    Parsed p;
    auto r = parseHeader(f, p);
    if (r.ok) info = p.info;
    return r;
}

WavResult readWav(const std::filesystem::path& path, AudioClip& clip, AudioFileInfo* infoOut) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return fail("cannot open file");
    Parsed p;
    auto r = parseHeader(f, p);
    if (!r.ok) return r;
    if (p.info.numFrames > (1ll << 31) - 1) return fail("file too long for in-memory clip");

    const int ch = p.info.numChannels;
    const int bps = p.info.bitsPerSample / 8;
    const int frames = static_cast<int>(p.info.numFrames);
    clip.sampleRate = p.info.sampleRate;
    clip.samples.setSize(ch, frames);

    f.seekg(p.dataOffset);
    constexpr int kChunkFrames = 8192;
    std::vector<uint8_t> raw(static_cast<size_t>(kChunkFrames) * ch * bps);
    for (int done = 0; done < frames; done += kChunkFrames) {
        const int n = std::min(kChunkFrames, frames - done);
        if (!f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(n) * ch * bps))
            return fail("truncated data chunk");
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c)
                clip.samples.channel(c)[done + i] =
                    decodeSample(&raw[(static_cast<size_t>(i) * ch + c) * bps], p.info.bitsPerSample, p.info.isFloat);
    }
    if (infoOut) *infoOut = p.info;
    return {true, {}};
}

WavResult writeWav(const std::filesystem::path& path, const AudioBuffer& audio, double sampleRate,
                   WavSampleFormat format, int64_t numFramesArg) {
    const int ch = audio.numChannels();
    const int64_t frames = numFramesArg < 0 ? audio.numFrames() : std::min<int64_t>(numFramesArg, audio.numFrames());
    if (ch <= 0) return fail("no channels");
    const int bits = format == WavSampleFormat::Int16 ? 16 : format == WavSampleFormat::Int24 ? 24 : 32;
    const int bps = bits / 8;
    const uint64_t dataBytes = static_cast<uint64_t>(frames) * ch * bps;
    if (dataBytes > 0xFFFFFFFFull - 36) return fail("render too large for RIFF (4 GiB limit); RF64 not yet supported");

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return fail("cannot create file");

    auto put16 = [&](uint16_t v) { uint8_t b[2] = {uint8_t(v), uint8_t(v >> 8)}; f.write(reinterpret_cast<char*>(b), 2); };
    auto put32 = [&](uint32_t v) {
        uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        f.write(reinterpret_cast<char*>(b), 4);
    };

    f.write("RIFF", 4); put32(static_cast<uint32_t>(36 + dataBytes)); f.write("WAVE", 4);
    f.write("fmt ", 4); put32(16);
    put16(format == WavSampleFormat::Float32 ? kFormatFloat : kFormatPcm);
    put16(static_cast<uint16_t>(ch));
    put32(static_cast<uint32_t>(std::lround(sampleRate)));
    put32(static_cast<uint32_t>(std::lround(sampleRate)) * ch * bps);
    put16(static_cast<uint16_t>(ch * bps));
    put16(static_cast<uint16_t>(bits));
    f.write("data", 4); put32(static_cast<uint32_t>(dataBytes));

    std::vector<uint8_t> row(static_cast<size_t>(ch) * bps);
    for (int64_t i = 0; i < frames; ++i) {
        for (int c = 0; c < ch; ++c) {
            const float s = audio.channel(c)[i];
            uint8_t* d = &row[static_cast<size_t>(c) * bps];
            if (format == WavSampleFormat::Float32) {
                std::memcpy(d, &s, 4);
            } else {
                const float clamped = std::clamp(std::isfinite(s) ? s : 0.0f, -1.0f, 1.0f);
                if (format == WavSampleFormat::Int16) {
                    const int32_t v = std::clamp<int32_t>(static_cast<int32_t>(std::lround(clamped * 32767.0f)), -32768, 32767);
                    d[0] = uint8_t(v); d[1] = uint8_t(v >> 8);
                } else {
                    const int32_t v = std::clamp<int32_t>(static_cast<int32_t>(std::lround(clamped * 8388607.0)), -8388608, 8388607);
                    d[0] = uint8_t(v); d[1] = uint8_t(v >> 8); d[2] = uint8_t(v >> 16);
                }
            }
        }
        f.write(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size()));
    }
    if (!f) return fail("write failed");
    return {true, {}};
}

} // namespace atmos::core
