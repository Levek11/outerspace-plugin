#include "OuterLookAndFeel.h"
#include "OuterSpaceAssets.h"
#include <cmath>

namespace outerspace::ui
{
namespace
{
constexpr float pi = juce::MathConstants<float>::pi;

inline float hash (std::uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (float) (x & 0xffffff) / (float) 0xffffff;   // 0..1
}

inline juce::uint8 toByte (float v) { return (juce::uint8) juce::jlimit (0, 255, (int) std::lround (v * 255.0f)); }

/** Light comes from the top left. 1 = facing the light, 0 = facing away. */
inline float facing (float angle) { return 0.5f + 0.5f * std::cos (angle - (-0.75f * pi)); }
} // namespace

//==============================================================================
juce::Font Fonts::wordmark (float height)
{
    static auto tf = juce::Typeface::createSystemTypefaceFor (OuterSpaceAssets::ArchivoBlackRegular_ttf,
                                                              OuterSpaceAssets::ArchivoBlackRegular_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

juce::Font Fonts::lcd (float height)
{
    static auto tf = juce::Typeface::createSystemTypefaceFor (OuterSpaceAssets::GoldmanBold_ttf,
                                                              OuterSpaceAssets::GoldmanBold_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

juce::Font Fonts::silk (float height)
{
    return juce::Font (juce::FontOptions (height, juce::Font::bold)).withExtraKerningFactor (0.14f);
}

//==============================================================================
juce::Image Hardware::makeFaceplate (int w, int h, float scale)
{
    const int W = juce::roundToInt ((float) w * scale), H = juce::roundToInt ((float) h * scale);
    juce::Image img (juce::Image::ARGB, W, H, true);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const auto base = colours::plate;
        const float br = base.getFloatRed(), bg = base.getFloatGreen(), bb = base.getFloatBlue();

        std::vector<float> row ((size_t) H);
        for (int y = 0; y < H; ++y) row[(size_t) y] = hash ((std::uint32_t) y * 2654435761U) - 0.5f;

        for (int y = 0; y < H; ++y)
        {
            const float fy = (float) y / (float) H;
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x / (float) W;
                // Horizontal brushing: per-row streaks that drift slowly along x
                const float streak = row[(size_t) y] * 0.55f
                                   + (hash ((std::uint32_t) (y * 977 + (x / 37) * 131)) - 0.5f) * 0.25f
                                   + (hash ((std::uint32_t) (x * 7919 + y * 104729)) - 0.5f) * 0.25f;
                // Lighting: brighter at the top left, falling off toward the bottom
                float light = 1.08f - 0.30f * fy - 0.08f * fx;
                light += 0.10f * std::exp (-((fx - 0.25f) * (fx - 0.25f) * 6.0f + (fy - 0.10f) * (fy - 0.10f) * 10.0f));
                const float k = light * (1.0f + streak * 0.11f);
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (br * k, bg * k, bb * k, 1.0f));
            }
        }
    }

    // Bevelled edge
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0, 0,
                                             juce::Colours::black.withAlpha (0.55f), 0, (float) h, false));
    g.drawRoundedRectangle (r.reduced (1.0f), 12.0f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.25f), 12.5f, 0.75f);
    return img;
}

void Hardware::drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float slotAngle)
{
    // Countersink shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r - 1.5f, c.y - r - 0.5f, (r + 1.5f) * 2, (r + 1.5f) * 2);
    // Head
    juce::ColourGradient head (juce::Colour (0xffd5dae4), c.x - r * 0.6f, c.y - r * 0.7f,
                               juce::Colour (0xff4a5163), c.x + r * 0.7f, c.y + r * 0.9f, true);
    g.setGradientFill (head);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
    // Phillips slot, with a highlight on the lit edge
    for (float a : { slotAngle, slotAngle + pi * 0.5f })
    {
        const auto d = juce::Point<float> (std::cos (a), std::sin (a)) * (r * 0.68f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine ({ c - d + juce::Point<float> (0.6f, 0.8f), c + d + juce::Point<float> (0.6f, 0.8f) }, 1.4f);
        g.setColour (juce::Colour (0xff141821));
        g.drawLine ({ c - d, c + d }, 1.6f);
    }
}

void Hardware::drawSilk (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float height,
                         juce::Justification just, bool dim)
{
    g.setFont (Fonts::silk (height));
    g.setColour (dim ? colours::silkDim : colours::silk.withAlpha (0.88f));
    g.drawText (text, r, just, false);
}

void Hardware::drawRecess (juce::Graphics& g, juce::Rectangle<float> r, float corner, float depth)
{
    // Chamfer: dark on the top/left (in shadow), lit on the bottom/right
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.75f), r.getX(), r.getY(),
                                             juce::Colours::white.withAlpha (0.20f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r.expanded (depth), corner + depth);
    g.setColour (colours::glass);
    g.fillRoundedRectangle (r, corner);
}

void Hardware::drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col, float brightness)
{
    // Bezel hole
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillEllipse (c.x - r - 1.2f, c.y - r - 1.2f, (r + 1.2f) * 2, (r + 1.2f) * 2);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (c.x - r - 1.2f, c.y - r - 0.6f, (r + 1.2f) * 2, (r + 1.2f) * 2, 0.6f);

    const float b = juce::jlimit (0.0f, 1.0f, brightness);
    // Lens
    const auto off = col.withMultipliedBrightness (0.22f).withMultipliedSaturation (0.6f);
    const auto lens = off.interpolatedWith (col, b);
    juce::ColourGradient lg (lens.brighter (0.6f * b + 0.1f), c.x - r * 0.3f, c.y - r * 0.4f, lens.darker (0.3f), c.x, c.y + r, true);
    g.setGradientFill (lg);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    // Glow
    if (b > 0.02f)
    {
        const float gr = r * 3.2f;
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.38f * b), c.x, c.y, col.withAlpha (0.0f), c.x + gr, c.y, true));
        g.fillEllipse (c.x - gr, c.y - gr, gr * 2, gr * 2);
    }
    // Specular dot
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.45f, r * 0.35f);
}

//==============================================================================
OuterLookAndFeel::OuterLookAndFeel()
{
    using namespace colours;
    setColour (juce::ComboBox::textColourId, lcd);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0b0f1a));
    setColour (juce::PopupMenu::textColourId, silk);
    setColour (juce::PopupMenu::headerTextColourId, silkDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, lcd.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::textColourId, lcd);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, lcd.withAlpha (0.4f));
    setColour (juce::TextEditor::highlightColourId, lcd.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, lcd);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0b0f1a));
    setColour (juce::TooltipWindow::textColourId, silk);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff2a3352));
}

const juce::Image& OuterLookAndFeel::capImage (int d)
{
    auto it = caps.find (d);
    if (it != caps.end()) return it->second;

    // Spun ("turned") aluminium: a conic sheen that stays put while the knob
    // turns, fine concentric machining rings, and a soft dome highlight.
    juce::Image img (juce::Image::ARGB, d, d, true);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float c = (float) d * 0.5f, R = c;
        for (int y = 0; y < d; ++y)
            for (int x = 0; x < d; ++x)
            {
                const float dx = (float) x + 0.5f - c, dy = (float) y + 0.5f - c;
                const float rr = std::sqrt (dx * dx + dy * dy);
                if (rr > R) continue;
                const float th = std::atan2 (dy, dx);
                float v = 0.60f
                        + 0.17f * std::cos (2.0f * (th + 0.80f))
                        + 0.06f * std::cos (4.0f * th + 1.3f)
                        + 0.035f * std::sin (rr * 1.9f) * (rr / R)
                        + (hash ((std::uint32_t) (std::lround (th * 900.0f) + 5000)) - 0.5f) * 0.05f;
                v += 0.10f * std::exp (-((dx + R * 0.3f) * (dx + R * 0.3f) + (dy + R * 0.35f) * (dy + R * 0.35f)) / (R * R * 0.18f));
                const float a = juce::jlimit (0.0f, 1.0f, R - rr + 0.5f);   // anti-aliased edge
                const auto col = juce::Colour::fromRGBA (toByte (v * 0.95f), toByte (v * 0.97f), toByte (v * 1.02f), 255);
                bd.setPixelColour (x, y, col.withAlpha (a));
            }
    }
    return caps.emplace (d, img).first->second;
}

void OuterLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const bool small = s.getProperties()["size"] == "small";
    const bool dimmed = (bool) s.getProperties()["dimmed"];
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const float outer = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float R = small ? outer * 0.62f : outer * 0.70f;   // knob radius; the rest is the scale / LED ring
    const float angle = startAngle + pos * (endAngle - startAngle);

    // --- Scale printed on the plate (small) / LED ring (big) -----------------
    if (small)
    {
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const bool major = i == 0 || i == 5 || i == 10;
            const auto p1 = c.getPointOnCircumference (R + 6.0f, a), p2 = c.getPointOnCircumference (R + (major ? 13.0f : 10.0f), a);
            g.setColour (colours::silk.withAlpha (major ? 0.85f : 0.5f));
            g.drawLine ({ p1, p2 }, major ? 1.6f : 1.1f);
        }
    }
    else
    {
        constexpr int leds = 21;
        const float ringR = R + 17.0f;
        for (int i = 0; i < leds; ++i)
        {
            const float t = (float) i / (leds - 1);
            const float a = startAngle + (endAngle - startAngle) * t;
            const float lit = juce::jlimit (0.0f, 1.0f, (pos - t) * (leds - 1) + 1.0f);
            const auto col = colours::amber.interpolatedWith (colours::ember, t);
            Hardware::drawLed (g, c.getPointOnCircumference (ringR, a), 3.2f, col, (dimmed ? 0.25f : 1.0f) * lit);
        }
    }

    // --- Shadow on the plate ---------------------------------------------------
    {
        juce::Path disc;
        disc.addEllipse (c.x - R, c.y - R, R * 2, R * 2);
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), (int) (R * 0.45f), { 0, (int) (R * 0.22f) }).drawForPath (g, disc);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), (int) (R * 0.10f), { 0, (int) (R * 0.05f) + 1 }).drawForPath (g, disc);
    }

    // --- Knurled skirt -----------------------------------------------------------
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3f4556), c.x, c.y - R,
                                             juce::Colour (0xff12151d), c.x, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);

    const int ridges = small ? 36 : 64;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = angle + 2.0f * pi * (float) i / (float) ridges;   // ridges turn with the knob
        const float lightAmt = facing (a);
        const auto p1 = c.getPointOnCircumference (R * 0.86f, a), p2 = c.getPointOnCircumference (R * 0.995f, a);
        g.setColour (juce::Colours::white.withAlpha (0.05f + 0.20f * lightAmt));
        g.drawLine ({ p1, p2 }, small ? 1.0f : 1.3f);
        const float a2 = a + pi / (float) ridges;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine ({ c.getPointOnCircumference (R * 0.86f, a2), c.getPointOnCircumference (R * 0.995f, a2) }, small ? 1.0f : 1.3f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);

    // --- Chamfer between skirt and cap ----------------------------------------
    const float rc = R * 0.82f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), c.x - rc * 0.5f, c.y - rc,
                                             juce::Colours::black.withAlpha (0.7f), c.x + rc * 0.4f, c.y + rc, false));
    g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);

    // --- Spun aluminium cap (lighting doesn't turn with the knob) -------------
    const float rcap = R * 0.76f;
    const float scale = juce::jmax (2.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int px = juce::roundToInt (rcap * 2.0f * scale);
    g.drawImage (capImage (px), juce::Rectangle<float> (c.x - rcap, c.y - rcap, rcap * 2, rcap * 2),
                 juce::RectanglePlacement::stretchToFit);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - rcap, c.y - rcap, rcap * 2, rcap * 2, 0.8f);

    // --- Engraved pointer ---------------------------------------------------------
    {
        const auto inner = c.getPointOnCircumference (rcap * 0.22f, angle);
        const auto tip = c.getPointOnCircumference (rcap * 0.86f, angle);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawLine ({ inner.translated (0.6f, 0.9f), tip.translated (0.6f, 0.9f) }, small ? 2.0f : 3.0f);
        g.setColour (juce::Colour (0xff15181f));
        g.drawLine ({ inner, tip }, small ? 2.0f : 3.0f);
        // painted dot at the tip
        const auto dot = c.getPointOnCircumference (rcap * 0.70f, angle);
        const float dr = small ? 2.4f : 3.6f;
        const auto paint = small ? colours::cyan : colours::amber;
        g.setColour (paint.withAlpha (dimmed ? 0.4f : 1.0f));
        g.fillEllipse (dot.x - dr, dot.y - dr, dr * 2, dr * 2);
    }

    if (dimmed)
    {
        g.setColour (colours::plateDark.withAlpha (0.35f));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    }
}

//==============================================================================
void OuterLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (3, 3, (float) w - 6, (float) h - 6);
    Hardware::drawRecess (g, r, 4.0f, 2.5f);
    // glass sheen
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0, r.getY(),
                                             juce::Colours::transparentWhite, 0, r.getCentreY(), false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f), 4.0f);

    const float cy = (float) h * 0.5f, ax = (float) w - 20.0f;
    juce::Path tri;
    tri.addTriangle (ax - 5.0f, cy - 2.5f, ax + 5.0f, cy - 2.5f, ax, cy + 3.5f);
    g.setColour ((box.isMouseOver (true) || box.isPopupActive()) ? colours::lcd : colours::lcdDim);
    g.fillPath (tri);
}

void OuterLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 3, box.getWidth() - 40, box.getHeight() - 6);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}
} // namespace outerspace::ui
