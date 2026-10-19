#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace mdt
{

/** "Is the keyboard focus inside this container?" -- what the editor's click-focus re-assert
    (`PluginEditor.cpp`) asks before taking focus back: a focus change to a sibling of the click is
    one it may undo, one to another window or a modal file chooser is not.

    The container itself counts, and that is the whole reason this is a named function with a test
    rather than a call to `isParentOf()`. A component is not its own parent, so `isParentOf()` alone
    answers "no" for the container -- and the container *is* where focus lands in the single most
    common failure this guard exists for: `ComponentPeer::handleFocusGain()` falling back to
    `grabKeyboardFocus()` on the peer's component, which stops at the plugin editor because that is
    the first thing in the hierarchy that wants focus. Getting this predicate wrong makes the
    re-assert back off in exactly the case it was written for, silently and only inside a host. */
inline bool focusIsWithin (const juce::Component& container, const juce::Component* focused)
{
    return focused != nullptr && (focused == &container || container.isParentOf (focused));
}

} // namespace mdt
