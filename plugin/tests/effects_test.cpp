// SPDX-License-Identifier: GPL-3.0-or-later
// Effects test: Chorus, Phaser and Ensemble (ported from Polytwelve) each make
// a finite, sane-level stereo signal from a factory program, P48 off bypasses
// them, and the speed control changes the result.
#include "Engine.h"
#include "ParamSpec.h"
#include "FactoryPrograms.h"
#include <cmath>
#include <cstdio>
#include <vector>

struct Out { bool finite; double peak, sideRatio, rms; };

static Out render (int type, float speed, bool on)
{
    p800::Engine e;
    e.prepare (48000, 512);
    e.setEffect (type, speed);
    Poly800Params p;
    poly800_params_default (&p);
    p800::fillCoreParams (p, factory::programs[0].values); // 11 Strings
    p.chorus_on = on ? 1 : 0;
    p.master_gain = p800::volumeDefault * p800::volumeToGain;
    e.setParams (p);
    for (int n : { 48, 60, 64, 67 }) e.noteOn (n, 100);
    const int total = 96000;
    std::vector<float> l ((size_t) total), r ((size_t) total);
    e.render (l.data(), r.data(), total);
    Out o { true, 0, 0, 0 };
    double mid = 0, side = 0;
    for (int i = total / 2; i < total; ++i)
    {
        const float a = l[(size_t) i], b = r[(size_t) i];
        if (! std::isfinite (a) || ! std::isfinite (b)) o.finite = false;
        o.peak = std::fmax (o.peak, std::fmax (std::fabs (a), std::fabs (b)));
        mid += 0.25 * (a + b) * (a + b);
        side += 0.25 * (a - b) * (a - b);
    }
    o.sideRatio = side / std::fmax (1e-12, mid);
    o.rms = std::sqrt (mid / (total / 2));
    return o;
}

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s %s\n", ok ? "ok  " : "FAIL", what); fails += ok ? 0 : 1; };

    const Out off = render (0, 0.4f, false);
    std::printf ("off: peak %.3f, stereo %.5f\n", off.peak, off.sideRatio);
    check (off.finite && off.sideRatio < 1e-9, "P48 off: dry mono signal");

    const char* names[3] = { "Chorus", "Phaser", "Ensemble" };
    for (int t = 0; t < 3; ++t)
    {
        const Out o = render (t, 0.4f, true);
        const Out fast = render (t, 0.95f, true);
        std::printf ("%s: peak %.3f, rms %.4f (dry %.4f), stereo %.4f\n", names[t], o.peak, o.rms, off.rms, o.sideRatio);
        char what[96];
        std::snprintf (what, sizeof what, "%s: finite, under full scale, level within 6 dB of dry", names[t]);
        check (o.finite && o.peak < 1.25 && o.rms > off.rms * 0.5 && o.rms < off.rms * 2.0, what);
        std::snprintf (what, sizeof what, "%s: makes a stereo image", names[t]);
        check (o.sideRatio > 1e-4, what);
        std::snprintf (what, sizeof what, "%s: speed/intensity control changes the sound", names[t]);
        check (std::fabs (fast.sideRatio - o.sideRatio) > 1e-5 || std::fabs (fast.rms - o.rms) > 1e-5, what);
    }

    std::printf (fails ? "\n%d failure(s)\n" : "\nall effects checks pass\n", fails);
    return fails ? 1 : 0;
}
