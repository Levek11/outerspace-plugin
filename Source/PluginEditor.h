#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/OuterLookAndFeel.h"
#include "ui/SpectrumDisplay.h"
#include "ui/BatToggle.h"

class OuterSpaceEditor : public juce::AudioProcessorEditor,
                         public juce::Timer
{
public:
    explicit OuterSpaceEditor (OuterSpaceProcessor&);
    ~OuterSpaceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    static constexpr int width = 600, height = 700;

private:
    void rebuildSourceList (bool force);
    void sourceChosen();
    void updateRoleView();
    void setRole (bool send);

    OuterSpaceProcessor& proc;
    outerspace::ui::OuterLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    juce::ComboBox sourceBox;
    outerspace::ui::SpectrumDisplay display;
    outerspace::ui::BatToggle roleSwitch { "CARVE", "SEND" };
    juce::Slider amount, lowKnob, highKnob;
    juce::TextEditor sendName;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> amountAtt, lowAtt, highAtt;

    struct Entry { juce::String id, name; };
    std::vector<Entry> entries;   // index = combo item id - 1
    juce::String listSignature;
    bool lastRoleSend = false;
    int tick = 0;

    juce::Image faceplate;
    juce::Colour linkColour;
    float linkBrightness = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OuterSpaceEditor)
};
