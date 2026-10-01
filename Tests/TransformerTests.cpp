#include <juce_core/juce_core.h>
#include "Engine/Transformer.h"

using namespace mdt;

struct TransformerTests final : juce::UnitTest
{
    TransformerTests() : juce::UnitTest ("Transformer") {}

    static Pool scalePool (ScaleType s, int root, float octave, bool wrap)
    {
        TransformConfig c; c.mode = TransformMode::scale; c.scale = s; c.root = root; c.octave = octave; c.wrap = wrap;
        return Pool::make (c, nullptr, 0);
    }

    void runTest() override
    {
        beginTest ("scale, wrap on: A minor from octave 4, negatives continue downward");
        auto a = scalePool (ScaleType::minor, 9, 4, true);
        expectEquals (*a.transform (0), 69);   // 60 + 9
        expectEquals (*a.transform (1), 71);
        expectEquals (*a.transform (7), 81);   // next octave
        expectEquals (*a.transform (-1), 67);  // G below
        expectEquals (*a.transform (-7), 57);

        beginTest ("scale octave is whole: 4.7 acts as 4");
        expectEquals (*scalePool (ScaleType::major, 0, 4.7f, true).transform (0), 60);

        beginTest ("scale, wrap off: tones fold within one period");
        auto f = scalePool (ScaleType::major, 0, 4, false);
        expectEquals (*f.transform (7), 60);
        expectEquals (*f.transform (-1), 71);
        expectEquals (*f.transform (15), 62);

        beginTest ("set: OCT LEN 5 repeats every 5 semitones");
        TransformConfig s; s.mode = TransformMode::set; s.pcs = 1 << 0; s.octave = 4; s.period = 5;
        auto five = Pool::make (s, nullptr, 0);
        expectEquals (*five.transform (0), 60);
        expectEquals (*five.transform (1), 65);
        expectEquals (*five.transform (-2), 50);

        beginTest ("set: fractional octave moves the border note, pitch classes stay put");
        s.pcs = (1 << 0) | (1 << 4) | (1 << 7); s.period = 12; s.octave = 4.0f + 5.0f / 12.0f;   // border F4 = 65
        auto br = Pool::make (s, nullptr, 0);
        expectEquals (br.base, 65);
        expectEquals (*br.transform (0), 67);   // G, first set note at or above the border
        expectEquals (*br.transform (1), 72);   // C
        expectEquals (*br.transform (2), 76);   // E
        expectEquals (*br.transform (3), 79);

        beginTest ("held: tone 0 is the lowest held note, wrap continues by octave");
        const int held[] { 64, 57, 60 };
        TransformConfig h; h.mode = TransformMode::held;
        auto hp = Pool::make (h, held, 3);
        expectEquals (*hp.transform (0), 57);
        expectEquals (*hp.transform (2), 64);
        expectEquals (*hp.transform (3), 69);
        expectEquals (*hp.transform (-1), 52);

        beginTest ("held-mode inverse lands on the voice's own note (spec 6.5 octave bug)");
        expectEquals (*hp.transform (0, 64), 64);
        expectEquals (*hp.transform (1, 64), 69);
        expectEquals (*hp.transform (-1, 57), 52);

        beginTest ("empty pool and out-of-range notes give nothing");
        expect (! Pool::make (h, held, 0).transform (0));
        expect (! a.transform (100));
        expect (! a.transform (-100));

        beginTest ("inverse o forward = identity on pool notes, every mode x wrap");
        for (bool wrap : { true, false })
        {
            for (auto mode : { TransformMode::scale, TransformMode::set, TransformMode::held })
            {
                TransformConfig c; c.mode = mode; c.wrap = wrap; c.pcs = 0x0a95; c.octave = 3.25f; c.scale = ScaleType::dorian; c.root = 2;
                c.period = 12;
                const auto pool = Pool::make (c, held, 3);
                for (int d = -10; d <= 10; ++d)
                {
                    const auto m = pool.transform (d);
                    if (! m) continue;
                    // A pool note used as the reference makes tone 0 land on it exactly.
                    expectEquals (*pool.transform (0, *m), *m);
                    if (wrap)
                        expectEquals (pool.tone (pool.locate (*m).k), *m);
                }
            }
        }

        beginTest ("wrap off: a reference an octave up shifts by whole octaves");
        expectEquals (*f.transform (0, 72), 72);
        expectEquals (*f.transform (1, 72), 74);
        expectEquals (*f.transform (0, 73), 72);   // C#: nearest pool pitch class below is C
    }
};

static TransformerTests transformerTests;
