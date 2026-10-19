// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginProcessor.h"
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
    engine.allNotesOff();
}

bool P800Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
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

    engine.setParams (currentCoreParams());

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
            engine.noteOn (msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            engine.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.allNotesOff();
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
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* P800Processor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new P800Processor();
}
