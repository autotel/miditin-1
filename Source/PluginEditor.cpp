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
        builtInData.push_back (mdt::factoryPresetData (f, presetFormat()));
        builtIns.push_back ({ f.name, f.category, builtInData.back().getData(), (int) builtInData.back().getSize() });
    }
    presetPanel.setColours (panelColours());
    presetPanel.setBuiltIns (builtIns);

    if (proc.openPresetName.isEmpty())
        proc.openPresetName = mdt::factoryPresets().front().name;
    shell.setPresetPanel (&presetPanel);
    shell.setPresetName (proc.openPresetName);

    // Keyboard focus, both halves. Without them a text field -- TONES, the librarian's name or
    // search box -- takes the click, shows a caret, and ignores every keystroke:
    //
    // 1. This editor has to absorb focus itself. juce::ComponentPeer::handleFocusGain() falls back
    //    to grabKeyboardFocus() on the peer's component whenever nothing here was focused, and a
    //    container that does not want focus hands it on to "the first child that does" -- a button,
    //    which then eats every printable key. Wanting it stops that walk at the top.
    setWantsKeyboardFocus (true);

    // 2. ...and a click has to beat the peer's focus *restore*. handleFocusLoss() remembers what
    //    was focused, handleFocusGain() puts focus straight back there, and a click into a field on
    //    a window the host still holds focus on does both in that order: the click focuses the
    //    field, then the X11 FocusIn that same click provoked drags focus back to wherever it was
    //    last time. The two are not ordered, which is why it is intermittent and nearly always the
    //    first click after touching another window.
    //
    //    Don't race the restore -- react to it. mouseDown records what the click aimed the keyboard
    //    at, and any focus change within kClickFocusGraceMs of it re-asserts that target. The grace
    //    window leaves Tab and every later deliberate focus move alone.
    addMouseListener (this, true);   // nested children too: one guard covers every field
    juce::Desktop::getInstance().addFocusChangeListener (this);
    addAndMakeVisible (shell);

    // The design aspect, 75 %..250 %. After the shell, so the corner resizer sits on top of it.
    using mdt::Shell;
    setResizeLimits (Shell::width * 3 / 4, Shell::height * 3 / 4, Shell::width * 5 / 2, Shell::height * 5 / 2);
    getConstrainer()->setFixedAspectRatio ((double) mdt::Shell::width / mdt::Shell::height);
    setResizable (true, true);
    // The size is the machine's, not the project's: the last one used, in any session.
    const int w = juce::jlimit (Shell::width * 3 / 4, Shell::width * 5 / 2, settings.getIntValue ("editor.width", 1170));
    setSize (w, w * Shell::height / Shell::width);
}

// Saved on close, not per resize step: the settings file writes on every setValue.
MiditinEditor::~MiditinEditor()
{
    juce::Desktop::getInstance().removeFocusChangeListener (this);
    settings.setValue ("editor.width", getWidth());
}

// How long after a click this editor still treats a focus change as "something stole the click's
// focus". Long enough for the FocusIn that arrives a frame or two after the ButtonPress that
// provoked it; far shorter than any deliberate move.
static constexpr double kClickFocusGraceMs = 500.0;

void MiditinEditor::mouseDown (const juce::MouseEvent& e)
{
    // The component JUCE would have focused for this click: the one clicked, or the first parent
    // that wants focus -- the same walk Component::grabKeyboardFocusInternal() makes.
    juce::Component* target = e.eventComponent;
    while (target != nullptr && ! (target->getWantsKeyboardFocus() && target->isEnabled()))
        target = target->getParentComponent();

    // A click on something that takes no focus or on the editor itself has nothing to defend, and
    // clearing the target stops a stale one being restored under a later change.
    if (target == nullptr || target == this)
    {
        clickFocusTarget = nullptr;
        return;
    }

    clickFocusTarget = target;
    clickFocusTimeMs = juce::Time::getMillisecondCounterHiRes();

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MiditinEditor> (this)]
    {
        if (auto* self = safeThis.getComponent())
            self->reassertClickFocus (true);
    });
}

void MiditinEditor::globalFocusChanged (juce::Component*)
{
    // Fires for every focus change in the process, including the ones this method causes -- the
    // re-assert is a no-op once focus is already on the target, so there is no loop.
    if (juce::Time::getMillisecondCounterHiRes() - clickFocusTimeMs > kClickFocusGraceMs)
    {
        clickFocusTarget = nullptr;
        return;
    }

    reassertClickFocus (false);
}

void MiditinEditor::reassertClickFocus (bool allowUnfocused)
{
    auto* t = clickFocusTarget.getComponent();
    if (t == nullptr || ! t->isShowing() || ! t->getWantsKeyboardFocus() || ! t->isEnabled())
        return;
    if (juce::Component::getCurrentlyFocusedComponent() == t)
        return;
    // A modal file chooser -- the librarian's folder picker -- is entitled to the keyboard.
    if (juce::ModalComponentManager::getInstance()->getNumModalComponents() > 0)
        return;

    // Only take focus back from within this editor, and never when there is no focus at all: both
    // of those are somebody else's window. focusIsWithin() rather than isParentOf(), because the
    // editor itself is exactly where the peer's fallback grabKeyboardFocus() lands.
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if (focused == nullptr ? ! allowUnfocused : ! mdt::focusIsWithin (*this, focused))
        return;

    t->grabKeyboardFocus();
}

void MiditinEditor::resized()
{
    // The layout is always built at the design size; one transform carries it to the window.
    shell.setTransform (juce::AffineTransform::scale ((float) getWidth() / mdt::Shell::width));
    shell.setBounds (0, 0, mdt::Shell::width, mdt::Shell::height);
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
