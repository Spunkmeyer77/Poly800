// SPDX-License-Identifier: GPL-3.0-or-later
// Stock Poly-800 sound-program parameters, as exposed to the host.
#pragma once

#include "poly800_core.h"

namespace p800
{
struct ParamSpec
{
    int number;        // original Poly-800 parameter number
    const char* name;  // short panel label
    int minValue;
    int maxValue;
    int defaultValue;
};

// Order matches the factory CSV columns.
inline constexpr ParamSpec params[] = {
    { 11, "DCO1 Octave", 1, 3, 2 },     { 12, "DCO1 Wave", 1, 2, 2 },
    { 13, "DCO1 16'", 0, 1, 1 },        { 14, "DCO1 8'", 0, 1, 1 },
    { 15, "DCO1 4'", 0, 1, 1 },         { 16, "DCO1 2'", 0, 1, 1 },
    { 17, "DCO1 Level", 0, 31, 31 },    { 18, "DCO Mode", 1, 2, 1 },
    { 21, "DCO2 Octave", 1, 3, 2 },     { 22, "DCO2 Wave", 1, 2, 2 },
    { 23, "DCO2 16'", 0, 1, 1 },        { 24, "DCO2 8'", 0, 1, 1 },
    { 25, "DCO2 4'", 0, 1, 1 },         { 26, "DCO2 2'", 0, 1, 1 },
    { 27, "DCO2 Level", 0, 31, 31 },    { 31, "DCO2 Interval", 0, 12, 0 },
    { 32, "DCO2 Detune", 0, 3, 0 },     { 33, "Noise Level", 0, 15, 0 },
    { 41, "VCF Cutoff", 0, 99, 60 },    { 42, "VCF Resonance", 0, 15, 0 },
    { 43, "VCF Kbd Track", 0, 2, 1 },   { 44, "VCF EG Polarity", 1, 2, 2 },
    { 45, "VCF EG Intensity", 0, 15, 8 }, { 46, "VCF Trigger", 1, 2, 2 },
    { 48, "Chorus", 0, 1, 1 },
    { 51, "DEG1 Attack", 0, 31, 0 },    { 52, "DEG1 Decay", 0, 31, 31 },
    { 53, "DEG1 Break Pt", 0, 31, 31 }, { 54, "DEG1 Slope", 0, 31, 31 },
    { 55, "DEG1 Sustain", 0, 31, 31 },  { 56, "DEG1 Release", 0, 31, 10 },
    { 61, "DEG2 Attack", 0, 31, 0 },    { 62, "DEG2 Decay", 0, 31, 31 },
    { 63, "DEG2 Break Pt", 0, 31, 31 }, { 64, "DEG2 Slope", 0, 31, 31 },
    { 65, "DEG2 Sustain", 0, 31, 31 },  { 66, "DEG2 Release", 0, 31, 10 },
    { 71, "DEG3 Attack", 0, 31, 0 },    { 72, "DEG3 Decay", 0, 31, 20 },
    { 73, "DEG3 Break Pt", 0, 31, 15 }, { 74, "DEG3 Slope", 0, 31, 20 },
    { 75, "DEG3 Sustain", 0, 31, 10 },  { 76, "DEG3 Release", 0, 31, 10 },
    { 81, "MG Frequency", 0, 15, 6 },   { 82, "MG Delay", 0, 15, 0 },
    { 83, "MG DCO", 0, 15, 0 },         { 84, "MG VCF", 0, 15, 0 },
};
inline constexpr int numParams = (int) (sizeof (params) / sizeof (params[0]));

// The core's output runs hot (a 4-note chord on the loudest factory program
// peaks around 6x full scale at its own default gain). The Volume parameter
// (0..1) maps to core master_gain = volume * volumeToGain; the default puts
// that worst-case chord at about 0 dBFS and leaves ~9 dB of boost.
inline constexpr float volumeDefault = 0.35f;
inline constexpr float volumeToGain = 0.15f;

// values[i] corresponds to params[i].
inline void fillCoreParams (Poly800Params& p, const int* v)
{
    int i = 0;
    p.dco1_octave = v[i++]; p.dco1_waveform = v[i++];
    p.dco1_h16 = v[i++]; p.dco1_h8 = v[i++]; p.dco1_h4 = v[i++]; p.dco1_h2 = v[i++];
    p.dco1_level = (float) v[i++]; p.dco_mode = v[i++];
    p.dco2_octave = v[i++]; p.dco2_waveform = v[i++];
    p.dco2_h16 = v[i++]; p.dco2_h8 = v[i++]; p.dco2_h4 = v[i++]; p.dco2_h2 = v[i++];
    p.dco2_level = (float) v[i++]; p.dco2_interval = v[i++];
    p.dco2_detune = (float) v[i++]; p.noise_level = (float) v[i++];
    p.vcf_cutoff = (float) v[i++]; p.vcf_resonance = (float) v[i++];
    p.vcf_keytrack = v[i++]; p.vcf_polarity = v[i++];
    p.vcf_env_intensity = (float) v[i++]; p.vcf_trigger = v[i++]; p.chorus_on = v[i++];
    p.deg1_attack = (float) v[i++]; p.deg1_decay = (float) v[i++]; p.deg1_breakpoint = (float) v[i++];
    p.deg1_slope = (float) v[i++]; p.deg1_sustain = (float) v[i++]; p.deg1_release = (float) v[i++];
    p.deg2_attack = (float) v[i++]; p.deg2_decay = (float) v[i++]; p.deg2_breakpoint = (float) v[i++];
    p.deg2_slope = (float) v[i++]; p.deg2_sustain = (float) v[i++]; p.deg2_release = (float) v[i++];
    p.deg3_attack = (float) v[i++]; p.deg3_decay = (float) v[i++]; p.deg3_breakpoint = (float) v[i++];
    p.deg3_slope = (float) v[i++]; p.deg3_sustain = (float) v[i++]; p.deg3_release = (float) v[i++];
    p.mg_frequency = (float) v[i++]; p.mg_delay = (float) v[i++];
    p.mg_dco = (float) v[i++]; p.mg_vcf = (float) v[i++];
}
} // namespace p800
