#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace params
{
inline constexpr const char* role    = "role";     // 0 = Carve, 1 = Send
inline constexpr const char* amount  = "amount";   // %
inline constexpr const char* lowCut  = "lowcut";   // Hz
inline constexpr const char* highCut = "highcut";  // Hz

inline juce::String hzText (float hz, int)
{
    return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz"
                        : juce::String (hz / 1000.0f, hz < 10000.0f ? 2 : 1) + " kHz";
}

inline float hzValue (const juce::String& t)
{
    const auto v = t.retainCharacters ("0123456789.").getFloatValue();
    return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { role, 1 }, "Role",
                                                        StringArray { "Carve", "Send" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { amount, 1 }, "Amount", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
        AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v)) + "%"; })));

    NormalisableRange<float> hz (20.0f, 20000.0f);
    hz.setSkewForCentre (632.0f);   // log-ish: 632 Hz is the geometric middle

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { lowCut, 1 }, "Low Cut", hz, 20.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hzText)
            .withValueFromStringFunction (hzValue)));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { highCut, 1 }, "High Cut", hz, 20000.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (hzText)
            .withValueFromStringFunction (hzValue)));

    return layout;
}
} // namespace params
