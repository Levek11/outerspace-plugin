#include "OuterLookAndFeel.h"
#include "OuterSpaceAssets.h"

namespace outerspace::ui
{
juce::Font Fonts::wordmark (float height)
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor (OuterSpaceAssets::ArchivoBlackRegular_ttf,
                                                                    OuterSpaceAssets::ArchivoBlackRegular_ttfSize);
    return juce::Font (juce::FontOptions (typeface).withHeight (height));
}

juce::Font Fonts::ui (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

OuterLookAndFeel::OuterLookAndFeel()
{
    using namespace colours;
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, cyan);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::textColourId, juce::Colours::white);
    setColour (juce::TextEditor::outlineColourId, edge);
    setColour (juce::TextEditor::focusedOutlineColourId, cyan);
    setColour (juce::TextEditor::highlightColourId, cyan.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, cyan);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, edge);
}

void OuterLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    using namespace colours;
    const bool small = s.getProperties()["accent"] == "cyan";
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (small ? 8.0f : 16.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float ringR = size * 0.5f - (small ? 2.0f : 4.0f);
    const float ringW = small ? 3.5f : 7.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = small ? cyan : amber;

    // Orbit track
    juce::Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    g.setColour (edge);
    g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, angle, true);
        if (small)
            g.setColour (accent);
        else
            g.setGradientFill (juce::ColourGradient (ember, c.x - ringR, c.y + ringR, amber, c.x + ringR, c.y - ringR, false));
        g.strokePath (value, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Planet body
    const float bodyR = ringR - ringW * (small ? 1.6f : 2.2f);
    {
        juce::ColourGradient grad (juce::Colour (0xff2a3354), c.x - bodyR * 0.5f, c.y - bodyR * 0.6f,
                                   juce::Colour (0xff10152a), c.x + bodyR * 0.6f, c.y + bodyR * 0.8f, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
        g.setColour (edge.brighter (0.2f));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);
    }

    // The moon: a dot on the orbit at the current value
    {
        const float mr = small ? 4.0f : 8.0f;
        const auto p = c.getPointOnCircumference (ringR, angle);
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (p.x - mr * 1.8f, p.y - mr * 1.8f, mr * 3.6f, mr * 3.6f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (p.x - mr * 0.6f, p.y - mr * 0.6f, mr * 1.2f, mr * 1.2f);
    }

    if (! small)
    {
        g.setColour (juce::Colours::white);
        g.setFont (Fonts::wordmark (bodyR * 0.62f));
        g.drawText (juce::String (juce::roundToInt (s.getValue())), juce::Rectangle<float> (c.x - bodyR, c.y - bodyR * 0.55f, bodyR * 2, bodyR * 0.8f),
                    juce::Justification::centred);
        g.setColour (dim);
        g.setFont (Fonts::ui (bodyR * 0.2f, true));
        g.drawText ("AMOUNT %", juce::Rectangle<float> (c.x - bodyR, c.y + bodyR * 0.28f, bodyR * 2, bodyR * 0.3f),
                    juce::Justification::centred);
    }
}

void OuterLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace colours;
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState();
    const auto accent = b.getProperties()["accent"] == "amber" ? amber : cyan;

    g.setColour (on ? accent.withAlpha (0.18f) : panel);
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? accent : (over ? edge.brighter (0.4f) : edge));
    g.drawRoundedRectangle (r, rad, down ? 1.8f : 1.2f);
}

void OuterLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    using namespace colours;
    const bool on = b.getToggleState();
    const auto accent = b.getProperties()["accent"] == "amber" ? amber : cyan;
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (on ? accent.brighter (0.3f) : (over ? text : dim));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void OuterLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace colours;
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.isMouseOver (true) || box.isPopupActive() ? cyan : edge);
    g.drawRoundedRectangle (r, 8.0f, 1.2f);

    // little orbit icon
    const float cy = (float) h * 0.5f;
    g.setColour (cyan);
    g.drawEllipse (12.0f, cy - 6.0f, 12.0f, 12.0f, 1.4f);
    g.fillEllipse (16.0f, cy - 2.0f, 4.0f, 4.0f);

    juce::Path arrow;
    const float ax = (float) w - 20.0f;
    arrow.startNewSubPath (ax - 5.0f, cy - 2.5f);
    arrow.lineTo (ax, cy + 2.5f);
    arrow.lineTo (ax + 5.0f, cy - 2.5f);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void OuterLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (32, 1, box.getWidth() - 60, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}
} // namespace outerspace::ui
