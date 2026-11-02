#include "MainComponent.h"

#include "ExperiencePage.h"
#include "PlaceholderPage.h"
#include "SettingsPage.h"
#include "Theme.h"

MainComponent::MainComponent() {
    const juce::String err = host_.start();
    if (err.isNotEmpty()) juce::Logger::writeToLog("Audio device error: " + err);

    const char* names[NumPages] = {"Studio", "Experience", "Worlds", "Library", "Settings"};
    for (int i = 0; i < NumPages; ++i) {
        auto& b = navButtons_[static_cast<size_t>(i)];
        b.setButtonText(names[i]);
        b.setClickingTogglesState(true);
        b.setRadioGroupId(1);
        b.setColour(juce::TextButton::buttonColourId, theme::sidebar);
        b.setColour(juce::TextButton::buttonOnColourId, theme::panel);
        b.setColour(juce::TextButton::textColourOffId, theme::dim);
        b.setColour(juce::TextButton::textColourOnId, theme::text);
        b.onClick = [this, i] { show(static_cast<Page>(i)); };
        addAndMakeVisible(b);
    }

    pages_[Studio] = std::make_unique<PlaceholderPage>(
        "Studio", "Phase 2-3 - not built yet",
        "The Studio will edit layered projects: songs, aligned stems and atmospheric layers on a timeline, with "
        "mixer, spatial canvas and effect inspector. The engine and project format already support aligned "
        "multi-layer playback and save/reopen.",
        juce::StringArray{"Multilayer timeline and waveform view", "Mixer with A/B against the original",
                          "Spatial canvas with trajectories", "Effect/texture inspector and automation",
                          "Stereo / binaural export"});
    pages_[Worlds] = std::make_unique<PlaceholderPage>(
        "Worlds", "Phase 6 - not built yet",
        "Worlds will package sounds, processing, spatial motion and reactive rules into environments that respond to "
        "your surroundings.",
        juce::StringArray{"Starter Worlds (names provisional): Nocturnal City, Submerged, Nearfield, Lucid Forest",
                          "Reactive rules from loudness, silence, transients and voice activity",
                          "Create, duplicate, import and export World packages"});
    pages_[Library] = std::make_unique<PlaceholderPage>(
        "Library", "Phase 2 - not built yet",
        "The Library will index tracks, stems, textures, projects, Worlds and recordings with search and tags. "
        "For now, import a song from Experience or drop an audio file onto this window.",
        juce::StringArray{"Searchable media library (SQLite)", "Projects and recent sessions", "Missing-file relinking"});
    auto exp = std::make_unique<ExperiencePage>(host_);
    experience_ = exp.get();
    pages_[Experience] = std::move(exp);
    pages_[Settings] = std::make_unique<SettingsPage>(host_);
    for (auto& p : pages_) addChildComponent(*p);

    if (err.isNotEmpty()) {
        startupError_.setText("Audio device could not be opened: " + err + "  (see Settings)", juce::dontSendNotification);
        startupError_.setColour(juce::Label::textColourId, theme::danger);
        addAndMakeVisible(startupError_);
    }

    navButtons_[Experience].setToggleState(true, juce::dontSendNotification);
    show(Experience);
    setSize(1200, 780);
}

MainComponent::~MainComponent() = default;

void MainComponent::show(Page p) {
    current_ = p;
    for (int i = 0; i < NumPages; ++i) pages_[static_cast<size_t>(i)]->setVisible(i == p);
    navButtons_[static_cast<size_t>(p)].setToggleState(true, juce::dontSendNotification);
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(theme::background);
    g.setColour(theme::sidebar);
    g.fillRect(getLocalBounds().removeFromLeft(180));
    g.setColour(theme::text);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText(juce::JUCEApplication::getInstance()->getApplicationName(), juce::Rectangle<int>(20, 18, 150, 30),
               juce::Justification::centredLeft);
}

void MainComponent::resized() {
    auto r = getLocalBounds();
    auto side = r.removeFromLeft(180).reduced(10);
    side.removeFromTop(56);
    // Experience-first ordering on mobile; desktop keeps the spec's sidebar order.
    for (auto& b : navButtons_) b.setBounds(side.removeFromTop(40).reduced(0, 3));
    if (startupError_.isVisible()) startupError_.setBounds(r.removeFromTop(28).reduced(12, 2));
    for (auto& p : pages_) p->setBounds(r);
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size() == 1 && juce::File(files[0]).existsAsFile();
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int) {
    show(Experience);
    experience_->loadFile(juce::File(files[0]));
}
