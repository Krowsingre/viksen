#pragma once
// JUCE-based decoder shared by the Windows and macOS apps. With
// registerBasicFormats(): WAV, AIFF, FLAC, Ogg Vorbis, MP3 (JUCE decoder), plus
// AAC/M4A/ALAC via Core Audio on macOS. AAC/M4A on Windows needs a Media
// Foundation adapter (not implemented yet). Never modifies the file.

#include <juce_audio_formats/juce_audio_formats.h>

#include <string>

#include "core/assets/WavFile.h"
#include "engine/audio/AudioClip.h"

namespace atmos::desktop {

// `formats` must already have formats registered; it is only read here, so one
// manager can serve a background decoding thread.
bool decodeAudioFile(juce::AudioFormatManager& formats, const juce::File& file, AudioClip& clip,
                     core::AudioFileInfo& info, std::string& error);

} // namespace atmos::desktop
