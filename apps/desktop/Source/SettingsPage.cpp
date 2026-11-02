#include "SettingsPage.h"

#include "platform/desktop/DesktopAudioDefaults.h"

SettingsPage::SettingsPage(AudioHost& host)
    : host_(host),
      selector_(host.devices(), 0, 1, 2, 2, false, false, true, false),
      ceiling_("Output ceiling", -24.0, -1.0, host.session().settings().limiterCeilingDb,
               [this](double v) { host_.session().setLimiterCeilingDb(static_cast<float>(v)); }, theme::accent, false, " dBFS") {
    addAndMakeVisible(selector_);
    addAndMakeVisible(ceiling_);
    for (auto* l : {&apiLabel_, &latencyLabel_, &adviceLabel_, &privacyLabel_, &ceilingNote_}) {
        l->setColour(juce::Label::textColourId, theme::dim);
        l->setFont(juce::FontOptions(14.0f));
        l->setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(*l);
    }
    apiLabel_.setColour(juce::Label::textColourId, theme::text);
    adviceLabel_.setText(atmos::desktop::lowLatencyAdvice(), juce::dontSendNotification);
    ceilingNote_.setText("A safety limiter on the final output never lets the signal exceed this level.", juce::dontSendNotification);
    privacyLabel_.setText("Privacy: all processing is local. Nothing is recorded unless you explicitly start a recording, "
                          "nothing is uploaded, and no account is required.", juce::dontSendNotification);
    startTimerHz(4);
}

void SettingsPage::timerCallback() {
    juce::String api = "Audio API: " + atmos::desktop::platformAudioApiDescription() + "  |  Driver: " +
                       host_.devices().getCurrentAudioDeviceType();
    apiLabel_.setText(api, juce::dontSendNotification);
    if (auto* d = host_.devices().getCurrentAudioDevice()) {
        const double rate = d->getCurrentSampleRate();
        const int buffer = d->getCurrentBufferSizeSamples();
        const int total = d->getInputLatencyInSamples() + d->getOutputLatencyInSamples() + 2 * buffer;
        latencyLabel_.setText("Reported round trip: " + juce::String(1000.0 * total / rate, 1) +
                                  " ms (driver-reported, not measured)  |  Dropouts: " + juce::String(d->getXRunCount()),
                              juce::dontSendNotification);
    } else {
        latencyLabel_.setText(host_.lastDeviceError().isNotEmpty() ? host_.lastDeviceError() : "No device open",
                              juce::dontSendNotification);
    }
}

void SettingsPage::paint(juce::Graphics& g) {
    g.fillAll(theme::background);
    g.setColour(theme::text);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("Settings", getLocalBounds().reduced(24).removeFromTop(40), juce::Justification::centredLeft);
}

void SettingsPage::resized() {
    auto r = getLocalBounds().reduced(24);
    r.removeFromTop(48);
    auto left = r.removeFromLeft(juce::jmin(560, r.getWidth() / 2));
    selector_.setBounds(left.removeFromTop(360));
    r.removeFromLeft(24);
    apiLabel_.setBounds(r.removeFromTop(24));
    latencyLabel_.setBounds(r.removeFromTop(24));
    adviceLabel_.setBounds(r.removeFromTop(64));
    r.removeFromTop(16);
    ceiling_.setBounds(r.removeFromTop(32));
    ceilingNote_.setBounds(r.removeFromTop(24));
    r.removeFromTop(16);
    privacyLabel_.setBounds(r.removeFromTop(48));
}
