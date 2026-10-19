// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPrograms.h"

static juce::String paramId (int number) { return "p" + juce::String (number); }

P800Processor::P800Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "P800", createLayout())
{
    static_assert (factory::numParams == p800::numParams, "factory CSV and ParamSpec disagree");
    for (int i = 0; i < p800::numParams; ++i)
    {
        jassert (factory::paramNumbers[i] == p800::params[i].number);
        paramValues[(size_t) i] = apvts.getRawParameterValue (paramId (p800::params[i].number));
    }
    masterParam = apvts.getRawParameterValue ("master");
    tuneParam = apvts.getRawParameterValue ("tune");
    joyXParam = apvts.getRawParameterValue ("joyX");
    joyYParam = apvts.getRawParameterValue ("joyY");
    bendRangeParam = apvts.getRawParameterValue ("bendRange");
    holdParam = apvts.getRawParameterValue ("hold");
    keyModeParam = apvts.getRawParameterValue ("keyMode");
    chorusTypeParam = apvts.getRawParameterValue ("chorusType");
    chorusSpeedParam = apvts.getRawParameterValue ("chorusSpeed");
    setCurrentProgram (0);
}

juce::AudioProcessorValueTreeState::ParameterLayout P800Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : p800::params)
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { paramId (s.number), 1 },
            juce::String (s.number) + " " + s.name, s.minValue, s.maxValue, s.defaultValue));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "master", 1 }, "Volume", 0.0f, 1.0f, p800::volumeDefault));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tune", 1 }, "Tune",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("cents")));

    // Joystick: X bends pitch; Y up adds MG to the DCOs, Y down adds MG to the VCF.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "joyX", 1 }, "Joystick X (Bend)", -1.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "joyY", 1 }, "Joystick Y (DCO+ / VCF-)", -1.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "bendRange", 1 }, "Bend Range", 1, 12, 2,
        juce::AudioParameterIntAttributes().withLabel ("st")));

    // Key assign: POLY or CHORD (chord memory, ported from Polytwelve), plus HOLD.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "keyMode", 1 }, "Key Assign", juce::StringArray { "Poly", "Chord" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "hold", 1 }, "Hold", false));

    // Effects, the same as Polytwelve: Chorus, Phaser or Ensemble. P48 turns the effect on/off.
    // (Parameter IDs kept from 0.2.0 so saved sessions still load.)
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "chorusType", 1 }, "Effect Type",
        juce::StringArray { "Chorus", "Phaser", "Ensemble" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "chorusSpeed", 1 }, "Effect Speed / Intensity", 0.0f, 1.0f, 0.4f));
    return layout;
}

void P800Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    scratch.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    engine.setParams (currentCoreParams());
}

void P800Processor::releaseResources()
{
    keys.allNotesOff (sink);
    engine.allNotesOff();
}

bool P800Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void P800Processor::updatePerformance()
{
    const float range = bendRangeParam->load();
    const float x = juce::jlimit (-1.0f, 1.0f, joyXParam->load() + midiBend);
    const float y = joyYParam->load();
    // Mod wheel works like pushing the joystick up.
    const float dco = juce::jlimit (0.0f, 1.0f, juce::jmax (0.0f, y) + midiModWheel);
    const float vcf = juce::jmax (0.0f, -y);
    engine.setPerformance (x * range, dco, vcf);
}

Poly800Params P800Processor::currentCoreParams() const
{
    int values[p800::numParams];
    for (int i = 0; i < p800::numParams; ++i)
        values[i] = juce::roundToInt (paramValues[(size_t) i]->load());

    Poly800Params p;
    poly800_params_default (&p);
    p800::fillCoreParams (p, values);
    p.master_gain = masterParam->load() * p800::volumeToGain;
    p.tune_cents = tuneParam->load();
    return p;
}

void P800Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (! engine.isPrepared())
    {
        buffer.clear();
        return;
    }

    engine.setEffect (juce::roundToInt (chorusTypeParam->load()), chorusSpeedParam->load());
    const auto coreParams = currentCoreParams();
    engine.setParams (coreParams);
    updatePerformance();

    // Key assign. DOUBLE mode (P18 = 2) has 4 voices, WHOLE has 8.
    keys.setMode (juce::roundToInt (keyModeParam->load()));
    keys.setHold (holdParam->load() > 0.5f || sustainPedal);
    keys.setVoiceLimit (coreParams.dco_mode == 2 ? 4 : 8);
    keys.update (sink);

    float* left = buffer.getWritePointer (0);
    float* right = nullptr;
    if (buffer.getNumChannels() > 1)
        right = buffer.getWritePointer (1);
    else
    {
        if ((int) scratch.size() < numSamples)
            scratch.resize ((size_t) numSamples); // only if the host exceeds its announced block size
        right = scratch.data();
    }

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        if (end > pos)
            engine.render (left + pos, right + pos, end - pos);
        pos = end;
    };

    for (const auto meta : midi)
    {
        renderTo (juce::jlimit (pos, numSamples, meta.samplePosition));
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            keys.noteOn (sink, msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            keys.noteOff (sink, msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            keys.allNotesOff (sink);
        else if (msg.isSustainPedalOn() || msg.isSustainPedalOff())
        {
            // The sustain pedal works like HOLD.
            sustainPedal = msg.isSustainPedalOn();
            keys.setHold (holdParam->load() > 0.5f || sustainPedal);
            keys.update (sink);
        }
        else if (msg.isPitchWheel())
        {
            midiBend = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f;
            updatePerformance();
        }
        else if (msg.isController() && msg.getControllerNumber() == 1)
        {
            midiModWheel = (float) msg.getControllerValue() / 127.0f;
            updatePerformance();
        }
    }
    renderTo (numSamples);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (buffer.getNumChannels() == 1)
    {
        juce::FloatVectorOperations::add (left, scratch.data(), numSamples);
        juce::FloatVectorOperations::multiply (left, 0.5f, numSamples);
    }
}

int P800Processor::getNumPrograms() { return factory::numPrograms; }

void P800Processor::setCurrentProgram (int index)
{
    if (index < 0 || index >= factory::numPrograms)
        return;
    currentProgram = index;
    const auto& prog = factory::programs[index];
    for (int i = 0; i < p800::numParams; ++i)
        if (auto* param = apvts.getParameter (paramId (p800::params[i].number)))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) prog.values[i]));
}

void P800Processor::selectProgram (int index)
{
    setCurrentProgram (index);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

const juce::String P800Processor::getProgramName (int index)
{
    if (index < 0 || index >= factory::numPrograms)
        return {};
    const auto& prog = factory::programs[index];
    return juce::String (prog.number) + " " + prog.name;
}

void P800Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    int iv[p800::KeyLayer::maxChord], root = 60;
    const int n = keys.getChord (iv, root);
    juce::StringArray chord;
    for (int i = 0; i < n; ++i) chord.add (juce::String (iv[i]));
    state.setProperty ("chordMemory", chord.joinIntoString (","), nullptr);
    state.setProperty ("chordRoot", root, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void P800Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, factory::numPrograms - 1, (int) state.getProperty ("program", 0));
            const auto chord = juce::StringArray::fromTokens (state.getProperty ("chordMemory", "0,3,7").toString(), ",", "");
            int iv[p800::KeyLayer::maxChord], count = 0;
            for (const auto& t : chord)
                if (count < p800::KeyLayer::maxChord && t.trim().isNotEmpty())
                    iv[count++] = juce::jlimit (0, 48, t.getIntValue());
            if (count > 0)
                keys.setChord (iv, count, (int) state.getProperty ("chordRoot", 60));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* P800Processor::createEditor()
{
    return new P800Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new P800Processor();
}
