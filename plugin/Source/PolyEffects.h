// SPDX-License-Identifier: GPL-3.0-or-later
// Chorus and Ensemble, ported unchanged from the Polytwelve synth (Ticco Ross,
// Source/dsp/PolysixEngine.h, Effects modes 1 and 3). Mono in, stereo out.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace p800
{
namespace ptw
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float triFromPhase (float ph) // phase [0,1) -> triangle [-1,1]
{
    return 1.0f - 4.0f * std::fabs (ph - 0.5f);
}

struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int minSize)
    {
        int n = 1; while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float delaySamples) const // linear interpolation, delay >= 1
    {
        float rp = (float) w - delaySamples;
        while (rp < 0.0f) rp += (float) (mask + 1);
        const int   i0 = (int) rp;
        const float f  = rp - (float) i0;
        const float a  = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }
};
} // namespace ptw

class PolyEffects
{
public:
    enum Mode { chorus = 1, ensemble = 3 }; // Polytwelve's mode numbers

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        dl.prepare ((int) (0.08f * sr) + 8);
        reset();
    }

    void reset()
    {
        dl.clear(); lpL = lpR = 0.0f; lfo = lfoFast = 0.0f;
    }

    // knob: speed for Chorus, intensity for Ensemble (0..1), as in Polytwelve.
    void process (float in, float& L, float& R, int mode, float knob)
    {
        using namespace ptw;
        if (mode != lastMode) { reset(); lastMode = mode; }
        const float invSr = 1.0f / sr;
        const float ms    = sr * 0.001f;
        const float bbdLp = 1.0f - std::exp (-kTwoPi * 9000.0f * invSr); // BBD-ish wet roll-off

        if (mode == chorus)
        {
            const float rate = 0.1f * std::exp2 (knob * 5.3f); // 0.1 .. ~4 Hz
            lfo += rate * invSr; if (lfo >= 1.0f) lfo -= 1.0f;
            const float t = triFromPhase (lfo);
            dl.push (in);
            const float wl = dl.read ((6.0f + 2.5f * t) * ms);
            const float wr = dl.read ((6.0f - 2.5f * t) * ms);
            lpL += (wl - lpL) * bbdLp; lpR += (wr - lpR) * bbdLp;
            L = 0.7f * in + 0.7f * lpL;
            R = 0.7f * in + 0.7f * lpR;
        }
        else if (mode == ensemble)
        {
            lfo     += 0.6f * invSr; if (lfo >= 1.0f) lfo -= 1.0f;
            lfoFast += 6.0f * invSr; if (lfoFast >= 1.0f) lfoFast -= 1.0f;
            dl.push (in);
            const float depth = 0.15f + 0.85f * knob;
            float taps[3];
            for (int i = 0; i < 3; ++i)
            {
                const float off = (float) i / 3.0f;
                const float s   = std::sin (kTwoPi * (lfo + off));
                const float f   = std::sin (kTwoPi * (lfoFast + off));
                taps[i] = dl.read ((10.0f + depth * (2.0f * s + 0.35f * f)) * ms);
            }
            const float wl = taps[0] + 0.5f * taps[1];
            const float wr = taps[2] + 0.5f * taps[1];
            lpL += (wl - lpL) * bbdLp; lpR += (wr - lpR) * bbdLp;
            L = 0.55f * in + 0.5f * lpL;
            R = 0.55f * in + 0.5f * lpR;
        }
        else
        {
            L = R = in;
        }
    }

private:
    float sr = 48000.0f;
    ptw::DelayLine dl;
    float lfo = 0.0f, lfoFast = 0.0f;
    float lpL = 0.0f, lpR = 0.0f;
    int lastMode = -1;
};
} // namespace p800
