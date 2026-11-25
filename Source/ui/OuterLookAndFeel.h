#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace outerspace::ui
{
namespace colours
{
    // Anodized faceplate
    const juce::Colour plate     { 0xff181d2a };   // powder-coated, blue-black
    const juce::Colour plateDark { 0xff0c0f17 };
    // Gold trim
    const juce::Colour gold      { 0xffd9b46c };
    const juce::Colour goldLight { 0xfff7e2b0 };
    const juce::Colour goldDark  { 0xff7a5c28 };
    const juce::Colour silk      { 0xffd9dfec };   // silkscreen print
    const juce::Colour silkDim   { 0xff8c97b3 };
    // Screen / LCD
    const juce::Colour glass     { 0xff04070f };
    const juce::Colour lcd       { 0xff6ee7ea };
    const juce::Colour lcdDim    { 0xff2b6a72 };
    // Light
    const juce::Colour amber     { 0xffffb04a };
    const juce::Colour ember     { 0xffff7a38 };
    const juce::Colour cyan      { 0xff5ce1e6 };
    const juce::Colour violet    { 0xff8c7cf0 };
    const juce::Colour green     { 0xff52f08f };
}

struct Fonts
{
    static juce::Font display (float height);   // Michroma: logo and panel print
    static juce::Font lcd (float height);
    static juce::Font silk (float height);      // panel print, spaced capitals
};

/** Drawing helpers for the hardware look. */
struct Hardware
{
    /** Brushed, anodized aluminium plate with lighting and a bevelled edge. */
    static juce::Image makeFaceplate (int w, int h, float scale);
    static void drawScrew (juce::Graphics&, juce::Point<float> c, float r, float slotAngle);
    static void drawSilk (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float height,
                          juce::Justification = juce::Justification::centred, bool dim = false);
    /** A recessed window cut into the plate (screen, LCD). */
    static void drawRecess (juce::Graphics&, juce::Rectangle<float>, float corner, float depth);
    static void drawLed (juce::Graphics&, juce::Point<float> c, float r, juce::Colour, float brightness);
};

/** Slider properties: "size" = "small" for the range knobs; "dimmed" = true when inactive. */
class OuterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OuterLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::lcd (15.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (15.0f)); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }

private:
    const juce::Image& capImage (int diameterPx);
    std::map<int, juce::Image> caps;
    void drawKnobWell (juce::Graphics&, juce::Point<float> c, float wellR);
};
} // namespace outerspace::ui
