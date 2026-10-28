// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "PanelView.h"

class P800Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit P800Editor (P800Processor&);
    ~P800Editor() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void refreshState();
    float scale() const { return (float) getWidth() / (float) p800::PanelView::width; }
    juce::Point<float> toDesign (juce::Point<float> p) const { return p / scale(); }

    juce::RangedAudioParameter* param (const juce::String& id) const;
    juce::RangedAudioParameter* chartParam (int index) const;
    float getReal (const juce::String& id) const;
    void setReal (const juce::String& id, float value, bool gesture = true);
    void stepSelected (int delta);
    void typeDigit (int digit);
    void setFromDrag (p800::HitKind, juce::Point<float> design);

    // Presets
    juce::Rectangle<int> screenAreaFor (juce::Rectangle<float> design) const;
    void showBankMenu();
    void showPresetMenu();
    void askSave();
    void saveTo (const juce::String& bank, const juce::String& name);
    void askNewBank();
    void askDeletePreset();
    void askDeleteBank();
    void openPresetFile();
    std::unique_ptr<juce::FileChooser> chooser;

    // Sequencer screen
    bool seqMouseDown (const p800::Hit&, const juce::MouseEvent&, juce::Point<float> design);
    void seqMouseDrag (juce::Point<float> design);
    void seqMouseUp();
    void seqWheel (const p800::Hit&, int delta);
    void setLaneValue (int step, float y, bool reset);
    void showRateMenu();
    void showLockParamMenu();
    void askClearPattern();
    p800::SeqPattern clipboard;
    int lastSeqVersion = -1;
    int seqDragStart = -1, seqDragIndex = -1, seqDragValue = 0;
    bool seqDragNew = false, seqDragMoved = false;

    P800Processor& processor;
    p800::PanelView view;
    p800::PanelState state;

    p800::Hit dragHit;
    int dragStartValue = 0;
    float dragStartY = 0.0f, dragStartX = 0.0f;
    float dragAccum = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (P800Editor)
};
