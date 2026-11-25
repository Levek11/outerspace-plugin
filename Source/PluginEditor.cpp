#include "PluginEditor.h"

using namespace outerspace::ui;
using outerspace::link::Registry;

namespace
{
// Layout (the window is a fixed 600 x 700 panel)
const juce::Rectangle<int> screenArea   { 44, 96, 512, 230 };
const juce::Rectangle<int> selectorArea { 292, 38, 264, 38 };
const juce::Rectangle<int> switchArea   { 44, 346, 90, 118 };
const juce::Point<float>   linkLed      { 522.0f, 362.0f };
const juce::Point<int>     amountCentre { 300, 478 };
const juce::Point<int>     lowCentre    { 112, 596 };
const juce::Point<int>     highCentre   { 488, 596 };
constexpr int amountSize = 240, smallSize = 130;
}

OuterSpaceEditor::OuterSpaceEditor (OuterSpaceProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      display (p.getDisplay(), *p.apvts.getParameter (params::lowCut), *p.apvts.getParameter (params::highCut))
{
    setLookAndFeel (&lnf);

    // LISTEN TO selector (an LCD window)
    sourceBox.setTooltip ("The track to make room for. Put OuterSpace on that track (e.g. your lead vocal) and set it to SEND, or use Logic's side chain.");
    sourceBox.onChange = [this] { sourceChosen(); };
    addAndMakeVisible (sourceBox);

    addAndMakeVisible (display);

    roleSwitch.setTooltip ("CARVE: make room in this track. SEND: make this track a source other OuterSpace instances can listen to (audio passes through untouched).");
    roleSwitch.onChange = [this] (bool down) { setRole (down); };
    addAndMakeVisible (roleSwitch);

    auto setupKnob = [this] (juce::Slider& s, bool small)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        s.setMouseDragSensitivity (small ? 180 : 240);
        if (small) s.getProperties().set ("size", "small");
        addAndMakeVisible (s);
    };
    setupKnob (amount, false);
    setupKnob (lowKnob, true);
    setupKnob (highKnob, true);
    amount.setTooltip ("How deep the carve goes. Double-click to reset.");
    lowKnob.setTooltip ("Carve only above this frequency. You can also drag LOW on the screen.");
    highKnob.setTooltip ("Carve only below this frequency. You can also drag HIGH on the screen.");

    amountAtt = std::make_unique<SliderAttachment> (proc.apvts, params::amount, amount);
    lowAtt    = std::make_unique<SliderAttachment> (proc.apvts, params::lowCut, lowKnob);
    highAtt   = std::make_unique<SliderAttachment> (proc.apvts, params::highCut, highKnob);
    amount.setDoubleClickReturnValue (true, 30.0);
    lowKnob.setDoubleClickReturnValue (true, 20.0);
    highKnob.setDoubleClickReturnValue (true, 20000.0);

    auto hz = [] (double v) { return params::hzText ((float) v, 0).toUpperCase(); };
    amount.onValueChange   = [this] { if (amount.isMouseButtonDown()) display.showReadout ("AMOUNT " + juce::String (juce::roundToInt (amount.getValue())) + "%"); };
    lowKnob.onValueChange  = [this, hz] { if (lowKnob.isMouseButtonDown()) display.showReadout ("LOW CUT " + hz (lowKnob.getValue())); };
    highKnob.onValueChange = [this, hz] { if (highKnob.isMouseButtonDown()) display.showReadout ("HIGH CUT " + hz (highKnob.getValue())); };

    setSize (width, height);
    faceplate = Hardware::makeFaceplate (width, height, 2.0f);

    lastRoleSend = ! proc.isSendRole();   // force the first update
    rebuildSourceList (true);
    timerCallback();
    startTimerHz (30);
}

OuterSpaceEditor::~OuterSpaceEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void OuterSpaceEditor::setRole (bool send)
{
    auto* prm = proc.apvts.getParameter (params::role);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (send ? 1.0f : 0.0f);
    prm->endChangeGesture();
    updateRoleView();
}

void OuterSpaceEditor::updateRoleView()
{
    const bool send = proc.isSendRole();
    roleSwitch.setDown (send);

    for (auto* s : { &amount, &lowKnob, &highKnob })
    {
        s->getProperties().set ("dimmed", send);
        s->setEnabled (! send);
        s->repaint();
    }
    sourceBox.setEnabled (! send);
    sourceBox.setAlpha (send ? 0.45f : 1.0f);


    lastRoleSend = send;
    repaint();
}

void OuterSpaceEditor::rebuildSourceList (bool force)
{
    if (sourceBox.isPopupActive()) return;

    const auto sources = Registry::get().list();
    const auto currentId = proc.getListenId();
    juce::String sig = currentId + "|" + proc.getListenName();
    for (auto& s : sources) sig << "|" << s.id << "=" << s.name;
    if (! force && sig == listSignature) return;
    listSignature = sig;

    entries.clear();
    sourceBox.clear (juce::dontSendNotification);

    entries.push_back ({ OuterSpaceProcessor::hostSideChainId, {} });
    sourceBox.addItem ("Logic side chain", 1);

    sourceBox.addSectionHeading ("OuterSpace sends");
    int selected = currentId == OuterSpaceProcessor::hostSideChainId ? 1 : 0;
    for (auto& s : sources)
    {
        entries.push_back ({ s.id, s.name });
        const int itemId = (int) entries.size();
        sourceBox.addItem (s.name, itemId);
        if (s.id == currentId) selected = itemId;
    }

    if (selected == 0 && currentId != OuterSpaceProcessor::hostSideChainId)
    {
        // The chosen send isn't in the project right now: keep it, marked missing
        entries.push_back ({ currentId, proc.getListenName() });
        sourceBox.addItem (proc.getListenName() + "  (not found)", (int) entries.size());
        selected = (int) entries.size();
    }
    if (sources.empty())
    {
        sourceBox.addItem ("No sends yet: set OuterSpace on your vocal to SEND", 9999);
        sourceBox.setItemEnabled (9999, false);
    }

    sourceBox.setSelectedId (selected, juce::dontSendNotification);
}

void OuterSpaceEditor::sourceChosen()
{
    const int id = sourceBox.getSelectedId();
    if (id < 1 || id > (int) entries.size()) return;
    const auto& e = entries[(size_t) id - 1];
    if (e.id == proc.getListenId()) return;
    proc.setListenTo (e.id, e.name);
    rebuildSourceList (true);
}

void OuterSpaceEditor::timerCallback()
{
    const bool send = proc.isSendRole();
    if (send != lastRoleSend) updateRoleView();

    juce::Colour col;
    float bright = 1.0f;
    if (send)
    {
        const float lvl = proc.getSendLevelDb();
        display.setSendMode (true, lvl, proc.getSendName());
        const bool live = lvl > -60.0f;
        display.setStatus (live ? "Sending as " + proc.getSendName() + " - audio passes through untouched"
                                : "Waiting for audio on this track", live ? colours::green : colours::silkDim);
        col = colours::green; bright = live ? 1.0f : 0.0f;
    }
    else
    {
        display.setSendMode (false, -100.0f, {});
        if (++tick % 15 == 0) rebuildSourceList (false);

        using S = OuterSpaceProcessor::ListenStatus;
        const auto st = proc.getListenStatus();
        const bool host = proc.getListenId() == OuterSpaceProcessor::hostSideChainId;
        const auto name = proc.getListenName();
        switch (st)
        {
            case S::listening:
                display.setStatus (host ? "Listening to Logic side chain" : "Listening to " + name, colours::green);
                col = colours::green; break;
            case S::hostNotConnected:
                display.setStatus ("No signal: pick a track in Logic's Side Chain menu, or a send under Listen To", colours::amber);
                col = colours::amber; break;
            case S::sourceMissing:
                display.setStatus (name + " not found: set OuterSpace on that track to SEND", colours::amber);
                col = colours::amber; break;
            case S::sourceSilent:
                display.setStatus (name + " is quiet: carving starts when it plays", colours::silkDim);
                col = colours::amber; bright = 0.0f; break;
        }
        display.setActive (st == S::listening);
    }

    if (col != linkColour || std::abs (bright - linkBrightness) > 0.01f)
    {
        linkColour = col; linkBrightness = bright;
        repaint (juce::Rectangle<int> (0, 0, 40, 40).withCentre (linkLed.toInt()));
    }
    display.refresh();
}

//==============================================================================
void OuterSpaceEditor::paint (juce::Graphics& g)
{
    using namespace colours;
    g.drawImage (faceplate, getLocalBounds().toFloat());

    // Wordmark, printed on the plate. Michroma has one weight, so the ink is
    // thickened with a fine outline for a bolder logo.
    {
        const auto f = Fonts::display (25.0f).withExtraKerningFactor (0.06f);
        auto glyphs = [&f] (const juce::String& text, float x, float y)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (f, text, x, y);
            juce::Path p;
            ga.createPath (p);
            return p;
        };
        const float baseline = 61.0f;
        const auto outer = glyphs ("OUTER", 44.0f, baseline);
        const auto space = glyphs ("SPACE", 44.0f + outer.getBounds().getWidth() + 6.0f, baseline);
        const juce::PathStrokeType ink (0.9f, juce::PathStrokeType::mitered);

        for (auto* p : { &outer, &space })
        {
            const auto shadowT = juce::AffineTransform::translation (0.0f, 1.3f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillPath (*p, shadowT);
            g.strokePath (*p, ink, shadowT);
        }
        g.setColour (juce::Colour (0xffd5dae4));
        g.fillPath (outer);
        g.strokePath (outer, ink);
        g.setColour (gold);
        g.fillPath (space);
        g.strokePath (space, ink);

        Hardware::drawSilk (g, "SPECTRAL CARVER", { 45.0f, 68.0f, 240.0f, 13.0f }, 9.35f, juce::Justification::centredLeft, true);
    }

    // LISTEN TO
    Hardware::drawSilk (g, "LISTEN TO", selectorArea.toFloat().withY ((float) selectorArea.getY() - 16.0f).withHeight (12.0f).translated (4.0f, 0),
                        10.0f, juce::Justification::centredLeft);

    // Screen window
    Hardware::drawRecess (g, screenArea.toFloat(), 7.0f, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (screenArea.toFloat().expanded (1.0f), 7.5f, 1.5f);

    // LINK lamp
    Hardware::drawLed (g, linkLed, 4.5f, linkColour, linkBrightness);
    Hardware::drawSilk (g, "LINK", juce::Rectangle<float> (0, 0, 60, 12).withCentre (linkLed.translated (0, 18)), 10.0f);

    // Engraved rings around the Amount knob, fading toward the bottom
    {
        const auto c = amountCentre.toFloat();
        for (float r : { 132.0f, 150.0f })
        {
            juce::Path ring;
            ring.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), c.x, c.y - r,
                                                     juce::Colours::black.withAlpha (0.0f), c.x, c.y + r * 0.7f, false));
            g.strokePath (ring, juce::PathStrokeType (1.3f));
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), c.x, c.y - r,
                                                     juce::Colours::white.withAlpha (0.0f), c.x, c.y + r * 0.7f, false));
            g.strokePath (ring, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (0.0f, 1.3f));
        }
        Hardware::drawSilk (g, "0", juce::Rectangle<float> (0, 0, 30, 14).withCentre (c.getPointOnCircumference (100.0f, juce::degreesToRadians (225.0f)).translated (-10, 16)), 10.0f);
        Hardware::drawSilk (g, "100", juce::Rectangle<float> (0, 0, 44, 14).withCentre (c.getPointOnCircumference (100.0f, juce::degreesToRadians (495.0f)).translated (12, 16)), 10.0f);
        Hardware::drawSilk (g, "AMOUNT", juce::Rectangle<float> (0, 0, 160, 16).withCentre (c.translated (0, 126)), 13.0f);
    }

    // Low / high cut: name above, scale ends below
    auto smallLabels = [&] (juce::Point<int> centre, const juce::String& name)
    {
        const auto c = centre.toFloat();
        Hardware::drawSilk (g, name, juce::Rectangle<float> (0, 0, 160, 14).withCentre (c.translated (0, -76)), 11.5f);
        Hardware::drawSilk (g, "20", juce::Rectangle<float> (0, 0, 34, 12).withCentre (c.getPointOnCircumference (54.0f, juce::degreesToRadians (225.0f)).translated (-8, 14)), 8.5f, juce::Justification::centred, true);
        Hardware::drawSilk (g, "20K", juce::Rectangle<float> (0, 0, 34, 12).withCentre (c.getPointOnCircumference (54.0f, juce::degreesToRadians (495.0f)).translated (8, 14)), 8.5f, juce::Justification::centred, true);
    };
    smallLabels (lowCentre, "LOW CUT");
    smallLabels (highCentre, "HIGH CUT");

    // Model plate
    Hardware::drawSilk (g, juce::String::fromUTF8 ("MODEL OS-1   \xc2\xb7   LEVEK11 AUDIO"), juce::Rectangle<float> (0, (float) height - 30.0f, (float) width, 12.0f), 9.0f,
                        juce::Justification::centred, true);
}

void OuterSpaceEditor::resized()
{
    sourceBox.setBounds (selectorArea);
    display.setBounds (screenArea);
    roleSwitch.setBounds (switchArea);
    amount.setBounds (juce::Rectangle<int> (amountSize, amountSize).withCentre (amountCentre));
    lowKnob.setBounds (juce::Rectangle<int> (smallSize, smallSize).withCentre (lowCentre));
    highKnob.setBounds (juce::Rectangle<int> (smallSize, smallSize).withCentre (highCentre));
}
