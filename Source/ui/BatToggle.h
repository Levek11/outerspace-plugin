#pragma once
#include "OuterLookAndFeel.h"

namespace outerspace::ui
{
/** A chrome bat-lever toggle switch: up = first label, down = second. */
class BatToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    BatToggle (juce::String upLabel, juce::String downLabel) : up (std::move (upLabel)), down (std::move (downLabel))
    {
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (bool)> onChange;   // true = down
    void setDown (bool d) { if (d != isDown) { isDown = d; repaint(); } }
    bool getDown() const noexcept { return isDown; }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        // Click the top half to flip up, bottom half to flip down (or anywhere to flip)
        const bool want = e.mouseWasDraggedSinceMouseDown() ? e.position.y > getHeight() * 0.5f : ! isDown;
        if (want != isDown) { isDown = want; repaint(); if (onChange) onChange (isDown); }
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX(), cy = b.getCentreY();

        Hardware::drawSilk (g, up, b.withHeight (14.0f), 11.0f, juce::Justification::centred, isDown);
        Hardware::drawSilk (g, down, b.withTrimmedTop (b.getHeight() - 14.0f), 11.0f, juce::Justification::centred, ! isDown);

        // Washer and hex nut
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (cx - 15.0f, cy - 13.5f, 30.0f, 30.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d95a8), cx - 10, cy - 12, juce::Colour (0xff2a2f3b), cx + 10, cy + 14, false));
        g.fillEllipse (cx - 14.0f, cy - 14.0f, 28.0f, 28.0f);
        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + 0.52f;
            const juce::Point<float> p (cx + 10.5f * std::cos (a), cy + 10.5f * std::sin (a));
            if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
        }
        hex.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e9f0), cx - 8, cy - 9, juce::Colour (0xff4c5262), cx + 8, cy + 10, false));
        g.fillPath (hex);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (hex, juce::PathStrokeType (0.8f));
        g.setColour (juce::Colour (0xff0c0e14));
        g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

        // The lever, tilted toward its position
        const float dir = isDown ? 1.0f : -1.0f;
        const float len = b.getHeight() * 0.5f - 28.0f;
        const juce::Point<float> base (cx, cy), tip (cx + 2.0f, cy + dir * len);
        juce::Path lever;
        const float w0 = 3.2f, w1 = 4.6f;
        lever.startNewSubPath (base.x - w0, base.y);
        lever.lineTo (tip.x - w1, tip.y);
        lever.lineTo (tip.x + w1, tip.y);
        lever.lineTo (base.x + w0, base.y);
        lever.closeSubPath();
        lever.addEllipse (tip.x - w1 - 0.5f, tip.y - w1 - 0.5f, (w1 + 0.5f) * 2, (w1 + 0.5f) * 2);

        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 6, { 3, 4 }).drawForPath (g, lever);
        juce::ColourGradient chrome (juce::Colour (0xff5a6070), tip.x - w1, 0, juce::Colour (0xff5a6070), tip.x + w1, 0, false);
        chrome.addColour (0.35, juce::Colour (0xfff4f6fa));
        chrome.addColour (0.55, juce::Colour (0xffb7bdc9));
        g.setGradientFill (chrome);
        g.fillPath (lever);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (lever, juce::PathStrokeType (0.7f));
        // ball tip highlight
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (tip.x - 2.5f, tip.y - 3.0f, 2.6f, 2.2f);

        if (isMouseOver())
        {
            g.setColour (colours::lcd.withAlpha (0.08f));
            g.fillEllipse (cx - 22.0f, cy - 22.0f, 44.0f, 44.0f);
        }
    }

private:
    juce::String up, down;
    bool isDown = false;
};
} // namespace outerspace::ui
