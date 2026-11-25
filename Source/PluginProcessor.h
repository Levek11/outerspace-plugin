#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Spacer.h"
#include "link/SourceRegistry.h"
#include "Parameters.h"

class OuterSpaceProcessor : public juce::AudioProcessor,
                            private juce::Timer
{
public:
    OuterSpaceProcessor();
    ~OuterSpaceProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& properties) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    bool isSendRole() const noexcept { return pRole->load() > 0.5f; }

    // --- Carve: what to listen to (message thread) ---
    static constexpr const char* hostSideChainId = "host";
    juce::String getListenId() const;               // hostSideChainId or a sender's id
    juce::String getListenName() const;             // remembered name of the chosen sender
    void setListenTo (const juce::String& id, const juce::String& name);

    enum class ListenStatus { listening, hostNotConnected, sourceMissing, sourceSilent };
    ListenStatus getListenStatus() const;

    // --- Send: this track's name in the lists (message thread) ---
    juce::String getSendName() const;
    void setSendName (const juce::String& name);    // user rename
    float getSendLevelDb() const noexcept { return sendLevelDb.load(); }

    outerspace::DisplayData& getDisplay() noexcept { return spacer.getDisplay(); }

private:
    void timerCallback() override;
    void resolveListenSource();
    void syncSendSlot();
    static juce::String defaultSendName();

    outerspace::Spacer spacer;
    std::atomic<float>* pRole = nullptr;
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pLow = nullptr;
    std::atomic<float>* pHigh = nullptr;

    juce::AudioBuffer<float> sideScratch;

    // Send
    juce::String sendId, sendName;
    bool sendNameCustom = false;
    int sendSlot = -1;
    std::atomic<int> sendSlotAudio { -1 };
    std::atomic<float> sendLevelDb { -100.0f };

    // Carve
    juce::String listenId { hostSideChainId }, listenName;
    std::atomic<int> listenSlot { -1 };
    std::atomic<std::uint32_t> listenGen { 0 };
    std::atomic<bool> useHostSideChain { true };
    std::atomic<bool> hostSideChainLive { false };
    std::atomic<bool> sourceLive { false };
    std::atomic<std::uint32_t> lastSideSignalMs { 0 };

    // Running position when the host has no song position
    std::int64_t ownPos = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (OuterSpaceProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OuterSpaceProcessor)
};
