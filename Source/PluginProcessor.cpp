#include "PluginProcessor.h"
#include "PluginEditor.h"

using outerspace::link::Registry;

OuterSpaceProcessor::OuterSpaceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OuterSpace", params::createLayout())
{
    pRole   = apvts.getRawParameterValue (params::role);
    pAmount = apvts.getRawParameterValue (params::amount);
    pLow    = apvts.getRawParameterValue (params::lowCut);
    pHigh   = apvts.getRawParameterValue (params::highCut);

    sendId = juce::Uuid().toString();
    sendName = defaultSendName();
    sendSlot = Registry::get().acquire (sendId, sendName);
    sendSlotAudio = sendSlot;

    startTimer (250);
}

OuterSpaceProcessor::~OuterSpaceProcessor()
{
    stopTimer();
    sendSlotAudio = -1;
    Registry::get().release (sendSlot);
}

juce::String OuterSpaceProcessor::defaultSendName()
{
    return "Source " + juce::String (Registry::get().countAdvertised() + 1);
}

//==============================================================================
bool OuterSpaceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out || (in != mono && in != stereo)) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! (sc.isDisabled() || sc == mono || sc == stereo)) return false;
    }
    return true;
}

void OuterSpaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    spacer.prepare (sampleRate, juce::jmax (1, getMainBusNumOutputChannels()));
    sideScratch.setSize (1, juce::jmax (samplesPerBlock, 4096), false, true, true);
    setLatencySamples (outerspace::Spacer::latencySamples);   // 0
}

void OuterSpaceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    if (sideScratch.getNumSamples() < n) sideScratch.setSize (1, n, false, true, true);   // shouldn't happen

    auto main = getBusBuffer (buffer, true, 0);
    const int numMain = main.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    // Song position (for lining up with a sender on another track)
    bool playing = false;
    std::int64_t songPos = ownPos;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getIsPlaying())
                if (auto t = pos->getTimeInSamples()) { playing = true; songPos = *t; }
    ownPos += n;

    float* side = sideScratch.getWritePointer (0);

    auto mixToMono = [n, side] (const juce::AudioBuffer<float>& b) -> float
    {
        const int ch = b.getNumChannels();
        juce::FloatVectorOperations::clear (side, n);
        for (int c = 0; c < ch; ++c)
            juce::FloatVectorOperations::addWithMultiply (side, b.getReadPointer (c), 1.0f / (float) ch, n);
        auto r = juce::FloatVectorOperations::findMinAndMax (side, n);
        return juce::jmax (std::abs (r.getStart()), std::abs (r.getEnd()));
    };

    //--- SEND: publish this track, pass audio through untouched -----------------
    if (isSendRole())
    {
        float peak = 0.0f;
        if (numMain > 0) peak = mixToMono (main);
        Registry::get().write (sendSlotAudio.load(), side, n, songPos, playing);
        const float db = juce::Decibels::gainToDecibels (peak, -100.0f);
        const float prev = sendLevelDb.load();
        const float fall = 24.0f * (float) n / (float) juce::jmax (1.0, getSampleRate());   // 24 dB/s meter fall
        sendLevelDb = db > prev ? db : juce::jmax (db, prev - fall);
        return;
    }

    //--- CARVE --------------------------------------------------------------
    float sidePeak = 0.0f;
    bool haveSide = false;
    if (useHostSideChain.load())
    {
        if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0) { sidePeak = mixToMono (sc); haveSide = true; }
        }
    }
    else
    {
        const bool ok = Registry::get().read (listenSlot.load(), listenGen.load(), side, n, songPos, playing);
        sourceLive = ok;
        if (ok)
        {
            auto r = juce::FloatVectorOperations::findMinAndMax (side, n);
            sidePeak = juce::jmax (std::abs (r.getStart()), std::abs (r.getEnd()));
        }
        haveSide = true;   // silence when missing, so a cut releases smoothly
    }
    if (! haveSide) juce::FloatVectorOperations::clear (side, n);
    if (sidePeak > 1.0e-5f) lastSideSignalMs = juce::Time::getMillisecondCounter();

    outerspace::Settings s;
    s.amount = pAmount->load() / 100.0f;
    s.lowCutHz = pLow->load();
    s.highCutHz = pHigh->load();
    spacer.setSettings (s);
    spacer.process (main.getArrayOfWritePointers(), numMain, side, n);
}

//==============================================================================
juce::String OuterSpaceProcessor::getListenId() const { return listenId; }
juce::String OuterSpaceProcessor::getListenName() const { return listenName; }

void OuterSpaceProcessor::setListenTo (const juce::String& id, const juce::String& name)
{
    listenId = id.isEmpty() ? juce::String (hostSideChainId) : id;
    listenName = listenId == hostSideChainId ? juce::String() : name;
    resolveListenSource();
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

void OuterSpaceProcessor::resolveListenSource()
{
    if (listenId == hostSideChainId)
    {
        useHostSideChain = true;
        listenSlot = -1;
        return;
    }
    auto& reg = Registry::get();
    const int slot = reg.find (listenId);
    if (slot >= 0)
    {
        listenGen = reg.generationOf (slot);
        listenSlot = slot;
        const auto nm = reg.nameOf (slot);
        if (nm.isNotEmpty()) listenName = nm;   // follow renames
    }
    else
        listenSlot = -1;
    useHostSideChain = false;
}

OuterSpaceProcessor::ListenStatus OuterSpaceProcessor::getListenStatus() const
{
    const bool recentSignal = juce::Time::getMillisecondCounter() - lastSideSignalMs.load() < 1500;
    if (useHostSideChain.load())
        return recentSignal ? ListenStatus::listening : ListenStatus::hostNotConnected;
    if (listenSlot.load() < 0) return ListenStatus::sourceMissing;
    return (sourceLive.load() && recentSignal) ? ListenStatus::listening : ListenStatus::sourceSilent;
}

juce::String OuterSpaceProcessor::getSendName() const { return sendName; }

void OuterSpaceProcessor::setSendName (const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || trimmed == sendName) return;
    sendName = trimmed;
    sendNameCustom = true;
    Registry::get().rename (sendSlot, sendName);
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

void OuterSpaceProcessor::updateTrackProperties (const TrackProperties& properties)
{
    // Logic and other hosts may tell us the track's name: use it as the send's
    // name until the user types their own.
    juce::String name;
   #if JUCE_MAJOR_VERSION >= 8
    if (properties.name.has_value()) name = *properties.name;
   #else
    name = properties.name;
   #endif
    name = name.trim();
    if (name.isEmpty()) return;

    juce::MessageManager::callAsync ([safe = juce::WeakReference<OuterSpaceProcessor> (this), name]
    {
        auto* self = safe.get();
        if (self == nullptr || self->sendNameCustom || name == self->sendName) return;
        self->sendName = name;
        Registry::get().rename (self->sendSlot, self->sendName);
    });
}

void OuterSpaceProcessor::syncSendSlot()
{
    Registry::get().setAdvertised (sendSlot, isSendRole());
}

void OuterSpaceProcessor::timerCallback()
{
    syncSendSlot();
    resolveListenSource();
}

//==============================================================================
void OuterSpaceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree link ("Link");
    link.setProperty ("sendId", sendId, nullptr);
    link.setProperty ("sendName", sendName, nullptr);
    link.setProperty ("sendNameCustom", sendNameCustom, nullptr);
    link.setProperty ("listenId", listenId, nullptr);
    link.setProperty ("listenName", listenName, nullptr);
    state.removeChild (state.getChildWithName ("Link"), nullptr);
    state.appendChild (link, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OuterSpaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto link = state.getChildWithName ("Link");
    state.removeChild (link, nullptr);
    apvts.replaceState (state);

    if (link.isValid())
    {
        auto& reg = Registry::get();
        juce::String id = link.getProperty ("sendId", sendId).toString();
        sendName = link.getProperty ("sendName", sendName).toString();
        sendNameCustom = (bool) link.getProperty ("sendNameCustom", false);
        if (id != sendId)
        {
            // Take back this send's saved id, so carve instances find it again.
            sendSlotAudio = -1;
            reg.release (sendSlot);
            sendSlot = reg.acquire (id, sendName);
            sendId = id;
            sendSlotAudio = sendSlot;
        }
        else
            reg.rename (sendSlot, sendName);

        listenId = link.getProperty ("listenId", hostSideChainId).toString();
        listenName = link.getProperty ("listenName", "").toString();
    }
    syncSendSlot();
    resolveListenSource();
}

//==============================================================================
juce::AudioProcessorEditor* OuterSpaceProcessor::createEditor() { return new OuterSpaceEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OuterSpaceProcessor(); }
