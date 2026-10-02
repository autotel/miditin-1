#pragma once
#include <PresetPanel.h>
#include "PluginProcessor.h"
#include "UI/Shell.h"

class MiditinEditor final : public juce::AudioProcessorEditor, private mdt::Host, private atl::PresetHost
{
public:
    explicit MiditinEditor (MiditinProcessor&);
    void resized() override;

    static const atl::PresetFormat& presetFormat();

private:
    MiditinProcessor& proc;
    juce::PropertiesFile settings;
    std::vector<juce::MemoryBlock> builtInData;   // what the built-in entries point into
    mdt::Shell shell;
    atl::PresetPanel presetPanel;

    // -- mdt::Host --------------------------------------------------------------------------------
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

    // -- atl::PresetHost --------------------------------------------------------------------------
    juce::ValueTree captureDocument() override { return proc.captureDocument(); }
    void applyDocument (const juce::ValueTree&) override;
    juce::File presetFolder() const override;
    void setPresetFolder (const juce::File&) override;
    void documentSaved (const juce::File&, const atl::PresetMetadata&) override;
    void documentLoaded (const juce::File&, const atl::PresetMetadata& m) override { adopt (m.name); }
    bool documentHasUnsavedChanges() const override { return proc.hasUnsavedChanges(); }
    juce::String openDocumentKey() const override { return proc.openPresetKey; }
    void openDocumentKeyChanged (const juce::String& key) override { proc.openPresetKey = key; }

    void adopt (const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiditinEditor)
};
