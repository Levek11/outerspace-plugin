#pragma once
// OuterSpace carving engine.
//
// The side chain (the vocal) and this track (the piano, pad, guitar...) are
// each measured in 32 bands, a third of an octave apart from 20 Hz to 20 kHz.
// A band is cut on this track only where the two actually overlap: the vocal
// has to be present in that band, and this track has to be loud enough there
// to cover it. The cut is applied with 32 zero-latency bell filters whose
// interaction is compensated, so the curve you see is the curve you get.

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace outerspace
{
constexpr int kNumBands = 32;
constexpr float kMaxDepthDb = 24.0f;   // cut at Amount 100%, where the overlap is total

struct Settings
{
    float amount = 0.3f;        // 0..1
    float lowCutHz = 20.0f;     // carve only above this
    float highCutHz = 20000.0f; // ... and below this
};

/** Per-band display data, written by the audio thread, read by the UI. */
struct DisplayData
{
    std::array<std::atomic<float>, kNumBands> sideDb {};   // side chain level per band (dBFS)
    std::array<std::atomic<float>, kNumBands> mainDb {};   // this track's level per band (dBFS)
    std::array<std::atomic<float>, kNumBands> cutDb {};    // applied cut (<= 0 dB)
    DisplayData() { for (int i = 0; i < kNumBands; ++i) { sideDb[i] = -120.0f; mainDb[i] = -120.0f; cutDb[i] = 0.0f; } }
};

class Spacer
{
public:
    static constexpr int latencySamples = 0;
    static constexpr int updateInterval = 32;   // samples between curve updates

    static float bandCentre (int band);         // Hz

    void prepare (double sampleRate, int maxChannels);
    void reset();
    void setSettings (const Settings& s) noexcept { settings = s; }

    /** Processes `main` in place. `side` is a mono side-chain signal, or nullptr
        for none (the track then passes through untouched). */
    void process (float* const* main, int numChannels, const float* side, int numSamples) noexcept;

    DisplayData& getDisplay() noexcept { return display; }

    /** Target curve -> band gains (exposed for tests). */
    void solveGains (const float* targetDb, float* gainDb) const noexcept;
    /** Response in dB at `hz` for the given band gains (exposed for tests). */
    double responseDb (const float* gainDb, double hz) const noexcept;

private:
    struct Analyser   // TPT state-variable band-pass
    {
        float ic1 = 0, ic2 = 0;
        float process (float x, float g, float k, float a1, float a2) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + g * v1;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return k * v1;   // unity gain at the centre
        }
    };

    struct BellState { float ic1 = 0, ic2 = 0; };

    void updateCurve() noexcept;
    void computeTargets (float* targetDb) noexcept;

    double fs = 48000.0;
    Settings settings;
    DisplayData display;

    // Band layout
    std::array<float, kNumBands> centre {}, g {}, attackCoef {}, releaseCoef {};
    float anaK = 0, bellQ = 0;
    std::array<float, kNumBands> anaA1 {}, anaA2 {};

    // Analysis
    std::array<Analyser, kNumBands> anaSide {}, anaMain {}, anaSide2 {}, anaMain2 {};   // two stages = 4th order
    std::array<float, kNumBands> accSide {}, accMain {};   // power summed over the update interval
    std::array<float, kNumBands> envSide {}, envMain {};   // smoothed power
    int accCount = 0;

    // Curve
    std::array<float, kNumBands> target {}, gainDb {}, smoothGain {};
    std::array<float, kNumBands * kNumBands> inverse {};   // interaction compensation

    // Bells: shared coefficients, state per channel
    std::array<float, kNumBands> bK {}, bA1 {}, bA2 {}, bA3 {}, bM1 {};
    std::vector<std::array<BellState, kNumBands>> bells;
    bool bellsActive = false;
};
} // namespace outerspace
