#pragma once
// Honest status for workspaces that are not built yet. Deliberately has no
// interactive controls (spec §12: no fake controls).
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

class PlaceholderPage final : public juce::Component {
public:
    PlaceholderPage(juce::String title, juce::String phase, juce::String summary, juce::StringArray planned)
        : title_(std::move(title)), phase_(std::move(phase)), summary_(std::move(summary)), planned_(std::move(planned)) {}

    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().reduced(32);
        g.setColour(theme::text);
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.drawText(title_, r.removeFromTop(40), juce::Justification::left);
        g.setColour(theme::accent);
        g.setFont(juce::FontOptions(14.0f));
        g.drawText(phase_, r.removeFromTop(24), juce::Justification::left);
        r.removeFromTop(12);
        g.setColour(theme::text);
        g.setFont(juce::FontOptions(16.0f));
        g.drawFittedText(summary_, r.removeFromTop(70), juce::Justification::topLeft, 4);
        r.removeFromTop(8);
        g.setColour(theme::dim);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("PLANNED", r.removeFromTop(22), juce::Justification::left);
        g.setFont(juce::FontOptions(15.0f));
        for (auto& item : planned_) g.drawText(juce::String::fromUTF8("\xe2\x80\xa2  ") + item, r.removeFromTop(26), juce::Justification::left);
    }

private:
    juce::String title_, phase_, summary_;
    juce::StringArray planned_;
};
