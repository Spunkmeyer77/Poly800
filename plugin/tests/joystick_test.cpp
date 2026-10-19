// SPDX-License-Identifier: GPL-3.0-or-later
// Checks the joystick inputs through the plugin Engine:
//  - X (bend) moves pitch by exactly the requested semitones,
//  - Y up adds pitch wobble (MG -> DCO), Y down adds filter movement (MG -> VCF),
//  - with the joystick centred, nothing changes.
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

static Poly800Params testPatch()
{
    Poly800Params p;
    poly800_params_default (&p);
    p.dco_mode = 1;
    p.dco1_octave = 2; p.dco1_waveform = 1;
    p.dco1_h16 = 0; p.dco1_h8 = 1; p.dco1_h4 = 0; p.dco1_h2 = 0;
    p.dco1_level = 31;
    p.noise_level = 0;
    p.vcf_cutoff = 60; p.vcf_resonance = 0; p.vcf_keytrack = 0;
    p.vcf_env_intensity = 0; p.chorus_on = 0;
    p.deg1_attack = 0; p.deg1_decay = 31; p.deg1_breakpoint = 31;
    p.deg1_slope = 31; p.deg1_sustain = 31; p.deg1_release = 0;
    p.mg_frequency = 10; p.mg_delay = 0; p.mg_dco = 0; p.mg_vcf = 0;
    p.master_gain = 0.05f;
    return p;
}

// Renders 2 s, returns mean frequency (rising zero crossings) over the last 1 s,
// the spread of per-50ms frequencies (pitch wobble) and of per-50ms RMS (filter wobble).
struct Measure { double freq, freqSpread, rmsSpread; };

static Measure run (double sr, float bend, float dco, float vcf)
{
    p800::Engine e;
    e.prepare (sr, 256);
    e.setParams (testPatch());
    e.setPerformance (bend, dco, vcf);
    e.noteOn (57, 100); // A3

    const int total = (int) (2.0 * sr), seg = (int) (0.05 * sr);
    std::vector<float> l ((size_t) total), r ((size_t) total);
    e.render (l.data(), r.data(), total);

    // Pitch: length of each individual cycle (rising zero crossing to the next),
    // so vibrato faster than any fixed window is still seen.
    std::vector<double> f, rms;
    int crossings = 0, last = -1;
    for (int k = total / 2 + 1; k < total; ++k)
        if (l[(size_t) k - 1] < 0 && l[(size_t) k] >= 0)
        {
            if (last >= 0) f.push_back (sr / (k - last));
            last = k; ++crossings;
        }
    // Filter: RMS per 20 ms segment.
    const int rseg = (int) (0.02 * sr);
    for (int s = total / 2; s + rseg <= total; s += rseg)
    {
        double sq = 0;
        for (int k = s; k < s + rseg; ++k) sq += l[(size_t) k] * l[(size_t) k];
        rms.push_back (std::sqrt (sq / rseg));
    }
    (void) seg;
    auto spread = [] (const std::vector<double>& v)
    {
        double lo = v[0], hi = v[0];
        for (double x : v) { lo = std::fmin (lo, x); hi = std::fmax (hi, x); }
        return (hi - lo) / std::fmax (1e-9, 0.5 * (hi + lo));
    };
    return { crossings / ((total / 2) / sr), spread (f), spread (rms) };
}

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++fails; };

    for (double sr : { 48000.0, 96000.0 })
    {
        std::printf ("-- %.0f Hz\n", sr);
        const Measure base = run (sr, 0, 0, 0);
        const Measure up12 = run (sr, 12, 0, 0);
        const Measure dn2 = run (sr, -2, 0, 0);
        const Measure dco = run (sr, 0, 1, 0);
        const Measure vcf = run (sr, 0, 0, 1);

        std::printf ("base %.1f Hz, +12 %.1f Hz, -2 %.1f Hz\n", base.freq, up12.freq, dn2.freq);
        std::printf ("pitch spread: base %.4f, joy up %.4f | rms spread: base %.4f, joy down %.4f\n",
                     base.freqSpread, dco.freqSpread, base.rmsSpread, vcf.rmsSpread);

        check (std::fabs (up12.freq / base.freq - 2.0) < 0.01, "bend +12 st doubles the pitch");
        check (std::fabs (dn2.freq / base.freq - std::pow (2.0, -2.0 / 12.0)) < 0.01, "bend -2 st");
        check (base.freqSpread < 0.03 && base.rmsSpread < 0.05, "centred joystick: no modulation");
        check (dco.freqSpread > 0.10, "joystick up: MG wobbles the pitch");
        check (vcf.rmsSpread > 0.10, "joystick down: MG moves the filter");
    }
    std::printf (fails ? "\n%d failure(s)\n" : "\nall joystick checks pass\n", fails);
    return fails ? 1 : 0;
}
