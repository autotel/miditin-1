#include "PluginProcessor.h"
#include "PluginEditor.h"

MiditinProcessor::MiditinProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MiditinProcessor::prepareToPlay (double, int) {}
void MiditinProcessor::releaseResources() {}

bool MiditinProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MiditinProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
}

juce::AudioProcessorEditor* MiditinProcessor::createEditor() { return new MiditinEditor (*this); }

void MiditinProcessor::getStateInformation (juce::MemoryBlock&) {}
void MiditinProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MiditinProcessor(); }
