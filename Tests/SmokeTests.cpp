#include <juce_core/juce_core.h>

struct SmokeTests final : juce::UnitTest
{
    SmokeTests() : juce::UnitTest ("Smoke") {}
    void runTest() override { beginTest ("the runner runs"); expect (true); }
};

static SmokeTests smokeTests;
