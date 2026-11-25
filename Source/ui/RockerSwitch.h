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
        juce::ignoreUnused (e);
        isDown = ! isDown;   // a click anywhere on the switch flips it
        repaint();
        if (onChange) onChange (isDown);
    }

    void resized() override { texture = {}; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setFont (Fonts::silk (11.0f));
        g.setColour (isDown ? juce::Colour (0xff5d6068) : juce::Colour (0xffd2d5dc));
        g.drawText (top, b.withHeight (14.0f), juce::Justification::centred, false);
        g.setColour (isDown ? juce::Colour (0xffd2d5dc) : juce::Colour (0xff5d6068));
        g.drawText (bottom, b.withTrimmedTop (b.getHeight() - 14.0f), juce::Justification::centred, false);

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

        // --- Paddle -----------------------------------------------------------------
        // Engaged half: pressed in, flat, lighter grey. Other half: tilted out,
        // dark, with the lit lip of its raised end showing at the outer edge.
        const float mid = std::round (pad.getCentreY());
        const auto topHalf = pad.withBottom (mid), bottomHalf = pad.withTop (mid);
        const auto engaged = isDown ? bottomHalf : topHalf;
        const auto raised  = isDown ? topHalf : bottomHalf;

        g.setColour (juce::Colour (0xff35383f));
        g.fillRect (engaged);
        g.setColour (juce::Colour (0xff1f2024));
        g.fillRect (raised);

        // the lip at the raised end: a dark band, then the lit edge face
        const float lipH = std::round (raised.getHeight() * 0.11f), bandH = std::round (raised.getHeight() * 0.07f);
        if (isDown)
        {
            const auto lip = raised.withHeight (lipH);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d55), 0, lip.getY(), juce::Colour (0xff2c2e34), 0, lip.getBottom(), false));
            g.fillRect (lip);
            g.setColour (juce::Colour (0xff17181b));
            g.fillRect (raised.withTrimmedTop (lipH).withHeight (bandH));
        }
        else
        {
            const auto lip = raised.withTop (raised.getBottom() - lipH);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e3037), 0, lip.getY(), juce::Colour (0xff474a52), 0, lip.getBottom(), false));
            g.fillRect (lip);
            g.setColour (juce::Colour (0xff18191c));
            g.fillRect (raised.withTrimmedBottom (lipH).withTop (raised.getBottom() - lipH - bandH));
        }

        // the seam between the halves, and the thin dark gaps along the sides
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawHorizontalLine ((int) mid, pad.getX(), pad.getRight());
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRect (pad.withWidth (1.2f));
        g.fillRect (pad.withLeft (pad.getRight() - 1.2f));

        // --- Matte texture over bezel and paddle -----------------------------------------
        if (! texture.isValid()) texture = makeTexture (getWidth(), getHeight());
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (bezel, 3.5f);
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
