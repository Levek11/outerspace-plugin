// Local tool: renders the editor to PNGs (Carve with live audio, and Send).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshots");
    out.createDirectory();

    OuterSpaceProcessor p;
    p.enableAllBuses();
    const double fs = 48000.0;
    p.setRateAndBufferSizeDetails (fs, 512);
    p.prepareToPlay (fs, 512);
    p.apvts.getParameter (params::amount)->setValueNotifyingHost (0.45f);
    auto* lo = p.apvts.getParameter (params::lowCut);  lo->setValueNotifyingHost (lo->convertTo0to1 (150.0f));
    auto* hi = p.apvts.getParameter (params::highCut); hi->setValueNotifyingHost (hi->convertTo0to1 (6000.0f));

    juce::AudioBuffer<float> buf (4, 512);
    juce::MidiBuffer midi;
    juce::Random r (3);
    double ph = 0, b0 = 0, b1 = 0, b2 = 0;
    for (int blk = 0; blk < 200; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float w = r.nextFloat() * 2 - 1;
            b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
            const float pink = (float) (b0 + b1 + b2 + w * 0.1848) * 0.15f;
            ph += 2 * 3.14159265 * 220.0 / fs;
            double v = 0;
            for (int h = 1; h <= 20; ++h) { const double o = std::log2 (220.0 * h / 700.0); v += (std::exp (-o * o / 0.5) + 0.05) * std::sin (ph * h) * 2.0 / h; }
            buf.setSample (0, i, pink); buf.setSample (1, i, pink);
            buf.setSample (2, i, (float) (0.15 * v)); buf.setSample (3, i, (float) (0.15 * v));
        }
        p.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    for (int i = 0; i < 40; ++i) dynamic_cast<juce::Timer*> (ed.get())->timerCallback();
    auto save = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::FileOutputStream os (out.getChildFile (name));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
    };
    save ("carve.png");

    p.apvts.getParameter (params::role)->setValueNotifyingHost (1.0f);
    {
        juce::AudioProcessor::TrackProperties tp;
        tp.name = juce::String ("Lead Vocal");
        p.updateTrackProperties (tp);
    }
    for (int blk = 0; blk < 20; ++blk) p.processBlock (buf, midi);
    for (int i = 0; i < 3; ++i) dynamic_cast<juce::Timer*> (ed.get())->timerCallback();
    save ("send.png");
    ed.reset();
    return 0;
}
