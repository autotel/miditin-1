#pragma once
#include "PluginProcessor.h"
#include "UI/Shell.h"

class MiditinEditor final : public juce::AudioProcessorEditor, private mdt::Host
{
public:
    explicit MiditinEditor (MiditinProcessor&);
    void resized() override { shell.setBounds (getLocalBounds()); }

private:
    MiditinProcessor& proc;
    mdt::Shell shell;

    const mdt::Patch& patch() const override { return proc.getPatch(); }
    void setPatch (const mdt::Patch& p) override { proc.setPatch (p); }
    uint32_t patchVersion() const override { return proc.patchVersion.load(); }

    juce::RangedAudioParameter* parameter (mdt::P p) const { return proc.apvts.getParameter (mdt::spec (p).id); }
    float param (mdt::P p) const override { return proc.apvts.getRawParameterValue (mdt::spec (p).id)->load(); }
    void setParam (mdt::P p, float v) override { auto* r = parameter (p); r->setValueNotifyingHost (r->convertTo0to1 (v)); }
    void beginGesture (mdt::P p) override { parameter (p)->beginChangeGesture(); }
    void endGesture (mdt::P p) override { parameter (p)->endChangeGesture(); }

    const mdt::UiSnapshot& snapshot() override { return proc.snapshot(); }
    void noteOn (int n, float v) override { proc.post ({ MiditinProcessor::UiEvent::noteOn, n, v }); }
    void noteOff (int n) override { proc.post ({ MiditinProcessor::UiEvent::noteOff, n, 0 }); }
    void releaseHeld() override { proc.post ({ MiditinProcessor::UiEvent::releaseHeld, 0, 0 }); }
    void kick() override { proc.post ({ MiditinProcessor::UiEvent::kick, 0, 0 }); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiditinEditor)
};
