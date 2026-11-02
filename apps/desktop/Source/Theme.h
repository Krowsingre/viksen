#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/routing/Bus.h"

namespace theme {
inline const juce::Colour background{0xff0c0d14};
inline const juce::Colour panel{0xff151722};
inline const juce::Colour sidebar{0xff10121a};
inline const juce::Colour text{0xffe6e7ef};
inline const juce::Colour dim{0xff8a8da0};
inline const juce::Colour accent{0xff8c9eff};
inline const juce::Colour music{0xff739eff};
inline const juce::Colour atmosphere{0xff59d1bd};
inline const juce::Colour micRaw{0xffffb84d};
inline const juce::Colour micTransformed{0xffbd8cff};
inline const juce::Colour danger{0xffff4d4d};

inline juce::Colour forBus(atmos::Bus b) {
    switch (b) {
        case atmos::Bus::Music: return music;
        case atmos::Bus::Atmosphere: return atmosphere;
        case atmos::Bus::MicRaw: return micRaw;
        case atmos::Bus::MicTransformed: return micTransformed;
        case atmos::Bus::Count: break;
    }
    return text;
}
} // namespace theme

class AtmosLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    AtmosLookAndFeel()
        : LookAndFeel_V4(juce::LookAndFeel_V4::ColourScheme{
              theme::background, theme::panel, theme::panel, juce::Colour(0xff2a2d3c), theme::text,
              theme::accent, juce::Colours::white, theme::panel, theme::text}) {
        setColour(juce::ResizableWindow::backgroundColourId, theme::background);
        setColour(juce::Slider::trackColourId, theme::accent);
        setColour(juce::Slider::thumbColourId, theme::text);
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222536));
        setColour(juce::ToggleButton::tickColourId, theme::accent);
    }
};
