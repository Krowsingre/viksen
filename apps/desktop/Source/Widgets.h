#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>

#include "Theme.h"

// Peak meter, -60..0 dBFS.
class PeakMeter final : public juce::Component {
public:
    explicit PeakMeter(juce::Colour c) : colour_(c) {}
    void setPeak(float p) {
        if (std::abs(p - peak_) > 1e-4f) { peak_ = p; repaint(); }
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff252836));
        g.fillRoundedRectangle(r, r.getHeight() / 2);
        const float db = peak_ > 0 ? 20.0f * std::log10(peak_) : -100.0f;
        const float frac = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour(colour_.withAlpha(0.9f));
        g.fillRoundedRectangle(r.withWidth(r.getWidth() * frac), r.getHeight() / 2);
    }

private:
    juce::Colour colour_;
    float peak_ = 0.0f;
};

// Label + horizontal slider (+ optional meter) in one row.
class LabeledSlider final : public juce::Component {
public:
    LabeledSlider(const juce::String& name, double min, double max, double value, std::function<void(double)> onChange,
                  juce::Colour colour = theme::accent, bool withMeter = false, const juce::String& suffix = {})
        : meter_(colour), hasMeter_(withMeter) {
        label_.setText(name, juce::dontSendNotification);
        label_.setColour(juce::Label::textColourId, theme::text);
        addAndMakeVisible(label_);
        slider_.setSliderStyle(juce::Slider::LinearHorizontal);
        slider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        slider_.setRange(min, max);
        slider_.setNumDecimalPlacesToDisplay(max - min > 100.0 ? 0 : 2);
        slider_.setTextValueSuffix(suffix);
        slider_.setValue(value, juce::dontSendNotification);
        slider_.setColour(juce::Slider::trackColourId, colour);
        slider_.onValueChange = [this, onChange] { onChange(slider_.getValue()); };
        slider_.setTitle(name); // accessibility
        addAndMakeVisible(slider_);
        if (hasMeter_) addAndMakeVisible(meter_);
    }

    juce::Slider& slider() noexcept { return slider_; }
    void setPeak(float p) { meter_.setPeak(p); }
    void setValueFormatter(std::function<juce::String(double)> f) {
        slider_.textFromValueFunction = std::move(f);
        slider_.updateText();
    }

    void resized() override {
        auto r = getLocalBounds();
        label_.setBounds(r.removeFromLeft(130));
        if (hasMeter_) {
            auto m = r.removeFromBottom(6);
            meter_.setBounds(m.withTrimmedRight(84));
        }
        slider_.setBounds(r);
    }

private:
    juce::Label label_;
    juce::Slider slider_;
    PeakMeter meter_;
    bool hasMeter_;
};

// Section heading drawn in the page.
inline void drawSection(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title) {
    g.setColour(theme::panel);
    g.fillRoundedRectangle(r.toFloat(), 8.0f);
    g.setColour(theme::dim);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(title.toUpperCase(), r.reduced(14, 8).removeFromTop(18), juce::Justification::topLeft);
}

inline juce::String gainText(double v) {
    return v <= 0.0 ? juce::String("off") : juce::String(20.0 * std::log10(v), 1) + " dB";
}
