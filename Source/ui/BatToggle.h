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

        Hardware::drawSilk (g, up, b.withHeight (14.0f), 10.5f, juce::Justification::centred, isDown);
        Hardware::drawSilk (g, down, b.withTrimmedTop (b.getHeight() - 14.0f), 10.5f, juce::Justification::centred, ! isDown);

        // Polished chrome: a studio-light reflection, bright top-left, dark band, bright rim bottom-right
        auto chrome = [] (float x0, float y0, float x1, float y1)
        {
            juce::ColourGradient cg (juce::Colour (0xfff4f6fb), x0, y0, juce::Colour (0xffd9dee8), x1, y1, false);
            cg.addColour (0.30, juce::Colour (0xffaab1bf));
            cg.addColour (0.48, juce::Colour (0xff2a2f3a));
            cg.addColour (0.62, juce::Colour (0xff0d1016));
            cg.addColour (0.82, juce::Colour (0xff7c8392));
            return cg;
        };

        // 1. Shadow of the whole assembly on the plate
        {
            juce::Path p; p.addEllipse (cx - 21.0f, cy - 21.0f, 42.0f, 42.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 10, { 2, 5 }).drawForPath (g, p);
        }
        // 2. Outer chrome bezel ring
        g.setGradientFill (chrome (cx - 20, cy - 20, cx + 20, cy + 20));
        g.fillEllipse (cx - 21.0f, cy - 21.0f, 42.0f, 42.0f);
        g.setColour (juce::Colours::black.withAlpha (0.9f));
        g.fillEllipse (cx - 18.0f, cy - 18.0f, 36.0f, 36.0f);   // dark gap between bezel and nut
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (cx - 18.0f, cy - 17.5f, 36.0f, 36.0f, 0.6f);

        // 3. Hex nut: each facet catches the light differently
        {
            const float rn = 15.5f;
            juce::Point<float> pts[6];
            for (int i = 0; i < 6; ++i)
            {
                const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
                pts[i] = { cx + rn * std::cos (a), cy + rn * std::sin (a) };
            }
            for (int i = 0; i < 6; ++i)
            {
                juce::Path facet;
                facet.addTriangle (cx, cy, pts[i].x, pts[i].y, pts[(i + 1) % 6].x, pts[(i + 1) % 6].y);
                const float mid = juce::MathConstants<float>::pi / 3.0f * ((float) i + 0.5f) + juce::MathConstants<float>::pi / 6.0f;
                const float lit = 0.5f + 0.5f * std::cos (mid - (-0.75f * juce::MathConstants<float>::pi));
                const auto edge = juce::Colour::greyLevel (0.16f + 0.84f * std::pow (lit, 1.3f));
                g.setGradientFill (juce::ColourGradient (edge, (pts[i].x + pts[(i + 1) % 6].x) * 0.5f, (pts[i].y + pts[(i + 1) % 6].y) * 0.5f,
                                                         juce::Colour (0xff1a1d24), cx, cy, false));
                g.fillPath (facet);
            }
            juce::Path hex;
            hex.startNewSubPath (pts[0]);
            for (int i = 1; i < 6; ++i) hex.lineTo (pts[i]);
            hex.closeSubPath();
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.strokePath (hex, juce::PathStrokeType (0.8f));
        }

        // 4. Threaded bushing: dark bore with a chrome collar
        g.setGradientFill (chrome (cx - 9, cy - 9, cx + 9, cy + 9));
        g.fillEllipse (cx - 9.5f, cy - 9.5f, 19.0f, 19.0f);
        g.setColour (juce::Colour (0xff050608));
        g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);

        // 5. Lever: thin at the pivot, swelling to a polished ball
        const float dir = isDown ? 1.0f : -1.0f;
        const float len = juce::jmin (36.0f, b.getHeight() * 0.5f - 22.0f);
        const juce::Point<float> base (cx, cy), tip (cx, cy + dir * len);
        juce::Path lever;
        const float w0 = 2.6f, w1 = 4.2f;
        lever.startNewSubPath (base.x - w0, base.y);
        lever.lineTo (tip.x - w1, tip.y);
        lever.lineTo (tip.x + w1, tip.y);
        lever.lineTo (base.x + w0, base.y);
        lever.closeSubPath();
        {
            juce::Path sh = lever;
            sh.addEllipse (tip.x - 6.0f, tip.y - 6.0f, 12.0f, 12.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 5, { 3, 4 }).drawForPath (g, sh);
        }
        juce::ColourGradient rod (juce::Colour (0xff1b1e25), tip.x - w1, 0, juce::Colour (0xff3a3f4b), tip.x + w1, 0, false);
        rod.addColour (0.28, juce::Colour (0xffffffff));
        rod.addColour (0.45, juce::Colour (0xffb9c0cc));
        rod.addColour (0.70, juce::Colour (0xff555b68));
        g.setGradientFill (rod);
        g.fillPath (lever);

        // pivot ball inside the bushing (hidden partly by the lever root)
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx - 2.0f, cy - 2.5f, juce::Colour (0xff20232b), cx + 3.0f, cy + 5.0f, true));
        g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

        // ball tip
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, tip.x - 2.0f, tip.y - 2.5f,
                                                 juce::Colour (0xff15181e), tip.x + 4.0f, tip.y + 6.0f, true));
        g.fillEllipse (tip.x - 6.0f, tip.y - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (tip.x - 3.2f, tip.y - 4.0f, 3.0f, 2.4f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (tip.x - 6.0f, tip.y - 6.0f, 12.0f, 12.0f, 0.6f);

        if (isMouseOver())
        {
            g.setColour (colours::gold.withAlpha (0.10f));
            g.drawEllipse (cx - 24.0f, cy - 24.0f, 48.0f, 48.0f, 1.5f);
        }
    }

private:
    juce::String up, down;
    bool isDown = false;
};
} // namespace outerspace::ui
