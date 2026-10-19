// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamSpec.h"
#include "Engine.h"

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

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Poly800Params currentCoreParams() const;

    p800::Engine engine;
    std::array<std::atomic<float>*, p800::numParams> paramValues {};
    std::atomic<float>* masterParam = nullptr;
    std::atomic<float>* tuneParam = nullptr;
    int currentProgram = 0;
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (P800Processor)
};
