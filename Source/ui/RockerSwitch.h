#pragma once
#include "OuterLookAndFeel.h"

namespace outerspace::ui
{
/** A matte-black snap rocker switch, seen from the front. The pressed half
    sits flush in the bezel; the other half tilts up toward you and catches
    the light. Up (top half pressed) = first label, down = second. */
class RockerSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    RockerSwitch (juce::String topLabel, juce::String bottomLabel) : top (std::move (topLabel)), bottom (std::move (bottomLabel))
    {
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (bool)> onChange;   // true = bottom (down)
    void setDown (bool d) { if (d != isDown) { isDown = d; repaint(); } }
    bool getDown() const noexcept { return isDown; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Press the half you want, like the real thing
        const bool want = e.position.y > paddle().getCentreY();
        if (want != isDown) { isDown = want; repaint(); if (onChange) onChange (isDown); }
    }

    void resized() override { texture = {}; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        Hardware::drawSilk (g, top, b.withHeight (14.0f), 10.5f, juce::Justification::centred, isDown);
        Hardware::drawSilk (g, bottom, b.withTrimmedTop (b.getHeight() - 14.0f), 10.5f, juce::Justification::centred, ! isDown);

        const auto bezel = bezelRect();
        const auto hole = bezel.reduced (5.5f);
        const auto pad = paddle();

        // --- Bezel (moulded frame) -------------------------------------------------
        {
            juce::Path p; p.addRoundedRectangle (bezel, 3.5f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 9, { 1, 5 }).drawForPath (g, p);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2d33), 0, bezel.getY(), juce::Colour (0xff111216), 0, bezel.getBottom(), false));
        g.fillRoundedRectangle (bezel, 3.5f);
        // outer chamfer catches light on top, shadow at the bottom
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0, bezel.getY(),
                                                 juce::Colours::black.withAlpha (0.5f), 0, bezel.getBottom(), false));
        g.drawRoundedRectangle (bezel.reduced (0.6f), 3.2f, 1.2f);
        // inner lip around the opening: shadowed on top, lit at the bottom
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), 0, hole.getY(),
                                                 juce::Colours::white.withAlpha (0.15f), 0, hole.getBottom(), false));
        g.fillRoundedRectangle (hole.expanded (1.5f), 2.5f);
        g.setColour (juce::Colour (0xff030304));
        g.fillRoundedRectangle (hole, 2.0f);

        // --- Paddle ------------------------------------------------------------------
        const float mid = pad.getCentreY();
        auto pressed = isDown ? pad.withTop (mid) : pad.withBottom (mid);
        auto raised  = isDown ? pad.withBottom (mid) : pad.withTop (mid);
        const float lift = 3.0f;   // the raised end stands out of the bezel
        raised = isDown ? raised.withTop (raised.getY() - lift).expanded (0.8f, 0) : raised.withBottom (raised.getBottom() + lift).expanded (0.8f, 0);

        // Pressed half: flush and tilted slightly away; lighter near the crest
        if (isDown)
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2f36), 0, pressed.getY(), juce::Colour (0xff1b1d22), 0, pressed.getBottom(), false));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2d34), 0, pressed.getY(), juce::Colour (0xff33363e), 0, pressed.getBottom(), false));
        g.fillRect (pressed);
        {
            // thin shadow line where the pressed end tucks under the bezel lip
            const auto gap = isDown ? pressed.withTop (pressed.getBottom() - 1.2f) : pressed.withHeight (1.2f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRect (gap);
        }

        // Raised half casts a shadow onto the pressed half (light from the top)
        {
            juce::Path r; r.addRectangle (raised);
            juce::DropShadow (juce::Colours::black.withAlpha (0.9f), 6, { 0, isDown ? 4 : 2 }).drawForPath (g, r);
        }

        // Raised half: brighter toward its lifted end when it's the top half
        // (facing the light), darker when it's the bottom half (facing away)
        if (isDown)
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5d626e), 0, raised.getY(), juce::Colour (0xff363a43), 0, raised.getBottom(), false));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d4049), 0, raised.getY(), juce::Colour (0xff1c1e23), 0, raised.getBottom(), false));
        g.fillRect (raised);

        // The lifted end shows its thickness: a lit face on top, a dark face at the bottom
        if (isDown)
        {
            const auto face = raised.withHeight (3.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a909c), 0, face.getY(), juce::Colour (0xff4a4e58), 0, face.getBottom(), false));
            g.fillRect (face);
        }
        else
        {
            const auto face = raised.withTop (raised.getBottom() - 3.0f);
            g.setColour (juce::Colour (0xff0c0d10));
            g.fillRect (face);
        }

        // Crest where the halves meet
        const float crestY = isDown ? raised.getBottom() - 0.5f : raised.getY() + 0.5f;
        g.setColour (juce::Colours::white.withAlpha (isDown ? 0.18f : 0.38f));
        g.drawHorizontalLine ((int) crestY, raised.getX() + 0.5f, raised.getRight() - 0.5f);
        // side edges
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) raised.getX(), raised.getY(), raised.getBottom());
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawVerticalLine ((int) raised.getRight() - 1, raised.getY(), raised.getBottom());

        // --- Matte texture over bezel and paddle -----------------------------------------
        if (! texture.isValid()) texture = makeTexture (getWidth(), getHeight());
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (bezel, 3.5f); clip.addRectangle (raised);
        g.reduceClipRegion (clip);
        g.drawImageAt (texture, 0, 0);
        g.restoreState();

        if (isMouseOver())
        {
            g.setColour (colours::gold.withAlpha (0.35f));
            g.drawRoundedRectangle (bezel.expanded (2.5f), 5.0f, 1.0f);
        }
    }

private:
    juce::Rectangle<float> bezelRect() const
    {
        return getLocalBounds().toFloat().withSizeKeepingCentre (44.0f, 68.0f);
    }
    juce::Rectangle<float> paddle() const { return bezelRect().reduced (6.5f); }

    static juce::Image makeTexture (int w, int h)
    {
        // Fine speckle for moulded matte plastic
        juce::Image img (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
        juce::Graphics g (img);
        juce::Random r (99);
        for (int i = 0; i < w * h / 3; ++i)
        {
            const float x = r.nextFloat() * (float) w, y = r.nextFloat() * (float) h;
            g.setColour (r.nextBool() ? juce::Colours::white.withAlpha (0.05f) : juce::Colours::black.withAlpha (0.12f));
            g.fillRect (x, y, 0.6f, 0.6f);
        }
        return img;
    }

    juce::String top, bottom;
    bool isDown = false;
    juce::Image texture;
};
} // namespace outerspace::ui
