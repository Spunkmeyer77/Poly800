// SPDX-License-Identifier: GPL-3.0-or-later
// Key assign: POLY or CHORD (chord memory), with HOLD.
// Chord memory is ported from Polytwelve (Ticco Ross): hold two or more keys and
// switch to CHORD (or press MEMORY) to store them; each key then plays the stored
// chord transposed, with last-note priority. JUCE-free so it can be unit tested.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

namespace p800
{
class KeyLayer
{
public:
    static constexpr int maxChord = 16; // EX-800: up to 16 voices

    struct Sink
    {
        virtual ~Sink() = default;
        virtual void keyOn (int note, int velocity) = 0;
        virtual void keyOff (int note) = 0;
    };

    enum Mode { poly = 0, chordMode = 1 };

    KeyLayer() { const int minor[] = { 0, 3, 7 }; setChord (minor, 3, 60); }

    // ---- settings (audio thread, once per block) ------------------------------------
    void setMode (int m)        { mode = m == chordMode ? chordMode : poly; }
    void setHold (bool h)       { hold = h; }
    void setVoiceLimit (int n)  { voiceLimit = std::clamp (n, 1, maxChord); }

    // ---- chord memory (thread safe) -------------------------------------------------
    void setChord (const int* intervals, int count, int root)
    {
        count = std::clamp (count, 1, maxChord);
        for (int i = 0; i < maxChord; ++i) chordInt[(size_t) i].store (i < count ? intervals[i] : 0, std::memory_order_relaxed);
        chordRoot.store (std::clamp (root, 0, 127), std::memory_order_relaxed);
        chordSize.store (count, std::memory_order_release);
        chordVersion.fetch_add (1, std::memory_order_release);
    }
    int getChord (int* intervalsOut, int& rootOut) const
    {
        const int n = std::clamp (chordSize.load (std::memory_order_acquire), 1, maxChord);
        for (int i = 0; i < n; ++i) intervalsOut[i] = chordInt[(size_t) i].load (std::memory_order_relaxed);
        rootOut = chordRoot.load (std::memory_order_relaxed);
        return n;
    }
    int  getChordVersion() const { return chordVersion.load (std::memory_order_acquire); }
    void requestChordCapture()   { captureRequest.store (true, std::memory_order_release); }

    // ---- MIDI (audio thread) --------------------------------------------------------
    // Call update() once per block after the settings, before the block's MIDI.
    void update (Sink& sink)
    {
        if (captureRequest.exchange (false, std::memory_order_acq_rel))
        {
            captureChord (1);
            if (mode == chordMode) { releaseAll (sink); replay (sink); }
        }

        // HOLD released: drop latched keys that are no longer physically down.
        if (lastHold && ! hold)
            for (int n = 0; n < 128; ++n)
                if (logical[(size_t) n] && ! physDown[(size_t) n]) { logical[(size_t) n] = false; logicalOff (sink, n); }
        lastHold = hold;

        if (mode != lastMode || voiceLimit != lastLimit)
        {
            // Switching to CHORD with two or more keys held memorises them (as in Polytwelve).
            if (mode == chordMode && lastMode != chordMode)
                captureChord (2);
            releaseAll (sink);
            lastMode = mode;
            lastLimit = voiceLimit;
            replay (sink);
        }
    }

    void noteOn (Sink& sink, int note, int velocity)
    {
        note = std::clamp (note, 0, 127);
        lastVelocity = velocity;
        physDown[(size_t) note] = true;
        if (! logical[(size_t) note]) { logical[(size_t) note] = true; logicalOn (sink, note); }
        else if (mode == poly) { sink.keyOff (note); sink.keyOn (note, velocity); } // re-strike
        else logicalOn (sink, note);
    }

    void noteOff (Sink& sink, int note)
    {
        note = std::clamp (note, 0, 127);
        physDown[(size_t) note] = false;
        if (hold) return;
        if (logical[(size_t) note]) { logical[(size_t) note] = false; logicalOff (sink, note); }
    }

    void allNotesOff (Sink& sink)
    {
        physDown.fill (false);
        logical.fill (false);
        releaseAll (sink);
    }

private:
    int mode = poly, lastMode = poly;
    bool hold = false, lastHold = false;
    int voiceLimit = maxChord, lastLimit = maxChord;
    int lastVelocity = 100;

    std::array<bool, 128> physDown {}, logical {}, sounding {};
    std::vector<int> monoStack; // CHORD: last-note priority
    int monoNote = -1;

    std::array<std::atomic<int>, maxChord> chordInt {};
    std::atomic<int> chordSize { 3 }, chordRoot { 60 }, chordVersion { 0 };
    std::atomic<bool> captureRequest { false };

    // Store the currently held keys (lowest first) if at least minKeys are down.
    void captureChord (int minKeys)
    {
        int held[maxChord]; int n = 0;
        for (int k = 0; k < 128 && n < maxChord; ++k) if (logical[(size_t) k]) held[n++] = k;
        if (n < minKeys || n == 0) return;
        int iv[maxChord];
        for (int i = 0; i < n; ++i) iv[i] = held[i] - held[0];
        setChord (iv, n, held[0]);
    }

    void soundOn (Sink& sink, int note)
    {
        if (sounding[(size_t) note]) sink.keyOff (note);
        sounding[(size_t) note] = true;
        sink.keyOn (note, lastVelocity);
    }
    void soundOff (Sink& sink, int note)
    {
        if (! sounding[(size_t) note]) return;
        sounding[(size_t) note] = false;
        sink.keyOff (note);
    }
    void releaseAll (Sink& sink)
    {
        for (int n = 0; n < 128; ++n) soundOff (sink, n);
        monoStack.clear();
        monoNote = -1;
    }
    void replay (Sink& sink)
    {
        for (int n = 0; n < 128; ++n) if (logical[(size_t) n]) logicalOn (sink, n);
    }

    void logicalOn (Sink& sink, int note)
    {
        if (mode == poly) { soundOn (sink, note); return; }
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        monoStack.push_back (note);
        triggerChord (sink, note);
    }

    void logicalOff (Sink& sink, int note)
    {
        if (mode == poly) { soundOff (sink, note); return; }
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        if (note != monoNote) return;
        if (! monoStack.empty()) triggerChord (sink, monoStack.back());
        else { for (int n = 0; n < 128; ++n) soundOff (sink, n); monoNote = -1; }
    }

    void triggerChord (Sink& sink, int note)
    {
        monoNote = note;
        int iv[maxChord], root = 0;
        const int n = std::min (getChord (iv, root), voiceLimit);
        std::array<bool, 128> want {};
        for (int i = 0; i < n; ++i) want[(size_t) std::clamp (note + iv[i], 0, 127)] = true;
        for (int k = 0; k < 128; ++k) if (sounding[(size_t) k] && ! want[(size_t) k]) soundOff (sink, k);
        for (int k = 0; k < 128; ++k) if (want[(size_t) k]) soundOn (sink, k); // retrigger all, like Polytwelve
    }
};
} // namespace p800
