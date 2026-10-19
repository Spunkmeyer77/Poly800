// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamSpec.h"
#include "Engine.h"
#include "KeyLayer.h"

class P800Processor : public juce::AudioProcessor
{
public:
    P800Processor();
    ~P800Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Message thread: switch program and tell the host.
    void selectProgram (int index);

    // Chord memory (panel MEMORY / CHORD buttons and readout).
    void requestChordMemory()                       { keys.requestChordCapture(); }
    int  getChord (int* intervals, int& root) const { return keys.getChord (intervals, root); }
    int  getChordVersion() const                    { return keys.getChordVersion(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Poly800Params currentCoreParams() const;
    void updatePerformance();

    p800::Engine engine;
    std::array<std::atomic<float>*, p800::numParams> paramValues {};
    std::atomic<float>* masterParam = nullptr;
    std::atomic<float>* tuneParam = nullptr;
    std::atomic<float>* joyXParam = nullptr;
    std::atomic<float>* joyYParam = nullptr;
    std::atomic<float>* bendRangeParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* keyModeParam = nullptr;
    std::atomic<float>* chorusTypeParam = nullptr;
    std::atomic<float>* chorusSpeedParam = nullptr;
    bool sustainPedal = false;

    struct EngineSink : p800::KeyLayer::Sink
    {
        explicit EngineSink (p800::Engine& e) : engine (e) {}
        void keyOn (int note, int velocity) override { engine.noteOn (note, velocity); }
        void keyOff (int note) override { engine.noteOff (note); }
        p800::Engine& engine;
    };
    p800::KeyLayer keys;
    EngineSink sink { engine };
    float midiBend = 0.0f;    // pitch wheel, -1..1
    float midiModWheel = 0.0f; // CC1, 0..1
    int currentProgram = 0;
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (P800Processor)
};
