// SPDX-License-Identifier: GPL-3.0-or-later
// Key assign tests: POLY, CHORD memory (capture, transpose, last-note priority,
// voice limit) and HOLD, through the same KeyLayer the plugin uses.
#include "KeyLayer.h"
#include <cstdio>
#include <set>
#include <string>

struct Rec : p800::KeyLayer::Sink
{
    std::set<int> on;
    int strikes = 0;
    void keyOn (int n, int) override { on.insert (n); ++strikes; }
    void keyOff (int n) override { on.erase (n); }
};

static int fails = 0;
static void check (bool ok, const std::string& what, const Rec& r)
{
    std::string s;
    for (int n : r.on) s += std::to_string (n) + " ";
    std::printf ("%s %s  [sounding: %s]\n", ok ? "ok  " : "FAIL", what.c_str(), s.c_str());
    if (! ok) ++fails;
}

int main()
{
    using K = p800::KeyLayer;

    { // POLY
        K k; Rec r; k.update (r);
        k.noteOn (r, 60, 100); k.noteOn (r, 64, 100);
        check (r.on == std::set<int> { 60, 64 }, "poly: two keys sound", r);
        k.noteOff (r, 60); k.noteOff (r, 64);
        check (r.on.empty(), "poly: release silences", r);
    }
    { // CHORD capture by switching mode with keys held
        K k; Rec r; k.update (r);
        k.noteOn (r, 60, 100); k.noteOn (r, 63, 100); k.noteOn (r, 67, 100);
        k.setMode (K::chordMode); k.update (r);
        int iv[8], root = 0; const int n = k.getChord (iv, root);
        check (n == 3 && iv[0] == 0 && iv[1] == 3 && iv[2] == 7 && root == 60, "chord: C-Eb-G captured as 0 3 7", r);
        k.noteOff (r, 60); k.noteOff (r, 63); k.noteOff (r, 67);
        check (r.on.empty(), "chord: releasing all keys silences", r);

        k.noteOn (r, 50, 100);
        check (r.on == std::set<int> { 50, 53, 57 }, "chord: one key plays the stored chord transposed", r);
        k.noteOn (r, 55, 100);
        check (r.on == std::set<int> { 55, 58, 62 }, "chord: newest key wins (last-note priority)", r);
        k.noteOff (r, 55);
        check (r.on == std::set<int> { 50, 53, 57 }, "chord: releasing it returns to the held key", r);
        k.noteOff (r, 50);
        check (r.on.empty(), "chord: release silences", r);
    }
    { // switching to CHORD with fewer than 2 keys keeps the previous chord
        K k; Rec r; k.update (r);
        const int maj7[] = { 0, 4, 7, 11 }; k.setChord (maj7, 4, 48);
        k.noteOn (r, 70, 100);
        k.setMode (K::chordMode); k.update (r);
        int iv[8], root = 0;
        check (k.getChord (iv, root) == 4, "chord: one key held does not overwrite the memory", r);
        check (r.on == std::set<int> { 70, 74, 77, 81 }, "chord: held key replays as the stored chord", r);
    }
    { // DOUBLE mode has 4 voices: a 5-note chord plays its lowest 4 notes
        K k; Rec r; k.update (r);
        const int five[] = { 0, 4, 7, 10, 14 }; k.setChord (five, 5, 60);
        k.setVoiceLimit (4); k.setMode (K::chordMode); k.update (r);
        k.noteOn (r, 60, 100);
        check (r.on == std::set<int> { 60, 64, 67, 70 }, "chord: voice limit 4 plays 4 notes", r);
    }
    { // MEMORY button while in CHORD mode
        K k; Rec r; k.setMode (K::chordMode); k.update (r);
        k.setHold (true); k.update (r);
        k.noteOn (r, 62, 100); k.noteOff (r, 62);
        k.noteOn (r, 65, 100); k.noteOff (r, 65);
        k.noteOn (r, 69, 100); k.noteOff (r, 69);
        k.requestChordCapture(); k.update (r);
        int iv[8], root = 0; const int n = k.getChord (iv, root);
        check (n == 3 && iv[1] == 3 && iv[2] == 7 && root == 62, "memory: held (latched) keys are stored", r);
    }
    { // HOLD in POLY mode
        K k; Rec r; k.setHold (true); k.update (r);
        k.noteOn (r, 60, 100); k.noteOff (r, 60);
        k.noteOn (r, 67, 100); k.noteOff (r, 67);
        check (r.on == std::set<int> { 60, 67 }, "hold: released keys keep sounding", r);
        k.noteOn (r, 72, 100);
        k.setHold (false); k.update (r);
        check (r.on == std::set<int> { 72 }, "hold off: latched keys stop, a key still down keeps sounding", r);
    }
    { // HOLD in CHORD mode
        K k; Rec r; k.setMode (K::chordMode); k.update (r); k.setHold (true); k.update (r);
        k.noteOn (r, 48, 100); k.noteOff (r, 48);
        check (r.on == std::set<int> { 48, 51, 55 }, "hold + chord: chord keeps sounding after release", r);
        k.setHold (false); k.update (r);
        check (r.on.empty(), "hold + chord: switching hold off silences", r);
    }

    std::printf (fails ? "\n%d failure(s)\n" : "\nall key-assign checks pass\n", fails);
    return fails ? 1 : 0;
}
