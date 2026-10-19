// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginEditor.h"
#include "FactoryPrograms.h"
#include "ChordNames.h"

using p800::HitKind;

namespace
{
juce::String paramId (int index) { return "p" + juce::String (p800::params[index].number); }
}

P800Editor::P800Editor (P800Processor& p)
    : AudioProcessorEditor (p), processor (p)
{
    constexpr double ratio = (double) p800::PanelView::width / (double) p800::PanelView::height;
    setResizable (true, true);
    setResizeLimits (720, 400, 2160, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (ratio);
    setSize (1080, 600);

    state.selectedParam = p800::PanelView::indexOfParam (41);
    state.version = JucePlugin_VersionString;
    refreshState();
    startTimerHz (30);
}

P800Editor::~P800Editor() { stopTimer(); }

juce::RangedAudioParameter* P800Editor::param (const juce::String& id) const
{
    return processor.apvts.getParameter (id);
}

juce::RangedAudioParameter* P800Editor::chartParam (int index) const
{
    return index >= 0 && index < p800::numParams ? param (paramId (index)) : nullptr;
}

float P800Editor::getReal (const juce::String& id) const
{
    if (auto* prm = param (id))
        return prm->convertFrom0to1 (prm->getValue());
    return 0.0f;
}

void P800Editor::setReal (const juce::String& id, float value, bool gesture)
{
    if (auto* prm = param (id))
    {
        if (gesture) prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        if (gesture) prm->endChangeGesture();
    }
}

void P800Editor::refreshState()
{
    p800::PanelState s = state;
    for (int i = 0; i < p800::numParams; ++i)
        s.values[(size_t) i] = juce::roundToInt (getReal (paramId (i)));
    const int prog = processor.getCurrentProgram();
    s.programNumber = factory::programs[prog].number;
    s.programName = juce::String (factory::programs[prog].number) + " " + factory::programs[prog].name;
    s.volume = getReal ("master");
    s.tune = getReal ("tune");
    s.joyX = getReal ("joyX");
    s.joyY = getReal ("joyY");
    s.hold = getReal ("hold") > 0.5f;
    s.bendRange = juce::roundToInt (getReal ("bendRange"));
    s.chordMode = getReal ("keyMode") > 0.5f;
    s.chorusType = juce::roundToInt (getReal ("chorusType"));
    s.chorusSpeed = getReal ("chorusSpeed");
    {
        int iv[p800::KeyLayer::maxChord], root = 60;
        const int n = processor.getChord (iv, root);
        juce::StringArray notes;
        for (int i = 0; i < n; ++i) notes.add (p800::noteName (root + iv[i]));
        s.chordName = p800::chordName (iv, n, root);
        s.chordNotes = notes.joinIntoString (" ");
    }

    const bool changed = s.values != state.values || s.programNumber != state.programNumber
                      || s.volume != state.volume || s.tune != state.tune || s.joyX != state.joyX
                      || s.joyY != state.joyY || s.hold != state.hold || s.bendRange != state.bendRange
                      || s.chordMode != state.chordMode || s.chordNotes != state.chordNotes
                      || s.chorusType != state.chorusType || s.chorusSpeed != state.chorusSpeed;
    state = s;
    if (changed)
        repaint();
}

void P800Editor::timerCallback() { refreshState(); }

void P800Editor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale()));
    view.paint (g, state);
}

void P800Editor::stepSelected (int delta)
{
    const int i = state.selectedParam;
    const auto& spec = p800::params[i];
    const int v = juce::jlimit (spec.minValue, spec.maxValue, state.values[(size_t) i] + delta);
    setReal (paramId (i), (float) v);
    refreshState();
}

void P800Editor::typeDigit (int digit)
{
    if (state.entryBuffer.isEmpty())
    {
        state.entryBuffer = juce::String (digit);
        repaint();
        return;
    }
    const int number = state.entryBuffer.getIntValue() * 10 + digit;
    state.entryBuffer.clear();
    if (state.progEntry)
    {
        for (int i = 0; i < factory::numPrograms; ++i)
            if (factory::programs[i].number == number)
                processor.selectProgram (i);
    }
    else if (const int idx = p800::PanelView::indexOfParam (number); idx >= 0)
    {
        state.selectedParam = idx;
    }
    refreshState();
    repaint();
}

void P800Editor::setFromDrag (HitKind kind, juce::Point<float> d)
{
    if (kind == HitKind::joystick)
    {
        const auto box = view.joystickArea();
        float x = (d.x - box.getCentreX()) / (box.getWidth() * 0.5f - 22.0f);
        float y = -(d.y - box.getCentreY()) / (box.getHeight() * 0.5f - 22.0f);
        const float len = std::sqrt (x * x + y * y);
        if (len > 1.0f) { x /= len; y /= len; }
        setReal ("joyX", x, false);
        setReal ("joyY", y, false);
    }
    else if (kind == HitKind::volume)
    {
        const auto t = view.volumeTrack();
        setReal ("master", juce::jlimit (0.0f, 1.0f, (t.getBottom() - 13.0f - d.y) / (t.getHeight() - 26.0f)), false);
    }
    else if (kind == HitKind::tune)
    {
        const auto t = view.tuneTrack();
        const float f = juce::jlimit (0.0f, 1.0f, (d.x - t.getX() - 15.0f) / (t.getWidth() - 30.0f));
        setReal ("tune", f * 200.0f - 100.0f, false);
    }
    else if (kind == HitKind::chorusSpeed)
    {
        const auto t = view.chorusSpeedTrack();
        setReal ("chorusSpeed", juce::jlimit (0.0f, 1.0f, (d.x - t.getX() - 15.0f) / (t.getWidth() - 30.0f)), false);
    }
    else if (kind == HitKind::cell)
    {
        // Drag up to increase: one step per 6 px.
        const auto& spec = p800::params[dragHit.index];
        const int v = juce::jlimit (spec.minValue, spec.maxValue,
                                    dragStartValue + juce::roundToInt ((dragStartY - d.y) / 6.0f));
        setReal (paramId (dragHit.index), (float) v, false);
    }
    refreshState();
}

void P800Editor::mouseDown (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);
    dragHit = view.hitTest (d);

    switch (dragHit.kind)
    {
        case HitKind::joystick:
            param ("joyX")->beginChangeGesture();
            param ("joyY")->beginChangeGesture();
            setFromDrag (dragHit.kind, d);
            break;
        case HitKind::volume:
            param ("master")->beginChangeGesture();
            setFromDrag (dragHit.kind, d);
            break;
        case HitKind::tune:
            param ("tune")->beginChangeGesture();
            if (e.getNumberOfClicks() > 1) setReal ("tune", 0.0f, false); // double-click resets
            else setFromDrag (dragHit.kind, d);
            break;
        case HitKind::cell:
            state.selectedParam = dragHit.index;
            state.entryBuffer.clear();
            dragStartValue = state.values[(size_t) dragHit.index];
            dragStartY = d.y;
            chartParam (dragHit.index)->beginChangeGesture();
            repaint();
            break;
        case HitKind::key:      typeDigit (dragHit.index); break;
        case HitKind::progPara: state.progEntry = ! state.progEntry; state.entryBuffer.clear(); repaint(); break;
        case HitKind::valueUp:  stepSelected (+1); break;
        case HitKind::valueDown: stepSelected (-1); break;
        case HitKind::voices:
        {
            const int i = p800::PanelView::indexOfParam (18);
            setReal (paramId (i), state.values[(size_t) i] == 2 ? 1.0f : 2.0f);
            break;
        }
        case HitKind::chorus:
        {
            const int i = p800::PanelView::indexOfParam (48);
            setReal (paramId (i), state.values[(size_t) i] == 1 ? 0.0f : 1.0f);
            break;
        }
        // POLY also releases HOLD, as on the original. Switching to CHORD with two or
        // more keys held stores them as the chord (Polytwelve behaviour).
        case HitKind::poly:  setReal ("keyMode", 0.0f); setReal ("hold", 0.0f); break;
        case HitKind::chord: setReal ("keyMode", 1.0f); break;
        case HitKind::hold:  setReal ("hold", state.hold ? 0.0f : 1.0f); break;
        case HitKind::memory:
            processor.requestChordMemory();
            setReal ("keyMode", 1.0f);
            break;
        case HitKind::chorusType: setReal ("chorusType", (float) dragHit.index); break;
        case HitKind::chorusSpeed:
            param ("chorusSpeed")->beginChangeGesture();
            setFromDrag (dragHit.kind, d);
            break;
        case HitKind::progPrev:
            processor.selectProgram ((processor.getCurrentProgram() + factory::numPrograms - 1) % factory::numPrograms);
            break;
        case HitKind::progNext:
            processor.selectProgram ((processor.getCurrentProgram() + 1) % factory::numPrograms);
            break;
        case HitKind::none: break;
    }
    refreshState();
}

void P800Editor::mouseDrag (const juce::MouseEvent& e)
{
    switch (dragHit.kind)
    {
        case HitKind::joystick: case HitKind::volume: case HitKind::tune: case HitKind::cell:
        case HitKind::chorusSpeed:
            setFromDrag (dragHit.kind, toDesign (e.position));
            break;
        default: break;
    }
}

void P800Editor::mouseUp (const juce::MouseEvent&)
{
    switch (dragHit.kind)
    {
        case HitKind::joystick:
            // Spring back to centre, like the hardware.
            setReal ("joyX", 0.0f, false);
            setReal ("joyY", 0.0f, false);
            param ("joyX")->endChangeGesture();
            param ("joyY")->endChangeGesture();
            break;
        case HitKind::volume: param ("master")->endChangeGesture(); break;
        case HitKind::tune:   param ("tune")->endChangeGesture(); break;
        case HitKind::chorusSpeed: param ("chorusSpeed")->endChangeGesture(); break;
        case HitKind::cell:   chartParam (dragHit.index)->endChangeGesture(); break;
        default: break;
    }
    dragHit = {};
    refreshState();
}

void P800Editor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto hit = view.hitTest (toDesign (e.position));
    if (hit.kind != HitKind::cell || w.deltaY == 0.0f)
        return;
    dragAccum += w.deltaY * (w.isReversed ? -1.0f : 1.0f) * 8.0f;
    const int steps = (int) dragAccum;
    if (steps == 0)
        return;
    dragAccum -= (float) steps;
    state.selectedParam = hit.index;
    stepSelected (steps);
}
