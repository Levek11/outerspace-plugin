#pragma once
// Track linking: OuterSpace instances set to SEND publish their track's audio
// here; instances set to CARVE pick one from a list and read it as their
// side chain. Everything lives in this plugin binary, so it works between
// instances in the same Logic project (Logic hosts Audio Units in-process).
//
// Alignment: while the song plays, a sender writes each block at its song
// position (in samples) and a receiver reads the block at its own song
// position, so the two line up even when Logic processes the tracks in a
// different order or with different buffer sizes. When stopped, the receiver
// reads the most recent audio.

#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <vector>

namespace outerspace::link
{
constexpr int kMaxSources = 128;
constexpr int kRingSize = 1 << 17;   // ~2.7 s at 48 kHz
constexpr int kRingMask = kRingSize - 1;

struct SourceInfo
{
    juce::String id, name;
    int slot = -1;
};

class Registry
{
public:
    static Registry& get();

    // --- Message thread --------------------------------------------------
    /** Claims a slot for a sender. If another live sender already uses `id`
        (a duplicated track), `id` is replaced with a fresh one. -1 if full. */
    int acquire (juce::String& id, const juce::String& name);
    /** Number of senders currently advertised (for default names). */
    int countAdvertised() const;
    void release (int slot);
    void rename (int slot, const juce::String& name);
    /** Shown in receivers' lists only while advertised (the instance is set to SEND). */
    void setAdvertised (int slot, bool advertised);
    std::vector<SourceInfo> list() const;
    /** Slot of an advertised sender with this id, or -1. */
    int find (const juce::String& id) const;
    juce::String nameOf (int slot) const;
    std::uint32_t generationOf (int slot) const;

    // --- Audio thread ----------------------------------------------------
    /** Sender: publish a mono block. */
    void write (int slot, const float* mono, int n, std::int64_t songPos, bool playing) noexcept;
    /** Receiver: fills `out` (n samples). Returns false (and silence) if the
        source isn't there or hasn't sent anything lately. */
    bool read (int slot, std::uint32_t generation, float* out, int n, std::int64_t songPos, bool playing) const noexcept;

private:
    Registry();

    struct Slot
    {
        std::atomic<bool> inUse { false }, advertised { false };
        std::atomic<std::uint32_t> generation { 0 };
        std::unique_ptr<float[]> ring;            // allocated once, never freed while the plugin is loaded
        std::atomic<float*> ringPtr { nullptr };
        std::atomic<std::int64_t> endPos { 0 };   // position just after the last written sample
        std::atomic<bool> timeline { false };     // last block was written at a song position
        std::atomic<std::uint32_t> lastWriteMs { 0 };
        juce::String id, name;                    // guarded by lock
    };

    mutable juce::CriticalSection lock;
    std::vector<std::unique_ptr<Slot>> slots;
};
} // namespace outerspace::link
