#include <juce_gui_basics/juce_gui_basics.h>

#include "MainComponent.h"
#include "Theme.h"

class DesktopApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override {
        // Runtime/error log in the user's application-data folder (local only).
        logger_.reset(juce::FileLogger::createDefaultAppLogger(getApplicationName(), "runtime.log",
                                                               getApplicationName() + " " + getApplicationVersion()));
        juce::Logger::setCurrentLogger(logger_.get());
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
        window_ = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        window_.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        juce::Logger::setCurrentLogger(nullptr);
        logger_.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, theme::background, DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(1000, 700, 4000, 3000);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    AtmosLookAndFeel lookAndFeel_;
    std::unique_ptr<juce::FileLogger> logger_;
    std::unique_ptr<MainWindow> window_;
};

START_JUCE_APPLICATION(DesktopApplication)
