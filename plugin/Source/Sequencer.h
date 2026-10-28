// SPDX-License-Identifier: GPL-3.0-or-later
// EX-800 step sequencer: polyphonic, 16 or 32 steps, 8 patterns, with per-step
// probability, gate and parameter locks (several parameters per step).
// JUCE-free so it can be unit tested; the processor owns one SeqData (edited on
// the message thread) and one SeqPlayer (audio thread) that works on a copy.
#pragma once

#include "ParamSpec.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

namespace p800
{
constexpr int kSeqMaxSteps = 32;
constexpr int kSeqMaxNotes = 8;    // the synth's voice count
constexpr int kSeqPatterns = 8;    // A..H

// ---- what can be parameter-locked ------------------------------------------------------------
// Slots 0..numParams-1 are the chart parameters (index into p800::params); the rest are the other
// assignable controls. Values are integers in each slot's range; kNoLock means "not locked".
constexpr int16_t kNoLock = INT16_MIN;
struct LockExtra { const char* id; const char* name; int minValue, maxValue; };
inline constexpr LockExtra lockExtras[] = {
    { "chorusSpeed", "Effect Speed", 0, 100 },   // % of the SPEED slider
    { "chorusType",  "Effect Type",  0, 2 },     // 0 Chorus, 1 Phaser, 2 Ensemble
    { "master",      "Volume",       0, 100 },   // % of the VOLUME fader
    { "tune",        "Tune",      -100, 100 },   // cents
    { "joyX",        "Joystick X (Bend)", -100, 100 },
    { "joyY",        "Joystick Y (DCO+ / VCF-)", -100, 100 },
    { "seqRate",     "Sequencer Rate", 0, 6 },   // that step's length: 0 = 1/2 ... 6 = 1/32 (seqRateOrder)
};
constexpr int numLockExtras = (int) (sizeof (lockExtras) / sizeof (lockExtras[0]));
constexpr int kLockSlots = numParams + numLockExtras;

struct LockSpec { int number; const char* name; int minValue, maxValue; }; // number 0 for the extras
inline LockSpec lockSpec (int slot)
{
    if (slot < numParams) return { params[slot].number, params[slot].name, params[slot].minValue, params[slot].maxValue };
    const auto& e = lockExtras[slot - numParams];
    return { 0, e.name, e.minValue, e.maxValue };
}
inline int lockSlotOf (const char* extraId)
{
    for (int i = 0; i < numLockExtras; ++i)
        if (std::string (lockExtras[i].id) == extraId) return numParams + i;
    return -1;
}

struct SeqNote
{
    int8_t note = -1;              // -1 = empty slot
    uint8_t velocity = 100;
    uint8_t length = 1;            // in steps
    bool used() const { return note >= 0; }
};

struct SeqStep
{
    std::array<SeqNote, kSeqMaxNotes> notes {};
    uint8_t probability = 100;     // % chance the step plays
    uint8_t gate = 100;            // % of the global gate, for this step
    std::array<int16_t, kLockSlots> locks; // per lock slot (see lockSpec), kNoLock = not locked

    SeqStep() { locks.fill (kNoLock); }
    int noteCount() const { int n = 0; for (auto& x : notes) n += x.used() ? 1 : 0; return n; }
    bool hasLocks() const { for (auto v : locks) if (v != kNoLock) return true; return false; }
    bool empty() const { return noteCount() == 0 && ! hasLocks(); }
};

struct SeqPattern
{
    int length = 16;               // 16 or 32
    std::array<SeqStep, kSeqMaxSteps> steps {};
};

struct SeqSettings
{
    int rate = 1;                  // index into seqRates
    int swing = 50;                // 50 (straight) .. 75 %
    int gate = 75;                 // % of a step, for the last step of a note
    bool hostSync = true;          // follow the host transport / internal clock
    int bpm = 120;                 // internal clock
};

struct SeqRate { const char* name; double stepsPerBeat; };
// Indices are stored in sessions and presets, so new rates go at the end (1/4 and 1/2 in 0.8.0).
inline constexpr SeqRate seqRates[] = { { "1/8", 2.0 }, { "1/16", 4.0 }, { "1/32", 8.0 }, { "1/8T", 3.0 }, { "1/16T", 6.0 },
                                        { "1/4", 1.0 }, { "1/2", 0.5 } };
inline constexpr int numSeqRates = (int) (sizeof (seqRates) / sizeof (seqRates[0]));
// The rates from slowest to fastest (menus, and the Sequencer Rate lock value 0..6).
inline constexpr int seqRateOrder[] = { 6, 5, 0, 3, 1, 4, 2 };
inline int seqRateOrderOf (int rateIndex)
{
    for (int i = 0; i < numSeqRates; ++i) if (seqRateOrder[i] == rateIndex) return i;
    return 2;
}

// ---- the editable data (message thread) ------------------------------------------------------
struct SeqData
{
    SeqSettings settings;
    std::array<SeqPattern, kSeqPatterns> patterns {};
    int current = 0;               // pattern being played and edited

    SeqPattern& pat() { return patterns[(size_t) current]; }
    const SeqPattern& pat() const { return patterns[(size_t) current]; }

    // Index of the note starting at 'step' with this pitch, or of a longer note covering it.
    // Returns the start step in startOut; -1 if none.
    int findNote (int step, int note, int& startOut) const
    {
        const auto& p = pat();
        for (int s = step; s >= 0; --s)
            for (int i = 0; i < kSeqMaxNotes; ++i)
            {
                const auto& n = p.steps[(size_t) s].notes[(size_t) i];
                if (n.used() && n.note == note && s + n.length > step) { startOut = s; return i; }
            }
        startOut = -1;
        return -1;
    }

    // Adds a note; false if the step is full (8 notes) or the pitch is already there.
    bool addNote (int step, int note, int length = 1, int velocity = 100)
    {
        if (step < 0 || step >= kSeqMaxSteps || note < 0 || note > 127) return false;
        auto& st = pat().steps[(size_t) step];
        for (auto& n : st.notes) if (n.used() && n.note == note) return false;
        for (auto& n : st.notes)
            if (! n.used())
            {
                n.note = (int8_t) note;
                n.velocity = (uint8_t) std::clamp (velocity, 1, 127);
                n.length = (uint8_t) std::clamp (length, 1, kSeqMaxSteps);
                return true;
            }
        return false;
    }

    void removeNote (int step, int index) { pat().steps[(size_t) step].notes[(size_t) index] = SeqNote(); }

    void setNoteLength (int step, int index, int length)
    {
        auto& n = pat().steps[(size_t) step].notes[(size_t) index];
        if (n.used()) n.length = (uint8_t) std::clamp (length, 1, pat().length - step);
    }

    void setLock (int step, int slot, int value)
    {
        const auto s = lockSpec (slot);
        pat().steps[(size_t) step].locks[(size_t) slot] = (int16_t) std::clamp (value, s.minValue, s.maxValue);
    }
    void clearLock (int step, int slot) { pat().steps[(size_t) step].locks[(size_t) slot] = kNoLock; }
    void clearLocksFor (int slot) { for (auto& st : pat().steps) st.locks[(size_t) slot] = kNoLock; }
    bool patternHasLocksFor (int slot) const
    {
        for (int s = 0; s < pat().length; ++s) if (pat().steps[(size_t) s].locks[(size_t) slot] != kNoLock) return true;
        return false;
    }

    // Sets the length of the note with this pitch starting at 'step' (live recording of held notes).
    void setLengthOf (int step, int note, int length)
    {
        if (step < 0 || step >= kSeqMaxSteps) return;
        for (int i = 0; i < kSeqMaxNotes; ++i)
            if (pat().steps[(size_t) step].notes[(size_t) i].note == note) { setNoteLength (step, i, length); return; }
    }

    // Step record TIE: notes that end exactly at 'step' are lengthened by one step.
    // Returns true if any note was tied.
    bool tieInto (int step)
    {
        bool any = false;
        for (int s = 0; s < step; ++s)
            for (auto& n : pat().steps[(size_t) s].notes)
                if (n.used() && s + n.length == step && step < pat().length) { n.length = (uint8_t) (n.length + 1); any = true; }
        return any;
    }

    void clearPattern() { const int len = pat().length; pat() = SeqPattern(); pat().length = len; }
    bool patternEmpty (int index) const
    {
        for (auto& st : patterns[(size_t) index].steps) if (! st.empty()) return false;
        return true;
    }

    // ---- save format: one line of text, robust to missing fields -----------------------------
    // "P800SEQ1 rate swing gate sync bpm current | len s:prob,gate,n.vel.len/..,locks idx=v/.. ; ... | ..."
    std::string serialise() const
    {
        std::ostringstream o;
        o << "P800SEQ1 " << settings.rate << ' ' << settings.swing << ' ' << settings.gate << ' '
          << (settings.hostSync ? 1 : 0) << ' ' << settings.bpm << ' ' << current;
        for (const auto& p : patterns)
        {
            o << " |" << p.length;
            for (int s = 0; s < kSeqMaxSteps; ++s)
            {
                const auto& st = p.steps[(size_t) s];
                if (st.empty() && st.probability == 100 && st.gate == 100) continue;
                o << ' ' << s << ':' << (int) st.probability << ',' << (int) st.gate << ',';
                bool first = true;
                for (const auto& n : st.notes)
                    if (n.used()) { o << (first ? "" : "/") << (int) n.note << '.' << (int) n.velocity << '.' << (int) n.length; first = false; }
                o << ',';
                first = true;
                for (int i = 0; i < kLockSlots; ++i)
                    if (st.locks[(size_t) i] != kNoLock) { o << (first ? "" : "/") << i << '=' << st.locks[(size_t) i]; first = false; }
            }
        }
        return o.str();
    }

    bool deserialise (const std::string& text)
    {
        if (text.rfind ("P800SEQ1", 0) != 0) return false;
        SeqData d;
        const auto bar = text.find ('|');
        {
            std::istringstream h (text.substr (8, bar == std::string::npos ? std::string::npos : bar - 8));
            int sync = 1;
            h >> d.settings.rate >> d.settings.swing >> d.settings.gate >> sync >> d.settings.bpm >> d.current;
            d.settings.rate = std::clamp (d.settings.rate, 0, numSeqRates - 1);
            d.settings.swing = std::clamp (d.settings.swing, 50, 75);
            d.settings.gate = std::clamp (d.settings.gate, 5, 100);
            d.settings.hostSync = sync != 0;
            d.settings.bpm = std::clamp (d.settings.bpm, 30, 300);
            d.current = std::clamp (d.current, 0, kSeqPatterns - 1);
        }
        size_t pos = bar;
        for (int pi = 0; pi < kSeqPatterns && pos != std::string::npos; ++pi)
        {
            const auto next = text.find ('|', pos + 1);
            std::istringstream ps (text.substr (pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1));
            auto& p = d.patterns[(size_t) pi];
            ps >> p.length;
            p.length = p.length == 32 ? 32 : 16;
            std::string tok;
            while (ps >> tok)
            {
                const auto colon = tok.find (':');
                if (colon == std::string::npos) continue;
                const int s = std::atoi (tok.substr (0, colon).c_str());
                if (s < 0 || s >= kSeqMaxSteps) continue;
                auto& st = p.steps[(size_t) s];
                std::string fields[4];
                {
                    std::istringstream fs (tok.substr (colon + 1));
                    for (auto& f : fields) std::getline (fs, f, ',');
                }
                st.probability = (uint8_t) std::clamp (std::atoi (fields[0].c_str()), 0, 100);
                st.gate = (uint8_t) std::clamp (std::atoi (fields[1].c_str()), 1, 100);
                std::istringstream ns (fields[2]);
                std::string nt;
                int ni = 0;
                while (std::getline (ns, nt, '/') && ni < kSeqMaxNotes)
                {
                    int note = -1, vel = 100, len = 1;
                    if (std::sscanf (nt.c_str(), "%d.%d.%d", &note, &vel, &len) >= 1 && note >= 0 && note <= 127)
                        st.notes[(size_t) ni++] = { (int8_t) note, (uint8_t) std::clamp (vel, 1, 127), (uint8_t) std::clamp (len, 1, kSeqMaxSteps) };
                }
                std::istringstream ls (fields[3]);
                std::string lt;
                while (std::getline (ls, lt, '/'))
                {
                    int idx = -1, v = 0;
                    if (std::sscanf (lt.c_str(), "%d=%d", &idx, &v) == 2 && idx >= 0 && idx < kLockSlots)
                        st.locks[(size_t) idx] = (int16_t) std::clamp (v, lockSpec (idx).minValue, lockSpec (idx).maxValue);
                }
            }
            pos = next;
        }
        *this = d;
        return true;
    }
};

// ---- playback (audio thread) ------------------------------------------------------------------
struct SeqTransport
{
    bool hasHost = false;          // the host reports a position
    bool hostPlaying = false;
    double ppq = 0.0;              // host position in beats at the block start
    double bpm = 120.0;            // host tempo
};

struct SeqEvent
{
    enum Type { noteOff = 0, step = 1, noteOn = 2 }; // this order at equal offsets
    int offset = 0;
    Type type = step;
    int note = 0, velocity = 0, stepIndex = 0;
};

class SeqPlayer
{
public:
    static constexpr int maxEvents = 512;

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { pendingCount = 0; running = false; lastStep = -1; pos = 0.0; playing = -1; cycleStart = 0; }
    void setData (const SeqData& d) { data = d; }
    const SeqData& getData() const { return data; }

    int currentStep() const { return running ? lastStep : -1; }
    // Pattern playing now. A pattern change made while running waits until the
    // playing pattern finishes its cycle (queued), then starts from its step 1.
    int playingPattern() const { return running && playing >= 0 ? playing : data.current; }
    bool changeQueued() const { return running && playing >= 0 && playing != data.current; }
    // Step of the playing pattern at a position (in steps), for live recording.
    int stepAt (double position) const
    {
        const int len = std::max (1, data.patterns[(size_t) playingPattern()].length);
        const long long k = std::llround (position) - cycleStart;
        return (int) (((k % len) + len) % len);
    }
    double position() const { return pos; }    // in steps, for live recording
    bool isRunning() const { return running; }

    // Locks of the step playing now, or nullptr.
    const int16_t* activeLocks() const
    {
        if (! running || lastStep < 0) return nullptr;
        const auto& st = data.patterns[(size_t) playingPattern()].steps[(size_t) lastStep];
        return st.hasLocks() ? st.locks.data() : nullptr;
    }

    // Fills 'out' with this block's events, sorted by offset. Returns how many.
    int process (int numSamples, const SeqTransport& t, bool run, SeqEvent* out)
    {
        int count = 0;
        auto emit = [&] (SeqEvent e) { if (count < maxEvents) out[count++] = e; };

        const auto& st = data.settings;
        const bool useHost = st.hostSync && t.hasHost;
        const bool shouldRun = run && (useHost ? t.hostPlaying : true);
        const double baseSpb = seqRates[std::clamp (st.rate, 0, numSeqRates - 1)].stepsPerBeat;
        const double bpm = useHost ? std::max (1.0, t.bpm) : (double) st.bpm;
        const double beatsPerSample = bpm / 60.0 / sr;

        if (! shouldRun)
        {
            if (running) { releaseAll (0, emit); running = false; lastStep = -1; }
            sortEvents (out, count);
            return count;
        }

        const double swingOffset = (std::clamp (st.swing, 50, 75) / 100.0 - 0.5) * 2.0;
        auto stepTime = [swingOffset] (long long k) { return (double) k + ((k & 1) ? swingOffset : 0.0); };
        auto resync = [&] (double at)
        {
            pos = at;
            long long k = std::max (0LL, (long long) std::floor (at) - 1);
            while (stepTime (k) < at - 1e-9) ++k;
            nextK = k;
        };

        if (! running)
        {
            running = true;
            lastStep = -1;
            playing = data.current;
            cycleStart = 0;
            // Starting exactly on a step plays it; starting mid-step waits for the next.
            resync (useHost ? t.ppq * baseSpb : 0.0);
        }
        else if (useHost && std::abs (t.ppq - expectedPpq) > 0.01)
        {
            releaseAll (0, emit); // host jumped or looped
            cycleStart = 0;       // realign patterns to the song position
            lastStep = -1;
            resync (t.ppq * baseSpb);
        }
        expectedPpq = t.ppq + numSamples * beatsPerSample;

        // Time is counted in steps. A step's length follows the RATE setting, or that step's
        // Sequencer Rate lock, so the block is walked one step at a time.
        double done = 0.0; // samples of this block already covered
        auto offsetOf = [&] (double at, double sps)
        {
            return std::clamp ((int) std::floor (done + (at - pos) / sps), 0, std::max (0, numSamples - 1));
        };
        auto releaseUntil = [&] (double until, double sps)
        {
            for (int i = 0; i < pendingCount;)
            {
                if (pending[i].endPos <= until + 1e-9)
                {
                    emit ({ offsetOf (pending[i].endPos, sps), SeqEvent::noteOff, pending[i].note, 0, 0 });
                    pending[i] = pending[--pendingCount];
                }
                else ++i;
            }
        };

        for (;;)
        {
            const double sps = std::max (1e-12, beatsPerSample * currentSpb (baseSpb)); // steps per sample
            const double tk = stepTime (nextK);
            const double toNext = std::max (0.0, (tk - pos) / sps);
            if (done + toNext >= (double) numSamples)
            {
                const double end = pos + ((double) numSamples - done) * sps;
                releaseUntil (end - 1e-9, sps);
                pos = end;
                break;
            }
            releaseUntil (tk - 1e-9, sps);
            done += toNext;
            pos = tk;
            fireStep (nextK, tk, emit, offsetOf (tk, sps), st);
            ++nextK;
        }

        sortEvents (out, count);
        return count;
    }

private:
    struct Pending { int note; double endPos; };
    SeqData data;
    double sr = 48000.0;
    bool running = false;
    int lastStep = -1;
    int playing = -1;            // pattern playing (data.current may be queued after it)
    long long cycleStart = 0;    // step counter value where the playing pattern started
    double pos = 0.0, expectedPpq = 0.0;
    long long nextK = 0;          // the next step to play (absolute step counter)

    // Steps per beat while the current step plays: its Sequencer Rate lock, or RATE.
    double currentSpb (double baseSpb) const
    {
        static const int slot = lockSlotOf ("seqRate");
        if (lastStep < 0 || playing < 0) return baseSpb;
        const int v = data.patterns[(size_t) playing].steps[(size_t) lastStep].locks[(size_t) slot];
        if (v == kNoLock) return baseSpb;
        return seqRates[seqRateOrder[std::clamp ((int) v, 0, numSeqRates - 1)]].stepsPerBeat;
    }

    template <typename Emit>
    void fireStep (long long k, double tk, Emit& emit, int offset, const SeqSettings& st)
    {
        // Pattern changes take effect when the playing pattern wraps to its first step.
        {
            const int len = data.patterns[(size_t) playing].length;
            if (playing != data.current && (((k - cycleStart) % len) + len) % len == 0)
            {
                playing = data.current;
                cycleStart = k;
            }
        }
        const auto& p = data.patterns[(size_t) playing];
        const int stepIndex = (int) ((((k - cycleStart) % p.length) + p.length) % p.length);
        lastStep = stepIndex;
        emit ({ offset, SeqEvent::step, 0, 0, stepIndex });

        const auto& step = p.steps[(size_t) stepIndex];
        if (step.noteCount() == 0) return;
        if (step.probability < 100 && (int) (nextRandom() % 100) >= step.probability) return;
        const double gateFrac = std::max (0.05, step.gate / 100.0 * std::clamp (st.gate, 5, 100) / 100.0);
        for (const auto& n : step.notes)
        {
            if (! n.used()) continue;
            // Retrigger: end a still-sounding copy of the same note first.
            for (int i = 0; i < pendingCount; ++i)
                if (pending[i].note == n.note)
                {
                    emit ({ offset, SeqEvent::noteOff, n.note, 0, 0 });
                    pending[i] = pending[--pendingCount];
                    break;
                }
            emit ({ offset, SeqEvent::noteOn, n.note, n.velocity, stepIndex });
            if (pendingCount < (int) pending.size())
                pending[(size_t) pendingCount++] = { n.note, tk + (n.length - 1) + gateFrac };
        }
    }
    std::array<Pending, 128> pending {};
    int pendingCount = 0;
    uint32_t rng = 0x12345678u;

    uint32_t nextRandom() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

    template <typename Emit>
    void releaseAll (int offset, Emit& emit)
    {
        for (int i = 0; i < pendingCount; ++i)
            emit ({ offset, SeqEvent::noteOff, pending[i].note, 0, 0 });
        pendingCount = 0;
    }

    static void sortEvents (SeqEvent* e, int n)
    {
        std::stable_sort (e, e + n, [] (const SeqEvent& a, const SeqEvent& b)
                          { return a.offset != b.offset ? a.offset < b.offset : (int) a.type < (int) b.type; });
    }
};
} // namespace p800
