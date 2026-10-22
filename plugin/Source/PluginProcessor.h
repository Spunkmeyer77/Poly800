// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamSpec.h"
#include "Engine.h"
#include "KeyLayer.h"
#include "PresetManager.h"
#include "Sequencer.h"

class P800Processor : public juce::AudioProcessor, public p800::PresetTarget, private juce::Timer
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

    // Presets (banks, factory programs). Message thread.
    p800::PresetManager& getPresetManager() { return *presets; }
    juce::ValueTree captureSound() override;
    void restoreSound (const juce::ValueTree&) override;
    void loadFactory (int index) override { selectProgram (index); }
    int numFactory() const override;
    juce::String factoryName (int index) const override;

    // ---- step sequencer (message thread unless noted) -------------------------------------
    p800::SeqData getSeqData() const;                              // a copy, for drawing
    void editSeq (const std::function<void (p800::SeqData&)>& change); // edit and publish
    int  getSeqVersion() const          { return seqVersion.load(); }
    void setSeqRun (bool run)           { seqRun.store (run); }
    bool getSeqRun() const              { return seqRun.load(); }
    int  getSeqPlayStep() const         { return seqPlayStep.load(); } // -1 when stopped
    int  getSeqPlayingPattern() const   { return seqPlayingPattern.load(); } // -1 when stopped
    void setSeqRecording (bool rec)     { seqRecording.store (rec); heldForStepRec = 0; }
    bool getSeqRecording() const        { return seqRecording.load(); }
    void setSeqSelectedStep (int step)  { seqSelectedStep.store (step); }
    int  getSeqSelectedStep() const     { return seqSelectedStep.load(); }
    double getHostBpm() const           { return hostBpm.load(); }  // 0 if the host gives none
    int  getSeqLaneHeight() const       { return seqLaneHeight.load(); }
    void setSeqLaneHeight (int h)       { seqLaneHeight.store (h); }

private:
    static const juce::StringArray& soundParamIds(); // everything a preset stores
    void setParamValue (const juce::String& id, float value);
    std::unique_ptr<p800::PresetManager> presets;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Poly800Params currentCoreParams (const int16_t* locks = nullptr) const;
    void timerCallback() override;      // applies notes recorded from the keyboard

    // Sequencer: the model is edited on the message thread under seqLock; the player
    // (audio thread) takes a copy whenever seqVersion changes.
    p800::SeqData seqModel;
    mutable juce::CriticalSection seqLock;
    std::atomic<int> seqVersion { 1 };
    int playerVersion = 0;
    p800::SeqPlayer player;
    std::array<p800::SeqEvent, p800::SeqPlayer::maxEvents> seqEvents {};
    std::atomic<bool> seqRun { false }, seqRecording { false };
    std::atomic<int> seqPlayStep { -1 }, seqSelectedStep { 0 }, seqPlayingPattern { -1 };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> seqLaneHeight { 80 };   // the sequencer's lower lane, resizable by the user
    int heldForStepRec = 0;             // audio thread: keys down while step recording

    struct RecEvent { int type, step, note, length; }; // 0 add note, 1 advance selected step, 2 set note length
    juce::AbstractFifo recFifo { 256 };
    std::array<RecEvent, 256> recBuffer {};
    void pushRec (RecEvent e);
    void updatePerformance (const int16_t* locks = nullptr);
    void applyLocks (const int16_t* locks);

    p800::Engine engine;
    std::array<std::atomic<float>*, p800::numParams> paramValues {};
    std::atomic<float>* masterParam = nullptr;
    std::atomic<float>* tuneParam = nullptr;
    std::atomic<float>* joyXParam = nullptr;
    std::atomic<float>* joyYParam = nullptr;
    std::atomic<float>* bendRangeParam = nullptr;
    std::atomic<float>* voices16Param = nullptr;
    std::atomic<float>* vcfModeParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* keyModeParam = nullptr;
    std::atomic<float>* chorusTypeParam = nullptr;
    std::atomic<float>* chorusSpeedParam = nullptr;
    bool sustainPedal = false;

    // What the key assign layer plays (POLY, CHORD, HOLD) goes to the engine and, with REC on,
    // into the sequencer, so a chord played in CHORD mode is recorded as the whole chord.
    struct EngineSink : p800::KeyLayer::Sink
    {
        EngineSink (p800::Engine& e, P800Processor& o) : engine (e), owner (o) {}
        void keyOn (int note, int velocity) override { engine.noteOn (note, velocity); owner.recordPlayedNote (note); }
        void keyOff (int note) override { engine.noteOff (note); owner.recordReleasedNote (note); }
        p800::Engine& engine;
        P800Processor& owner;
    };
    void recordPlayedNote (int note);   // audio thread
    void recordReleasedNote (int note); // audio thread: sets the recorded note's length
    std::array<double, 128> recStartPos {};
    std::array<int, 128> recStartStep {};
    p800::KeyLayer keys;
    EngineSink sink { engine, *this };
    float midiBend = 0.0f;    // pitch wheel, -1..1
    float midiModWheel = 0.0f; // CC1, 0..1
    int currentProgram = 0;
    std::vector<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (P800Processor)
};
