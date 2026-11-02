#include "platform/ios/IOSAudioIO.h"

#include <algorithm>

#include "engine/audio/AudioEngine.h"

namespace atmos::ios {

namespace {
constexpr AudioUnitElement kOutputBus = 0;
constexpr AudioUnitElement kInputBus = 1;

AudioStreamBasicDescription floatFormat(double sampleRate, UInt32 channels) {
    AudioStreamBasicDescription f{};
    f.mSampleRate = sampleRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    f.mBitsPerChannel = 32;
    f.mChannelsPerFrame = channels;
    f.mFramesPerPacket = 1;
    f.mBytesPerFrame = 4; // per channel (non-interleaved)
    f.mBytesPerPacket = 4;
    return f;
}

bool check(OSStatus s, const char* what, std::string& error) {
    if (s == noErr) return true;
    error = std::string(what) + " failed (OSStatus " + std::to_string(static_cast<int>(s)) + ")";
    return false;
}
} // namespace

bool IOSAudioIO::start(double sampleRate, bool withInput, std::string& error) {
    stop();
    withInput_ = withInput;
    inputBuffer_.assign(kMaxFramesPerSlice, 0.0f);

    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) { error = "RemoteIO component not found"; return false; }
    if (!check(AudioComponentInstanceNew(comp, &unit_), "AudioComponentInstanceNew", error)) { unit_ = nullptr; return false; }

    auto fail = [&]() { AudioComponentInstanceDispose(unit_); unit_ = nullptr; return false; };

    UInt32 one = 1, enableInput = withInput ? 1 : 0;
    if (!check(AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, kOutputBus, &one, sizeof one), "EnableIO(out)", error)) return fail();
    if (!check(AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, kInputBus, &enableInput, sizeof enableInput), "EnableIO(in)", error)) return fail();

    const auto outFmt = floatFormat(sampleRate, 2);
    if (!check(AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, kOutputBus, &outFmt, sizeof outFmt), "StreamFormat(out)", error)) return fail();
    if (withInput) {
        const auto inFmt = floatFormat(sampleRate, 1);
        if (!check(AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, kInputBus, &inFmt, sizeof inFmt), "StreamFormat(in)", error)) return fail();
    }
    UInt32 maxFrames = kMaxFramesPerSlice;
    AudioUnitSetProperty(unit_, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof maxFrames);

    AURenderCallbackStruct cb{renderCallback, this};
    if (!check(AudioUnitSetProperty(unit_, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, kOutputBus, &cb, sizeof cb), "SetRenderCallback", error)) return fail();
    if (!check(AudioUnitInitialize(unit_), "AudioUnitInitialize", error)) return fail();
    if (!check(AudioOutputUnitStart(unit_), "AudioOutputUnitStart", error)) { AudioUnitUninitialize(unit_); return fail(); }
    return true;
}

void IOSAudioIO::stop() {
    if (!unit_) return;
    AudioOutputUnitStop(unit_);
    AudioUnitUninitialize(unit_);
    AudioComponentInstanceDispose(unit_);
    unit_ = nullptr;
}

OSStatus IOSAudioIO::renderCallback(void* refCon, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                                    UInt32 /*bus*/, UInt32 frames, AudioBufferList* ioData) {
    auto* self = static_cast<IOSAudioIO*>(refCon);
    const int n = static_cast<int>(std::min<UInt32>(frames, kMaxFramesPerSlice));

    const float* inputs[1] = {nullptr};
    int numInputs = 0;
    if (self->withInput_) {
        AudioBufferList in{};
        in.mNumberBuffers = 1;
        in.mBuffers[0].mNumberChannels = 1;
        in.mBuffers[0].mDataByteSize = static_cast<UInt32>(n * sizeof(float));
        in.mBuffers[0].mData = self->inputBuffer_.data();
        if (AudioUnitRender(self->unit_, flags, ts, kInputBus, static_cast<UInt32>(n), &in) == noErr) {
            inputs[0] = self->inputBuffer_.data();
            numInputs = 1;
        }
    }

    float* outputs[8] = {};
    const int numOutputs = static_cast<int>(std::min<UInt32>(ioData->mNumberBuffers, 8));
    for (int ch = 0; ch < numOutputs; ++ch) outputs[ch] = static_cast<float*>(ioData->mBuffers[ch].mData);
    self->engine_.process(numInputs ? inputs : nullptr, numInputs, outputs, numOutputs, n);
    return noErr;
}

} // namespace atmos::ios
