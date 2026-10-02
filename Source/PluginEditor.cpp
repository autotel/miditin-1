#include "PluginEditor.h"

MiditinEditor::MiditinEditor (MiditinProcessor& p) : juce::AudioProcessorEditor (p), proc (p), shell (*this)
{
    addAndMakeVisible (shell);
    setSize (mdt::Shell::width, mdt::Shell::height);
}
