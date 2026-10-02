#include "Shell.h"
#include "Theme.h"

namespace mdt
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr int pad = 10, gap = 6, rowH = 12, buttonH = 18, bigButtonH = 24;

    juce::String feedsText (const Patch& p)
    {
        juce::StringArray f;
        if (p.pulse == PulseKind::midiIn) f.add ("pulse");
        if (p.gen == GenKind::input) f.add ("generate");
        if (p.runMode == RunMode::resetOnIn) f.add ("reset");
        if (p.tfMode == TransformMode::held) f.add ("transform");
        return f.isEmpty() ? juce::String ("nothing") : f.joinIntoString (fonts::utf8 (" \xc2\xb7 "));
    }

    juce::String heldText (const UiSnapshot& s)
    {
        if (s.heldCount == 0) return "none";
        std::vector<int> h (s.held.begin(), s.held.begin() + s.heldCount);
        std::sort (h.begin(), h.end());
        juce::StringArray names;
        for (int m : h) names.add (views::noteName (m));
        return names.joinIntoString (" ");
    }
}

juce::Rectangle<float> Shell::canvas (int column)
{
    return { (float) (column * colW + pad), (float) (gridTop + 48), views::CW, views::CH };
}

juce::Rectangle<float> Shell::keyboard (int side)
{
    return { (float) (side * 450 + pad), (float) (bottomTop + 48), views::KW, views::KH };
}

Shell::Shell (Host& h) : host (h), patch (h.patch()), seenVersion (h.patchVersion())
{
    snap = &host.snapshot();

    auto add = [this] (juce::Component& c) { addAndMakeVisible (c); };
    auto row = [this, &h] (std::unique_ptr<RangeRow>& r, P p, int lw = 42, int vw = 36) {
        r = std::make_unique<RangeRow> (h, p, lw, vw);
        addAndMakeVisible (*r);
    };

    // header
    for (auto* c : { &runBtn, &voiceBtn, &soundBtn }) add (*c);
    runBtn.onClick = [this] { edit ([] (Patch& p) { p.run = ! p.run; }); };
    voiceBtn.onClick = [this] { edit ([] (Patch& p) { p.voiceMode = p.voiceMode == VoiceMode::poly ? VoiceMode::mono : VoiceMode::poly; }); };
    soundBtn.onClick = [this] { edit ([] (Patch& p) { p.preview = ! p.preview; }); };
    row (bpm, P::bpm, 22, 22);
    bpm->valueLeft = true;

    // energy
    for (juce::Component* c : std::initializer_list<juce::Component*> { &enSource, &enShape, &syncBtn, &retrigBtn, &enFilter }) add (*c);
    enSource.onChange = [this] (int i) { edit ([i] (Patch& p) { p.enSource = (EnergySource) i; }); };
    enShape.padY = 3;
    enShape.onChange = [this] (int i) { edit ([i] (Patch& p) { p.enShape = (Shape) i; }); };
    syncBtn.onClick = [this] { edit ([] (Patch& p) { p.enSync = ! p.enSync; }); };
    retrigBtn.onClick = [this] { edit ([] (Patch& p) { p.enRetrig = ! p.enRetrig; }); };
    enFilter.padY = 3;
    enFilter.onChange = [this] (int i) { edit ([i] (Patch& p) { p.enFilter = (EnergyFilter) i; }); };
    ccNumber = std::make_unique<RangeRow> ("CC #", 0.0f, 119.0f, 1.0f,
        [this] { return (float) patch.ccNumber; },
        [this] (float v) { edit ([v] (Patch& p) { p.ccNumber = (int) v; }); },
        [] (float v) { return juce::String ((int) v); });
    add (*ccNumber);
    row (enAmount, P::enFilterAmt);

    // pulse
    for (juce::Component* c : std::initializer_list<juce::Component*> { &pulseKind, &kickBtn, &digBase, &modMode }) add (*c);
    pulseKind.onChange = [this] (int i) { edit ([i] (Patch& p) { p.pulse = (PulseKind) i; }); };
    row (slices, P::whSlices);
    row (speed, P::whSpeed);
    row (agitate, P::shAgitate);
    row (balls, P::shBalls);
    row (bounce, P::shBounce);
    row (digDiv, P::dgDiv);
    row (swing, P::dgSwing);
    const P offsets[] { P::whModOffset, P::shModOffset, P::dgModOffset }, scales[] { P::whModScale, P::shModScale, P::dgModScale };
    for (size_t i = 0; i < 3; ++i)
    {
        row (modOffset[i], offsets[i]);
        row (modScale[i], scales[i]);
    }
    kickBtn.onClick = [this] { host.kick(); };
    digBase.padY = 3;
    digBase.onChange = [this] (int i) { edit ([i] (Patch& p) { p.digBase = std::array<int, 4> { 2, 3, 5, 7 }[(size_t) i]; }); };
    modMode.padY = 3;
    modMode.onChange = [this] (int i) { edit ([i] (Patch& p) {
        if (p.pulse != PulseKind::midiIn) p.modMode[(size_t) p.pulse] = (ModMode) i; }); };

    // generate
    for (juce::Component* c : std::initializer_list<juce::Component*> { &genKind, &tones, &order, &transposeBtn, &clearBtn, &runMode }) add (*c);
    genKind.onChange = [this] (int i) { edit ([i] (Patch& p) { p.gen = (GenKind) i; }); };
    tones.setFont (fonts::font (fonts::field));
    tones.setJustification (juce::Justification::centredLeft);
    tones.setIndents (6, 3);
    tones.setColour (juce::TextEditor::backgroundColourId, theme::field);
    tones.setColour (juce::TextEditor::outlineColourId, theme::ink);
    tones.setColour (juce::TextEditor::focusedOutlineColourId, theme::ink);
    tones.setColour (juce::TextEditor::textColourId, theme::ink);
    tones.setColour (juce::TextEditor::highlightColourId, theme::accentA (0.3f));
    tones.setColour (juce::CaretComponent::caretColourId, theme::ink);
    tones.onTextChange = [this] { edit ([this] (Patch& p) { p.setArpText (tones.getText()); }); };
    order.onChange = [this] (int i) { edit ([i] (Patch& p) { p.order = (Order) i; }); };
    row (melLength, P::melLength);
    transposeBtn.onClick = [this] { edit ([] (Patch& p) { p.melTranspose = ! p.melTranspose; }); };
    clearBtn.onClick = [this] { edit ([] (Patch& p) { p.melody.fill (melRest); }); };
    runMode.onChange = [this] (int i) { edit ([i] (Patch& p) { p.runMode = (RunMode) i; }); };

    // transform
    for (juce::Component* c : std::initializer_list<juce::Component*> { &tfMode, &wrapBtn, &scale }) add (*c);
    tfMode.onChange = [this] (int i) { edit ([i] (Patch& p) { p.tfMode = (TransformMode) i; }); };
    row (octLen, P::tfOctLen);
    row (octave, P::tfOctave);
    wrapBtn.onClick = [this] { edit ([] (Patch& p) { p.wrap = ! p.wrap; }); };
    scale.padY = 3;
    scale.onChange = [this] (int i) { edit ([i] (Patch& p) { p.scale = (ScaleType) i; }); };

    // in / out
    add (releaseBtn);
    releaseBtn.onClick = [this] { host.releaseHeld(); };
    row (gate, P::gate, 30, 40);
    gate->valueLeft = true;

    setSize (width, height);
    syncControls();
    startTimerHz (60);
}

Shell::~Shell() { stopTimer(); }

void Shell::timerCallback()
{
    advanceFrame();
    if (presetPanel != nullptr && presetPanel->isVisible())
        return;
    // Only what animates: the canvas band (cables included), the transform's held-notes line, and
    // the keyboards with their text rows. A full 900 x 600 repaint at 60 Hz is wasted CPU in a DAW.
    repaint (0, gridTop + 40, width, (int) views::CH + 20);
    repaint (heldTextArea.getSmallestIntegerContainer());
    repaint (0, bottomTop, width, height - bottomTop);
}

void Shell::setPresetPanel (juce::Component* panel)
{
    presetPanel = panel;
    addAndMakeVisible (presetsBtn);
    addChildComponent (*panel);
    panel->setBounds (0, headerH, width, height - headerH);
    layoutControls();
}

void Shell::setPresetName (const juce::String& name)
{
    // The open preset names the button that opens the librarian.
    presetsBtn.setText (name.isEmpty() ? juce::String ("PRESETS") : name);
}

void Shell::patchReloaded()
{
    patch = host.patch();
    seenVersion = host.patchVersion();
    syncControls();
}

void Shell::edit (const std::function<void (Patch&)>& change)
{
    Patch next = patch;
    change (next);
    if (next == patch)
        return;
    patch = next;
    host.setPatch (patch);
    seenVersion = host.patchVersion();
    syncControls();
}

void Shell::syncControls()
{
    const auto& p = patch;
    runBtn.setText (p.run ? fonts::utf8 ("\xe2\x96\xa0 STOP") : fonts::utf8 ("\xe2\x96\xb6 RUN"));
    runBtn.setOn (p.run);
    voiceBtn.setText (p.voiceMode == VoiceMode::poly ? "POLY" : "MONO");
    soundBtn.setText (p.preview ? "SOUND ON" : "SOUND OFF");
    soundBtn.setOn (p.preview);

    enSource.setSelected ((int) p.enSource);
    enShape.setSelected ((int) p.enShape);
    syncBtn.setText (p.enSync ? "SYNC ON" : "SYNC OFF");
    syncBtn.setOn (p.enSync);
    retrigBtn.setText (p.enRetrig ? "RETRIG ON" : "RETRIG OFF");
    retrigBtn.setOn (p.enRetrig);
    enFilter.setSelected ((int) p.enFilter);
    enAmount->setLabel (p.enFilter == EnergyFilter::hpf ? "WINDOW" : "INERTIA");

    pulseKind.setSelected ((int) p.pulse);
    kickBtn.setText (p.pulse == PulseKind::wheel ? fonts::utf8 ("SPIN \xe2\x86\xbb") : fonts::utf8 ("SHAKE \xe2\x87\x84"));
    digBase.setSelected (p.digBase == 2 ? 0 : p.digBase == 3 ? 1 : p.digBase == 5 ? 2 : 3);
    if (p.pulse != PulseKind::midiIn)
        modMode.setSelected ((int) p.modMode[(size_t) p.pulse]);

    genKind.setSelected ((int) p.gen);
    if (! tones.hasKeyboardFocus (true))
        tones.setText (p.arpText(), juce::dontSendNotification);
    order.setSelected ((int) p.order);
    order.setLabels ({ "UP", "DN", "UP/DN", "RND", p.gen == GenKind::input ? "PLAYED" : "TYPED" });
    transposeBtn.setOn (p.melTranspose);
    runMode.setSelected ((int) p.runMode);

    tfMode.setSelected ((int) p.tfMode);
    wrapBtn.setText (p.wrap ? "OCTAVE WRAP ON" : "OCTAVE WRAP OFF");
    wrapBtn.setOn (p.wrap);
    scale.setSelected ((int) p.scale);
    // The octave is whole in scale mode; set mode takes it in semitone steps (spec §6.3).
    octave->stepOverride = p.tfMode == TransformMode::set ? 1.0f / 12.0f : 1.0f;

    layoutControls();
    repaint();
}

void Shell::resized() { layoutControls(); }

void Shell::layoutControls()
{
    const auto& p = patch;
    sideLabels.clear();
    auto show = [] (juce::Component& c, bool visible) { c.setVisible (visible); };

    // A column's control stack, as the prototype's flex column with 6 px gaps.
    struct Stack
    {
        int x, y, w;
        juce::Rectangle<int> next (int h) { auto r = juce::Rectangle<int> (x, y, w, h); y += h + gap; return r; }
    };
    const int controlsTop = gridTop + 48 + (int) views::CH + pad;
    auto column = [&] (int i) { return Stack { i * colW + pad, controlsTop, (int) views::CW }; };
    auto labelled = [&] (Stack& st, const juce::String& text, juce::Component& c, int h) {
        const auto r = st.next (h);
        sideLabels.push_back ({ text, r.withWidth (42).toFloat() });
        c.setBounds (r.withTrimmedLeft (48));
    };

    // header: right to left, 10 px apart, 16 px in from the edge.
    {
        int right = width - 16;
        const int y = (headerH - 25) / 2;
        auto place = [&] (juce::Component& c, int w) { c.setBounds (right - w, y, w, 25); right -= w + 10; };
        place (soundBtn, 84);
        place (voiceBtn, 52);
        place (*bpm, 22 + 6 + 70 + 6 + 22);
        place (runBtn, 66);
        if (presetPanel != nullptr)
            place (presetsBtn, 120);
    }

    // ENERGY
    {
        auto st = column (0);
        enSource.setBounds (st.next (Segmented::heightFor (1, 4)));
        const bool osc = p.enSource == EnergySource::osc, cc = p.enSource == EnergySource::cc;
        show (enShape, osc); show (syncBtn, osc); show (retrigBtn, osc); show (*ccNumber, cc);
        if (osc)
        {
            enShape.setBounds (st.next (Segmented::heightFor (1, 3)));
            auto r = st.next (buttonH);
            syncBtn.setBounds (r.removeFromLeft ((r.getWidth() - gap) / 2));
            retrigBtn.setBounds (r.withTrimmedLeft (gap));
        }
        if (cc)
            ccNumber->setBounds (st.next (rowH));
        labelled (st, "FILTER", enFilter, Segmented::heightFor (1, 3));
        enAmount->setBounds (st.next (rowH));
    }

    // PULSE
    {
        auto st = column (1);
        pulseKind.setBounds (st.next (Segmented::heightFor (2, 4)));
        const bool wheel = p.pulse == PulseKind::wheel, shaker = p.pulse == PulseKind::shaker,
                   digital = p.pulse == PulseKind::digital, none = p.pulse == PulseKind::midiIn;
        for (auto* c : { slices.get(), speed.get() }) show (*c, wheel);
        for (auto* c : { agitate.get(), balls.get(), bounce.get() }) show (*c, shaker);
        for (juce::Component* c : std::initializer_list<juce::Component*> { digDiv.get(), swing.get(), &digBase }) show (*c, digital);
        show (kickBtn, wheel || shaker);
        show (modMode, ! none);
        for (size_t i = 0; i < 3; ++i)
        {
            show (*modOffset[i], (int) p.pulse == (int) i);
            show (*modScale[i], (int) p.pulse == (int) i);
        }
        if (wheel)
        {
            slices->setBounds (st.next (rowH));
            speed->setBounds (st.next (rowH));
            kickBtn.setBounds (st.next (bigButtonH));
        }
        if (shaker)
        {
            agitate->setBounds (st.next (rowH));
            balls->setBounds (st.next (rowH));
            bounce->setBounds (st.next (rowH));
            kickBtn.setBounds (st.next (bigButtonH));
        }
        if (digital)
        {
            labelled (st, "BASE", digBase, Segmented::heightFor (1, 3));
            digDiv->setBounds (st.next (rowH));
            swing->setBounds (st.next (rowH));
        }
        if (! none)
        {
            st.y += 4;
            labelled (st, "ENERGY", modMode, Segmented::heightFor (1, 3));
            modOffset[(size_t) p.pulse]->setBounds (st.next (rowH));
            modScale[(size_t) p.pulse]->setBounds (st.next (rowH));
        }
    }

    // GENERATE
    {
        auto st = column (2);
        genKind.setBounds (st.next (Segmented::heightFor (1, 4)));
        const bool arp = p.gen != GenKind::melody, typed = p.gen == GenKind::arp, mel = p.gen == GenKind::melody;
        show (tones, typed); show (order, arp);
        show (*melLength, mel); show (transposeBtn, mel); show (clearBtn, mel);
        if (typed)
            labelled (st, "TONES", tones, 22);
        if (arp)
            order.setBounds (st.next (Segmented::heightFor (1, 4)));
        if (mel)
        {
            melLength->setBounds (st.next (rowH));
            auto r = st.next (buttonH);
            clearBtn.setBounds (r.removeFromRight (46));
            r.removeFromRight (gap);
            transposeBtn.setBounds (r.removeFromRight (100));
        }
        labelled (st, "RUN", runMode, Segmented::heightFor (1, 4));
    }

    // TRANSFORM
    {
        auto st = column (3);
        tfMode.setBounds (st.next (Segmented::heightFor (1, 4)));
        const bool set = p.tfMode == TransformMode::set, sc = p.tfMode == TransformMode::scale, held = p.tfMode == TransformMode::held;
        show (*octLen, set); show (scale, sc); show (*octave, ! held);
        if (set)
            octLen->setBounds (st.next (rowH));
        wrapBtn.setBounds (st.next (buttonH));
        if (sc)
            scale.setBounds (st.next (Segmented::heightFor (2, 3)));
        if (! held)
            octave->setBounds (st.next (rowH));
        heldTextArea = held ? st.next (rowH).toFloat() : juce::Rectangle<float>();
    }

    // IN / OUT rows under the keyboards
    {
        const int y = bottomTop + 48 + (int) views::KH + 7;
        releaseBtn.setBounds (450 - pad - 80, y, 80, buttonH);
        inHeldArea = juce::Rectangle<float> ((float) pad, (float) y, 450.0f - 2 * pad - 88, (float) buttonH);
        gate->setBounds (450 + pad, y + 3, 30 + 6 + 70 + 6 + 40, rowH);
        logArea = juce::Rectangle<float> (450.0f + pad + 160, (float) y, 450.0f - 2 * pad - 160, (float) buttonH);
    }
}

// -- frames --------------------------------------------------------------------------------------

views::Frame Shell::frame() const
{
    views::Frame f;
    f.s = snap;
    f.patch = &patch;
    for (int i = 0; i < numParams; ++i)
        f.pv.v[(size_t) i] = host.param ((P) i);
    f.histories = &histories;
    f.numHistories = numHistories;
    f.flow = flow;
    f.energyHover = drag.mode == Drag::rate ? "rate" : drag.mode == Drag::mid ? "mid"
                  : drag.mode == Drag::top ? "top" : drag.mode == Drag::bot ? "bot" : energyHover;
    return f;
}

void Shell::advanceFrame()
{
    snap = &host.snapshot();
    if (host.patchVersion() != seenVersion)
    {
        seenVersion = host.patchVersion();
        patch = host.patch();
        syncControls();
    }

    for (auto* c : getChildren())
        if (auto* r = dynamic_cast<RangeRow*> (c))
            r->refresh();

    if (snap->serial == lastSerial)
        return;
    lastSerial = snap->serial;

    // Keep each view's history with the view: poly voices come and go, and their lines must not
    // jump between them.
    std::array<views::History, maxVoices> next {};
    for (int i = 0; i < snap->numViews; ++i)
    {
        const int note = snap->views[(size_t) i].note;
        for (int k = 0; k < numHistories; ++k)
            if (histories[(size_t) k].note == note) { next[(size_t) i] = histories[(size_t) k]; break; }
        next[(size_t) i].note = note;
        next[(size_t) i].push (snap->views[(size_t) i].energy, snap->views[(size_t) i].energyRaw);
    }
    histories = next;
    numHistories = snap->numViews;

    const double dt = juce::jlimit (0.0, 0.05, snap->t - lastT);
    lastT = snap->t;
    flow += dt * (6 + snap->energy * 70);
}

// -- painting ------------------------------------------------------------------------------------

void Shell::paint (juce::Graphics& g)
{
    g.fillAll (theme::paper);

    // header
    g.setColour (theme::ink);
    g.fillRect (0, headerH - 1, width, 1);
    fonts::text (g, "miditin 1", 16, 29, fonts::title, theme::ink);
    g.setColour (theme::muted);
    const float titleW = juce::GlyphArrangement::getStringWidth (fonts::font (fonts::title), "miditin 1");
    const auto caption = fonts::utf8 ("ENERGY \xe2\x86\x92 PULSE \xe2\x86\x92 GENERATE \xe2\x86\x92 TRANSFORM \xe2\x86\x92 MIDI");
    fonts::drawTracked (g, caption, { 16 + titleW + 10, 0, 400, (float) headerH }, fonts::caption, juce::Justification::centredLeft);

    drawModuleTitles (g);
    drawBottomText (g);
    for (const auto& [text, r] : sideLabels)
    {
        g.setColour (theme::muted);
        fonts::drawTracked (g, text, r, fonts::label, juce::Justification::centredLeft);
    }

    const auto f = frame();
    const auto local = views::anchorsFor (f);
    auto at = [] (juce::Point<float> p, int col) { return p + canvas (col).getPosition(); };
    views::Anchors grid;
    grid.lfo = at (local.lfo, 0);
    grid.pTop = at (local.pTop, 1);
    grid.pOut = at (local.pOut, 1);
    grid.gC = at (local.gC, 2);
    grid.gOut = at (local.gOut, 2);
    grid.tC = at (local.tC, 3);

    auto drawIn = [&] (int col, void (*fn) (juce::Graphics&, const views::Frame&)) {
        juce::Graphics::ScopedSaveState s2 (g);
        g.addTransform (juce::AffineTransform::translation (canvas (col).getPosition()));
        fn (g, f);
    };
    views::drawConnections (g, f, grid, true);
    drawIn (0, views::drawEnergy);
    drawIn (1, views::drawPulse);
    drawIn (2, views::drawGenerate);
    drawIn (3, views::drawTransform);
    views::drawConnections (g, f, grid, false);

    for (int side = 0; side < 2; ++side)
    {
        juce::Graphics::ScopedSaveState s2 (g);
        g.addTransform (juce::AffineTransform::translation (keyboard (side).getPosition()));
        side ? views::drawOutKeys (g, f) : views::drawInKeys (g, f);
    }
}

void Shell::drawModuleTitles (juce::Graphics& g)
{
    static const char* const titles[] { "ENERGY", "PULSE", "GENERATE", "TRANSFORM" };
    static const char* const pulseNames[] { "wheel", "shaker", "digital", "none" };
    static const char* const genNames[] { "arp", "input", "melody" };
    static const char* const tfNames[] { "scale", "set", "held" };
    const juce::String subs[] { "lfo", pulseNames[(int) patch.pulse], genNames[(int) patch.gen], tfNames[(int) patch.tfMode] };
    for (int i = 0; i < 4; ++i)
    {
        const float x = (float) (i * colW);
        g.setColour (theme::ink);
        fonts::drawTracked (g, titles[i], { x, (float) gridTop + 12, (float) colW, 13 }, fonts::moduleTitle, juce::Justification::centred);
        g.setColour (theme::muted);
        fonts::drawTracked (g, subs[i], { x, (float) gridTop + 26, (float) colW, 12 }, fonts::moduleSub, juce::Justification::centred);
    }
    if (! heldTextArea.isEmpty())
    {
        g.setColour (theme::muted);
        fonts::drawTracked (g, heldText (*snap), heldTextArea, fonts::label, juce::Justification::centredLeft);
    }
}

void Shell::drawBottomText (juce::Graphics& g)
{
    const juce::String subs[] { fonts::utf8 ("\xe2\x86\x92 ") + feedsText (patch),
                                patch.preview ? "midi out + preview synth" : "midi out" };
    for (int i = 0; i < 2; ++i)
    {
        const float x = (float) (i * 450);
        g.setColour (theme::ink);
        fonts::drawTracked (g, i ? "OUT" : "IN", { x, (float) bottomTop + 12, 450, 13 }, fonts::moduleTitle, juce::Justification::centred);
        g.setColour (theme::muted);
        fonts::drawTracked (g, subs[i], { x, (float) bottomTop + 26, 450, 12 }, fonts::moduleSub, juce::Justification::centred);
    }

    g.setColour (theme::muted);
    fonts::drawTracked (g, "HELD", inHeldArea.withWidth (30), fonts::label, juce::Justification::centredLeft);
    g.setColour (theme::ink);
    fonts::drawTracked (g, heldText (*snap), inHeldArea.withTrimmedLeft (38), fonts::label, juce::Justification::centredLeft);
    g.setColour (theme::muted);
    fonts::drawTracked (g, "GATE", juce::Rectangle<float> (450.0f + pad, (float) gate->getY(), 30, rowH), fonts::label, juce::Justification::centredLeft);

    // Recent-notes log: newest on the left of the right-aligned run, fading with age.
    float right = logArea.getRight();
    for (int i = snap->logCount - 1; i >= 0; --i)
    {
        const auto& l = snap->log[(size_t) i];
        const auto deg = views::signedTone (l.deg), name = views::noteName (l.note);
        const float w = 4 + fonts::trackedWidth (deg, fonts::canvas) + 3 + fonts::trackedWidth (name, fonts::label) + 4 + 2;
        const juce::Rectangle<float> chip (right - w, logArea.getY(), w, 16);
        right -= w + 3;
        const float op = 1.0f - (float) i * 0.16f;
        g.setColour (theme::logEdge.withMultipliedAlpha (op));
        g.drawRect (chip, 1.0f);
        g.setColour (theme::muted.withMultipliedAlpha (op));
        fonts::drawTracked (g, deg, chip.withTrimmedLeft (5), fonts::canvas, juce::Justification::centredLeft);
        g.setColour (theme::ink.withMultipliedAlpha (op));
        fonts::drawTracked (g, name, chip.withTrimmedRight (4), fonts::label, juce::Justification::centredRight);
    }
}

// -- mouse ---------------------------------------------------------------------------------------

juce::String Shell::energyZone (juce::Point<float> l) const
{
    const views::EnergyGeo G;
    if (patch.enSource == EnergySource::osc)
    {
        const float d = l.getDistanceFrom ({ G.cx, G.cy });
        const float a = std::atan2 (l.y - G.cy, l.x - G.cx) + pi / 2;
        if (d > G.rm - 1 && d < G.rm + 16 && std::abs (a) < views::rateArc + 0.25f)
            return "rate";
    }
    const float v = G.v (l.y), c = host.param (P::enCenter), dep = host.param (P::enDepth);
    const float b0 = juce::jlimit (0.0f, 1.0f, c - dep / 2), b1 = juce::jlimit (0.0f, 1.0f, c + dep / 2);
    if (std::abs (v - b1) < 0.06f) return "top";
    if (std::abs (v - b0) < 0.06f) return "bot";
    if (v > b0 && v < b1) return "mid";
    return {};
}

void Shell::mouseMove (const juce::MouseEvent& e)
{
    const auto r = canvas (0);
    const auto z = r.contains (e.position) ? energyZone (e.position - r.getPosition()) : juce::String();
    if (z != energyHover)
    {
        energyHover = z;
        setMouseCursor (z == "rate" ? juce::MouseCursor::LeftRightResizeCursor
                      : z == "mid" ? juce::MouseCursor::DraggingHandCursor
                      : z.isNotEmpty() ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
    else if (canvas (2).contains (e.position) && patch.gen == GenKind::melody)
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else if (canvas (3).contains (e.position) && patch.tfMode != TransformMode::held)
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else if (keyboard (0).contains (e.position))
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else if (z.isEmpty())
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void Shell::mouseExit (const juce::MouseEvent&)
{
    if (energyHover.isNotEmpty()) { energyHover = {}; repaint(); }
}

void Shell::mouseDown (const juce::MouseEvent& e)
{
    drag = {};

    if (const auto r = canvas (0); r.contains (e.position))
    {
        const auto l = e.position - r.getPosition();
        const auto z = energyZone (l);
        const views::EnergyGeo G;
        if (z == "rate")
        {
            drag.mode = Drag::rate;
            drag.x0 = l.x;
            drag.n0 = views::EnergyGeo::rateN (patch, frame().pv);
            host.beginGesture (patch.enSync ? P::enDiv : P::enRate);
        }
        else if (z.isNotEmpty())
        {
            drag.mode = z == "mid" ? Drag::mid : z == "top" ? Drag::top : Drag::bot;
            drag.v0 = G.v (l.y);
            drag.base0 = host.param (P::enCenter);
            host.beginGesture (drag.mode == Drag::mid ? P::enCenter : P::enDepth);
        }
        return;
    }

    if (const auto r = canvas (2); r.contains (e.position) && patch.gen == GenKind::melody)
    {
        const auto l = e.position - r.getPosition();
        const int len = juce::roundToInt (host.param (P::melLength));
        const float cw = views::CW / (float) len;
        const int col = juce::jlimit (0, len - 1, (int) std::floor (l.x / cw));
        const int deg = juce::jlimit (-7, 7, 7 - (int) std::floor ((l.y - 13) / ((views::CH - 13) / 15)));
        auto mel = patch.melody;

        // The note under the click, if any.
        int ns = -1, ne = -1;
        for (int i = 0; i < len; ++i)
        {
            if (mel[(size_t) i] == melTie || mel[(size_t) i] == melRest) continue;
            int j = i + 1;
            while (j < len && mel[(size_t) j] == melTie) ++j;
            if (col >= i && col < j) { ns = i; ne = j; }
        }
        if (ns >= 0 && mel[(size_t) ns] == deg)
        {
            // Its right-edge handle lengthens it; anywhere else on it deletes it.
            if (col == ne - 1 && (float) ne * cw - l.x < 6)
            {
                drag = { Drag::melResize, 0, 0, 0, 0, ns, col };
                return;
            }
            for (int i = ns; i < ne; ++i) mel[(size_t) i] = melRest;
        }
        else
        {
            mel[(size_t) col] = (int8_t) deg;
            for (int i = col + 1; i < melodySteps && mel[(size_t) i] == melTie; ++i) mel[(size_t) i] = melRest;
        }
        drag.mode = Drag::melDraw;
        drag.c = col;
        edit ([mel] (Patch& p) { p.melody = mel; });
        return;
    }

    if (const auto r = canvas (3); r.contains (e.position) && patch.tfMode != TransformMode::held)
    {
        const auto l = e.position - r.getPosition();
        const auto G = views::WheelGeo::make (patch, frame().pv, *snap);
        float a = std::atan2 (l.y - G.cy, l.x - G.cx) + pi / 2;
        if (a < 0) a += 2 * pi;
        const int i = juce::roundToInt (a / (2 * pi / 12)) % 12;
        edit ([i] (Patch& p) {
            if (p.tfMode == TransformMode::set) p.pcs ^= (uint16_t) (1 << i);
            else p.root = i;
        });
        return;
    }

    if (const auto r = keyboard (0); r.contains (e.position))
    {
        const auto l = e.position - r.getPosition();
        const auto ks = views::keys (36, 83, views::KW);
        const views::Key* hit = nullptr;
        if (l.y < views::KH * 0.6f)
            for (const auto& k : ks)
                if (k.black && l.x >= k.x && l.x < k.x + k.w) hit = &k;
        if (hit == nullptr)
            for (const auto& k : ks)
                if (! k.black && l.x >= k.x && l.x < k.x + k.w) hit = &k;
        if (hit == nullptr)
            return;
        // Lower on the key is louder, as on a real one.
        const float vel = juce::jlimit (0.05f, 1.0f, l.y / (hit->black ? views::KH * 0.6f : views::KH));
        if (std::find (snap->held.begin(), snap->held.begin() + snap->heldCount, hit->m) != snap->held.begin() + snap->heldCount)
            host.noteOff (hit->m);
        else
            host.noteOn (hit->m, vel);
    }
}

void Shell::mouseDrag (const juce::MouseEvent& e)
{
    if (drag.mode == Drag::none)
        return;

    if (drag.mode == Drag::melDraw || drag.mode == Drag::melResize)
    {
        const auto l = e.position - canvas (2).getPosition();
        const int len = juce::roundToInt (host.param (P::melLength));
        const float cw = views::CW / (float) len;
        const int col = juce::jlimit (0, len - 1, (int) std::floor (l.x / cw));
        const int deg = juce::jlimit (-7, 7, 7 - (int) std::floor ((l.y - 13) / ((views::CH - 13) / 15)));
        auto mel = patch.melody;
        if (drag.mode == Drag::melDraw)
        {
            if (col == drag.c) return;
            drag.c = col;
            mel[(size_t) col] = (int8_t) deg;
            for (int i = col + 1; i < melodySteps && mel[(size_t) i] == melTie; ++i) mel[(size_t) i] = melRest;
        }
        else
        {
            const int c = std::max (col, drag.s);
            if (c == drag.c) return;
            drag.c = c;
            for (int i = drag.s + 1; i < melodySteps; ++i)
            {
                if (i <= c) mel[(size_t) i] = melTie;
                else if (mel[(size_t) i] == melTie) mel[(size_t) i] = melRest;
                else break;
            }
        }
        edit ([mel] (Patch& p) { p.melody = mel; });
        return;
    }

    const auto l = e.position - canvas (0).getPosition();
    const views::EnergyGeo G;
    const float v = G.v (l.y);
    if (drag.mode == Drag::rate)
    {
        const float n = juce::jlimit (0.0f, 1.0f, drag.n0 + (l.x - drag.x0) / 110.0f);
        if (patch.enSync) host.setParam (P::enDiv, std::round (n * 7));
        else host.setParam (P::enRate, 0.02f * std::pow (150.0f, n));
    }
    else if (drag.mode == Drag::mid)
        host.setParam (P::enCenter, juce::jlimit (0.0f, 1.0f, drag.base0 + v - drag.v0));
    else
        host.setParam (P::enDepth, juce::jlimit (0.0f, 1.0f, 2 * std::abs (v - host.param (P::enCenter))));
    repaint();
}

void Shell::mouseUp (const juce::MouseEvent&)
{
    switch (drag.mode)
    {
        case Drag::rate: host.endGesture (patch.enSync ? P::enDiv : P::enRate); break;
        case Drag::mid:  host.endGesture (P::enCenter); break;
        case Drag::top:
        case Drag::bot:  host.endGesture (P::enDepth); break;
        case Drag::none:
        case Drag::melDraw:
        case Drag::melResize: break;
    }
    drag = {};
}
} // namespace mdt
