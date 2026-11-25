#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/OuterLookAndFeel.h"
#include "ui/SpectrumDisplay.h"

class OuterSpaceEditor : public juce::AudioProcessorEditor,
                         public juce::Timer
{
public:
    explicit OuterSpaceEditor (OuterSpaceProcessor&);
    ~OuterSpaceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int width = 820, height = 430;

private:
    void timerCallback() override;
    void rebuildSourceList (bool force);
    void sourceChosen();
    void updateRoleView();
    void setRole (bool send);

    OuterSpaceProcessor& proc;
    outerspace::ui::OuterLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // Header
    juce::TextButton carveButton { "CARVE" }, sendButton { "SEND" };
    juce::Label listenLabel;
    juce::ComboBox sourceBox;

    // Carve
    juce::Slider amount, lowKnob, highKnob;
    juce::Label lowValue, highValue, lowTitle, highTitle;
    juce::TextButton freezeButton { "FREEZE" };
    outerspace::ui::SpectrumDisplay display;

    // Send
    juce::Label sendTitle, sendHelp;
    juce::TextEditor sendName;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> amountAtt, lowAtt, highAtt;
    std::unique_ptr<ButtonAttachment> freezeAtt;

    struct Entry { juce::String id, name; };
    std::vector<Entry> entries;      // index = combo item id - 1
    juce::String listSignature;
    bool lastRoleSend = false;

    std::vector<juce::Point<float>> stars;
    juce::String statusText;
    juce::Colour statusColour;
    float sendMeter = -100.0f;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OuterSpaceEditor)
};
