#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "AudioHost.h"
#include "Widgets.h"

// Desktop Experience: music, atmosphere and environment with independent levels.
class ExperiencePage final : public juce::Component, private juce::Timer {
public:
    explicit ExperiencePage(AudioHost& host);
    ~ExperiencePage() override;

    void loadFile(const juce::File& file);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseFile();
    void setStatus(const juce::String& s, bool isError = false);
    void refreshTrackControls();

    AudioHost& host_;
    std::unique_ptr<juce::FileChooser> chooser_;

    // Header
    juce::Label routeLabel_, statusLabel_;
    juce::TextButton stopAllButton_{"Stop all sound"};
    bool micLive_ = false, limiting_ = false;

    // Music
    juce::TextButton importButton_{"Import song..."}, playButton_{"Play"}, removeButton_{"Remove"};
    juce::Label trackLabel_, timeLabel_;
    juce::Slider positionSlider_;
    juce::ToggleButton loopToggle_{"Loop"};
    bool userDraggingPosition_ = false;

    // Atmosphere
    juce::ToggleButton bedToggle_{"Atmosphere bed"};
    LabeledSlider bedDensity_, bedColour_;

    // Environment
    juce::ToggleButton micToggle_{"Microphone"};
    juce::Label envLabel_, privacyLabel_;
    LabeledSlider micTone_, micEcho_, micFeedback_, micWet_;

    // Levels
    std::vector<std::unique_ptr<LabeledSlider>> busSliders_;
    LabeledSlider master_;
    juce::Label awarenessLabel_;

    juce::Rectangle<int> musicArea_, atmosArea_, envArea_, levelsArea_, headerArea_;
};
