#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Spacer.h"

namespace outerspace::ui
{
/** Side chain (cyan), this track (violet) and the live cut (amber, hanging from
    the top). Drag the LOW / HIGH markers to set the carve range. */
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay (DisplayData& data, juce::RangedAudioParameter& lowCut, juce::RangedAudioParameter& highCut);

    void refresh();   // ~30 Hz, from the editor's timer
    void setActive (bool a) { active = a; }
    void setStatus (const juce::String& text, juce::Colour c) { status = text; statusColour = c; }
    /** Shows a value (e.g. "AMOUNT 35%") at the top of the screen for a moment. */
    void showReadout (const juce::String& text) { readout = text; readoutUntil = juce::Time::getMillisecondCounter() + 1400; }
    /** Send mode: the screen shows the send's name (an editor on top) and its level. */
    void setSendMode (bool send, float levelDb) { sendMode = send; sendLevel = levelDb; }
    juce::Rectangle<int> nameArea() const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plot() const;
    float xFor (float hz) const;
    float hzFor (float x) const;
    int markerNear (float x) const;   // 0 = low, 1 = high, -1 = none
    juce::Path curve (const std::array<float, kNumBands>& v, std::function<float (float)> yFor) const;

    DisplayData& data;
    juce::RangedAudioParameter& low;
    juce::RangedAudioParameter& high;
    std::array<float, kNumBands> side {}, main {}, cut {};
    int dragging = -1, hover = -1;
    bool active = true, sendMode = false;
    float sendLevel = -100.0f;
    juce::String status, readout;
    juce::Colour statusColour;
    juce::uint32 readoutUntil = 0;
};
} // namespace outerspace::ui
