// SPDX-License-Identifier: GPL-3.0-or-later
// JUCE-free wrapper around the ISLA core: owns the core, runs it at >=88 kHz
// (2x oversampled at 44.1/48 kHz hosts) and overwrites the output buffers.
#pragma once

#include "poly800_core.h"
#include "Decimator2x.h"
#include "PolyEffects.h"
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
        setVoiceCount (voices16);
        setFilterPerVoice (filterPerVoice);
        maxFrames = std::max (1, maxBlock);
        workL.assign ((size_t) (maxFrames * factor), 0.0f);
        workR.assign ((size_t) (maxFrames * factor), 0.0f);
        decL.reset();
        decR.reset();
        fx.prepare ((float) hostRate);
        haveParams = false;
    }

    // Effect type, as in Polytwelve: 0 Chorus, 1 Phaser, 2 Ensemble.
    // speed: Chorus/Phaser rate or Ensemble intensity, 0..1. P48 switches the effect on and off.
    enum EffectType { fxChorus = 0, fxPhaser = 1, fxEnsemble = 2 };
    void setEffect (int type, float speed)
    {
        effectType = std::clamp (type, 0, 2);
        effectSpeed = std::clamp (speed, 0.0f, 1.0f);
    }

    bool isPrepared() const { return core != nullptr; }

    // EX-800: a filter per voice (true) or the original single shared filter. Kept across prepare().
    void setFilterPerVoice (bool perVoice)
    {
        filterPerVoice = perVoice;
        if (core) poly800_core_set_filter_mode (core, perVoice ? 1 : 0);
    }

    // EX-800: 16 voices (8 in DOUBLE mode) instead of 8 (4). Kept across prepare().
    void setVoiceCount (bool sixteen)
    {
        voices16 = sixteen;
        if (core) poly800_core_set_voice_count (core, sixteen ? 1 : 0);
    }
    int oversampling() const { return factor; }

    void setParams (const Poly800Params& in)
    {
        if (core == nullptr)
            return;
        // The core's own chorus is never used; P48 switches the Polytwelve effect.
        effectOn = in.chorus_on != 0;
        Poly800Params p = in;
        p.chorus_on = 0;
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

        // The core's chorus is off, so left == right: run the Polytwelve effect on it.
        if (effectOn)
        {
            const int mode = effectType == fxPhaser ? PolyEffects::phaser
                           : effectType == fxEnsemble ? PolyEffects::ensemble : PolyEffects::chorus;
            for (int i = 0; i < n; ++i)
                fx.process (0.5f * (left[i] + right[i]), left[i], right[i], mode, effectSpeed);
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
    bool voices16 = false;
    bool filterPerVoice = true;
    float perfBend = 0.0f, perfDco = 0.0f, perfVcf = 0.0f;
    PolyEffects fx;
    int effectType = fxChorus;
    float effectSpeed = 0.4f;
    bool effectOn = false;
};
} // namespace p800
