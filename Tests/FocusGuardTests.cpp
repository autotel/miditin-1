#include <juce_gui_basics/juce_gui_basics.h>
#include "UI/FocusGuard.h"

using namespace mdt;

struct FocusGuardTests final : juce::UnitTest
{
    FocusGuardTests() : juce::UnitTest ("FocusGuard", "ui") {}

    void runTest() override
    {
        juce::Component container, child, grandchild, outsider;
        container.addChildComponent (child);
        child.addChildComponent (grandchild);

        beginTest ("the container itself counts as inside");
        // The case isParentOf() alone gets wrong -- and the peer's fallback grabKeyboardFocus()
        // lands on exactly this component, so a guard that says "outside" here declines to fix the
        // one bug it exists for.
        expect (focusIsWithin (container, &container));

        beginTest ("descendants at any depth count");
        expect (focusIsWithin (container, &child));
        expect (focusIsWithin (container, &grandchild));

        beginTest ("anything else does not");
        expect (! focusIsWithin (container, &outsider));
        expect (! focusIsWithin (container, nullptr));   // a genuine window deactivation
        expect (! focusIsWithin (child, &container));    // upwards is not inside
        expect (focusIsWithin (child, &grandchild));
    }
};

static FocusGuardTests focusGuardTests;
