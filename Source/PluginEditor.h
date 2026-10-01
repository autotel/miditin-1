#pragma once
#include "PluginProcessor.h"

class MiditinEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MiditinEditor (MiditinProcessor&);
    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiditinEditor)
};
