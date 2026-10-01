#include "PluginEditor.h"

MiditinEditor::MiditinEditor (MiditinProcessor& p) : juce::AudioProcessorEditor (p)
{
    setSize (900, 600);
}

void MiditinEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xfffbfaf6));
}
