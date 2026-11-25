#include "SourceRegistry.h"
#include <algorithm>

namespace outerspace::link
{
Registry& Registry::get()
{
    static Registry instance;
    return instance;
}

Registry::Registry()
{
    for (int i = 0; i < kMaxSources; ++i)
        slots.push_back (std::make_unique<Slot>());
}

int Registry::acquire (juce::String& id, const juce::String& name)
{
    const juce::ScopedLock sl (lock);
    for (auto& s : slots)
        if (s->inUse.load() && s->id == id) { id = juce::Uuid().toString(); break; }

    // Reuse a slot that held this id before (e.g. the instance was re-created),
    // otherwise the first free one.
    int chosen = -1;
    for (int i = 0; i < kMaxSources && chosen < 0; ++i)
        if (! slots[(size_t) i]->inUse.load() && slots[(size_t) i]->id == id) chosen = i;
    for (int i = 0; i < kMaxSources && chosen < 0; ++i)
        if (! slots[(size_t) i]->inUse.load()) chosen = i;
    if (chosen < 0) return -1;

    auto& s = *slots[(size_t) chosen];
    if (s.ring == nullptr)
    {
        s.ring.reset (new float[kRingSize]);
        std::fill (s.ring.get(), s.ring.get() + kRingSize, 0.0f);
        s.ringPtr.store (s.ring.get());
    }
    s.id = id;
    s.name = name;
    s.advertised = false;
    s.lastWriteMs = 0;
    s.generation.fetch_add (1);
    s.inUse = true;
    return chosen;
}

int Registry::countAdvertised() const
{
    int n = 0;
    for (auto& s : slots) if (s->inUse.load() && s->advertised.load()) ++n;
    return n;
}

void Registry::release (int slot)
{
    if (slot < 0 || slot >= kMaxSources) return;
    const juce::ScopedLock sl (lock);
    auto& s = *slots[(size_t) slot];
    s.advertised = false;
    s.inUse = false;
    s.generation.fetch_add (1);   // receivers holding the old generation stop reading
}

void Registry::rename (int slot, const juce::String& name)
{
    if (slot < 0 || slot >= kMaxSources) return;
    const juce::ScopedLock sl (lock);
    slots[(size_t) slot]->name = name;
}

void Registry::setAdvertised (int slot, bool advertised)
{
    if (slot < 0 || slot >= kMaxSources) return;
    slots[(size_t) slot]->advertised = advertised;
}

std::vector<SourceInfo> Registry::list() const
{
    const juce::ScopedLock sl (lock);
    std::vector<SourceInfo> out;
    for (int i = 0; i < kMaxSources; ++i)
    {
        const auto& s = *slots[(size_t) i];
        if (s.inUse.load() && s.advertised.load())
            out.push_back ({ s.id, s.name, i });
    }
    std::sort (out.begin(), out.end(), [] (const SourceInfo& a, const SourceInfo& b)
               { return a.name.compareNatural (b.name) < 0; });
    return out;
}

int Registry::find (const juce::String& id) const
{
    if (id.isEmpty()) return -1;
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < kMaxSources; ++i)
    {
        const auto& s = *slots[(size_t) i];
        if (s.inUse.load() && s.advertised.load() && s.id == id) return i;
    }
    return -1;
}

juce::String Registry::nameOf (int slot) const
{
    if (slot < 0 || slot >= kMaxSources) return {};
    const juce::ScopedLock sl (lock);
    return slots[(size_t) slot]->name;
}

std::uint32_t Registry::generationOf (int slot) const
{
    if (slot < 0 || slot >= kMaxSources) return 0;
    return slots[(size_t) slot]->generation.load();
}

void Registry::write (int slot, const float* mono, int n, std::int64_t songPos, bool playing) noexcept
{
    if (slot < 0 || slot >= kMaxSources || n <= 0) return;
    auto& s = *slots[(size_t) slot];
    float* ring = s.ringPtr.load (std::memory_order_acquire);
    if (ring == nullptr) return;

    std::int64_t pos;
    if (playing)
        pos = songPos;
    else
    {
        // Stopped: keep writing (so the plugin reacts while you audition), on
        // our own counter just after the last block.
        pos = s.endPos.load (std::memory_order_relaxed);
    }

    for (int i = 0; i < n; ++i)
        ring[(size_t) ((pos + i) & kRingMask)] = mono[i];

    s.timeline.store (playing, std::memory_order_relaxed);
    s.endPos.store (pos + n, std::memory_order_release);
    s.lastWriteMs.store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
}

bool Registry::read (int slot, std::uint32_t generation, float* out, int n, std::int64_t songPos, bool playing) const noexcept
{
    auto silence = [&] { std::fill (out, out + n, 0.0f); return false; };
    if (slot < 0 || slot >= kMaxSources || n <= 0 || n > kRingSize / 4) return silence();
    const auto& s = *slots[(size_t) slot];
    if (! s.inUse.load (std::memory_order_relaxed) || s.generation.load (std::memory_order_relaxed) != generation)
        return silence();

    const float* ring = s.ringPtr.load (std::memory_order_acquire);
    const auto last = s.lastWriteMs.load (std::memory_order_relaxed);
    if (ring == nullptr || last == 0 || juce::Time::getMillisecondCounter() - last > 400)
        return silence();   // the sender's track isn't being processed (muted, deleted, ...)

    const std::int64_t end = s.endPos.load (std::memory_order_acquire);
    std::int64_t start = end - n;   // most recent audio

    if (playing && s.timeline.load (std::memory_order_relaxed))
    {
        const std::int64_t wantEnd = songPos + n;
        if (wantEnd <= end && end - songPos < kRingSize - 2 * n)
            start = songPos;   // the sender has already played this stretch: exact alignment
        // else the sender is behind us (or we jumped): use its latest audio
    }

    for (int i = 0; i < n; ++i)
        out[i] = ring[(size_t) ((start + i) & kRingMask)];
    return true;
}
} // namespace outerspace::link
