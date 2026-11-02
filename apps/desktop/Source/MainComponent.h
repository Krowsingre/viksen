#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "AudioHost.h"

class ExperiencePage;

// Sidebar (Studio, Experience, Worlds, Library, Settings) + content area.
class MainComponent final : public juce::Component, public juce::FileDragAndDropTarget {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    enum Page { Studio = 0, Experience, Worlds, Library, Settings, NumPages };
    void show(Page p);

    AudioHost host_;
    std::array<juce::TextButton, NumPages> navButtons_;
    std::array<std::unique_ptr<juce::Component>, NumPages> pages_;
    ExperiencePage* experience_ = nullptr;
    Page current_ = Experience;
    juce::Label startupError_;
};
