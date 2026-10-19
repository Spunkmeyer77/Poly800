// SPDX-License-Identifier: GPL-3.0-or-later
// JUCE-free wrapper around the ISLA core: owns the core, runs it at >=88 kHz
// (2x oversampled at 44.1/48 kHz hosts) and overwrites the output buffers.
#pragma once

#include "poly800_core.h"
#include "Decimator2x.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace p800
{
class Engine
{
public:
    ~Engine() { destroy(); }

    void prepare (double hostRate, int maxBlock)
    {
        destroy();
        factor = hostRate < 88000.0 ? 2 : 1;
        core = poly800_core_create (hostRate * factor);
        setPerformance (perfBend, perfDco, perfVcf);
        maxFrames = std::max (1, maxBlock);
        workL.assign ((size_t) (maxFrames * factor), 0.0f);
        workR.assign ((size_t) (maxFrames * factor), 0.0f);
        decL.reset();
        decR.reset();
        haveParams = false;
    }

    bool isPrepared() const { return core != nullptr; }
    int oversampling() const { return factor; }

    void setParams (const Poly800Params& p)
    {
        if (core == nullptr)
            return;
        if (! haveParams || std::memcmp (&p, &last, sizeof (p)) != 0)
        {
            poly800_core_set_params (core, &p);
            last = p;
            haveParams = true;
        }
    }

    void noteOn (int note, int velocity) { if (core) poly800_core_note_on (core, (uint8_t) note, (uint8_t) velocity); }
    void noteOff (int note)              { if (core) poly800_core_note_off (core, (uint8_t) note); }
    void allNotesOff()                   { if (core) poly800_core_all_notes_off (core); }

    // Joystick: bend in semitones, DCO/VCF mod amounts 0..1 (stay set across prepare()).
    void setPerformance (float bendSemitones, float dcoMod, float vcfMod)
    {
        perfBend = bendSemitones; perfDco = dcoMod; perfVcf = vcfMod;
        if (core)
        {
            poly800_core_set_pitch_bend (core, perfBend);
            poly800_core_set_joystick_mod (core, perfDco, perfVcf);
        }
    }

    // Overwrites left/right with numFrames of output. Splits long blocks.
    void render (float* left, float* right, int numFrames)
    {
        while (numFrames > 0)
        {
            const int n = std::min (numFrames, maxFrames);
            renderChunk (left, right, n);
            left += n;
            right += n;
            numFrames -= n;
        }
    }

private:
    void renderChunk (float* left, float* right, int n)
    {
        if (core == nullptr)
        {
            std::fill (left, left + n, 0.0f);
            std::fill (right, right + n, 0.0f);
            return;
        }
        // The core accumulates (+=) into its buffers, so they start cleared.
        const int inner = n * factor;
        std::fill (workL.begin(), workL.begin() + inner, 0.0f);
        std::fill (workR.begin(), workR.begin() + inner, 0.0f);
        poly800_core_render (core, workL.data(), workR.data(), (uint32_t) inner);

        if (factor == 1)
        {
            std::copy (workL.begin(), workL.begin() + n, left);
            std::copy (workR.begin(), workR.begin() + n, right);
        }
        else
        {
            decL.process (workL.data(), left, n);
            decR.process (workR.data(), right, n);
        }
    }

    void destroy()
    {
        if (core != nullptr)
            poly800_core_destroy (core);
        core = nullptr;
    }

    Poly800Core* core = nullptr;
    int factor = 1;
    int maxFrames = 1;
    std::vector<float> workL, workR;
    Decimator2x decL, decR;
    Poly800Params last {};
    bool haveParams = false;
    float perfBend = 0.0f, perfDco = 0.0f, perfVcf = 0.0f;
};
} // namespace p800
