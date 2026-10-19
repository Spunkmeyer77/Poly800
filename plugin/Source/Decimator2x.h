// SPDX-License-Identifier: GPL-3.0-or-later
// 2:1 FIR decimator. The ISLA core is validated on its >=88 kHz filter path,
// so at 44.1/48 kHz hosts the plugin runs the core at 2x and decimates here.
#pragma once

#include <array>
#include <cmath>

namespace p800
{
class Decimator2x
{
public:
    static constexpr int numTaps = 95;

    Decimator2x()
    {
        // Windowed-sinc lowpass at 0.235 of the input rate (Blackman window).
        constexpr double pi = 3.14159265358979323846;
        const double fc = 0.235;
        const int mid = numTaps / 2;
        double sum = 0.0;
        for (int i = 0; i < numTaps; ++i)
        {
            const int n = i - mid;
            const double sinc = n == 0 ? 2.0 * fc : std::sin (2.0 * pi * fc * n) / (pi * n);
            const double w = 0.42 - 0.5 * std::cos (2.0 * pi * i / (numTaps - 1))
                           + 0.08 * std::cos (4.0 * pi * i / (numTaps - 1));
            taps[(size_t) i] = sinc * w;
            sum += taps[(size_t) i];
        }
        for (auto& t : taps)
            t /= sum;
        reset();
    }

    void reset()
    {
        history.fill (0.0f);
        pos = 0;
    }

    // Consumes 2 * numOut input samples, writes numOut output samples.
    void process (const float* in, float* out, int numOut)
    {
        for (int o = 0; o < numOut; ++o)
        {
            push (in[2 * o]);
            push (in[2 * o + 1]);
            double acc = 0.0;
            int idx = pos;
            for (int t = 0; t < numTaps; ++t)
            {
                idx = idx == 0 ? numTaps - 1 : idx - 1;
                acc += taps[(size_t) t] * history[(size_t) idx];
            }
            out[o] = (float) acc;
        }
    }

private:
    void push (float x)
    {
        history[(size_t) pos] = x;
        pos = pos + 1 == numTaps ? 0 : pos + 1;
    }

    std::array<double, numTaps> taps {};
    std::array<float, numTaps> history {};
    int pos = 0;
};
} // namespace p800
