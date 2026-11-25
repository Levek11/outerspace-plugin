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
    auto r = getLocalBounds().toFloat().reduced (16.0f, 0.0f);
    return r.withTrimmedTop (14.0f).withTrimmedBottom (46.0f);
}

juce::Rectangle<int> SpectrumDisplay::nameArea() const
{
    return getLocalBounds().withSizeKeepingCentre (getWidth() - 80, 56).translated (0, -10);
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
    const auto p = plot();

    // Glass
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff081020), r.getCentreX(), r.getY(),
                                             glass, r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);

    if (sendMode)
    {
        g.setFont (Fonts::lcd (12.0f));
        g.setColour (lcdDim.brighter (0.3f));
        g.drawText ("SENDING AS", nameArea().toFloat().translated (0, -26).withHeight (18), juce::Justification::centred);
        g.setFont (Fonts::lcd (32.0f));
        g.setColour (lcd);
        g.drawFittedText (sendName, nameArea(), juce::Justification::centred, 1, 0.6f);

        // Segmented level meter
        const auto m = nameArea().toFloat().translated (0, 66).withHeight (10).reduced (30, 0);
        constexpr int segs = 30;
        const float t = juce::jlimit (0.0f, 1.0f, (sendLevel + 60.0f) / 60.0f);
        const float sw = m.getWidth() / segs;
        for (int i = 0; i < segs; ++i)
        {
            const bool on = (float) (i + 1) / segs <= t + 0.001f;
            const auto col = i < segs - 4 ? lcd : amber;
            g.setColour (on ? col : col.withAlpha (0.10f));
            g.fillRoundedRectangle (m.getX() + i * sw + 1.0f, m.getY(), sw - 2.0f, m.getHeight(), 1.5f);
        }
        g.setFont (Fonts::lcd (11.0f));
        g.setColour (lcdDim.brighter (0.2f));
        g.drawText ("NAMED AFTER THIS TRACK", m.translated (0, 18).withHeight (14), juce::Justification::centred);
    }
    else
    {
        // Grid
        g.setFont (Fonts::lcd (10.5f));
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xFor (hz);
            g.setColour (lcd.withAlpha (0.07f));
            g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
            g.setColour (lcdDim);
            g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "K" : juce::String ((int) hz),
                        juce::Rectangle<float> (x - 20.0f, p.getBottom() + 3.0f, 40.0f, 13.0f), juce::Justification::centred);
        }
        for (float db : { -6.0f, -12.0f, -18.0f })
        {
            const float y = p.getY() + p.getHeight() * (-db / cutRange) * 0.8f;
            g.setColour (lcd.withAlpha (0.05f));
            g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
            g.setColour (amber.withAlpha (0.4f));
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (p.getRight() - 28.0f, y - 13.0f, 26.0f, 12.0f),
                        juce::Justification::centredRight);
        }

        const float alpha = active ? 1.0f : 0.35f;
        auto ySpec = [p] (float db) { return juce::jmap (juce::jlimit (specBottom, specTop, db), specBottom, specTop, p.getBottom(), p.getY() + p.getHeight() * 0.12f); };
        auto yCut  = [p] (float db) { return p.getY() + p.getHeight() * 0.8f * juce::jlimit (0.0f, 1.0f, -db / cutRange); };

        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p.toNearestInt());

            // This track
            auto m = curve (main, ySpec);
            auto mFill = m;
            mFill.lineTo (p.getRight(), p.getBottom()); mFill.lineTo (p.getX(), p.getBottom()); mFill.closeSubPath();
            g.setColour (violet.withAlpha (0.13f * alpha));
            g.fillPath (mFill);
            g.setColour (violet.withAlpha (0.5f * alpha));
            g.strokePath (m, juce::PathStrokeType (1.2f));

            // The track it listens to: phosphor line with bloom
            auto s = curve (side, ySpec);
            g.setColour (cyan.withAlpha (0.08f * alpha));
            g.strokePath (s, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (cyan.withAlpha (0.22f * alpha));
            g.strokePath (s, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (cyan.brighter (0.4f).withAlpha (alpha));
            g.strokePath (s, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // The cut, hanging from the top
            auto c = curve (cut, yCut);
            auto cFill = c;
            cFill.lineTo (p.getRight(), p.getY()); cFill.lineTo (p.getX(), p.getY()); cFill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (amber.withAlpha (0.40f * alpha), 0, p.getY(),
                                                     ember.withAlpha (0.10f * alpha), 0, p.getY() + p.getHeight() * 0.8f, false));
            g.fillPath (cFill);
            g.setColour (amber.withAlpha (0.25f * alpha));
            g.strokePath (c, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (amber.brighter (0.3f).withAlpha (alpha));
            g.strokePath (c, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Range markers: shade what's outside
        const float loHz = low.convertFrom0to1 (low.getValue()), hiHz = high.convertFrom0to1 (high.getValue());
        const float lx = xFor (loHz), hx = xFor (hiHz);
        g.setColour (juce::Colour (0xb0020409));
        if (lx > p.getX()) g.fillRect (juce::Rectangle<float> (p.getX(), p.getY(), lx - p.getX(), p.getHeight()));
        if (hx < p.getRight()) g.fillRect (juce::Rectangle<float> (hx, p.getY(), p.getRight() - hx, p.getHeight()));

        auto hzShort = [] (float hz) { return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) : juce::String (hz / 1000.0f, 1) + "K"; };
        auto marker = [&] (float x, const juce::String& label, int idx, bool right)
        {
            const bool hot = hover == idx || dragging == idx;
            g.setColour (lcd.withAlpha (hot ? 0.95f : 0.55f));
            g.drawLine (x, p.getY(), x, p.getBottom(), hot ? 1.8f : 1.0f);
            g.setFont (Fonts::lcd (10.5f));
            const float tw = 62.0f;
            auto tag = juce::Rectangle<float> (right ? x + 1.0f : x - tw - 1.0f, p.getBottom() - 16.0f, tw, 15.0f);
            g.setColour (lcd.withAlpha (hot ? 0.9f : 0.6f));
            g.fillRoundedRectangle (tag, 3.0f);
            g.setColour (glass);
            g.drawText (label, tag, juce::Justification::centred);
        };
        marker (lx, "LOW " + hzShort (loHz), 0, true);
        marker (hx, "HIGH " + hzShort (hiHz), 1, false);
    }

    // Status line (bottom) and value readout (top)
    g.setFont (Fonts::lcd (11.5f));
    const auto statusArea = r.withTrimmedTop (r.getHeight() - 24.0f).reduced (16.0f, 4.0f);
    g.setColour (statusColour.withAlpha (0.9f));
    g.fillEllipse (statusArea.getX(), statusArea.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (lcd.withAlpha (0.75f));
    g.drawFittedText (status.toUpperCase(), statusArea.withTrimmedLeft (14.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

    if (juce::Time::getMillisecondCounter() < readoutUntil && readout.isNotEmpty())
    {
        g.setFont (Fonts::lcd (15.0f));
        const auto box = juce::Rectangle<float> (r.getCentreX() - 90.0f, 12.0f, 180.0f, 24.0f);
        g.setColour (glass.withAlpha (0.85f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (lcd);
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.drawText (readout, box, juce::Justification::centred);
    }

    // Scanlines and glass reflection
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    for (float y = r.getY(); y < r.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    {
        juce::Path refl;
        refl.startNewSubPath (r.getX(), r.getY());
        refl.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
        refl.quadraticTo (r.getX() + r.getWidth() * 0.35f, r.getY() + r.getHeight() * 0.35f, r.getX(), r.getY() + r.getHeight() * 0.55f);
        refl.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.4f, false));
        g.fillPath (refl);
    }
    // Inner shadow from the bezel
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0, r.getY(),
                                             juce::Colours::transparentBlack, 0, r.getY() + 14.0f, false));
    g.fillRect (r.withHeight (14.0f));
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
    if (sendMode) return;
    const int h = markerNear (e.position.x);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (sendMode) return;
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
    if (sendMode) return;
    // Double-click a marker to open it all the way
    const int m = markerNear (e.position.x);
    if (m < 0) return;
    auto& prm = m == 0 ? low : high;
    prm.beginChangeGesture();
    prm.setValueNotifyingHost (prm.getDefaultValue());
    prm.endChangeGesture();
}
} // namespace outerspace::ui
