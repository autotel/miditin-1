#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Engine.h"
#include "Engine/PreviewSynth.h"
#include "Model/TripleBuffer.h"

class MiditinProcessor final : public juce::AudioProcessor
{
public:
    MiditinProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // -- message thread ---------------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    const mdt::Patch& getPatch() const noexcept { return patch; }
    /** Publishes to the audio thread. `patchVersion` ticks so the editor can notice a state load. */
    void setPatch (const mdt::Patch&);
    std::atomic<uint32_t> patchVersion { 0 };

    struct UiEvent { enum Type { noteOn, noteOff, releaseHeld, kick } type; int note; float vel; };
    void post (UiEvent);

    /** The latest snapshot the audio thread published; stable until the next call. */
    const mdt::UiSnapshot& snapshot() noexcept { snapshots.fetch(); return snapshots.front(); }

private:
    mdt::Engine engine;
    mdt::PreviewSynth preview;
    mdt::Patch patch, audioPatch;
    mdt::TripleBuffer<mdt::Patch> patches;
    mdt::TripleBuffer<mdt::UiSnapshot> snapshots;
    std::array<std::atomic<float>*, mdt::numParams> raw {};

    juce::AbstractFifo fifo { 256 };
    std::array<UiEvent, 256> events {};
    juce::SpinLock postLock;   // several message-thread callers; the audio side never takes it

    juce::MidiBuffer input;
    std::array<bool, 128> pendingOffs {};
    bool bypassed = false;

    void publishSnapshot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiditinProcessor)
};
