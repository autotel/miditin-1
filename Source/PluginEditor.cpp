#include "PluginEditor.h"
#include "UI/Theme.h"

namespace
{
juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "settings";
    // Linux puts this straight under "~", so ".config" is part of the folder name.
    o.folderName = ".config/Autotel/MIDITIN_1";
    o.filenameSuffix = "xml";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    o.millisecondsBeforeSaving = 0;
    return o;
}

atl::PresetPanelColours panelColours()
{
    using namespace mdt;
    atl::PresetPanelColours c;
    c.background = theme::paper;
    c.panel      = theme::shade;
    c.text       = theme::ink;
    c.textDim    = theme::pillText;
    c.accent     = theme::accent;
    c.outline    = theme::line2;
    c.warning    = juce::Colour (0xffa86b1d);
    c.danger     = juce::Colour (0xffb23a2a);
    c.favourite  = juce::Colour (0xffc29a2a);
    return c;
}
} // namespace

const atl::PresetFormat& MiditinEditor::presetFormat()
{
    static const atl::PresetFormat f { ".miditin", mdt::documentType };
    return f;
}

MiditinEditor::MiditinEditor (MiditinProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p), settings (settingsOptions()), shell (*this),
      presetPanel (*this, presetFormat())
{
    // The factory sounds, serialised into the bytes a .miditin file holds.
    std::vector<atl::BuiltInPreset> builtIns;
    builtInData.reserve (mdt::factoryPresets().size());   // load-bearing: entries point into it
    for (const auto& f : mdt::factoryPresets())
    {
        auto doc = mdt::makeDocument (f.patch, f.pv);
        doc.setProperty (presetFormat().nameProperty, f.name, nullptr);
        juce::MemoryOutputStream out;
        doc.writeToStream (out);
        builtInData.push_back (out.getMemoryBlock());
        builtIns.push_back ({ f.name, f.category, builtInData.back().getData(), (int) builtInData.back().getSize() });
    }
    presetPanel.setColours (panelColours());
    presetPanel.setBuiltIns (builtIns);

    if (proc.openPresetName.isEmpty())
        proc.openPresetName = mdt::factoryPresets().front().name;
    shell.setPresetPanel (&presetPanel);
    shell.setPresetName (proc.openPresetName);

    // A text field in the librarian must keep the keyboard: without this the peer hands focus to
    // the first child that wants it, and a button eats the keystrokes.
    setWantsKeyboardFocus (true);
    addAndMakeVisible (shell);
    setSize (mdt::Shell::width, mdt::Shell::height);
}

void MiditinEditor::applyDocument (const juce::ValueTree& doc)
{
    proc.applyDocument (doc);
    proc.markClean();
    shell.patchReloaded();
    if (! doc.isValid())
        adopt ("Untitled");   // New: forget the open file, or Save would overwrite it
}

juce::File MiditinEditor::presetFolder() const
{
    const auto fallback = atl::PresetFile::defaultPresetFolder (settings.getFile().getParentDirectory());
    return juce::File (settings.getValue ("folder.presets", fallback.getFullPathName()));
}

void MiditinEditor::setPresetFolder (const juce::File& folder)
{
    settings.setValue ("folder.presets", folder.getFullPathName());
}

void MiditinEditor::documentSaved (const juce::File&, const atl::PresetMetadata& meta)
{
    proc.markClean();
    adopt (meta.name);
}

void MiditinEditor::adopt (const juce::String& name)
{
    proc.openPresetName = name;
    shell.setPresetName (name);
}
