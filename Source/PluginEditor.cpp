#include "PluginEditor.h"

using namespace outerspace::ui;
using outerspace::link::Registry;

namespace
{
// Layout (the window is a fixed 600 x 700 panel)
const juce::Rectangle<int> screenArea   { 44, 96, 512, 230 };
const juce::Rectangle<int> selectorArea { 292, 38, 264, 38 };
const juce::Rectangle<int> switchArea   { 52, 342, 70, 112 };
const juce::Point<float>   linkLed      { 522.0f, 362.0f };
const juce::Point<int>     amountCentre { 300, 478 };
const juce::Point<int>     lowCentre    { 116, 590 };
const juce::Point<int>     highCentre   { 484, 590 };
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

    // Send name, typed straight onto the screen
    sendName.setFont (Fonts::lcd (32.0f));
    sendName.setJustification (juce::Justification::centred);
    sendName.setIndents (8, 8);
    sendName.setTextToShowWhenEmpty ("LEAD VOCAL", colours::lcdDim);
    sendName.setTooltip ("This track's name in the LISTEN TO lists.");
    sendName.onReturnKey = [this] { proc.setSendName (sendName.getText()); sendName.giveAwayKeyboardFocus(); };
    sendName.onFocusLost = [this] { proc.setSendName (sendName.getText()); };
    sendName.onEscapeKey = [this] { sendName.setText (proc.getSendName(), false); sendName.giveAwayKeyboardFocus(); };
    addChildComponent (sendName);

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

    sendName.setVisible (send);
    if (send && ! sendName.hasKeyboardFocus (true))
        sendName.setText (proc.getSendName(), false);

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
        display.setSendMode (true, lvl);
        if (! sendName.hasKeyboardFocus (true) && sendName.getText() != proc.getSendName())
            sendName.setText (proc.getSendName(), false);
        const bool live = lvl > -60.0f;
        display.setStatus (live ? "Sending as " + proc.getSendName() + " - audio passes through untouched"
                                : "Waiting for audio on this track", live ? colours::green : colours::silkDim);
        col = colours::green; bright = live ? 1.0f : 0.0f;
    }
    else
    {
        display.setSendMode (false, -100.0f);
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

    // Screws
    Hardware::drawScrew (g, { 20.0f, 20.0f }, 6.0f, 0.35f);
    Hardware::drawScrew (g, { (float) width - 20.0f, 20.0f }, 6.0f, 1.1f);
    Hardware::drawScrew (g, { 20.0f, (float) height - 20.0f }, 6.0f, 0.8f);
    Hardware::drawScrew (g, { (float) width - 20.0f, (float) height - 20.0f }, 6.0f, 0.1f);

    // Wordmark: a ringed planet and the name, printed on the plate
    {
        const juce::Point<float> c (56.0f, 57.0f);
        g.setColour (amber);
        g.fillEllipse (c.x - 8.0f, c.y - 8.0f, 16.0f, 16.0f);
        g.setColour (silk.withAlpha (0.9f));
        g.drawEllipse (juce::Rectangle<float> (c.x - 15.0f, c.y - 4.5f, 30.0f, 9.0f), 1.6f);
        g.setColour (amber);   // the front of the planet over the ring
        g.fillRect (c.x - 6.5f, c.y + 0.5f, 13.0f, 3.0f);

        g.setFont (Fonts::wordmark (25.0f));
        g.setColour (silk.withAlpha (0.92f));
        g.drawText ("OUTER", 80, 42, 100, 30, juce::Justification::centredLeft);
        const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (Fonts::wordmark (25.0f), "OUTER"));
        g.setColour (amber);
        g.drawText ("SPACE", 80 + w + 1, 42, 110, 30, juce::Justification::centredLeft);
        Hardware::drawSilk (g, "SPECTRAL CARVER", { 81.0f, 70.0f, 200.0f, 12.0f }, 9.0f, juce::Justification::centredLeft, true);
    }

    // LISTEN TO
    Hardware::drawSilk (g, "LISTEN TO", selectorArea.toFloat().withY ((float) selectorArea.getY() - 15.0f).withHeight (12.0f).translated (4.0f, 0),
                        10.0f, juce::Justification::centredLeft);

    // Screen window
    Hardware::drawRecess (g, screenArea.toFloat(), 7.0f, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (screenArea.toFloat().expanded (1.0f), 7.5f, 1.5f);

    // LINK lamp
    Hardware::drawLed (g, linkLed, 4.5f, linkColour, linkBrightness);
    Hardware::drawSilk (g, "LINK", juce::Rectangle<float> (0, 0, 60, 12).withCentre (linkLed.translated (0, 18)), 10.0f);

    // Amount: scale labels
    {
        const auto c = amountCentre.toFloat();
        Hardware::drawSilk (g, "0", juce::Rectangle<float> (0, 0, 30, 14).withCentre (c.getPointOnCircumference (112.0f, juce::degreesToRadians (225.0f)).translated (-4, 10)), 11.0f);
        Hardware::drawSilk (g, "100", juce::Rectangle<float> (0, 0, 40, 14).withCentre (c.getPointOnCircumference (112.0f, juce::degreesToRadians (495.0f)).translated (6, 10)), 11.0f);
        Hardware::drawSilk (g, "AMOUNT", juce::Rectangle<float> (0, 0, 120, 16).withCentre (c.translated (0, 118)), 12.5f);
    }

    // Low / high cut labels and scale ends
    auto smallLabels = [&] (juce::Point<int> centre, const juce::String& name)
    {
        const auto c = centre.toFloat();
        Hardware::drawSilk (g, "20", juce::Rectangle<float> (0, 0, 30, 12).withCentre (c.getPointOnCircumference (60.0f, juce::degreesToRadians (225.0f)).translated (-6, 8)), 9.5f, juce::Justification::centred, true);
        Hardware::drawSilk (g, "20K", juce::Rectangle<float> (0, 0, 30, 12).withCentre (c.getPointOnCircumference (60.0f, juce::degreesToRadians (495.0f)).translated (6, 8)), 9.5f, juce::Justification::centred, true);
        Hardware::drawSilk (g, name, juce::Rectangle<float> (0, 0, 120, 14).withCentre (c.translated (0, 66)), 11.5f);
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
    sendName.setBounds (display.nameArea().translated (screenArea.getX(), screenArea.getY()));
}
