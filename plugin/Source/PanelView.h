// SPDX-License-Identifier: GPL-3.0-or-later
// P-800 hardware panel: layout, drawing and hit-testing.
// Depends only on juce_graphics, so it can also be rendered headless (tools/).
#pragma once

#include <juce_graphics/juce_graphics.h>
#include "ParamSpec.h"
#include "Sequencer.h"
#include <array>

namespace p800
{
// Sequencer screen contents (shown in place of the parameter chart).
struct SeqView
{
    bool visible = false;
    bool playing = false;            // PLAY pressed (running, or waiting for the host)
    bool recording = false;
    bool pasteArmed = false;         // COPY was pressed: the button reads PASTE
    int playStep = -1;               // playhead, -1 = stopped
    int lowNote = 48;                // bottom row of the 2-octave grid (C2)
    int lane = 0;                    // 0 probability, 1 gate, 2 parameter lock
    int lockParam = 18;              // index into p800::params shown in the P-LOCK lane (P41)
    int selectedStep = 0;
    double hostBpm = 0.0;            // shown when synced to the host, 0 if unknown
    SeqData data;
};

// Everything the panel shows. The editor fills this from the processor.
struct PanelState
{
    std::array<int, numParams> values {};  // indexed like p800::params
    int selectedParam = 18;                 // index into p800::params (P41)
    int programNumber = 11;                 // factory number shown on the LED
    juce::String programName { "Strings" };
    bool progEntry = false;                 // keypad enters program numbers
    juce::String entryBuffer;               // first digit typed, if any
    float volume = 0.35f;                   // 0..1
    float tune = 0.0f;                      // -100..100 cents
    float joyX = 0.0f, joyY = 0.0f;         // -1..1 (Y up = +1)
    bool hold = false;
    int bendRange = 2;
    bool chordMode = false;
    juce::String chordName { "Cm" };        // stored chord, e.g. "Cm"
    juce::String chordNotes { "C3 Eb3 G3" };
    int chorusType = 0;                     // effect: 0 Chorus, 1 Phaser, 2 Ensemble
    float chorusSpeed = 0.4f;               // 0..1
    juce::String version { "0.5.1" };       // set from JucePlugin_VersionString by the editor
    // Presets
    juce::String bankName { "Factory" };
    juce::String presetName { "11 Strings" };
    bool presetFavourite = false;
    bool presetIsUser = false;               // DELETE is only active for user presets
    SeqView seq;
};

enum class HitKind
{
    none, joystick, volume, tune, cell, key, progPara, valueUp, valueDown,
    voices, chorus, poly, hold, progPrev, progNext,
    chord, memory, chorusType, chorusSpeed,
    bankMenu, presetMenu, presetPrev, presetNext, presetSave, newBank, presetDelete, presetFavourite,
    seqToggle, seqPlay,
    seqLength, seqPattern, seqRate, seqSwing, seqGate, seqSync, seqBpm, seqOctUp, seqOctDown,
    seqRec, seqCopy, seqClear, seqStepNumber, seqGrid, seqLaneTab, seqLane, seqLockParam
};

struct Hit
{
    HitKind kind = HitKind::none;
    int index = -1; // param index for cell, digit for key, step for the sequencer
    int note = -1;  // sequencer grid: the row's MIDI note
};

class PanelView
{
public:
    static constexpr int width = 1440;
    static constexpr int height = 800;

    PanelView();

    void paint (juce::Graphics&, const PanelState&) const;
    Hit hitTest (juce::Point<float> designPos, const PanelState&) const;

    // Sequencer geometry (design coordinates), for the editor's dragging.
    static juce::Rectangle<float> seqGridArea()  { return { 96, 468, 1312, 216 }; }
    static juce::Rectangle<float> seqLaneValues() { return { 96, 732, 1312, 38 }; }
    static constexpr int seqRows = 24;
    juce::Rectangle<float> seqRateArea() const   { return { 598, 423, 50, 22 }; }
    juce::Rectangle<float> seqLockParamArea() const { return { 102, 696, 210, 18 }; }

    // Geometry the editor needs for dragging.
    juce::Rectangle<float> joystickArea() const { return joyBox; }
    juce::Rectangle<float> volumeTrack() const { return volTrack; }
    juce::Rectangle<float> tuneTrack() const { return tuneTrackR; }
    juce::Rectangle<float> chorusSpeedTrack() const { return speedTrack; }
    juce::Rectangle<float> bankArea() const { return bankR; }
    juce::Rectangle<float> presetNameArea() const { return presetNameR; }

    static int indexOfParam (int number);

private:
    struct Cell
    {
        int paramIndex;
        const char* label;
        juce::String hi, lo;
        juce::Rectangle<float> r;
    };
    struct Group
    {
        const char* name;
        juce::Rectangle<float> r;
    };

    void buildLayout();
    void paintMetal (juce::Graphics&) const;
    void paintChart (juce::Graphics&, const PanelState&) const;
    void paintDisplay (juce::Graphics&, const PanelState&) const;

    std::vector<Cell> cells;
    std::vector<Group> groups;
    juce::Rectangle<float> panel, joyBox, volTrack, tuneTrackR, display,
        progPrevR, progNextR, upR, downR, polyR, chordR, holdR, memoryR, voicesR, chorusR, speedTrack;
    std::array<juce::Rectangle<float>, 3> chorusTypeR {};
    juce::Rectangle<float> bankR, presetPrevR, presetNameR, presetNextR, saveR, newBankR, deleteR, favR;
    void paintPresets (juce::Graphics&, const PanelState&) const;
    void paintSequencer (juce::Graphics&, const PanelState&) const;
    juce::Rectangle<float> seqBtnR, playBtnR;
    std::array<juce::Rectangle<float>, 9> keyR {}; // 7 8 P / 4 5 6 / 1 2 3
    std::array<int, 9> keyDigit {};
    juce::Image metal; // pre-rendered brushed metal
};
} // namespace p800
