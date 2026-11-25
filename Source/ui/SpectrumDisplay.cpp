#include "SpectrumDisplay.h"
#include "OuterLookAndFeel.h"

namespace outerspace::ui
{
namespace
{
constexpr float minHz = 20.0f, maxHz = 20000.0f;
constexpr float specTop = -10.0f, specBottom = -90.0f;   // spectra range (dBFS per band)
constexpr float cutRange = kMaxDepthDb;                   // full plot depth for the cut
}

SpectrumDisplay::SpectrumDisplay (DisplayData& d, juce::RangedAudioParameter& lo, juce::RangedAudioParameter& hi)
    : data (d), low (lo), high (hi)
{
    side.fill (-120.0f); main.fill (-120.0f); cut.fill (0.0f);
}

juce::Rectangle<float> SpectrumDisplay::plot() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 12.0f).withTrimmedBottom (14.0f).withTrimmedTop (6.0f);
}

float SpectrumDisplay::xFor (float hz) const
{
    const auto p = plot();
    return p.getX() + p.getWidth() * std::log (juce::jlimit (minHz, maxHz, hz) / minHz) / std::log (maxHz / minHz);
}

float SpectrumDisplay::hzFor (float x) const
{
    const auto p = plot();
    const float t = juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth());
    return minHz * std::pow (maxHz / minHz, t);
}

void SpectrumDisplay::refresh()
{
    for (int b = 0; b < kNumBands; ++b)
    {
        auto follow = [] (float& v, float target, float up, float down)
        { v += (target > v ? up : down) * (target - v); };
        follow (side[b], data.sideDb[b].load(), 0.6f, 0.18f);
        follow (main[b], data.mainDb[b].load(), 0.6f, 0.18f);
        follow (cut[b], data.cutDb[b].load(), 0.5f, 0.5f);
    }
    repaint();
}

juce::Path SpectrumDisplay::curve (const std::array<float, kNumBands>& v, std::function<float (float)> yFor) const
{
    // Catmull-Rom through the band points, on a log frequency axis
    std::array<juce::Point<float>, kNumBands> pts;
    for (int b = 0; b < kNumBands; ++b)
        pts[(size_t) b] = { xFor (Spacer::bandCentre (b)), yFor (v[(size_t) b]) };

    juce::Path p;
    p.startNewSubPath (pts[0]);
    for (int i = 0; i < kNumBands - 1; ++i)
    {
        const auto p0 = pts[(size_t) juce::jmax (0, i - 1)], p1 = pts[(size_t) i], p2 = pts[(size_t) i + 1],
                   p3 = pts[(size_t) juce::jmin (kNumBands - 1, i + 2)];
        for (int k = 1; k <= 6; ++k)
        {
            const float t = k / 6.0f, t2 = t * t, t3 = t2 * t;
            const auto q = ((p1 * 2.0f) + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
                            + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
            p.lineTo (q);
        }
    }
    return p;
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 14.0f);

    const auto p = plot();

    // Grid
    g.setFont (Fonts::ui (10.5f, true));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xFor (hz);
        g.setColour (grid);
        g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
        g.setColour (dim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20.0f, p.getBottom() + 2.0f, 40.0f, 14.0f), juce::Justification::centred);
    }
    for (float db : { -6.0f, -12.0f, -18.0f })
    {
        const float y = p.getY() + p.getHeight() * (-db / cutRange) * 0.8f;
        g.setColour (grid.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
        g.setColour (amber.withAlpha (0.45f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (p.getRight() - 28.0f, y - 13.0f, 26.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    const float alpha = active ? 1.0f : 0.35f;
    auto ySpec = [p] (float db) { return juce::jmap (juce::jlimit (specBottom, specTop, db), specBottom, specTop, p.getBottom(), p.getY() + p.getHeight() * 0.12f); };
    auto yCut  = [p] (float db) { return p.getY() + p.getHeight() * 0.8f * juce::jlimit (0.0f, 1.0f, -db / cutRange); };

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p.toNearestInt());

        // This track (violet, filled)
        auto m = curve (main, ySpec);
        auto mFill = m;
        mFill.lineTo (p.getRight(), p.getBottom()); mFill.lineTo (p.getX(), p.getBottom()); mFill.closeSubPath();
        g.setColour (violet.withAlpha (0.16f * alpha));
        g.fillPath (mFill);
        g.setColour (violet.withAlpha (0.55f * alpha));
        g.strokePath (m, juce::PathStrokeType (1.3f));

        // Side chain (cyan line with glow)
        auto s = curve (side, ySpec);
        g.setColour (cyan.withAlpha (0.18f * alpha));
        g.strokePath (s, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cyan.withAlpha (alpha));
        g.strokePath (s, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // The cut, hanging from the top
        auto c = curve (cut, yCut);
        auto cFill = c;
        cFill.lineTo (p.getRight(), p.getY()); cFill.lineTo (p.getX(), p.getY()); cFill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (amber.withAlpha (0.45f * alpha), 0, p.getY(),
                                                 ember.withAlpha (0.12f * alpha), 0, p.getY() + p.getHeight() * 0.8f, false));
        g.fillPath (cFill);
        g.setColour (amber.withAlpha (alpha));
        g.strokePath (c, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Range markers: shade what's outside
    const float lx = xFor (low.convertFrom0to1 (low.getValue()));
    const float hx = xFor (high.convertFrom0to1 (high.getValue()));
    g.setColour (juce::Colour (0xaa05070f));
    if (lx > p.getX()) g.fillRect (juce::Rectangle<float> (p.getX(), p.getY(), lx - p.getX(), p.getHeight()));
    if (hx < p.getRight()) g.fillRect (juce::Rectangle<float> (hx, p.getY(), p.getRight() - hx, p.getHeight()));

    auto marker = [&] (float x, const juce::String& label, int idx, bool labelRight)
    {
        const bool hot = hover == idx || dragging == idx;
        g.setColour (cyan.withAlpha (hot ? 1.0f : 0.6f));
        g.drawLine (x, p.getY(), x, p.getBottom(), hot ? 2.0f : 1.2f);
        const float hw = 34.0f;
        auto tag = juce::Rectangle<float> (labelRight ? x : x - hw, p.getY(), hw, 16.0f);
        g.fillRoundedRectangle (tag, 4.0f);
        g.setColour (panel);
        g.setFont (Fonts::ui (10.0f, true));
        g.drawText (label, tag, juce::Justification::centred);
    };
    marker (lx, "LOW", 0, true);
    marker (hx, "HIGH", 1, false);

    g.setColour (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
}

int SpectrumDisplay::markerNear (float x) const
{
    const float lx = xFor (low.convertFrom0to1 (low.getValue()));
    const float hx = xFor (high.convertFrom0to1 (high.getValue()));
    const float dl = std::abs (x - lx), dh = std::abs (x - hx);
    if (juce::jmin (dl, dh) > 10.0f) return -1;
    return dl <= dh ? 0 : 1;
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = markerNear (e.position.x);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = markerNear (e.position.x);
    if (dragging < 0)
    {
        // Click in the plot: grab whichever marker is closer and move it there
        const float hz = hzFor (e.position.x);
        const float lo = low.convertFrom0to1 (low.getValue()), hi = high.convertFrom0to1 (high.getValue());
        dragging = std::abs (std::log (hz / lo)) <= std::abs (std::log (hz / hi)) ? 0 : 1;
    }
    (dragging == 0 ? low : high).beginChangeGesture();
    mouseDrag (e);
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    float hz = hzFor (e.position.x);
    const float lo = low.convertFrom0to1 (low.getValue()), hi = high.convertFrom0to1 (high.getValue());
    if (dragging == 0) hz = juce::jmin (hz, hi / 1.26f);   // keep at least a third of an octave open
    else               hz = juce::jmax (hz, lo * 1.26f);
    auto& prm = dragging == 0 ? low : high;
    prm.setValueNotifyingHost (prm.convertTo0to1 (hz));
    repaint();
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0) (dragging == 0 ? low : high).endChangeGesture();
    dragging = -1;
}

void SpectrumDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click a marker to open it all the way
    const int m = markerNear (e.position.x);
    if (m < 0) return;
    auto& prm = m == 0 ? low : high;
    prm.beginChangeGesture();
    prm.setValueNotifyingHost (prm.getDefaultValue());
    prm.endChangeGesture();
}
} // namespace outerspace::ui
