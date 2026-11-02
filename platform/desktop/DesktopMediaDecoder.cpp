#include "platform/desktop/DesktopMediaDecoder.h"

#include <algorithm>
#include <limits>

namespace atmos::desktop {

bool decodeAudioFile(juce::AudioFormatManager& formats, const juce::File& file, AudioClip& clip,
                     core::AudioFileInfo& info, std::string& error) {
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) {
        error = "Unsupported or unreadable audio file: " + file.getFileName().toStdString();
        return false;
    }
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max()) {
        error = "Unsupported file length";
        return false;
    }
    const int channels = static_cast<int>(reader->numChannels);
    const int frames = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> tmp(channels, frames);
    if (!reader->read(&tmp, 0, frames, 0, true, true)) {
        error = "Decoding failed";
        return false;
    }
    clip.sampleRate = reader->sampleRate;
    clip.samples.setSize(channels, frames);
    for (int ch = 0; ch < channels; ++ch)
        std::copy(tmp.getReadPointer(ch), tmp.getReadPointer(ch) + frames, clip.samples.channel(ch));
    info.sampleRate = reader->sampleRate;
    info.numChannels = channels;
    info.numFrames = frames;
    info.bitsPerSample = static_cast<int>(reader->bitsPerSample);
    info.isFloat = reader->usesFloatingPointData;
    return true;
}

} // namespace atmos::desktop
