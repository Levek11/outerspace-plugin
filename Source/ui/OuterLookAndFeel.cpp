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
juce::Font Fonts::display (float height)
{
    static auto tf = juce::Typeface::createSystemTypefaceFor (OuterSpaceAssets::MichromaRegular_ttf,
                                                              OuterSpaceAssets::MichromaRegular_ttfSize);
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
    return display (height).withExtraKerningFactor (0.10f);
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

        // Smooth value noise for a soft mottle across the plate
        constexpr int cell = 48;
        const int gw = W / cell + 2, gh = H / cell + 2;
        std::vector<float> lattice ((size_t) (gw * gh));
        for (int i = 0; i < gw * gh; ++i) lattice[(size_t) i] = hash ((std::uint32_t) i * 747796405U + 12345U) - 0.5f;
        auto mottle = [&] (int x, int y)
        {
            const int cx = x / cell, cy = y / cell;
            float fx = (float) (x % cell) / cell, fy = (float) (y % cell) / cell;
            fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
            auto L = [&] (int i, int j) { return lattice[(size_t) (j * gw + i)]; };
            const float top = L (cx, cy) + (L (cx + 1, cy) - L (cx, cy)) * fx;
            const float bot = L (cx, cy + 1) + (L (cx + 1, cy + 1) - L (cx, cy + 1)) * fx;
            return top + (bot - top) * fy;
        };

        for (int y = 0; y < H; ++y)
        {
            const float fy = (float) y / (float) H;
            for (int x = 0; x < W; ++x)
            {
                const float fx = (float) x / (float) W;
                // Powder-coat grain: fine speckle, a few brighter flecks, soft mottle
                const float n = hash (((std::uint32_t) x * 73856093U) ^ ((std::uint32_t) y * 19349663U));
                float grain = (n - 0.5f) * 0.16f;
                if (n > 0.985f) grain += 0.10f;
                if (n < 0.012f) grain -= 0.10f;
                grain += mottle (x, y) * 0.09f;
                // Lighting: a soft pool of light at the top centre, vignette toward the edges
                const float dx = fx - 0.5f, dy = fy - 0.18f;
                float light = 1.18f - 0.55f * std::sqrt (dx * dx * 1.2f + dy * dy);
                light -= 0.18f * (std::pow (std::abs (fx - 0.5f) * 2.0f, 4.0f) + std::pow (std::abs (fy - 0.5f) * 2.0f, 4.0f));
                const float k = juce::jmax (0.3f, light) * (1.0f + grain);
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
                // Dark gunmetal: two bright wedges of sheen (top left / bottom right),
                // fine radial brushing, concentric lathe rings
                const float sheen = std::pow (std::abs (std::cos (th + 0.80f)), 6.0f);
                float v = 0.11f
                        + 0.24f * sheen
                        + 0.05f * std::pow (std::abs (std::sin (th + 0.80f)), 10.0f)
                        + 0.015f * std::sin (rr * 2.3f)
                        + (hash ((std::uint32_t) (std::lround (th * 1400.0f) + 9000)) - 0.5f) * 0.035f;
                const float a = juce::jlimit (0.0f, 1.0f, R - rr + 0.5f);   // anti-aliased edge
                const auto col = juce::Colour::fromRGBA (toByte (v * 0.98f), toByte (v * 1.0f), toByte (v * 1.06f), 255);
                bd.setPixelColour (x, y, col.withAlpha (a));
            }
    }
    return caps.emplace (d, img).first->second;
}

void OuterLookAndFeel::drawKnobWell (juce::Graphics& g, juce::Point<float> c, float wellR)
{
    // A round pocket milled into the plate: lip in shadow at the top, lit at the bottom
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.85f), c.x, c.y - wellR,
                                             juce::Colours::white.withAlpha (0.16f), c.x, c.y + wellR, false));
    g.fillEllipse (c.x - wellR - 1.5f, c.y - wellR - 1.5f, (wellR + 1.5f) * 2, (wellR + 1.5f) * 2);
    juce::ColourGradient floor (juce::Colour (0xff0d1018), c.x, c.y - wellR * 0.6f,
                                juce::Colour (0xff161a24), c.x, c.y + wellR, false);
    g.setGradientFill (floor);
    g.fillEllipse (c.x - wellR, c.y - wellR, wellR * 2, wellR * 2);
    // inner shadow along the top edge
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), c.x, c.y - wellR,
                                             juce::Colours::transparentBlack, c.x, c.y - wellR * 0.55f, false));
    g.fillEllipse (c.x - wellR, c.y - wellR, wellR * 2, wellR * 2);
}

void OuterLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    using namespace colours;
    const bool small = s.getProperties()["size"] == "small";
    const bool dimmed = (bool) s.getProperties()["dimmed"];
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const float outer = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float wellR = outer * 0.93f, arcR = outer * 0.82f, R = outer * (small ? 0.64f : 0.66f);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float lit = dimmed ? 0.3f : 1.0f;

    drawKnobWell (g, c, wellR);

    // --- Value arc: a faint track, the lit gold arc with a glow, a bright tip -----
    const float aw = small ? 2.4f : 3.0f;
    {
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (gold.withAlpha (0.10f));
        g.strokePath (track, juce::PathStrokeType (aw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (pos > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        for (auto [width, alpha] : { std::pair { aw * 5.0f, 0.05f }, std::pair { aw * 2.8f, 0.10f }, std::pair { aw * 1.7f, 0.18f } })
        {
            g.setColour (gold.withAlpha (alpha * lit));
            g.strokePath (arc, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setGradientFill (juce::ColourGradient (goldDark.brighter (0.4f).withAlpha (lit), c.x - arcR, c.y + arcR,
                                                 goldLight.withAlpha (lit), c.x + arcR, c.y - arcR, false));
        g.strokePath (arc, juce::PathStrokeType (aw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    {
        const auto tip = c.getPointOnCircumference (arcR, angle);
        const float tr = small ? 2.6f : 3.4f;
        g.setGradientFill (juce::ColourGradient (goldLight.withAlpha (0.45f * lit), tip.x, tip.y, goldLight.withAlpha (0.0f), tip.x + tr * 4, tip.y, true));
        g.fillEllipse (tip.x - tr * 4, tip.y - tr * 4, tr * 8, tr * 8);
        g.setColour (juce::Colour (0xfffff6e0).withAlpha (lit));
        g.fillEllipse (tip.x - tr, tip.y - tr, tr * 2, tr * 2);
    }

    // --- Knob: shadow, dark spun-metal face, gold bevel ----------------------------
    {
        juce::Path disc;
        disc.addEllipse (c.x - R, c.y - R, R * 2, R * 2);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), (int) (R * 0.5f), { 0, (int) (R * 0.20f) }).drawForPath (g, disc);
    }
    // outer gold bevel ring
    g.setGradientFill (juce::ColourGradient (goldLight, c.x - R * 0.7f, c.y - R * 0.8f, goldDark.darker (0.3f), c.x + R * 0.6f, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    // thin black gap
    const float rGap = R - (small ? 1.6f : 2.2f);
    g.setColour (juce::Colour (0xff07080b));
    g.fillEllipse (c.x - rGap, c.y - rGap, rGap * 2, rGap * 2);
    // spun face (its sheen stays put: lit from the top left)
    const float rFace = rGap - 1.2f;
    const float scale = juce::jmax (2.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    g.drawImage (capImage (juce::roundToInt (rFace * 2.0f * scale)),
                 juce::Rectangle<float> (c.x - rFace, c.y - rFace, rFace * 2, rFace * 2), juce::RectanglePlacement::stretchToFit);
    // soft dome highlight and rim shading
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), c.x - rFace * 0.35f, c.y - rFace * 0.45f,
                                             juce::Colours::transparentWhite, c.x - rFace * 0.35f + rFace * 0.9f, c.y, true));
    g.fillEllipse (c.x - rFace, c.y - rFace, rFace * 2, rFace * 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - rFace, c.y - rFace, rFace * 2, rFace * 2, 1.0f);

    // Small engraved notch at the edge, so the knob reads as turning
    {
        const auto p1 = c.getPointOnCircumference (rFace * 0.74f, angle), p2 = c.getPointOnCircumference (rFace * 0.93f, angle);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine ({ p1, p2 }, small ? 2.2f : 2.8f);
        g.setColour (goldLight.withAlpha (0.85f * lit));
        g.drawLine ({ p1.translated (-0.4f, -0.4f), p2.translated (-0.4f, -0.4f) }, small ? 1.1f : 1.4f);
    }

    if (dimmed)
    {
        g.setColour (plateDark.withAlpha (0.4f));
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
