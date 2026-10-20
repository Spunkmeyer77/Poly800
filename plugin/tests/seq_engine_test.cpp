// SPDX-License-Identifier: GPL-3.0-or-later
// Sequencer + synth engine, wired the way the processor does it: notes go to
// the engine at their sample offsets and each step applies its parameter locks.
// Checks that steps sound when they should, are silent when they should, and
// that a cutoff lock really changes the sound of its step.
#include "Engine.h"
#include "Sequencer.h"
#include "FactoryPrograms.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace p800;

static std::vector<float> render (const SeqData& d, int steps)
{
    const double sr = 48000.0;
    const int block = 256;
    Engine engine;
    engine.prepare (sr, block);
    SeqPlayer player;
    player.prepare (sr);
    player.setData (d);

    // A plain bright patch with an instant release, so silence really is silence.
    int base[numParams];
    for (int i = 0; i < numParams; ++i) base[i] = factory::programs[0].values[i];
    auto paramsWith = [&] (const int16_t* locks)
    {
        int v[numParams];
        for (int i = 0; i < numParams; ++i) v[i] = locks != nullptr && locks[i] >= 0 ? locks[i] : base[i];
        Poly800Params p;
        poly800_params_default (&p);
        fillCoreParams (p, v);
        p.deg1_attack = 0; p.deg1_release = 0; p.deg1_sustain = 31; p.deg1_decay = 31; p.deg1_breakpoint = 31; p.deg1_slope = 31;
        p.deg2_attack = 0; p.deg2_release = 0;
        p.vcf_env_intensity = 0; p.vcf_resonance = 0; p.mg_vcf = 0; p.mg_dco = 0; p.chorus_on = 0;
        p.master_gain = 0.05f;
        return p;
    };
    engine.setParams (paramsWith (nullptr));

    const int total = 6000 * steps;
    std::vector<float> out ((size_t) total);
    std::vector<float> r ((size_t) block);
    SeqEvent ev[SeqPlayer::maxEvents];
    SeqTransport t; // internal clock
    for (int s = 0; s < total; s += block)
    {
        const int n = std::min (block, total - s);
        const int c = player.process (n, t, true, ev);
        int pos = 0;
        for (int i = 0; i <= c; ++i)
        {
            const int until = i < c ? ev[i].offset : n;
            if (until > pos) { engine.render (out.data() + s + pos, r.data(), until - pos); pos = until; }
            if (i == c) break;
            if (ev[i].type == SeqEvent::noteOn) engine.noteOn (ev[i].note, ev[i].velocity);
            else if (ev[i].type == SeqEvent::noteOff) engine.noteOff (ev[i].note);
            else engine.setParams (paramsWith (player.activeLocks()));
        }
    }
    return out;
}

static double rms (const std::vector<float>& x, int from, int to)
{
    double a = 0;
    for (int i = from; i < to; ++i) a += x[(size_t) i] * x[(size_t) i];
    return std::sqrt (a / std::max (1, to - from));
}

static double brightness (const std::vector<float>& x, int from, int to)
{
    double a = 0, d = 0;
    for (int i = from + 1; i < to; ++i) { a += x[(size_t) i] * x[(size_t) i]; const double df = x[(size_t) i] - x[(size_t) i - 1]; d += df * df; }
    return a > 0 ? std::sqrt (d / a) : 0.0;
}

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s %s\n", ok ? "ok  " : "FAIL", what); fails += ok ? 0 : 1; };

    { // a chord on step 1 only (gate 50%): sound in step 1, silence by step 3
        SeqData d; d.settings.hostSync = false; d.settings.gate = 50;
        for (int n : { 48, 52, 55, 59 }) d.addNote (0, n);
        const auto x = render (d, 4);
        const double during = rms (x, 500, 2900), after = rms (x, 12000 + 1000, 18000);
        std::printf ("     step 1 rms %.4f, step 3 rms %.6f\n", during, after);
        check (during > 0.01, "the chord sounds on its step");
        check (after < during * 0.02, "and is silent once its gate has ended");
    }
    { // same note on steps 1 and 5; step 5 locks the cutoff wide open
        SeqData d; d.settings.hostSync = false; d.settings.gate = 90;
        d.addNote (0, 48); d.addNote (4, 48);
        d.setLock (4, 18 /* P41 */, 99);
        d.setLock (0, 18 /* P41 */, 15);
        const auto x = render (d, 8);
        const double dark = brightness (x, 1500, 5000), open = brightness (x, 24000 + 1500, 24000 + 5000);
        std::printf ("     brightness step 1 (cutoff 15) %.4f, step 5 (cutoff 99) %.4f\n", dark, open);
        check (open > dark * 2.0, "a cutoff lock opens the filter on its step only");
    }

    std::printf (fails ? "\n%d failure(s)\n" : "\nall sequencer + engine checks pass\n", fails);
    return fails ? 1 : 0;
}
