// Headless checks for OuterSpace. Runs in CI after every build.
//
//   OuterSpaceDspTest            run all checks, exit code 0 = pass
//   OuterSpaceDspTest <dir>      also render before/after demo WAVs into <dir>

#include <juce_events/juce_events.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/Spacer.h"
#include "link/SourceRegistry.h"
#include <chrono>
#include <iostream>

using namespace outerspace;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  [pass] " : "  [FAIL] ") << what << "\n";
    if (! ok) ++failures;
}

constexpr double pi = 3.14159265358979323846;

// --- Test signals ---------------------------------------------------------
/** A sung "ah": 220 Hz with vibrato, harmonics shaped by a formant around 700 Hz. */
static std::vector<float> makeVocal (double fs, int n, float level = 0.15f)
{
    std::vector<float> v ((size_t) n);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs, f0 = 220.0 * (1.0 + 0.03 * std::sin (2 * pi * 5 * t));
        ph += 2 * pi * f0 / fs;
        double s = 0;
        for (int h = 1; h <= 20; ++h)
        {
            const double o = std::log2 (220.0 * h / 700.0);
            s += (std::exp (-o * o / 0.5) + 0.05) * std::sin (ph * h) * 2.0 / h;
        }
        v[(size_t) i] = (float) (level * s);
    }
    return v;
}

static std::vector<float> makePink (int n, float amp, unsigned seed = 1)
{
    juce::Random r ((juce::int64) seed);
    std::vector<float> v ((size_t) n);
    double b0 = 0, b1 = 0, b2 = 0;
    for (int i = 0; i < n; ++i)
    {
        const float w = r.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
        v[(size_t) i] = (float) (amp * (b0 + b1 + b2 + w * 0.1848) * 0.35);
    }
    return v;
}

static std::vector<float> makeTones (double fs, int n, std::initializer_list<double> freqs, float amp)
{
    std::vector<float> v ((size_t) n, 0.0f);
    for (double f : freqs)
        for (int i = 0; i < n; ++i) v[(size_t) i] += amp * (float) std::sin (2 * pi * f * i / fs);
    return v;
}

static double goertzelDb (const float* x, int n, double f, double fs)
{
    const double w = 2.0 * pi * f / fs, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return 20.0 * std::log10 (std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / n + 1e-20);
}


/** Runs `main` (as stereo, identical channels) against `side` through a Spacer. */
static std::vector<float> run (Spacer& s, double fs, const std::vector<float>& main, const std::vector<float>* side,
                               const Settings& st, int block = 512, int channels = 2)
{
    s.prepare (fs, channels);
    s.setSettings (st);
    const int n = (int) main.size();
    std::vector<std::vector<float>> ch ((size_t) channels, main);
    for (int p = 0; p < n; p += block)
    {
        const int m = std::min (block, n - p);
        std::vector<float*> ptrs;
        for (auto& c : ch) ptrs.push_back (c.data() + p);
        s.process (ptrs.data(), channels, side != nullptr ? side->data() + p : nullptr, m);
    }
    return ch[0];
}

static void writeWav (const juce::File& f, const std::vector<float>& l, double fs)
{
    juce::AudioBuffer<float> b (1, (int) l.size());
    b.copyFrom (0, 0, l.data(), (int) l.size());
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (auto* w = wav.createWriterFor (os.get(), fs, 1, 24, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

//==============================================================================
static void testCurveAccuracy()
{
    std::cout << "Curve accuracy\n";
    Spacer s; s.prepare (48000.0, 2);
    float t[kNumBands], g[kNumBands];
    for (float depth : { -3.0f, -12.0f, -24.0f })
    {
        for (int b = 0; b < kNumBands; ++b)
        {
            const double o = std::log2 (Spacer::bandCentre (b) / 650.0);
            t[b] = depth * (float) std::exp (-o * o / (2 * 0.8 * 0.8));
        }
        s.solveGains (t, g);
        double err = 0;
        for (int b = 0; b < kNumBands; ++b) err = std::max (err, std::abs (s.responseDb (g, Spacer::bandCentre (b)) - t[b]));
        check (err < 1.0, "a " + juce::String (depth) + " dB carve lands within 1 dB of the drawn curve (" + juce::String (err, 2) + ")");
    }
}

static void testPassThrough()
{
    std::cout << "Pass-through\n";
    const double fs = 48000.0;
    const int n = (int) fs * 2;
    auto main = makePink (n, 0.5f);
    Settings st; st.amount = 1.0f;

    Spacer s;
    auto outNone = run (s, fs, main, nullptr, st);
    double maxDiff = 0;
    for (int i = 0; i < n; ++i) maxDiff = std::max (maxDiff, (double) std::abs (outNone[(size_t) i] - main[(size_t) i]));
    check (maxDiff == 0.0, "no side chain: output is bit-identical to input");

    std::vector<float> silent ((size_t) n, 0.0f);
    auto outSilent = run (s, fs, main, &silent, st);
    maxDiff = 0;
    for (int i = 0; i < n; ++i) maxDiff = std::max (maxDiff, (double) std::abs (outSilent[(size_t) i] - main[(size_t) i]));
    check (maxDiff == 0.0, "silent side chain: output is bit-identical to input");

    auto vocal = makeVocal (fs, n);
    Settings zero; zero.amount = 0.0f;
    auto outZero = run (s, fs, main, &vocal, zero);
    maxDiff = 0;
    for (int i = 0; i < n; ++i) maxDiff = std::max (maxDiff, (double) std::abs (outZero[(size_t) i] - main[(size_t) i]));
    check (maxDiff == 0.0, "Amount 0: output is bit-identical to input");
    check (Spacer::latencySamples == 0, "zero latency");
}

static void testCarving()
{
    std::cout << "Carving\n";
    const double fs = 48000.0;
    const int n = (int) fs * 3, tail = (int) fs;   // measure the last second
    auto vocal = makeVocal (fs, n);
    // A "track" with energy low, in the vocal's range, and high
    auto main = makeTones (fs, n, { 60.0, 660.0, 8000.0 }, 0.2f);

    auto gainAt = [&] (const std::vector<float>& out, double f)
    {
        return goertzelDb (out.data() + n - tail, tail, f, fs) - goertzelDb (main.data() + n - tail, tail, f, fs);
    };

    Spacer s;
    Settings st; st.amount = 0.5f;
    auto out = run (s, fs, main, &vocal, st);
    const double g60 = gainAt (out, 60), g660 = gainAt (out, 660), g8k = gainAt (out, 8000);
    std::cout << "    at 50%: 60 Hz " << g60 << " dB, 660 Hz " << g660 << " dB, 8 kHz " << g8k << " dB\n";
    check (g660 < -6.0, "cuts where the vocal sits (660 Hz)");
    check (std::abs (g60) < 0.5, "leaves the lows alone (60 Hz, no vocal there)");
    check (std::abs (g8k) < 0.5, "leaves the highs alone (8 kHz, no vocal there)");

    Settings more = st; more.amount = 1.0f;
    auto out2 = run (s, fs, main, &vocal, more);
    check (gainAt (out2, 660) < g660 - 3.0, "more Amount, deeper cut");

    // Smart overlap: a track with nothing where the vocal is isn't touched
    auto bright = makeTones (fs, n, { 5000.0 }, 0.3f);
    auto out3 = run (s, fs, bright, &vocal, more);
    const double g5k = goertzelDb (out3.data() + n - tail, tail, 5000.0, fs) - goertzelDb (bright.data() + n - tail, tail, 5000.0, fs);
    check (std::abs (g5k) < 0.3, "smart overlap: a 5 kHz part under a 700 Hz vocal is untouched (" + juce::String (g5k, 2) + " dB)");

    // Smart overlap: a track already far below the vocal isn't touched
    auto quiet = makeTones (fs, n, { 660.0 }, 0.002f);
    auto out4 = run (s, fs, quiet, &vocal, st);
    const double gq = goertzelDb (out4.data() + n - tail, tail, 660.0, fs) - goertzelDb (quiet.data() + n - tail, tail, 660.0, fs);
    check (gq > -1.0, "smart overlap: a part 30+ dB under the vocal is left alone (" + juce::String (gq, 2) + " dB)");

    // Low/high cut range
    Settings range = st; range.lowCutHz = 2000.0f;
    auto out5 = run (s, fs, main, &vocal, range);
    check (std::abs (gainAt (out5, 660)) < 0.5, "Low Cut at 2 kHz: no carving at 660 Hz");
    range = st; range.highCutHz = 300.0f;
    auto out6 = run (s, fs, main, &vocal, range);
    check (std::abs (gainAt (out6, 660)) < 0.5, "High Cut at 300 Hz: no carving at 660 Hz");
}

static void testReleaseAndFreeze()
{
    std::cout << "Release and freeze\n";
    const double fs = 48000.0;
    const int n = (int) fs * 4, half = n / 2, win = (int) (fs * 0.5);
    auto main = makeTones (fs, n, { 660.0 }, 0.2f);
    auto vocal = makeVocal (fs, n);
    for (int i = half; i < n; ++i) vocal[(size_t) i] = 0.0f;   // the singer stops halfway

    auto lastGain = [&] (const std::vector<float>& out)
    { return goertzelDb (out.data() + n - win, win, 660.0, fs) - goertzelDb (main.data() + n - win, win, 660.0, fs); };

    Spacer s;
    Settings st; st.amount = 0.5f;
    auto out = run (s, fs, main, &vocal, st);
    check (std::abs (lastGain (out)) < 0.3, "cut releases after the vocal stops");

    // Freeze engaged while singing: render in two halves to flip the setting
    s.prepare (fs, 1);
    std::vector<float> buf = main;
    float* p = buf.data();
    s.setSettings (st);
    for (int i = 0; i < half; i += 512) { float* c[1] = { p + i }; s.process (c, 1, vocal.data() + i, std::min (512, half - i)); }
    Settings fr = st; fr.freeze = true; s.setSettings (fr);
    for (int i = half; i < n; i += 512) { float* c[1] = { p + i }; s.process (c, 1, vocal.data() + i, std::min (512, n - i)); }
    check (lastGain (buf) < -4.0, "Freeze holds the curve after the vocal stops (" + juce::String (lastGain (buf), 1) + " dB)");
}

static void testRobustness()
{
    std::cout << "Robustness\n";
    for (double fs : { 44100.0, 96000.0, 192000.0 })
    {
        const int n = (int) fs;
        auto main = makePink (n, 0.9f, 3);
        auto vocal = makeVocal (fs, n, 0.5f);
        Spacer s; Settings st; st.amount = 1.0f;
        auto out = run (s, fs, main, &vocal, st, 333, 1);
        bool finite = true; float peak = 0;
        for (float v : out) { finite &= std::isfinite (v); peak = std::max (peak, std::abs (v)); }
        float inPeak = 0; for (float v : main) inPeak = std::max (inPeak, std::abs (v));
        check (finite && peak <= inPeak * 1.5f, "stable at " + juce::String (fs / 1000.0, 1) + " kHz, odd block size, full Amount");
    }

    // Odd block sizes give the same result as one big block
    const double fs = 48000.0; const int n = 48000;
    auto main = makePink (n, 0.5f, 9); auto vocal = makeVocal (fs, n);
    Settings st; st.amount = 0.6f;
    Spacer a, b;
    auto o1 = run (a, fs, main, &vocal, st, 4096);
    auto o2 = run (b, fs, main, &vocal, st, 37);
    double d = 0; for (int i = 0; i < n; ++i) d = std::max (d, (double) std::abs (o1[(size_t) i] - o2[(size_t) i]));
    check (d < 1.0e-5, "result doesn't depend on Logic's buffer size");
}

static void testCpu()
{
    std::cout << "CPU\n";
    const double fs = 48000.0; const int n = (int) fs * 10;
    auto main = makePink (n, 0.5f); auto vocal = makeVocal (fs, n);
    Spacer s; Settings st; st.amount = 0.5f;
    const auto t0 = std::chrono::steady_clock::now();
    run (s, fs, main, &vocal, st, 256, 2);
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * sec / 10.0;
    std::cout << "    " << pct << "% of one core (stereo, 48 kHz)\n";
    check (pct < 12.0, "stereo instance well under 12% of a core");
}

static void testLink()
{
    std::cout << "Track linking\n";
    auto& reg = link::Registry::get();
    juce::String idA = "test-sender-A";
    const int slot = reg.acquire (idA, "Lead Vocal");
    check (slot >= 0, "a send gets a slot");
    reg.setAdvertised (slot, true);
    check (reg.find (idA) == slot && reg.list().size() == 1 && reg.list()[0].name == "Lead Vocal", "it shows up in the list by name");

    juce::String dup = idA;
    const int slot2 = reg.acquire (dup, "Lead Vocal copy");
    check (slot2 >= 0 && dup != idA, "a duplicated track's send gets its own id");
    reg.release (slot2);

    reg.setAdvertised (slot, false);
    check (reg.list().empty(), "set back to CARVE, it leaves the list");
    reg.setAdvertised (slot, true);

    // Song-position alignment: the sender writes blocks of 512 at positions
    // 10000.., the receiver reads with a different block size at the same positions.
    const auto gen = reg.generationOf (slot);
    std::vector<float> src (48000);
    for (int i = 0; i < 48000; ++i) src[(size_t) i] = (float) std::sin (i * 0.01) + (float) i * 1.0e-6f;
    for (int p = 0; p < 48000; p += 512)
        reg.write (slot, src.data() + p, std::min (512, 48000 - p), 10000 + p, true);

    std::vector<float> got (300);
    bool aligned = true;
    for (int p = 0; p + 300 <= 47000; p += 300)
    {
        reg.read (slot, gen, got.data(), 300, 10000 + p, true);
        for (int i = 0; i < 300; ++i) aligned &= got[(size_t) i] == src[(size_t) (p + i)];
    }
    check (aligned, "receiver reads the exact samples at its song position (different block sizes)");

    // Receiver ahead of sender: falls back to the latest audio, never garbage
    const bool ok = reg.read (slot, gen, got.data(), 300, 10000 + 60000, true);
    check (ok && got.back() == src.back(), "receiver ahead of the sender uses the latest audio");

    // Stale generation (sender deleted) reads silence
    reg.release (slot);
    const bool after = reg.read (slot, gen, got.data(), 300, 10000, true);
    bool silent = true; for (float v : got) silent &= v == 0.0f;
    check (! after && silent, "deleted send: receiver gets silence");
}

static void renderDemos (const juce::File& dir)
{
    std::cout << "Rendering demos to " << dir.getFullPathName() << "\n";
    dir.createDirectory();
    const double fs = 48000.0; const int n = (int) fs * 6;
    auto vocal = makeVocal (fs, n);
    for (int i = 0; i < n; ++i)   // sing for 1.5 s, rest 0.5 s
        if (std::fmod (i / fs, 2.0) > 1.5) vocal[(size_t) i] = 0.0f;
    auto pad = makePink (n, 0.4f, 5);
    Spacer s; Settings st; st.amount = 0.4f;
    auto carved = run (s, fs, pad, &vocal, st, 512, 1);

    std::vector<float> before (pad), after (carved);
    for (int i = 0; i < n; ++i) { before[(size_t) i] = 0.6f * (pad[(size_t) i] + vocal[(size_t) i]); after[(size_t) i] = 0.6f * (carved[(size_t) i] + vocal[(size_t) i]); }
    writeWav (dir.getChildFile ("1-mix-before.wav"), before, fs);
    writeWav (dir.getChildFile ("2-mix-after.wav"), after, fs);
    writeWav (dir.getChildFile ("3-pad-carved.wav"), carved, fs);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    testCurveAccuracy();
    testPassThrough();
    testCarving();
    testReleaseAndFreeze();
    testRobustness();
    testCpu();
    testLink();
    if (argc > 1)
        renderDemos (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));

    std::cout << (failures == 0 ? "\nAll checks passed\n" : "\n" + std::to_string (failures) + " check(s) FAILED\n");
    return failures == 0 ? 0 : 1;
}
