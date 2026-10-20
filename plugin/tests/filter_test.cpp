// SPDX-License-Identifier: GPL-3.0-or-later
// VCF and noise regression test: P41 Cutoff must sweep the filter at every
// keyboard tracking (P43) and EG intensity (P45) setting, tracking must make
// higher notes brighter, and P33 Noise must be clearly audible.
// (Guards the 0.3.0 tracking fix and the 0.4.0 noise level fix.)
#include "Engine.h"
#include <cmath>
#include <cstdio>
#include <vector>

// Brightness: RMS of the first difference over RMS. Higher = brighter.
static double brightness (int cutoff, int egInt, int kbd, int note)
{
    p800::Engine e;
    e.prepare (48000, 256);
    Poly800Params p;
    poly800_params_default (&p);
    p.dco_mode = 1; p.dco1_waveform = 2;
    p.dco1_h16 = p.dco1_h8 = p.dco1_h4 = p.dco1_h2 = 1; p.dco1_level = 31;
    p.noise_level = 0; p.vcf_cutoff = (float) cutoff; p.vcf_resonance = 0;
    p.vcf_keytrack = kbd; p.vcf_polarity = 2; p.vcf_env_intensity = (float) egInt; p.chorus_on = 0;
    p.deg3_attack = 0; p.deg3_decay = 31; p.deg3_breakpoint = 31; p.deg3_slope = 31; p.deg3_sustain = 20;
    p.deg1_attack = 0; p.deg1_decay = 31; p.deg1_breakpoint = 31; p.deg1_slope = 31; p.deg1_sustain = 31;
    p.mg_vcf = 0; p.mg_dco = 0; p.master_gain = 0.05f;
    e.setParams (p);
    e.noteOn (note, 100);
    const int n = 48000;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    e.render (l.data(), r.data(), n);
    double a = 0, d = 0;
    for (int i = n / 2; i < n; ++i)
    {
        a += l[(size_t) i] * l[(size_t) i];
        const double x = l[(size_t) i] - l[(size_t) i - 1];
        d += x * x;
    }
    return a > 0 ? std::sqrt (d / a) : 0.0;
}

// RMS of the steady part of a note: one full-level DCO 1 and/or noise.
static double level (int dcoLevel, int noise)
{
    p800::Engine e;
    e.prepare (48000, 256);
    Poly800Params p;
    poly800_params_default (&p);
    p.dco_mode = 1; p.dco1_waveform = 2;
    p.dco1_h16 = p.dco1_h8 = p.dco1_h4 = p.dco1_h2 = 1; p.dco1_level = (float) dcoLevel;
    p.noise_level = (float) noise; p.vcf_cutoff = 99; p.vcf_resonance = 0; p.vcf_keytrack = 0;
    p.vcf_env_intensity = 0; p.chorus_on = 0;
    p.deg1_attack = 0; p.deg1_decay = 31; p.deg1_breakpoint = 31; p.deg1_slope = 31; p.deg1_sustain = 31;
    p.deg3_attack = 0; p.deg3_decay = 31; p.deg3_breakpoint = 31; p.deg3_slope = 31; p.deg3_sustain = 31;
    p.mg_vcf = 0; p.mg_dco = 0; p.master_gain = 0.05f;
    e.setParams (p);
    e.noteOn (60, 100);
    const int n = 48000;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    e.render (l.data(), r.data(), n);
    double a = 0;
    for (int i = n / 2; i < n; ++i) a += l[(size_t) i] * l[(size_t) i];
    return std::sqrt (a / (n / 2));
}

int main()
{
    int fails = 0;
    for (int kbd : { 0, 1, 2 })
        for (int eg : { 0, 8, 15 })
        {
            double prev = -1.0;
            bool rising = true;
            for (int c : { 0, 25, 50, 75, 99 })
            {
                const double b = brightness (c, eg, kbd, 60);
                rising = rising && b > prev;
                prev = b;
            }
            const double range = brightness (99, eg, kbd, 60) - brightness (10, eg, kbd, 60);
            const bool ok = rising && range > 0.03;
            std::printf ("%s KBD %d, EG INT %2d: cutoff sweeps the filter (range %.3f)\n", ok ? "ok  " : "FAIL", kbd, eg, range);
            fails += ok ? 0 : 1;
        }

    for (int kbd : { 1, 2 })
    {
        const double low = brightness (50, 0, kbd, 36), high = brightness (50, 0, kbd, 84);
        const bool ok = high > low * 3.0;
        std::printf ("%s KBD %d: high notes open the filter (C1 %.3f, C5 %.3f)\n", ok ? "ok  " : "FAIL", kbd, low, high);
        fails += ok ? 0 : 1;
    }
    {
        const double half = brightness (50, 0, 1, 84), full = brightness (50, 0, 2, 84);
        const bool ok = full > half;
        std::printf ("%s full tracking follows the keyboard more than half (%.3f > %.3f)\n", ok ? "ok  " : "FAIL", full, half);
        fails += ok ? 0 : 1;
    }

    {
        const double voice = level (31, 0), noise15 = level (0, 15), noise5 = level (0, 5), both = level (31, 15);
        bool ok = noise15 > voice * 0.5;
        std::printf ("%s noise 15 is about as loud as one voice (noise %.4f, voice %.4f)\n", ok ? "ok  " : "FAIL", noise15, voice);
        fails += ok ? 0 : 1;
        ok = both > voice * 1.15;
        std::printf ("%s noise is audible over a note (voice %.4f, voice + noise %.4f)\n", ok ? "ok  " : "FAIL", voice, both);
        fails += ok ? 0 : 1;
        ok = noise15 > noise5 * 2.0 && level (0, 0) < 1e-6;
        std::printf ("%s P33 scales the noise (5: %.4f, 15: %.4f) and 0 is silent\n", ok ? "ok  " : "FAIL", noise5, noise15);
        fails += ok ? 0 : 1;
    }

    std::printf (fails ? "\n%d failure(s)\n" : "\nall filter and noise checks pass\n", fails);
    return fails ? 1 : 0;
}
