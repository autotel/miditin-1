#pragma once
#include <PresetPanel.h>
#include "PluginProcessor.h"
#include "UI/FocusGuard.h"
#include "UI/Shell.h"

class MiditinEditor final : public juce::AudioProcessorEditor, private mdt::Host, private atl::PresetHost,
                            private juce::FocusChangeListener
{
public:
    explicit MiditinEditor (MiditinProcessor&);
    ~MiditinEditor() override;
    void resized() override;
    /** Keyboard-focus defence: records what a click aimed the keyboard at. See the constructor. */
    void mouseDown (const juce::MouseEvent&) override;

    static const atl::PresetFormat& presetFormat();

private:
    MiditinProcessor& proc;
    juce::PropertiesFile settings;
    std::vector<juce::MemoryBlock> builtInData;   // what the built-in entries point into
    mdt::Shell shell;
    atl::PresetPanel presetPanel;

    /** Puts the keyboard back on what the click aimed it at, if something took it away inside this
        editor shortly afterwards. */
    void globalFocusChanged (juce::Component* focused) override;
    /** `allowUnfocused`: the deferred post-click call takes focus even when nothing holds it (the
        click's own grab can be lost before the message queue drains); the focus-change call does
        not, because "nothing is focused" is also what a genuine window deactivation looks like. */
    void reassertClickFocus (bool allowUnfocused);

    juce::Component::SafePointer<juce::Component> clickFocusTarget;
    double clickFocusTimeMs = 0.0;

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
