#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioHost.h"
#include "Widgets.h"

class SettingsPage final : public juce::Component, private juce::Timer {
public:
    explicit SettingsPage(AudioHost& host);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    AudioHost& host_;
    juce::AudioDeviceSelectorComponent selector_;
    LabeledSlider ceiling_;
    juce::Label apiLabel_, latencyLabel_, adviceLabel_, privacyLabel_, ceilingNote_;
};
