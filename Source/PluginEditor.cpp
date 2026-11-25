#include "PluginEditor.h"

using namespace outerspace::ui;
using outerspace::link::Registry;

OuterSpaceEditor::OuterSpaceEditor (OuterSpaceProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      display (p.getDisplay(), *p.apvts.getParameter (params::lowCut), *p.apvts.getParameter (params::highCut))
{
    setLookAndFeel (&lnf);

    // --- Header ---
    for (auto* b : { &carveButton, &sendButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    carveButton.setTooltip ("Carve this track to make room for the track you listen to.");
    sendButton.setTooltip ("Make this track a source that other OuterSpace instances can listen to. Audio passes through untouched.");
    carveButton.onClick = [this] { setRole (false); };
    sendButton.onClick  = [this] { setRole (true); };

    listenLabel.setText ("LISTEN TO", juce::dontSendNotification);
    listenLabel.setFont (Fonts::ui (11.5f, true));
    listenLabel.setColour (juce::Label::textColourId, colours::dim);
    listenLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (listenLabel);

    sourceBox.setTooltip ("The track to make room for. Put OuterSpace on that track (e.g. your lead vocal) and set it to SEND, or use Logic's side chain.");
    sourceBox.onChange = [this] { sourceChosen(); };
    addAndMakeVisible (sourceBox);

    // --- Carve ---
    amount.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    amount.setMouseDragSensitivity (220);
    amount.setTooltip ("How deep the carve goes. Double-click to reset.");
    addAndMakeVisible (amount);
    amountAtt = std::make_unique<SliderAttachment> (proc.apvts, params::amount, amount);
    amount.setDoubleClickReturnValue (true, 30.0);

    auto setupSmall = [this] (juce::Slider& s, juce::Label& title, juce::Label& value, const juce::String& name, const char* id)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        s.getProperties().set ("accent", "cyan");
        addAndMakeVisible (s);
        title.setText (name, juce::dontSendNotification);
        title.setFont (Fonts::ui (11.0f, true));
        title.setColour (juce::Label::textColourId, colours::dim);
        addAndMakeVisible (title);
        value.setFont (Fonts::ui (15.0f, true));
        value.setColour (juce::Label::textColourId, colours::text);
        value.setEditable (false, true);   // double-click to type a value
        addAndMakeVisible (value);
        juce::ignoreUnused (id);
    };
    setupSmall (lowKnob, lowTitle, lowValue, "LOW CUT", params::lowCut);
    setupSmall (highKnob, highTitle, highValue, "HIGH CUT", params::highCut);
    lowKnob.setTooltip ("Carve only above this frequency. You can also drag LOW on the display.");
    highKnob.setTooltip ("Carve only below this frequency. You can also drag HIGH on the display.");
    lowAtt  = std::make_unique<SliderAttachment> (proc.apvts, params::lowCut, lowKnob);
    highAtt = std::make_unique<SliderAttachment> (proc.apvts, params::highCut, highKnob);
    lowKnob.setDoubleClickReturnValue (true, 20.0);
    highKnob.setDoubleClickReturnValue (true, 20000.0);
    for (auto* l : { &lowValue, &highValue })
        l->onTextChange = [this, l]
        {
            auto& knob = l == &lowValue ? lowKnob : highKnob;
            knob.setValue (params::hzValue (l->getText()), juce::sendNotificationSync);
        };
    lowValue.setJustificationType (juce::Justification::centredLeft);
    highValue.setJustificationType (juce::Justification::centredRight);
    highTitle.setJustificationType (juce::Justification::centredRight);

    freezeButton.setClickingTogglesState (true);
    freezeButton.getProperties().set ("accent", "amber");
    freezeButton.setTooltip ("Hold the current carve curve.");
    addAndMakeVisible (freezeButton);
    freezeAtt = std::make_unique<ButtonAttachment> (proc.apvts, params::freeze, freezeButton);

    addAndMakeVisible (display);

    // --- Send ---
    sendTitle.setText ("THIS TRACK IS A SOURCE, NAMED", juce::dontSendNotification);
    sendTitle.setFont (Fonts::ui (12.0f, true));
    sendTitle.setColour (juce::Label::textColourId, colours::dim);
    sendTitle.setJustificationType (juce::Justification::centred);
    addChildComponent (sendTitle);

    sendName.setFont (Fonts::wordmark (30.0f));
    sendName.setJustification (juce::Justification::centred);
    sendName.setIndents (12, 10);
    sendName.setTextToShowWhenEmpty ("Lead Vocal", colours::dim);
    sendName.onReturnKey = [this] { proc.setSendName (sendName.getText()); sendName.giveAwayKeyboardFocus(); };
    sendName.onFocusLost = [this] { proc.setSendName (sendName.getText()); };
    sendName.onEscapeKey = [this] { sendName.setText (proc.getSendName(), false); sendName.giveAwayKeyboardFocus(); };
    addChildComponent (sendName);

    sendHelp.setText ("Audio passes through untouched.\nOn the piano, pads or guitars, open OuterSpace and pick this name under LISTEN TO.",
                      juce::dontSendNotification);
    sendHelp.setFont (Fonts::ui (14.0f));
    sendHelp.setColour (juce::Label::textColourId, colours::dim);
    sendHelp.setJustificationType (juce::Justification::centred);
    addChildComponent (sendHelp);

    // Starfield
    juce::Random rng (7);
    for (int i = 0; i < 90; ++i)
        stars.push_back ({ rng.nextFloat() * (float) width, rng.nextFloat() * (float) height });

    setSize (width, height);
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
    carveButton.setToggleState (! send, juce::dontSendNotification);
    sendButton.setToggleState (send, juce::dontSendNotification);

    for (juce::Component* c : { (juce::Component*) &amount, (juce::Component*) &lowKnob, (juce::Component*) &highKnob,
                                (juce::Component*) &lowValue, (juce::Component*) &highValue, (juce::Component*) &lowTitle,
                                (juce::Component*) &highTitle, (juce::Component*) &freezeButton, (juce::Component*) &display,
                                (juce::Component*) &sourceBox, (juce::Component*) &listenLabel })
        c->setVisible (! send);
    for (juce::Component* c : { (juce::Component*) &sendTitle, (juce::Component*) &sendName, (juce::Component*) &sendHelp })
        c->setVisible (send);

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
        // Chosen source isn't in the project right now: keep it, marked missing
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
    if (proc.isSendRole() != lastRoleSend) updateRoleView();

    if (proc.isSendRole())
    {
        sendMeter = proc.getSendLevelDb();
        if (! sendName.hasKeyboardFocus (true) && sendName.getText() != proc.getSendName())
            sendName.setText (proc.getSendName(), false);
        repaint (getLocalBounds().withTrimmedTop (height - 60));
        repaint (sendName.getBounds().translated (0, 70).withHeight (20));
        return;
    }

    if (++tick % 15 == 0) rebuildSourceList (false);

    auto hz = [] (double v) { return params::hzText ((float) v, 0); };
    if (! lowValue.isBeingEdited())  lowValue.setText (hz (lowKnob.getValue()), juce::dontSendNotification);
    if (! highValue.isBeingEdited()) highValue.setText (hz (highKnob.getValue()), juce::dontSendNotification);

    using S = OuterSpaceProcessor::ListenStatus;
    const auto st = proc.getListenStatus();
    const bool host = proc.getListenId() == OuterSpaceProcessor::hostSideChainId;
    const auto name = proc.getListenName();
    juce::String text; juce::Colour col;
    switch (st)
    {
        case S::listening:
            text = host ? "Listening to Logic's side chain" : "Listening to " + name;
            col = colours::ok; break;
        case S::hostNotConnected:
            text = "Nothing on Logic's side chain. Pick a track in the Side Chain menu at the top of this window, or choose a send under LISTEN TO.";
            col = colours::warn; break;
        case S::sourceMissing:
            text = name + " isn't in this project right now. Put OuterSpace on that track and set it to SEND.";
            col = colours::warn; break;
        case S::sourceSilent:
            text = name + " is quiet. OuterSpace carves when it plays.";
            col = colours::dim; break;
    }
    if (text != statusText || col != statusColour)
    {
        statusText = text; statusColour = col;
        repaint (getLocalBounds().withTrimmedTop (height - 40));
    }

    display.setActive (st == S::listening || proc.apvts.getRawParameterValue (params::freeze)->load() > 0.5f);
    display.refresh();
}

//==============================================================================
void OuterSpaceEditor::paint (juce::Graphics& g)
{
    using namespace colours;
    g.setGradientFill (juce::ColourGradient (bgTop, 0, 0, bgBottom, 0, (float) height, false));
    g.fillAll();

    // Stars
    juce::Random rng (11);
    for (auto& s : stars)
    {
        const float a = 0.12f + rng.nextFloat() * 0.35f, r = rng.nextFloat() < 0.1f ? 1.6f : 1.0f;
        g.setColour (juce::Colours::white.withAlpha (a));
        g.fillEllipse (s.x, s.y, r, r);
    }

    // A planet's limb rising in the bottom-left corner
    {
        const float R = 420.0f;
        const juce::Point<float> c (-120.0f, (float) height + 330.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x18ffb454), c.x + R * 0.5f, c.y - R,
                                                 juce::Colour (0x00ffb454), c.x, c.y, true));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        g.setColour (amber.withAlpha (0.18f));
        g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.2f);
    }

    // Wordmark
    g.setFont (Fonts::wordmark (24.0f));
    g.setColour (juce::Colours::white);
    g.drawText ("OUTER", 26, 18, 90, 30, juce::Justification::centredLeft);
    g.setColour (amber);
    g.drawText ("SPACE", 26 + juce::roundToInt (juce::GlyphArrangement::getStringWidth (Fonts::wordmark (24.0f), "OUTER")) + 1,
                18, 100, 30, juce::Justification::centredLeft);

    g.setColour (edge);
    g.drawHorizontalLine (64, 20.0f, (float) width - 20.0f);

    if (proc.isSendRole())
    {
        // Level meter under the name
        auto m = sendName.getBounds().translated (0, 76).withHeight (6).toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (m, 3.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (sendMeter + 60.0f) / 60.0f);
        g.setColour (cyan);
        g.fillRoundedRectangle (m.withWidth (m.getWidth() * t), 3.0f);

        g.setColour (sendMeter > -60.0f ? ok : dim);
        g.fillEllipse (26.0f, (float) height - 27.0f, 8.0f, 8.0f);
        g.setFont (Fonts::ui (13.0f));
        g.setColour (text);
        g.drawText (sendMeter > -60.0f ? "Sending as " + proc.getSendName() : "Waiting for audio on this track",
                    42, height - 36, width - 70, 24, juce::Justification::centredLeft);
        return;
    }

    // Status line
    g.setColour (statusColour);
    g.fillEllipse (26.0f, (float) height - 27.0f, 8.0f, 8.0f);
    g.setFont (Fonts::ui (13.0f));
    g.setColour (text.withAlpha (0.9f));
    g.drawFittedText (statusText, 42, height - 36, width - 70, 24, juce::Justification::centredLeft, 1, 0.85f);
}

void OuterSpaceEditor::resized()
{
    // Header
    sendButton.setBounds (width - 26 - 84, 20, 84, 30);
    carveButton.setBounds (sendButton.getX() - 90, 20, 84, 30);
    sourceBox.setBounds (carveButton.getX() - 30 - 280, 18, 280, 34);
    listenLabel.setBounds (sourceBox.getX() - 90, 18, 84, 34);

    // Carve body
    const int top = 82;
    amount.setBounds (22, top + 14, 230, 230);
    freezeButton.setBounds (22 + 115 - 50, top + 258, 100, 30);

    const int dx = 272, dw = width - dx - 26;
    display.setBounds (dx, top, dw, 232);

    const int rowY = top + 244;
    lowKnob.setBounds (dx, rowY, 46, 46);
    lowTitle.setBounds (dx + 50, rowY + 2, 120, 16);
    lowValue.setBounds (dx + 46, rowY + 18, 120, 24);

    highKnob.setBounds (dx + dw - 46, rowY, 46, 46);
    highTitle.setBounds (dx + dw - 50 - 120, rowY + 2, 120, 16);
    highValue.setBounds (dx + dw - 46 - 120, rowY + 18, 120, 24);

    // Send body
    sendTitle.setBounds (0, 120, width, 20);
    sendName.setBounds (width / 2 - 220, 150, 440, 56);
    sendHelp.setBounds (width / 2 - 320, 250, 640, 60);
}
