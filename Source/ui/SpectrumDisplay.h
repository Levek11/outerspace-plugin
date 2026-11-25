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
    bool active = true;
};
} // namespace outerspace::ui
