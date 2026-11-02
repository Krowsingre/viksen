#include "platform/ios/IOSMediaDecoder.h"

#import <AVFoundation/AVFoundation.h>

#include <algorithm>
#include <cstdint>

namespace atmos::ios {

bool decodeAudioFile(NSURL* url, AudioClip& clip, core::AudioFileInfo& info, std::string& error) {
    @autoreleasepool {
        NSError* err = nil;
        AVAudioFile* file = [[AVAudioFile alloc] initForReading:url
                                                   commonFormat:AVAudioPCMFormatFloat32
                                                    interleaved:NO
                                                          error:&err];
        if (!file) {
            error = err ? err.localizedDescription.UTF8String : "cannot open audio file";
            return false;
        }
        const AVAudioFramePosition length = file.length;
        if (length <= 0 || length > INT32_MAX) { error = "unsupported file length"; return false; }
        AVAudioFormat* fmt = file.processingFormat;
        AVAudioPCMBuffer* buf = [[AVAudioPCMBuffer alloc] initWithPCMFormat:fmt frameCapacity:(AVAudioFrameCount)length];
        if (!buf || ![file readIntoBuffer:buf error:&err]) {
            error = err ? err.localizedDescription.UTF8String : "decode failed";
            return false;
        }
        const int channels = static_cast<int>(fmt.channelCount);
        const int frames = static_cast<int>(buf.frameLength);
        clip.sampleRate = fmt.sampleRate;
        clip.samples.setSize(channels, frames);
        for (int ch = 0; ch < channels; ++ch)
            std::copy(buf.floatChannelData[ch], buf.floatChannelData[ch] + frames, clip.samples.channel(ch));

        info.sampleRate = fmt.sampleRate;
        info.numChannels = channels;
        info.numFrames = frames;
        info.bitsPerSample = static_cast<int>(file.fileFormat.streamDescription->mBitsPerChannel);
        info.isFloat = false;
        return true;
    }
}

} // namespace atmos::ios
