#include "ExperiencePage.h"

#include "platform/desktop/DesktopAudioDefaults.h"

using atmos::Bus;

namespace {
const char* busTitle(Bus b) {
    switch (b) {
        case Bus::Music: return "Music";
        case Bus::Atmosphere: return "Atmosphere";
        case Bus::MicRaw: return "Raw mic";
        case Bus::MicTransformed: return "Transformed mic";
        case Bus::Count: break;
    }
    return "?";
}
juce::String timeString(double s) {
    const int t = static_cast<int>(s);
    return juce::String(t / 60) + ":" + juce::String(t % 60).paddedLeft('0', 2);
}
void styleLabel(juce::Label& l, juce::Colour c = theme::dim, float size = 14.0f) {
    l.setColour(juce::Label::textColourId, c);
    l.setFont(juce::FontOptions(size));
}
} // namespace

ExperiencePage::ExperiencePage(AudioHost& host)
    : host_(host),
      bedDensity_("Density", 0.0, 0.6, host.session().settings().bedLevel,
                  [this](double v) { host_.session().setBedLevel(static_cast<float>(v)); }, theme::atmosphere),
      bedColour_("Colour", 150.0, 6000.0, host.session().settings().bedColourHz,
                 [this](double v) { host_.session().setBedColourHz(static_cast<float>(v)); }, theme::atmosphere, false, " Hz"),
      micTone_("Tone", 200.0, 12000.0, host.session().settings().micToneHz,
               [this](double v) { host_.session().setMicToneHz(static_cast<float>(v)); }, theme::micTransformed, false, " Hz"),
      micEcho_("Echo time", 0.05, 1.5, host.session().settings().micEchoSeconds,
               [this](double v) { host_.session().setMicEchoSeconds(static_cast<float>(v)); }, theme::micTransformed, false, " s"),
      micFeedback_("Echo feedback", 0.0, 0.9, host.session().settings().micFeedback,
                   [this](double v) { host_.session().setMicFeedback(static_cast<float>(v)); }, theme::micTransformed),
      micWet_("Effect mix", 0.0, 1.0, host.session().settings().micWet,
              [this](double v) { host_.session().setMicWet(static_cast<float>(v)); }, theme::micTransformed),
      master_("Master", 0.0, 1.0, host.session().settings().masterGain,
              [this](double v) { host_.session().setMasterGain(static_cast<float>(v)); }, theme::text, true) {
    for (auto* l : {&routeLabel_, &statusLabel_, &trackLabel_, &timeLabel_, &envLabel_, &privacyLabel_, &awarenessLabel_}) {
        styleLabel(*l);
        addAndMakeVisible(*l);
    }
    styleLabel(trackLabel_, theme::text, 16.0f);
    privacyLabel_.setText("Microphone audio is processed on this computer only and is never recorded or uploaded.",
                          juce::dontSendNotification);
    awarenessLabel_.setText("Immersive audio can mask important sounds around you. Stay aware of your surroundings.",
                            juce::dontSendNotification);

    stopAllButton_.setColour(juce::TextButton::buttonColourId, theme::danger.withAlpha(0.85f));
    stopAllButton_.onClick = [this] {
        host_.engine().panic();
        host_.engine().pause();
        setStatus("Output stopped. Press Play or toggle a source to resume.");
    };
    addAndMakeVisible(stopAllButton_);

    importButton_.onClick = [this] { chooseFile(); };
    playButton_.onClick = [this] {
        auto& e = host_.engine();
        e.clearPanic();
        if (e.meters().playing) e.pause(); else e.play();
    };
    removeButton_.onClick = [this] { host_.unloadTrack(); refreshTrackControls(); };
    positionSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    positionSlider_.setColour(juce::Slider::trackColourId, theme::music);
    positionSlider_.onDragStart = [this] { userDraggingPosition_ = true; };
    positionSlider_.onDragEnd = [this] {
        userDraggingPosition_ = false;
        host_.engine().seek(static_cast<int64_t>(positionSlider_.getValue() * host_.session().sampleRate()));
    };
    positionSlider_.setTitle("Playback position");
    loopToggle_.onClick = [this] { host_.session().setLooping(loopToggle_.getToggleState()); };
    for (juce::Component* c : std::initializer_list<juce::Component*>{&importButton_, &playButton_, &removeButton_, &positionSlider_, &loopToggle_})
        addAndMakeVisible(c);

    bedToggle_.onClick = [this] {
        host_.engine().clearPanic();
        host_.session().setBedEnabled(bedToggle_.getToggleState());
    };
    addAndMakeVisible(bedToggle_);
    addAndMakeVisible(bedDensity_);
    addAndMakeVisible(bedColour_);

    micToggle_.onClick = [this] {
        const bool want = micToggle_.getToggleState();
        micToggle_.setEnabled(false);
        host_.setMicEnabled(want, [this, want](bool ok, juce::String err) {
            micToggle_.setEnabled(true);
            micToggle_.setToggleState(ok ? want : false, juce::dontSendNotification);
            if (!ok) setStatus(err, true);
            else if (want) host_.engine().clearPanic();
        });
    };
    addAndMakeVisible(micToggle_);
    for (auto* s : {&micTone_, &micEcho_, &micFeedback_, &micWet_}) addAndMakeVisible(*s);

    for (int b = 0; b < atmos::kNumBuses; ++b) {
        const Bus bus = static_cast<Bus>(b);
        auto s = std::make_unique<LabeledSlider>(busTitle(bus), 0.0, 1.0, host.session().settings().busGain[b],
                                                 [this, bus](double v) { host_.session().setBusGain(bus, static_cast<float>(v)); },
                                                 theme::forBus(bus), true);
        s->setValueFormatter(gainText);
        addAndMakeVisible(*s);
        busSliders_.push_back(std::move(s));
    }
    master_.setValueFormatter(gainText);
    addAndMakeVisible(master_);

    refreshTrackControls();
    startTimerHz(30);
}

ExperiencePage::~ExperiencePage() { stopTimer(); }

void ExperiencePage::setStatus(const juce::String& s, bool isError) {
    statusLabel_.setColour(juce::Label::textColourId, isError ? theme::danger : theme::dim);
    statusLabel_.setText(s, juce::dontSendNotification);
}

void ExperiencePage::chooseFile() {
    chooser_ = std::make_unique<juce::FileChooser>("Import a song (the original file is never modified)",
                                                   juce::File{}, host_.supportedWildcard());
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this](const juce::FileChooser& fc) {
                              if (fc.getResult().existsAsFile()) loadFile(fc.getResult());
                          });
}

void ExperiencePage::loadFile(const juce::File& file) {
    setStatus("Decoding " + file.getFileName() + "...");
    importButton_.setEnabled(false);
    host_.loadTrack(file, [this](bool ok, juce::String err) {
        importButton_.setEnabled(true);
        setStatus(ok ? "Loaded " + host_.trackName() : err, !ok);
        refreshTrackControls();
    });
}

void ExperiencePage::refreshTrackControls() {
    const bool has = host_.session().hasTrack();
    trackLabel_.setText(has ? host_.trackName() : "No song loaded (optional)",
                        juce::dontSendNotification);
    playButton_.setEnabled(has);
    removeButton_.setEnabled(has);
    positionSlider_.setEnabled(has);
    importButton_.setButtonText(has ? "Replace song..." : "Import song...");
}

void ExperiencePage::timerCallback() {
    auto& e = host_.engine();
    e.collectGarbage();
    const auto m = e.meters();
    const double rate = host_.session().sampleRate();

    playButton_.setButtonText(m.playing ? "Pause" : "Play");
    const double duration = juce::jmax(0.001, e.timelineLengthFrames() / rate);
    positionSlider_.setRange(0.0, duration, 0.0);
    if (!userDraggingPosition_) positionSlider_.setValue(m.positionFrames / rate, juce::dontSendNotification);
    timeLabel_.setText(timeString(m.positionFrames / rate) + " / " + timeString(duration), juce::dontSendNotification);

    for (int b = 0; b < atmos::kNumBuses; ++b) busSliders_[static_cast<size_t>(b)]->setPeak(m.busPeak[b]);
    master_.setPeak(juce::jmax(m.outputPeak[0], m.outputPeak[1]));

    if (host_.micEnabled()) {
        const auto f = e.environmentFeatures();
        envLabel_.setText(f.silent ? "Surroundings: quiet" : "Surroundings: " + juce::String(f.levelDb, 0) + " dBFS",
                          juce::dontSendNotification);
        envLabel_.setText(envLabel_.getText() + "  |  " + juce::String(f.transientCount) + " events", juce::dontSendNotification);
    } else {
        envLabel_.setText("Microphone off", juce::dontSendNotification);
    }

    juce::String route = "No audio device";
    if (auto* d = host_.devices().getCurrentAudioDevice()) {
        route = d->getName() + "  |  " + juce::String(d->getCurrentSampleRate() / 1000.0, 1) + " kHz  |  " +
                juce::String(d->getCurrentBufferSizeSamples()) + " frames";
        if (host_.xruns() > 0) route << "  |  " << host_.xruns() << " dropouts";
    }
    routeLabel_.setText(route, juce::dontSendNotification);

    const bool live = m.micActive, limiting = m.limiterGain < 0.98f;
    if (live != micLive_ || limiting != limiting_) { micLive_ = live; limiting_ = limiting; repaint(headerArea_); }
}

void ExperiencePage::paint(juce::Graphics& g) {
    g.fillAll(theme::background);
    drawSection(g, musicArea_, "Music");
    drawSection(g, atmosArea_, "Atmosphere");
    drawSection(g, envArea_, "Environment");
    drawSection(g, levelsArea_, "Levels");

    auto h = headerArea_;
    g.setColour(theme::text);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("Experience", h.removeFromLeft(200), juce::Justification::centredLeft);
    if (micLive_) {
        auto pill = h.removeFromLeft(110).withSizeKeepingCentre(100, 26).toFloat();
        g.setColour(theme::danger);
        g.fillRoundedRectangle(pill, 13.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("MIC LIVE", pill, juce::Justification::centred);
    }
    if (limiting_) {
        g.setColour(theme::micRaw);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("LIMIT", h.removeFromLeft(60), juce::Justification::centred);
    }
}

void ExperiencePage::resized() {
    auto r = getLocalBounds().reduced(24);
    headerArea_ = r.removeFromTop(40);
    stopAllButton_.setBounds(headerArea_.withTrimmedLeft(headerArea_.getWidth() - 160).reduced(0, 4));
    routeLabel_.setBounds(r.removeFromTop(22));
    statusLabel_.setBounds(r.removeFromTop(22));
    awarenessLabel_.setBounds(r.removeFromBottom(22));
    r.removeFromTop(8);

    auto left = r.removeFromLeft(r.getWidth() / 2).withTrimmedRight(8);
    auto right = r.withTrimmedLeft(8);
    const int row = 32;

    musicArea_ = left.removeFromTop(4 * row + 46);
    {
        auto a = musicArea_.reduced(14).withTrimmedTop(24);
        auto top = a.removeFromTop(row);
        trackLabel_.setBounds(top.removeFromLeft(top.getWidth() - 260));
        importButton_.setBounds(top.removeFromLeft(140).reduced(4, 2));
        removeButton_.setBounds(top.reduced(4, 2));
        auto transport = a.removeFromTop(row);
        playButton_.setBounds(transport.removeFromLeft(90).reduced(2));
        loopToggle_.setBounds(transport.removeFromRight(80));
        timeLabel_.setBounds(transport.removeFromRight(110));
        positionSlider_.setBounds(transport);
    }
    left.removeFromTop(12);
    atmosArea_ = left.removeFromTop(3 * row + 46);
    {
        auto a = atmosArea_.reduced(14).withTrimmedTop(24);
        bedToggle_.setBounds(a.removeFromTop(row));
        bedDensity_.setBounds(a.removeFromTop(row));
        bedColour_.setBounds(a.removeFromTop(row));
    }
    left.removeFromTop(12);
    envArea_ = left;
    {
        auto a = envArea_.reduced(14).withTrimmedTop(24);
        auto top = a.removeFromTop(row);
        micToggle_.setBounds(top.removeFromLeft(140));
        envLabel_.setBounds(top);
        for (auto* s : {&micTone_, &micEcho_, &micFeedback_, &micWet_}) s->setBounds(a.removeFromTop(row));
        privacyLabel_.setBounds(a.removeFromTop(row));
    }

    levelsArea_ = right.removeFromTop((atmos::kNumBuses + 1) * (row + 8) + 46);
    {
        auto a = levelsArea_.reduced(14).withTrimmedTop(24);
        for (auto& s : busSliders_) s->setBounds(a.removeFromTop(row + 8));
        master_.setBounds(a.removeFromTop(row + 8));
    }
}
