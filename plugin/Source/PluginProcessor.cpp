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
    voices16Param = apvts.getRawParameterValue ("voices16");
    holdParam = apvts.getRawParameterValue ("hold");
    keyModeParam = apvts.getRawParameterValue ("keyMode");
    chorusTypeParam = apvts.getRawParameterValue ("chorusType");
    chorusSpeedParam = apvts.getRawParameterValue ("chorusSpeed");
    recStartPos.fill (-1.0);
    presets = std::make_unique<p800::PresetManager> (*this);
    setCurrentProgram (0);
    startTimerHz (30);
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

    // EX-800: double the polyphony - 16 voices in WHOLE mode, 8 in DOUBLE.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "voices16", 1 }, "16 Voices", false));

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
    player.prepare (sampleRate);
    playerVersion = 0;
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

// A lock slot's value, or the fallback when the playing step doesn't lock it.
static float lockedOr (const int16_t* locks, const char* extraId, float scale, float fallback)
{
    const int slot = p800::lockSlotOf (extraId);
    return locks != nullptr && slot >= 0 && locks[slot] != p800::kNoLock ? (float) locks[slot] * scale : fallback;
}

void P800Processor::updatePerformance (const int16_t* locks)
{
    const float range = bendRangeParam->load();
    const float x = juce::jlimit (-1.0f, 1.0f, lockedOr (locks, "joyX", 0.01f, joyXParam->load()) + midiBend);
    const float y = lockedOr (locks, "joyY", 0.01f, joyYParam->load());
    // Mod wheel works like pushing the joystick up.
    const float dco = juce::jlimit (0.0f, 1.0f, juce::jmax (0.0f, y) + midiModWheel);
    const float vcf = juce::jmax (0.0f, -y);
    engine.setPerformance (x * range, dco, vcf);
}

Poly800Params P800Processor::currentCoreParams (const int16_t* locks) const
{
    int values[p800::numParams];
    for (int i = 0; i < p800::numParams; ++i)
        values[i] = locks != nullptr && locks[i] != p800::kNoLock ? (int) locks[i]   // sequencer parameter lock
                                                                   : juce::roundToInt (paramValues[(size_t) i]->load());

    Poly800Params p;
    poly800_params_default (&p);
    p800::fillCoreParams (p, values);
    p.master_gain = lockedOr (locks, "master", 0.01f, masterParam->load()) * p800::volumeToGain;
    p.tune_cents = lockedOr (locks, "tune", 1.0f, tuneParam->load());
    return p;
}

// Everything a step's parameter locks can change: the sound, the effect and the joystick.
void P800Processor::applyLocks (const int16_t* locks)
{
    const int type = juce::roundToInt (lockedOr (locks, "chorusType", 1.0f, chorusTypeParam->load()));
    engine.setEffect (type, lockedOr (locks, "chorusSpeed", 0.01f, chorusSpeedParam->load()));
    engine.setParams (currentCoreParams (locks));
    updatePerformance (locks);
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

    const auto coreParams = currentCoreParams();
    // Keep the playing step's parameter locks for the whole step: they are re-applied at the
    // start of every block, not only when the step begins (in 0.5.0 they lasted one block).
    applyLocks (player.activeLocks());

    // Key assign. DOUBLE mode (P18 = 2) has 4 voices, WHOLE has 8; "16 Voices" doubles both.
    const bool sixteen = voices16Param->load() > 0.5f;
    engine.setVoiceCount (sixteen);
    keys.setMode (juce::roundToInt (keyModeParam->load()));
    keys.setHold (holdParam->load() > 0.5f || sustainPedal);
    keys.setVoiceLimit ((coreParams.dco_mode == 2 ? 4 : 8) * (sixteen ? 2 : 1));
    keys.update (sink);

    // Sequencer: pick up edits, read the host transport, schedule this block's steps.
    if (const int v = seqVersion.load(); v != playerVersion)
    {
        const juce::ScopedTryLock tl (seqLock);
        if (tl.isLocked()) { player.setData (seqModel); playerVersion = v; }
    }
    p800::SeqTransport transport;
    if (auto* ph = getPlayHead())
        if (const auto info = ph->getPosition())
        {
            transport.hasHost = info->getPpqPosition().hasValue();
            transport.hostPlaying = info->getIsPlaying();
            transport.ppq = info->getPpqPosition().orFallback (0.0);
            transport.bpm = info->getBpm().orFallback (120.0);
            hostBpm.store (info->getBpm().orFallback (0.0));
        }
    const int numSeqEvents = player.process (numSamples, transport, seqRun.load(), seqEvents.data());
    int seqIndex = 0;

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

    // Sequencer events up to (and including) 'until': notes go straight to the engine,
    // step changes apply that step's parameter locks.
    auto runSeqUntil = [&] (int until)
    {
        while (seqIndex < numSeqEvents && seqEvents[(size_t) seqIndex].offset <= until)
        {
            const auto& e = seqEvents[(size_t) seqIndex++];
            renderTo (juce::jlimit (pos, numSamples, e.offset));
            if (e.type == p800::SeqEvent::noteOn)       engine.noteOn (e.note, e.velocity);
            else if (e.type == p800::SeqEvent::noteOff) engine.noteOff (e.note);
            else                                        applyLocks (player.activeLocks());
        }
    };

    const bool recording = seqRecording.load();
    for (const auto meta : midi)
    {
        runSeqUntil (meta.samplePosition);
        renderTo (juce::jlimit (pos, numSamples, meta.samplePosition));
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            keys.noteOn (sink, msg.getNoteNumber(), msg.getVelocity()); // records what it plays (see EngineSink)
            if (recording && ! player.isRunning())
                ++heldForStepRec; // step record: count the keys held, to know when to move on
        }
        else if (msg.isNoteOff())
        {
            keys.noteOff (sink, msg.getNoteNumber());
            if (recording && ! player.isRunning() && heldForStepRec > 0 && --heldForStepRec == 0)
                pushRec ({ 1, 0, 0, 0 }); // all keys up: move to the next step
        }
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
    runSeqUntil (numSamples);
    renderTo (numSamples);
    seqPlayStep.store (player.currentStep());
    seqPlayingPattern.store (player.isRunning() ? player.playingPattern() : -1);

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
        setParamValue (paramId (p800::params[i].number), (float) prog.values[i]);
    // The original programs are heard with the default effect (P48 says whether it is on).
    setParamValue ("chorusType", 0.0f);
    setParamValue ("chorusSpeed", 0.4f);
    if (presets != nullptr)
        presets->noteFactoryProgram (index);
}

void P800Processor::setParamValue (const juce::String& id, float value)
{
    if (auto* param = apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

const juce::StringArray& P800Processor::soundParamIds()
{
    static const juce::StringArray ids = []
    {
        juce::StringArray a;
        for (const auto& s : p800::params) a.add (paramId (s.number));
        a.addArray ({ "chorusType", "chorusSpeed", "bendRange", "keyMode" });
        return a;
    }();
    return ids;
}

juce::ValueTree P800Processor::captureSound()
{
    juce::ValueTree t ("Sound");
    for (const auto& id : soundParamIds())
        if (auto* param = apvts.getParameter (id))
            t.setProperty (id, param->convertFrom0to1 (param->getValue()), nullptr);
    int iv[p800::KeyLayer::maxChord], root = 60;
    const int n = keys.getChord (iv, root);
    juce::StringArray chord;
    for (int i = 0; i < n; ++i) chord.add (juce::String (iv[i]));
    t.setProperty ("chordMemory", chord.joinIntoString (","), nullptr);
    t.setProperty ("chordRoot", root, nullptr);
    t.setProperty ("seq", juce::String (getSeqData().serialise()), nullptr); // patterns travel with the preset
    return t;
}

void P800Processor::restoreSound (const juce::ValueTree& t)
{
    for (const auto& id : soundParamIds())
        if (t.hasProperty (id))
            setParamValue (id, (float) t.getProperty (id));
    const auto chord = juce::StringArray::fromTokens (t.getProperty ("chordMemory", "").toString(), ",", "");
    int iv[p800::KeyLayer::maxChord], count = 0;
    for (const auto& c : chord)
        if (count < p800::KeyLayer::maxChord && c.trim().isNotEmpty())
            iv[count++] = juce::jlimit (0, 48, c.getIntValue());
    if (count > 0)
        keys.setChord (iv, count, (int) t.getProperty ("chordRoot", 60));
    // Presets saved with a sequence bring it back; others (factory, older) keep the current one.
    if (t.hasProperty ("seq"))
    {
        const auto text = t.getProperty ("seq").toString().toStdString();
        editSeq ([&text] (p800::SeqData& d) { d.deserialise (text); });
    }
}

p800::SeqData P800Processor::getSeqData() const
{
    const juce::ScopedLock sl (seqLock);
    return seqModel;
}

void P800Processor::editSeq (const std::function<void (p800::SeqData&)>& change)
{
    {
        const juce::ScopedLock sl (seqLock);
        change (seqModel);
    }
    seqVersion.fetch_add (1);
}

void P800Processor::recordPlayedNote (int note)
{
    if (! seqRecording.load()) return;
    if (player.isRunning())
    {
        // Live: quantise to the nearest step.
        pushRec ({ 0, player.stepAt (player.position()), note, 1 });
    }
    else
    {
        // Step record: everything sounding together goes on the selected step.
        pushRec ({ 0, seqSelectedStep.load(), note, 1 });
    }
    // Remember where it started, so a held note gets its full length when it is released.
    if (player.isRunning())
    {
        recStartPos[(size_t) note] = player.position();
        recStartStep[(size_t) note] = player.stepAt (player.position());
    }
    else recStartPos[(size_t) note] = -1.0;
}

void P800Processor::recordReleasedNote (int note)
{
    const double start = recStartPos[(size_t) note];
    recStartPos[(size_t) note] = -1.0;
    if (! seqRecording.load() || ! player.isRunning() || start < 0.0) return;
    // Live: the note lasts as many steps as it was held (at least one).
    const int steps = juce::jmax (1, (int) std::llround (player.position() - start));
    if (steps > 1) pushRec ({ 2, recStartStep[(size_t) note], note, steps });
}

void P800Processor::pushRec (RecEvent e)
{
    int s1, n1, s2, n2;
    recFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) recBuffer[(size_t) s1] = e;
    else if (n2 > 0) recBuffer[(size_t) s2] = e;
    recFifo.finishedWrite (n1 + n2);
}

void P800Processor::timerCallback()
{
    int s1, n1, s2, n2;
    recFifo.prepareToRead (recFifo.getNumReady(), s1, n1, s2, n2);
    if (n1 + n2 == 0) return;
    std::vector<RecEvent> events;
    for (int i = 0; i < n1; ++i) events.push_back (recBuffer[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) events.push_back (recBuffer[(size_t) (s2 + i)]);
    recFifo.finishedRead (n1 + n2);

    editSeq ([this, &events] (p800::SeqData& d)
    {
        for (const auto& e : events)
        {
            const int len = d.pat().length;
            if (e.type == 0)
                d.addNote (juce::jlimit (0, len - 1, e.step), e.note);
            else if (e.type == 2)
                d.setLengthOf (juce::jlimit (0, len - 1, e.step), e.note, e.length);
            else
                seqSelectedStep.store ((seqSelectedStep.load() + 1) % len);
        }
    });
}

int P800Processor::numFactory() const { return factory::numPrograms; }

juce::String P800Processor::factoryName (int index) const
{
    return juce::isPositiveAndBelow (index, factory::numPrograms) ? juce::String (factory::programs[index].name) : juce::String();
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
    state.setProperty ("presetKey", presets->getCurrentKey(), nullptr);
    state.setProperty ("seq", juce::String (getSeqData().serialise()), nullptr);
    state.setProperty ("seqLaneHeight", seqLaneHeight.load(), nullptr);
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
            seqLaneHeight.store ((int) state.getProperty ("seqLaneHeight", 80));
            if (state.hasProperty ("seq"))
            {
                const auto text = state.getProperty ("seq").toString().toStdString();
                editSeq ([&text] (p800::SeqData& d) { d.deserialise (text); });
            }
            apvts.replaceState (state);
            presets->setCurrentKey (state.getProperty ("presetKey", "F:" + juce::String (currentProgram)).toString());
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
