// SPDX-License-Identifier: GPL-3.0-or-later
// Chord and note naming, ported from Polytwelve's editor (Ticco Ross).
#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <vector>

namespace p800
{
// Ableton convention: MIDI 60 = C3
inline juce::String noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 2);
}

inline juce::String pitchClass (int pc)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[((pc % 12) + 12) % 12];
}

// Name a chord from its notes; tries every note as the root and adds "/bass" for inversions.
inline juce::String chordName (const int* intervals, int n, int bass)
{
    struct Shape { std::vector<int> pcs; const char* suffix; };
    static const std::vector<Shape> shapes {
        { { 0, 4, 7 }, "" },        { { 0, 3, 7 }, "m" },        { { 0, 3, 6 }, "dim" },     { { 0, 4, 8 }, "aug" },
        { { 0, 2, 7 }, "sus2" },    { { 0, 5, 7 }, "sus4" },     { { 0, 7 }, "5" },
        { { 0, 4, 7, 10 }, "7" },   { { 0, 4, 7, 11 }, "maj7" }, { { 0, 3, 7, 10 }, "m7" },  { { 0, 3, 6, 10 }, "m7b5" },
        { { 0, 3, 6, 9 }, "dim7" }, { { 0, 4, 7, 9 }, "6" },     { { 0, 3, 7, 9 }, "m6" },   { { 0, 3, 7, 11 }, "mMaj7" },
        { { 0, 5, 7, 10 }, "7sus4" },
        { { 0, 2, 4, 7 }, "add9" }, { { 0, 2, 3, 7 }, "m(add9)" },
        { { 0, 2, 4, 7, 10 }, "9" }, { { 0, 2, 4, 7, 11 }, "maj9" }, { { 0, 2, 3, 7, 10 }, "m9" },
        { { 0, 3, 5, 7, 10 }, "m11" }, { { 0, 2, 3, 5, 7, 10 }, "m11" }, { { 0, 4, 7, 9, 2 }, "6/9" },
    };

    std::vector<int> pcs;
    for (int i = 0; i < n; ++i) { const int pc = (intervals[i] % 12 + 12) % 12; if (std::find (pcs.begin(), pcs.end(), pc) == pcs.end()) pcs.push_back (pc); }
    std::sort (pcs.begin(), pcs.end());
    if (pcs.size() == 1) return noteName (bass) + (n > 1 ? " oct" : "");

    for (int rootPc : pcs) // pcs relative to the bass; try each as root
    {
        std::vector<int> rel;
        for (int pc : pcs) rel.push_back (((pc - rootPc) % 12 + 12) % 12);
        std::sort (rel.begin(), rel.end());
        for (const auto& s : shapes)
        {
            auto want = s.pcs; std::sort (want.begin(), want.end());
            if (want == rel)
            {
                juce::String nm = pitchClass (bass + rootPc) + s.suffix;
                if (rootPc != 0) nm << "/" << pitchClass (bass);
                return nm;
            }
        }
    }
    return juce::String (n) + "-note chord";
}
} // namespace p800
