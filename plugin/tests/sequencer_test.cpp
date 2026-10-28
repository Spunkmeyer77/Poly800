// SPDX-License-Identifier: GPL-3.0-or-later
// Sequencer test: editing rules (8-note limit), timing on the internal clock
// and in host sync (including loop jumps), swing, gate, polyphonic chords,
// 32-step wrap, probability, parameter locks and the save format.
#include "Sequencer.h"
#include "KeyLayer.h"
#include <cstdio>
#include <vector>

using namespace p800;

namespace
{
int fails = 0;
void check (bool ok, const char* what)
{
    std::printf ("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += ok ? 0 : 1;
}

struct Rec { long long sample; SeqEvent e; };

// Runs the player for 'samples' in blocks, collecting events with absolute sample times.
std::vector<Rec> run (SeqPlayer& pl, long long samples, int block, SeqTransport t, bool hostAdvance, double sr = 48000.0)
{
    std::vector<Rec> out;
    SeqEvent ev[SeqPlayer::maxEvents];
    for (long long s = 0; s < samples; s += block)
    {
        const int n = (int) std::min<long long> (block, samples - s);
        const int c = pl.process (n, t, true, ev);
        for (int i = 0; i < c; ++i) out.push_back ({ s + ev[i].offset, ev[i] });
        if (hostAdvance) t.ppq += n * t.bpm / 60.0 / sr;
    }
    return out;
}

int count (const std::vector<Rec>& r, SeqEvent::Type type)
{
    int n = 0;
    for (auto& x : r) n += x.e.type == type ? 1 : 0;
    return n;
}
} // namespace

int main()
{
    const double sr = 48000.0;

    { // editing
        SeqData d;
        bool all = true;
        for (int i = 0; i < 8; ++i) all = all && d.addNote (0, 60 + i);
        check (all && d.pat().steps[0].noteCount() == 8, "a step takes 8 notes");
        check (! d.addNote (0, 80), "a 9th note is refused (8 voices)");
        check (! d.addNote (0, 60), "the same pitch twice on a step is refused");
        d.addNote (4, 50, 3);
        int start = -1;
        check (d.findNote (6, 50, start) >= 0 && start == 4, "a 3-step note is found from a later step it covers");
        check (d.findNote (7, 50, start) < 0, "but not after it ends");
        d.setLock (2, 18, 150);
        check (d.pat().steps[2].locks[18] == 99, "locks are clamped to the parameter range (P41 0..99)");
        d.setLock (2, 19, 7);
        check (d.pat().steps[2].locks[18] == 99 && d.pat().steps[2].locks[19] == 7, "several locks on one step");
    }

    { // internal clock: 120 BPM 1/16 = 6000 samples per step
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = false; d.settings.bpm = 120; d.settings.gate = 50;
        d.addNote (0, 60); d.addNote (4, 62);
        pl.setData (d);
        const auto r = run (pl, 6000 * 16 - 100, 256, {}, false);
        std::vector<long long> ons;
        for (auto& x : r) if (x.e.type == SeqEvent::noteOn) ons.push_back (x.sample);
        check (ons.size() == 2 && ons[0] == 0 && std::llabs (ons[1] - 24000) <= 1, "notes start on time (step 1 at 0, step 5 at 24000)");
        long long off0 = -1;
        for (auto& x : r) if (x.e.type == SeqEvent::noteOff && x.e.note == 60) { off0 = x.sample; break; }
        check (std::llabs (off0 - 3000) <= 1, "gate 50% ends the note half a step later");
        check (count (r, SeqEvent::step) == 16, "16 step events in one bar");
    }

    { // chord: 4 notes on one step all at the same sample; second bar repeats
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = false;
        for (int n : { 48, 51, 55, 58 }) d.addNote (0, n, 2);
        pl.setData (d);
        const auto r = run (pl, 6000 * 32 - 100, 512, {}, false);
        int at0 = 0, at16 = 0;
        for (auto& x : r) if (x.e.type == SeqEvent::noteOn) { at0 += x.sample == 0; at16 += std::llabs (x.sample - 96000) <= 1; }
        check (at0 == 4 && at16 == 4, "polyphonic chord plays together, and again next bar");
        check (count (r, SeqEvent::noteOn) == count (r, SeqEvent::noteOff), "every note is released");
    }

    { // swing: odd steps late by (swing - 50) * 2 % of a step
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = false; d.settings.swing = 75;
        d.addNote (0, 60); d.addNote (1, 61);
        pl.setData (d);
        const auto r = run (pl, 12000, 128, {}, false);
        long long t1 = -1;
        for (auto& x : r) if (x.e.type == SeqEvent::noteOn && x.e.note == 61) t1 = x.sample;
        check (std::llabs (t1 - 9000) <= 1, "swing 75% moves step 2 from 6000 to 9000");
    }

    { // 32 steps wrap at 32, not 16
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = false; d.pat().length = 32;
        d.addNote (20, 60);
        pl.setData (d);
        const auto r = run (pl, 6000 * 64, 256, {}, false);
        std::vector<long long> ons;
        for (auto& x : r) if (x.e.type == SeqEvent::noteOn) ons.push_back (x.sample);
        check (ons.size() == 2 && std::llabs (ons[0] - 120000) <= 1 && std::llabs (ons[1] - 312000) <= 1, "32-step pattern: step 21 plays once per 32 steps");
    }

    { // host sync: follows the host position and handles a loop jump
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = true; d.addNote (0, 60, 16); // long note
        pl.setData (d);
        SeqTransport t; t.hasHost = true; t.hostPlaying = true; t.bpm = 120; t.ppq = 0.0;
        auto r = run (pl, 6000 * 4, 512, t, true);
        check (count (r, SeqEvent::noteOn) == 1 && r.front().sample == 0, "host sync: plays from the host's beat 0");
        SeqEvent ev[SeqPlayer::maxEvents];
        t.ppq = 0.0; // host loops back
        const int c = pl.process (512, t, true, ev);
        bool released = false, retriggered = false;
        for (int i = 0; i < c; ++i) { released |= ev[i].type == SeqEvent::noteOff; retriggered |= ev[i].type == SeqEvent::noteOn; }
        check (released && retriggered, "host loop: hanging notes are released and the step replays");
        t.hostPlaying = false;
        const int c2 = pl.process (512, t, true, ev);
        bool off = false;
        for (int i = 0; i < c2; ++i) off |= ev[i].type == SeqEvent::noteOff;
        check (off && pl.currentStep() == -1, "host stop releases notes");

        SeqPlayer pl2; pl2.prepare (sr); pl2.setData (d);
        SeqTransport mid = t; mid.hostPlaying = true; mid.ppq = 0.6; // 2.4 steps in
        const auto r2 = run (pl2, 6000 * 2, 256, mid, true);
        check (count (r2, SeqEvent::noteOn) == 0 && r2.front().e.stepIndex == 3, "starting mid-bar waits for the next step");
    }

    { // probability 0 never plays, 100 always
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = false;
        d.addNote (0, 60); d.pat().steps[0].probability = 0;
        d.addNote (8, 62);
        pl.setData (d);
        const auto r = run (pl, 6000 * 16 * 8, 512, {}, false);
        int n60 = 0, n62 = 0;
        for (auto& x : r) if (x.e.type == SeqEvent::noteOn) { n60 += x.e.note == 60; n62 += x.e.note == 62; }
        check (n60 == 0 && n62 == 8, "probability 0% skips, 100% always plays");
        SeqData h; h.settings.hostSync = false; h.addNote (0, 60); h.pat().steps[0].probability = 50;
        SeqPlayer p2; p2.prepare (sr); p2.setData (h);
        const auto r2 = run (p2, 6000LL * 16 * 200, 1024, {}, false);
        const int n = count (r2, SeqEvent::noteOn);
        check (n > 70 && n < 130, "probability 50% plays about half the time");
    }

    { // parameter locks follow the playing step
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = false;
        d.setLock (1, 18, 80); d.setLock (1, 19, 12);
        pl.setData (d);
        SeqEvent ev[SeqPlayer::maxEvents];
        pl.process (6000, {}, true, ev);
        check (pl.activeLocks() == nullptr, "step 1 has no locks");
        pl.process (6000, {}, true, ev);
        const auto* l = pl.activeLocks();
        check (l != nullptr && l[18] == 80 && l[19] == 12 && l[20] == kNoLock, "step 2 applies its two locks, others untouched");
        pl.process (6000, {}, true, ev);
        check (pl.activeLocks() == nullptr, "locks end with the step");
        pl.process (512, {}, false, ev);
        check (pl.activeLocks() == nullptr && pl.currentStep() == -1, "stopped: no locks, no playhead");
    }

    { // save format round trip
        SeqData d;
        d.settings = { 3, 62, 40, false, 98 };
        d.current = 2;
        d.pat().length = 32;
        for (int n : { 48, 51, 55 }) d.addNote (0, n, 2, 90);
        d.addNote (31, 72, 1, 127);
        d.pat().steps[5].probability = 25;
        d.pat().steps[5].gate = 60;
        d.setLock (5, 18, 33); d.setLock (5, 46, 9);
        d.current = 7; d.addNote (3, 40); d.current = 2;
        const auto text = d.serialise();
        SeqData e;
        check (e.deserialise (text), "parses its own output");
        check (e.serialise() == text, "round trip is exact");
        check (e.current == 2 && e.settings.rate == 3 && e.settings.swing == 62 && ! e.settings.hostSync && e.settings.bpm == 98,
               "settings survive");
        check (e.pat().length == 32 && e.pat().steps[0].noteCount() == 3 && e.pat().steps[0].notes[0].length == 2
               && e.pat().steps[31].notes[0].velocity == 127, "notes survive");
        check (e.pat().steps[5].probability == 25 && e.pat().steps[5].gate == 60 && e.pat().steps[5].locks[18] == 33
               && e.pat().steps[5].locks[46] == 9, "probability, gate and locks survive");
        check (e.patterns[7].steps[3].notes[0].note == 40, "every pattern is saved");
        SeqData f;
        check (! f.deserialise ("garbage") && ! f.deserialise (""), "rejects text that is not a sequence");
    }

    { // ties and held notes (0.5.2)
        SeqData d;
        for (int n : { 48, 52, 55 }) d.addNote (0, n);
        check (d.tieInto (1) && d.pat().steps[0].notes[0].length == 2, "TIE: notes ending at the selected step get one step longer");
        check (d.tieInto (2) && d.pat().steps[0].notes[2].length == 3, "TIE again: three steps");
        check (! d.tieInto (5), "TIE where nothing ends does nothing");
        d.addNote (8, 60);
        d.setLengthOf (8, 60, 4);
        check (d.pat().steps[8].notes[0].length == 4, "a held note recorded live gets its full length");
        d.setLengthOf (8, 60, 40);
        check (d.pat().steps[8].notes[0].length == 8, "and is clamped to the end of the pattern");
        d.pat().length = 16;
        d.addNote (15, 62);
        check (! d.tieInto (16), "no tie past the last step");
    }

    { // the other assignable controls can be locked too (0.5.2)
        const int speed = lockSlotOf ("chorusSpeed"), tune = lockSlotOf ("tune"), joyX = lockSlotOf ("joyX");
        check (speed >= numParams && tune > speed && joyX > tune && kLockSlots == numParams + numLockExtras,
               "Effect Speed, Tune and the joystick have lock slots after the chart parameters");
        SeqData d;
        d.setLock (3, speed, 150);
        d.setLock (3, tune, -50);
        d.setLock (4, joyX, -100);
        check (d.pat().steps[3].locks[(size_t) speed] == 100, "Effect Speed lock clamped to 0..100 %");
        check (d.pat().steps[3].locks[(size_t) tune] == -50 && d.pat().steps[3].hasLocks(), "negative values lock (tune -50 cents)");
        SeqData e;
        e.deserialise (d.serialise());
        check (e.pat().steps[3].locks[(size_t) tune] == -50 && e.pat().steps[4].locks[(size_t) joyX] == -100
               && e.pat().steps[5].locks[(size_t) tune] == kNoLock, "extra locks survive saving, unlocked stays unlocked");
        check (std::string (lockSpec (speed).name) == "Effect Speed" && lockSpec (18).number == 41, "lock names and numbers");
    }

    { // recording takes what the key assign layer plays (as the processor does since 0.5.1)
        struct RecordingSink : KeyLayer::Sink
        {
            SeqData* d; int* selected;
            void keyOn (int note, int) override { d->addNote (*selected, note); }
            void keyOff (int) override {}
        };
        SeqData d;
        int selected = 0;
        RecordingSink sink; sink.d = &d; sink.selected = &selected;
        KeyLayer keys;
        const int minor7[] = { 0, 3, 7, 10 };
        keys.setChord (minor7, 4, 48);
        keys.setMode (KeyLayer::chordMode);
        keys.update (sink);
        keys.noteOn (sink, 50, 100); keys.noteOff (sink, 50);   // one key in CHORD mode
        selected = 1;
        keys.noteOn (sink, 55, 100); keys.noteOff (sink, 55);
        const auto& s0 = d.pat().steps[0];
        const auto& s1 = d.pat().steps[1];
        auto has = [] (const SeqStep& st, int n) { for (auto& x : st.notes) if (x.note == n) return true; return false; };
        check (s0.noteCount() == 4 && has (s0, 50) && has (s0, 53) && has (s0, 57) && has (s0, 60),
               "CHORD mode: one key records the whole chord (D: 50 53 57 60)");
        check (s1.noteCount() == 4 && has (s1, 55) && has (s1, 65), "next key records the chord on its root");

        SeqData h; int sel = 0;
        RecordingSink hs; hs.d = &h; hs.selected = &sel;
        KeyLayer k2; k2.setHold (true); k2.update (hs);
        k2.noteOn (hs, 60, 100); k2.noteOff (hs, 60);
        k2.noteOn (hs, 64, 100); k2.noteOff (hs, 64);
        check (h.pat().steps[0].noteCount() == 2, "HOLD: notes played one by one record together");
    }

    { // 0.6.0: a pattern change while playing waits for the end of the playing pattern
        SeqPlayer pl; pl.prepare (sr);
        SeqData d; d.settings.hostSync = false; d.settings.bpm = 120;
        d.addNote (0, 60); d.addNote (8, 60);        // A: steps 1 and 9
        d.current = 1; d.addNote (0, 72); d.addNote (8, 72); // B
        d.current = 0;
        pl.setData (d);
        SeqEvent ev[SeqPlayer::maxEvents];
        std::vector<Rec> r;
        long long s = 0;
        auto runFor = [&] (long long samples)
        {
            for (long long end = s + samples; s < end; s += 256)
            {
                const int c = pl.process (256, {}, true, ev);
                for (int i = 0; i < c; ++i) r.push_back ({ s + ev[i].offset, ev[i] });
            }
        };
        runFor (6000 * 4);                            // into step 5 of A
        d.current = 1; pl.setData (d);                // choose B
        check (pl.changeQueued() && pl.playingPattern() == 0, "a change while playing is queued, A keeps playing");
        runFor (6000 * 12 - 6000 * 4 + 6000 * 4 + 100); // to just past the end of A
        long long firstB = -1, lastA = -1;
        for (auto& x : r)
            if (x.e.type == SeqEvent::noteOn)
            {
                if (x.e.note == 72 && firstB < 0) firstB = x.sample;
                if (x.e.note == 60) lastA = x.sample;
            }
        check (lastA >= 6000 * 8 - 300 && lastA <= 6000 * 8 + 300, "A finishes its pattern (its step 9 still plays)");
        check (firstB >= 6000 * 16 - 300 && firstB <= 6000 * 16 + 300, "B starts on its step 1 when A ends");
        check (! pl.changeQueued() && pl.playingPattern() == 1 && pl.currentStep() == 0, "B is now playing from step 1");

        SeqPlayer stopped; stopped.prepare (sr);
        d.current = 0; stopped.setData (d); d.current = 2; stopped.setData (d);
        check (stopped.playingPattern() == 2 && ! stopped.changeQueued(), "stopped: a change is immediate");
    }

    { // 0.8.0: 1/4 and 1/2 rates, and the Sequencer Rate lock
        auto stepTimes = [&] (SeqData d, long long samples)
        {
            SeqPlayer pl; pl.prepare (sr); pl.setData (d);
            std::vector<long long> t;
            for (auto& x : run (pl, samples, 256, {}, false)) if (x.e.type == SeqEvent::step) t.push_back (x.sample);
            return t;
        };
        SeqData d; d.settings.hostSync = false; d.settings.bpm = 120;
        d.settings.rate = 5; // 1/4: 24000 samples per step at 120 BPM
        auto t = stepTimes (d, 24000 * 3 + 10);
        check (t.size() == 4 && std::llabs (t[1] - 24000) <= 1 && std::llabs (t[3] - 72000) <= 1, "rate 1/4: a step per beat");
        d.settings.rate = 6; // 1/2
        t = stepTimes (d, 48000 * 2 + 10);
        check (t.size() == 3 && std::llabs (t[1] - 48000) <= 1, "rate 1/2: a step every two beats");
        check (seqRateOrderOf (6) == 0 && seqRateOrderOf (2) == 6, "rates ordered slowest (1/2) to fastest (1/32)");

        SeqData l; l.settings.hostSync = false; l.settings.bpm = 120; l.settings.rate = 1; // 1/16 = 6000
        const int slot = lockSlotOf ("seqRate");
        l.setLock (1, slot, seqRateOrderOf (5));  // step 2 lasts a 1/4 note
        l.setLock (3, slot, seqRateOrderOf (2));  // step 4 lasts a 1/32 note
        t = stepTimes (l, 6000 * 2 + 24000 + 6000 + 3000 + 10);
        check (t.size() >= 6 && std::llabs (t[1] - 6000) <= 1 && std::llabs (t[2] - 30000) <= 1
                   && std::llabs (t[3] - 36000) <= 1 && std::llabs (t[4] - 39000) <= 1,
               "Sequencer Rate lock changes that step's length (1/16, 1/4, 1/16, 1/32 ...)");
        SeqData e; e.deserialise (l.serialise());
        check (e.pat().steps[1].locks[(size_t) slot] == seqRateOrderOf (5), "rate locks are saved");
    }

    std::printf (fails ? "\n%d failure(s)\n" : "\nall sequencer checks pass\n", fails);
    return fails ? 1 : 0;
}
