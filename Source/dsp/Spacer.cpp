#include "Spacer.h"
#include <algorithm>
#include <complex>

namespace outerspace
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double kInteractionRefDb = -12.0;   // gain used to measure band interaction

inline float clamp01 (float x) noexcept { return std::min (1.0f, std::max (0.0f, x)); }
inline float powToDb (float p) noexcept { return 10.0f * std::log10 (p + 1.0e-12f); }

/** |H| in dB of one TPT bell (bilinear, so exact for the digital filter). */
double bellDb (double gTan, double q, double gainDb, double hz, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double W = std::tan (pi * std::min (hz, fs * 0.4999) / fs);
    const std::complex<double> s (0.0, W);
    const auto num = s * s + s * (A / q) * gTan + gTan * gTan;
    const auto den = s * s + s * (1.0 / (q * A)) * gTan + gTan * gTan;
    return 20.0 * std::log10 (std::abs (num / den));
}

/** In-place Gauss-Jordan inverse with partial pivoting. */
bool invert (std::vector<double>& m, int n)
{
    std::vector<double> inv (n * n, 0.0);
    for (int i = 0; i < n; ++i) inv[i * n + i] = 1.0;
    for (int c = 0; c < n; ++c)
    {
        int p = c;
        for (int r = c + 1; r < n; ++r)
            if (std::abs (m[r * n + c]) > std::abs (m[p * n + c])) p = r;
        if (std::abs (m[p * n + c]) < 1.0e-12) return false;
        if (p != c)
            for (int k = 0; k < n; ++k) { std::swap (m[c * n + k], m[p * n + k]); std::swap (inv[c * n + k], inv[p * n + k]); }
        const double d = m[c * n + c];
        for (int k = 0; k < n; ++k) { m[c * n + k] /= d; inv[c * n + k] /= d; }
        for (int r = 0; r < n; ++r)
        {
            if (r == c) continue;
            const double f = m[r * n + c];
            if (f == 0.0) continue;
            for (int k = 0; k < n; ++k) { m[r * n + k] -= f * m[c * n + k]; inv[r * n + k] -= f * inv[c * n + k]; }
        }
    }
    m = inv;
    return true;
}
} // namespace

float Spacer::bandCentre (int band)
{
    // 20 Hz .. 20 kHz, evenly spaced on a log scale (about a third of an octave apart)
    return (float) (20.0 * std::pow (1000.0, band / (double) (kNumBands - 1)));
}

void Spacer::prepare (double sampleRate, int maxChannels)
{
    fs = sampleRate;
    const double octavesPerBand = std::log2 (1000.0) / (kNumBands - 1);
    // Q for a bandwidth of one band spacing
    const double bw = std::pow (2.0, octavesPerBand);
    const double q = std::sqrt (bw) / (bw - 1.0);
    bellQ = (float) q;
    anaK = (float) (1.0 / q);

    const float rate = (float) (fs / updateInterval);
    for (int b = 0; b < kNumBands; ++b)
    {
        centre[b] = std::min (bandCentre (b), (float) (fs * 0.45));
        g[b] = (float) std::tan (pi * centre[b] / fs);
        anaA1[b] = 1.0f / (1.0f + g[b] * (g[b] + anaK));
        anaA2[b] = g[b] * anaA1[b];

        // Lows need longer windows to read steadily; highs can react fast.
        const float att = std::max (0.006f, 2.0f / centre[b]);
        const float rel = std::max (0.090f, 6.0f / centre[b]);
        attackCoef[b] = 1.0f - std::exp (-1.0f / (att * rate));
        releaseCoef[b] = 1.0f - std::exp (-1.0f / (rel * rate));
    }

    // Interaction matrix: how much a cut in band j shows up at band i's centre.
    std::vector<double> m (kNumBands * kNumBands);
    for (int i = 0; i < kNumBands; ++i)
        for (int j = 0; j < kNumBands; ++j)
            m[i * kNumBands + j] = bellDb (g[j], q, kInteractionRefDb, centre[i], fs) / kInteractionRefDb;
    if (invert (m, kNumBands))
        for (int i = 0; i < kNumBands * kNumBands; ++i) inverse[i] = (float) m[i];
    else
        for (int i = 0; i < kNumBands; ++i)
            for (int j = 0; j < kNumBands; ++j) inverse[i * kNumBands + j] = i == j ? 1.0f : 0.0f;

    bells.assign ((size_t) std::max (1, maxChannels), {});
    reset();
}

void Spacer::reset()
{
    for (auto& a : anaSide) a = {};
    for (auto& a : anaMain) a = {};
    for (auto& a : anaSide2) a = {};
    for (auto& a : anaMain2) a = {};
    for (auto& ch : bells) for (auto& s : ch) s = {};
    accSide.fill (0); accMain.fill (0);
    envSide.fill (0); envMain.fill (0);
    target.fill (0); gainDb.fill (0); smoothGain.fill (0); frozenTarget.fill (0);
    haveFrozen = false;
    accCount = 0;
    bellsActive = false;
    for (int b = 0; b < kNumBands; ++b)
    {
        bK[b] = 1.0f / bellQ; bM1[b] = 0.0f;
        bA1[b] = 1.0f / (1.0f + g[b] * (g[b] + bK[b]));
        bA2[b] = g[b] * bA1[b];
        bA3[b] = g[b] * bA2[b];
    }
}

void Spacer::solveGains (const float* targetDb, float* out) const noexcept
{
    for (int i = 0; i < kNumBands; ++i)
    {
        float s = 0.0f;
        const float* row = &inverse[(size_t) i * kNumBands];
        for (int j = 0; j < kNumBands; ++j) s += row[j] * targetDb[j];
        out[i] = std::min (0.0f, std::max (-40.0f, s));   // never boost
    }
}

double Spacer::responseDb (const float* gains, double hz) const noexcept
{
    double db = 0.0;
    for (int b = 0; b < kNumBands; ++b)
        if (gains[b] < -1.0e-4f)
            db += bellDb (g[b], bellQ, gains[b], hz, fs);
    return db;
}

void Spacer::computeTargets (float* t) noexcept
{
    std::array<float, kNumBands> sDb {}, mDb {};
    float peak = -200.0f;
    for (int b = 0; b < kNumBands; ++b)
    {
        sDb[b] = powToDb (envSide[b]);
        mDb[b] = powToDb (envMain[b]);
        peak = std::max (peak, sDb[b]);
    }

    const float depth = clamp01 (settings.amount) * kMaxDepthDb;
    const float lo = std::max (1.0f, settings.lowCutHz), hi = std::max (1.0f, settings.highCutHz);
    const float bandsPerOctave = (float) ((kNumBands - 1) / std::log2 (1000.0));

    for (int b = 0; b < kNumBands; ++b)
    {
        // How strongly is the vocal there? Follows its spectrum: full at its loudest
        // band, fading to nothing 24 dB below it, and nothing near the noise floor.
        const float vocal = clamp01 ((sDb[b] - (peak - 24.0f)) / 24.0f) * clamp01 ((sDb[b] + 70.0f) / 12.0f);
        // Does this track cover it? Present, and not already well below the vocal.
        const float track = clamp01 ((mDb[b] + 70.0f) / 12.0f) * clamp01 ((mDb[b] - sDb[b] + 18.0f) / 18.0f);
        // Inside the low/high cut range (soft edges, one band wide).
        const float wl = clamp01 (std::log2 (centre[b] / lo) * bandsPerOctave + 0.5f);
        const float wh = clamp01 (std::log2 (hi / centre[b]) * bandsPerOctave + 0.5f);

        t[b] = -depth * vocal * track * wl * wh;
    }
}

void Spacer::updateCurve() noexcept
{
    const float inv = 1.0f / (float) std::max (1, accCount);
    for (int b = 0; b < kNumBands; ++b)
    {
        const float ps = accSide[b] * inv, pm = accMain[b] * inv;
        envSide[b] += (ps > envSide[b] ? attackCoef[b] : releaseCoef[b]) * (ps - envSide[b]);
        envMain[b] += (pm > envMain[b] ? attackCoef[b] : releaseCoef[b]) * (pm - envMain[b]);
        accSide[b] = accMain[b] = 0.0f;
    }
    accCount = 0;

    if (settings.freeze)
    {
        if (! haveFrozen) { frozenTarget = target; haveFrozen = true; }
        target = frozenTarget;
    }
    else
    {
        haveFrozen = false;
        computeTargets (target.data());
    }

    solveGains (target.data(), gainDb.data());

    const float smooth = 1.0f - std::exp (-1.0f / (0.004f * (float) (fs / updateInterval)));
    bool active = false;
    for (int b = 0; b < kNumBands; ++b)
    {
        smoothGain[b] += smooth * (gainDb[b] - smoothGain[b]);
        if (smoothGain[b] < -0.01f) active = true;

        const float A = std::pow (10.0f, smoothGain[b] / 40.0f);
        bK[b] = 1.0f / (bellQ * A);
        bA1[b] = 1.0f / (1.0f + g[b] * (g[b] + bK[b]));
        bA2[b] = g[b] * bA1[b];
        bA3[b] = g[b] * bA2[b];
        bM1[b] = bK[b] * (A * A - 1.0f);

        display.sideDb[b].store (powToDb (envSide[b]), std::memory_order_relaxed);
        display.mainDb[b].store (powToDb (envMain[b]), std::memory_order_relaxed);
        display.cutDb[b].store (target[b], std::memory_order_relaxed);
    }

    if (active && ! bellsActive)
        for (auto& ch : bells) for (auto& s : ch) s = {};
    bellsActive = active;
}

void Spacer::process (float* const* main, int numChannels, const float* side, int numSamples) noexcept
{
    numChannels = std::min (numChannels, (int) bells.size());
    if (numChannels <= 0) return;
    const float chScale = 1.0f / (float) numChannels;

    int pos = 0;
    while (pos < numSamples)
    {
        const int n = std::min (numSamples - pos, updateInterval - accCount);

        // 1. Measure (this track before carving, and the side chain)
        for (int i = pos; i < pos + n; ++i)
        {
            float m = 0.0f;
            for (int c = 0; c < numChannels; ++c) m += main[c][i];
            m *= chScale;
            const float s = side != nullptr ? side[i] : 0.0f;
            for (int b = 0; b < kNumBands; ++b)
            {
                const float ys = anaSide2[b].process (anaSide[b].process (s, g[b], anaK, anaA1[b], anaA2[b]), g[b], anaK, anaA1[b], anaA2[b]);
                const float ym = anaMain2[b].process (anaMain[b].process (m, g[b], anaK, anaA1[b], anaA2[b]), g[b], anaK, anaA1[b], anaA2[b]);
                accSide[b] += ys * ys;
                accMain[b] += ym * ym;
            }
        }

        // 2. Carve with the current curve
        if (bellsActive)
        {
            for (int c = 0; c < numChannels; ++c)
            {
                float* d = main[c] + pos;
                auto& st = bells[(size_t) c];
                for (int b = 0; b < kNumBands; ++b)
                {
                    const float a1 = bA1[b], a2 = bA2[b], a3 = bA3[b], m1 = bM1[b];
                    float ic1 = st[b].ic1, ic2 = st[b].ic2;
                    for (int i = 0; i < n; ++i)
                    {
                        const float v0 = d[i];
                        const float v3 = v0 - ic2;
                        const float v1 = a1 * ic1 + a2 * v3;
                        const float v2 = ic2 + a2 * ic1 + a3 * v3;
                        ic1 = 2.0f * v1 - ic1;
                        ic2 = 2.0f * v2 - ic2;
                        d[i] = v0 + m1 * v1;
                    }
                    st[b].ic1 = ic1; st[b].ic2 = ic2;
                }
            }
        }

        accCount += n;
        pos += n;
        if (accCount >= updateInterval) updateCurve();
    }

    // Flush denormals in the analysers (long silences)
    auto flush = [] (Analyser& a) { if (std::abs (a.ic1) < 1.0e-20f) a.ic1 = 0.0f; if (std::abs (a.ic2) < 1.0e-20f) a.ic2 = 0.0f; };
    for (int b = 0; b < kNumBands; ++b)
    {
        flush (anaSide[b]); flush (anaMain[b]); flush (anaSide2[b]); flush (anaMain2[b]);
    }
}
} // namespace outerspace
