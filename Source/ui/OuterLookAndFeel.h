#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace outerspace::ui
{
namespace colours
{
    const juce::Colour bgTop    { 0xff0b0f1d };
    const juce::Colour bgBottom { 0xff151a30 };
    const juce::Colour panel    { 0xff0e1324 };
    const juce::Colour edge     { 0xff262f4d };
    const juce::Colour grid     { 0xff1d2540 };
    const juce::Colour text     { 0xffd3daf0 };
    const juce::Colour dim      { 0xff7380a6 };
    const juce::Colour amber    { 0xffffb454 };   // the cut
    const juce::Colour ember    { 0xffff7a45 };
    const juce::Colour cyan     { 0xff5ce1e6 };   // the side chain
    const juce::Colour violet   { 0xff8c7cf0 };   // this track
    const juce::Colour ok       { 0xff63e6a0 };
    const juce::Colour warn     { 0xffffc861 };
}

struct Fonts
{
    static juce::Font wordmark (float height);
    static juce::Font ui (float height, bool bold = false);
};

/** Slider property: "accent" = "cyan" for the small range knobs. */
class OuterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OuterLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Fonts::ui (12.5f, true); }

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::ui (14.5f, true); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return Fonts::ui (14.5f); }
};
} // namespace outerspace::ui
