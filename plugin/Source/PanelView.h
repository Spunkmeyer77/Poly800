// SPDX-License-Identifier: GPL-3.0-or-later
// P-800 hardware panel: layout, drawing and hit-testing.
// Depends only on juce_graphics, so it can also be rendered headless (tools/).
#pragma once

#include <juce_graphics/juce_graphics.h>
#include "ParamSpec.h"
#include <array>

namespace p800
{
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
    int chorusType = 0;                     // 0 Chorus, 1 Ensemble, 2 Original
    float chorusSpeed = 0.4f;               // 0..1
};

enum class HitKind
{
    none, joystick, volume, tune, cell, key, progPara, valueUp, valueDown,
    voices, chorus, poly, hold, progPrev, progNext,
    chord, memory, chorusType, chorusSpeed
};

struct Hit
{
    HitKind kind = HitKind::none;
    int index = -1; // param index for cell, digit for key
};

class PanelView
{
public:
    static constexpr int width = 1440;
    static constexpr int height = 800;

    PanelView();

    void paint (juce::Graphics&, const PanelState&) const;
    Hit hitTest (juce::Point<float> designPos) const;

    // Geometry the editor needs for dragging.
    juce::Rectangle<float> joystickArea() const { return joyBox; }
    juce::Rectangle<float> volumeTrack() const { return volTrack; }
    juce::Rectangle<float> tuneTrack() const { return tuneTrackR; }
    juce::Rectangle<float> chorusSpeedTrack() const { return speedTrack; }

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
    std::array<juce::Rectangle<float>, 9> keyR {}; // 7 8 P / 4 5 6 / 1 2 3
    std::array<int, 9> keyDigit {};
    juce::Image metal; // pre-rendered brushed metal
};
} // namespace p800
