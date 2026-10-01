#include <juce_events/juce_events.h>
#include <iostream>

int main (int argc, char** argv)
{
    // JUCE singletons (fonts, message manager) must be torn down before exit, or the leak detector
    // buries a real sanitizer finding under thousands of lines.
    const juce::ScopedJuceInitialiser_GUI juceInit;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    // `MiditinTests "Transformer"` runs only the tests whose name contains that text.
    if (argc > 1)
    {
        juce::Array<juce::UnitTest*> some;
        for (auto* t : juce::UnitTest::getAllTests())
            if (t->getName().containsIgnoreCase (argv[1]))
                some.add (t);
        runner.runTests (some);
    }
    else
    {
        runner.runAllTests();
    }

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    if (failures > 0)
        std::cerr << failures << " failure(s)\n";
    return failures > 0 ? 1 : 0;
}
