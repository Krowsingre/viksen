#include "platform/ios/IOSStemDecoder.h"

#import <AVFoundation/AVFoundation.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace atmos::ios {

namespace {

class AVStemDecoder final : public core::StemDecoder {
public:
    AVStemDecoder(AVAudioFile* file, double outRate) : file_(file), outRate_(outRate) {
        AVAudioFormat* in = file.processingFormat; // float32, deinterleaved, file rate
        fileRate_ = in.sampleRate;
        channels_ = static_cast<int>(in.channelCount);
        outFormat_ = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                      sampleRate:outRate
                                                        channels:2
                                                     interleaved:NO];
        AVAudioFormat* stereoIn = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                                   sampleRate:fileRate_
                                                                     channels:2
                                                                  interleaved:NO];
        stereoIn_ = stereoIn;
        if (std::fabs(fileRate_ - outRate) > 0.5) converter_ = [[AVAudioConverter alloc] initFromFormat:stereoIn toFormat:outFormat_];
        inBuf_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:in frameCapacity:kChunk];
        stereoBuf_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:stereoIn frameCapacity:kChunk];
        outBuf_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:outFormat_ frameCapacity:kChunk];
    }

    double outputRate() const override { return outRate_; }
    int64_t lengthFrames() const override {
        return static_cast<int64_t>(std::llround(static_cast<double>(file_.length) * outRate_ / fileRate_));
    }

    bool seek(int64_t frame) override {
        const AVAudioFramePosition src = static_cast<AVAudioFramePosition>(std::llround(frame * fileRate_ / outRate_));
        file_.framePosition = std::clamp<AVAudioFramePosition>(src, 0, file_.length);
        [converter_ reset];
        eof_ = false;
        convEnded_ = false;
        fifo_.clear();
        fifoPos_ = 0;
        return true;
    }

    int read(float* lr, int frames) override {
        @autoreleasepool {
            if (!converter_) {
                int done = 0;
                while (done < frames) {
                    const int got = readStereo(std::min(frames - done, static_cast<int>(kChunk)));
                    if (got <= 0) break;
                    copyOut(stereoBuf_, got, lr + 2 * done);
                    done += got;
                }
                return done;
            }
            // Rate conversion: the converter fills whole output buffers, so keep
            // any surplus in a FIFO for the next call (nothing is dropped).
            while (static_cast<int>(fifo_.size() / 2 - fifoPos_ / 2) < frames && !convEnded_) {
                outBuf_.frameLength = 0;
                NSError* err = nil;
                __block bool ended = false;
                const AVAudioConverterOutputStatus status =
                    [converter_ convertToBuffer:outBuf_ error:&err withInputFromBlock:^AVAudioBuffer*(AVAudioPacketCount n, AVAudioConverterInputStatus* st) {
                        const int r = this->readStereo(static_cast<int>(std::min<AVAudioPacketCount>(n, kChunk)));
                        if (r <= 0) { ended = true; *st = AVAudioConverterInputStatus_EndOfStream; return nil; }
                        *st = AVAudioConverterInputStatus_HaveData;
                        return this->stereoBuf_;
                    }];
                const int got = static_cast<int>(outBuf_.frameLength);
                if (got > 0) {
                    if (fifoPos_ > 0) { fifo_.erase(fifo_.begin(), fifo_.begin() + static_cast<long>(fifoPos_)); fifoPos_ = 0; }
                    const size_t old = fifo_.size();
                    fifo_.resize(old + static_cast<size_t>(got) * 2);
                    copyOut(outBuf_, got, fifo_.data() + old);
                }
                if (status == AVAudioConverterOutputStatus_Error || status == AVAudioConverterOutputStatus_EndOfStream
                    || (ended && got == 0)) convEnded_ = true;
            }
            const int avail = static_cast<int>((fifo_.size() - fifoPos_) / 2);
            const int n = std::min(frames, avail);
            std::copy(fifo_.begin() + static_cast<long>(fifoPos_), fifo_.begin() + static_cast<long>(fifoPos_) + 2 * n, lr);
            fifoPos_ += static_cast<size_t>(2 * n);
            return n;
        }
    }

private:
    static constexpr AVAudioFrameCount kChunk = 4096;

    // Reads up to `frames` source frames into stereoBuf_ (mono duplicated).
    int readStereo(int frames) {
        if (eof_) return 0;
        inBuf_.frameLength = 0;
        NSError* err = nil;
        const AVAudioFrameCount n = static_cast<AVAudioFrameCount>(std::min<int64_t>(frames, file_.length - file_.framePosition));
        if (n == 0 || ![file_ readIntoBuffer:inBuf_ frameCount:n error:&err] || inBuf_.frameLength == 0) { eof_ = true; return 0; }
        const AVAudioFrameCount got = inBuf_.frameLength;
        float* const* src = inBuf_.floatChannelData;
        float* const* dst = stereoBuf_.floatChannelData;
        std::copy(src[0], src[0] + got, dst[0]);
        std::copy(src[channels_ > 1 ? 1 : 0], src[channels_ > 1 ? 1 : 0] + got, dst[1]);
        stereoBuf_.frameLength = got;
        return static_cast<int>(got);
    }

    static void copyOut(AVAudioPCMBuffer* b, int frames, float* lr) {
        const float* l = b.floatChannelData[0];
        const float* r = b.floatChannelData[1];
        for (int i = 0; i < frames; ++i) { lr[2 * i] = l[i]; lr[2 * i + 1] = r[i]; }
    }

    AVAudioFile* file_;
    double outRate_, fileRate_ = 48000.0;
    int channels_ = 2;
    bool eof_ = false;
    bool convEnded_ = false;
    std::vector<float> fifo_;   // interleaved converted frames not yet returned
    size_t fifoPos_ = 0;
    AVAudioFormat* outFormat_;
    AVAudioFormat* stereoIn_;
    AVAudioConverter* converter_ = nil;
    AVAudioPCMBuffer* inBuf_;
    AVAudioPCMBuffer* stereoBuf_;
    AVAudioPCMBuffer* outBuf_;
};

} // namespace

std::unique_ptr<core::StemDecoder> makeStemDecoder(NSURL* url, double outputRate, std::string& error) {
    NSError* err = nil;
    AVAudioFile* file = [[AVAudioFile alloc] initForReading:url commonFormat:AVAudioPCMFormatFloat32 interleaved:NO error:&err];
    if (!file) {
        error = err ? err.localizedDescription.UTF8String : "cannot open stem";
        return nullptr;
    }
    return std::make_unique<AVStemDecoder>(file, outputRate);
}

} // namespace atmos::ios
