// SPDX-License-Identifier: GPL-3.0-or-later
// Renders the P-800 panel to a PNG without a host or window, for checking the
// look. Usage: render_panel out.png [scale] [params|seq16|seq32]
#include <juce_graphics/juce_graphics.h>
#include "PanelView.h"
#include "FactoryPrograms.h"

static void fillSequencerExample (p800::SeqView& q, int length)
{
    q.visible = true;
    q.playing = true;
    q.playStep = 5;
    q.lowNote = 36; // C1..B2
    q.lane = 2;
    q.selectedStep = 6;
    q.hostBpm = 124;
    auto& d = q.data;
    d.settings.swing = 54;
    d.settings.gate = 70;
    d.pat().length = length;
    // Electro: Cm7 / Abmaj7 stabs with a bass line under them.
    const int cm7[] = { 48, 51, 55, 58 }, ab[] = { 44, 48, 51, 55 }, bb[] = { 46, 50, 53, 57 };
    auto chord = [&] (int step, const int* notes) { for (int i = 0; i < 4; ++i) d.addNote (step, notes[i], 2); };
    for (int bar = 0; bar < length / 16; ++bar)
    {
        const int o = bar * 16;
        chord (o + 0, cm7);
        chord (o + 6, cm7);
        chord (o + 10, bar % 2 ? bb : ab);
        const int bass[][2] = { { 0, 36 }, { 3, 36 }, { 4, 48 }, { 7, 39 }, { 8, 36 }, { 10, 44 }, { 12, 43 }, { 14, 46 } };
        for (auto& b : bass) d.addNote (o + b[0], b[1], 1);
    }
    d.pat().steps[7].probability = 60;
    // Parameter locks on P41 Cutoff (and resonance on two steps).
    const int locks[][2] = { { 0, 38 }, { 4, 55 }, { 6, 72 }, { 10, 60 }, { 14, 85 }, { 18, 45 }, { 22, 80 }, { 26, 62 }, { 30, 95 } };
    for (auto& l : locks)
        if (l[0] < length) d.setLock (l[0], 18, l[1]);
    d.setLock (6, 19, 12);
    d.setLock (14, 19, 9);
    d.current = 0;
    d.patterns[1].steps[0].notes[0] = { 60, 100, 1 }; // so pattern B shows as used
}

int main (int argc, char** argv)
{
    if (argc < 2)
        return 1;
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;
    const juce::String mode = argc > 3 ? argv[3] : "params";

    p800::PanelView view;
    p800::PanelState s;
    for (int i = 0; i < p800::numParams; ++i)
        s.values[(size_t) i] = factory::programs[0].values[i];
    s.programNumber = factory::programs[0].number;
    s.programName = factory::programs[0].name;
    s.joyX = 0.35f;
    s.joyY = 0.4f;
    if (mode == "seq16") fillSequencerExample (s.seq, 16);
    if (mode == "seq32") fillSequencerExample (s.seq, 32);

    juce::Image img (juce::Image::RGB, juce::roundToInt (p800::PanelView::width * scale),
                     juce::roundToInt (p800::PanelView::height * scale), true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        view.paint (g, s);
    }
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    return png.writeImageToStream (img, os) ? 0 : 2;
}
