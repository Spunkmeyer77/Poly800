// SPDX-License-Identifier: GPL-3.0-or-later
// Renders a chord through every factory program, at 44.1, 48 and 96 kHz, using
// the same parameter mapping and Engine the plugin uses. Checks the output is
// finite, audible, sane in level, and consistent across host sample rates.
#include "ParamSpec.h"
#include "FactoryPrograms.h"
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

struct Result { bool finite; double peak; double rms; };

static Result renderProgram (const factory::Program& prog, double sr)
{
    // An odd, host-like block size that is not a power of two.
    const int block = 437;
    p800::Engine engine;
    engine.prepare (sr, block);

    Poly800Params p;
    poly800_params_default (&p);
    p800::fillCoreParams (p, prog.values);
    p.master_gain = p800::volumeDefault * p800::volumeToGain; // plugin default Volume
    engine.setParams (p);
    engine.noteOn (48, 100);
    engine.noteOn (60, 100);
    engine.noteOn (64, 100);
    engine.noteOn (67, 100);

    const int total = (int) (sr * 3.0);
    std::vector<float> l ((size_t) block), r ((size_t) block);
    Result res { true, 0.0, 0.0 };
    double sumSq = 0.0;
    for (int done = 0; done < total; done += block)
    {
        if (done >= total * 2 / 3 && done - block < total * 2 / 3)
            engine.allNotesOff();
        engine.render (l.data(), r.data(), block);
        for (int k = 0; k < block; ++k)
        {
            if (! std::isfinite (l[(size_t) k]) || ! std::isfinite (r[(size_t) k]))
                res.finite = false;
            res.peak = std::fmax (res.peak, std::fmax (std::fabs (l[(size_t) k]), std::fabs (r[(size_t) k])));
            sumSq += 0.5 * (l[(size_t) k] * l[(size_t) k] + r[(size_t) k] * r[(size_t) k]);
        }
    }
    res.rms = std::sqrt (sumSq / total);
    return res;
}

int main()
{
    static_assert (factory::numParams == p800::numParams, "CSV/ParamSpec mismatch");
    for (int i = 0; i < p800::numParams; ++i)
        if (factory::paramNumbers[i] != p800::params[i].number)
        {
            std::printf ("column %d: CSV has P%d, ParamSpec has P%d\n", i, factory::paramNumbers[i], p800::params[i].number);
            return 1;
        }

    int failures = 0;
    double worstRatio = 1.0;
    std::printf ("prog name                    peak@44.1 peak@48  peak@96  rms@48\n");
    for (const auto& prog : factory::programs)
    {
        for (int i = 0; i < p800::numParams; ++i)
        {
            const auto& s = p800::params[i];
            if (prog.values[i] < s.minValue || prog.values[i] > s.maxValue)
            {
                std::printf ("P%d program %d: value %d outside %d..%d\n", s.number, prog.number, prog.values[i], s.minValue, s.maxValue);
                ++failures;
            }
        }

        const Result a = renderProgram (prog, 44100.0);
        const Result b = renderProgram (prog, 48000.0);
        const Result c = renderProgram (prog, 96000.0);

        bool ok = true;
        for (const Result& x : { a, b, c })
            ok = ok && x.finite && x.peak > 1e-3 && x.peak < 1.25;

        // The 44.1/48 kHz (oversampled) and 96 kHz renders should match in level.
        const double ratio = std::fmax (b.rms, c.rms) / std::fmax (1e-9, std::fmin (b.rms, c.rms));
        worstRatio = std::fmax (worstRatio, ratio);
        if (ratio > 1.5)
            ok = false;

        if (! ok)
            ++failures;
        std::printf ("%s %2d %-22s %7.3f  %7.3f  %7.3f  %.4f\n", ok ? "ok  " : "FAIL", prog.number, prog.name, a.peak, b.peak, c.peak, b.rms);
    }

    std::printf ("\nworst 48k/96k level ratio: %.2f\n", worstRatio);
    if (failures)
        std::printf ("%d failure(s)\n", failures);
    else
        std::printf ("all %d programs render at 44.1, 48 and 96 kHz\n", factory::numPrograms);
    return failures ? 1 : 0;
}
