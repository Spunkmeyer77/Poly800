// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginEditor.h"
#include "FactoryPrograms.h"
#include "ChordNames.h"

using p800::HitKind;

namespace
{
juce::String paramId (int index) { return "p" + juce::String (p800::params[index].number); }
const juce::String kStar = juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85"));
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
    auto& pm = processor.getPresetManager();
    s.programNumber = pm.getCurrentNumber();
    s.bankName = pm.getCurrentBank().name;
    s.presetName = pm.getCurrentName();
    s.programName = s.presetName;
    s.presetFavourite = pm.isFavourite (pm.getCurrentKey());
    s.presetIsUser = pm.currentIsUser() && pm.getCurrentBank().isUser();

    // Sequencer (the view settings - visible, lane, octave - live in the editor's state).
    s.seq.playing = processor.getSeqRun();
    s.seq.recording = processor.getSeqRecording();
    s.seq.playStep = processor.getSeqPlayStep();
    s.seq.selectedStep = processor.getSeqSelectedStep();
    s.seq.hostBpm = processor.getHostBpm();
    s.seq.laneHeight = processor.getSeqLaneHeight();
    const int seqVersion = processor.getSeqVersion();
    const bool seqChanged = seqVersion != lastSeqVersion;
    if (seqChanged) { s.seq.data = processor.getSeqData(); lastSeqVersion = seqVersion; }
    s.volume = getReal ("master");
    s.tune = getReal ("tune");
    s.joyX = getReal ("joyX");
    s.joyY = getReal ("joyY");
    s.hold = getReal ("hold") > 0.5f;
    s.bendRange = juce::roundToInt (getReal ("bendRange"));
    s.voices16 = getReal ("voices16") > 0.5f;
    s.seq.playingPattern = processor.getSeqPlayingPattern();
    s.seq.blink = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
    const bool queued = s.seq.playingPattern >= 0 && s.seq.playingPattern != s.seq.data.current;
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
                      || s.chorusType != state.chorusType || s.chorusSpeed != state.chorusSpeed
                      || s.bankName != state.bankName || s.presetName != state.presetName
                      || s.presetFavourite != state.presetFavourite || s.presetIsUser != state.presetIsUser
                      || seqChanged || s.seq.playing != state.seq.playing || s.seq.recording != state.seq.recording
                      || s.seq.playStep != state.seq.playStep || s.seq.selectedStep != state.seq.selectedStep
                      || juce::roundToInt (s.seq.hostBpm) != juce::roundToInt (state.seq.hostBpm)
                      || s.seq.laneHeight != state.seq.laneHeight || s.voices16 != state.voices16
                      || s.seq.playingPattern != state.seq.playingPattern || (queued && s.seq.blink != state.seq.blink);
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
        // Two digits pick a program in the bank shown: 11..18, 21..28 ... 88.
        processor.getPresetManager().loadByNumber (number);
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
    dragHit = view.hitTest (d, state);
    if (seqMouseDown (dragHit, e, d)) { refreshState(); repaint(); return; }

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
        case HitKind::voices16: setReal ("voices16", state.voices16 ? 0.0f : 1.0f); break;
        case HitKind::bendDown: setReal ("bendRange", (float) juce::jmax (1, state.bendRange - 1)); break;
        case HitKind::bendUp:   setReal ("bendRange", (float) juce::jmin (12, state.bendRange + 1)); break;
        case HitKind::about:      state.showAbout = true; repaint(); break;
        case HitKind::aboutClose: state.showAbout = false; repaint(); break;
        case HitKind::aboutEmail:
            juce::URL ("mailto:ticcoross@gmail.com?subject=EX-800%20" + state.version).launchInDefaultBrowser();
            break;
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
        case HitKind::progPrev: case HitKind::presetPrev: processor.getPresetManager().step (-1); break;
        case HitKind::progNext: case HitKind::presetNext: processor.getPresetManager().step (+1); break;
        case HitKind::bankMenu:   showBankMenu(); break;
        case HitKind::presetMenu: showPresetMenu(); break;
        case HitKind::presetSave: askSave(); break;
        case HitKind::newBank:    askNewBank(); break;
        case HitKind::presetDelete: askDeletePreset(); break;
        case HitKind::presetFavourite:
        {
            auto& pm = processor.getPresetManager();
            pm.toggleFavourite (pm.getCurrentKey());
            break;
        }
        default: break; // sequencer clicks are handled in seqMouseDown
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
        case HitKind::seqGrid: case HitKind::seqLane: case HitKind::seqSwing: case HitKind::seqGate: case HitKind::seqBpm:
        case HitKind::seqSplitter:
            seqMouseDrag (toDesign (e.position));
            break;
        default: break;
    }
}

void P800Editor::mouseMove (const juce::MouseEvent& e)
{
    const auto kind = view.hitTest (toDesign (e.position), state).kind;
    setMouseCursor (kind == HitKind::seqSplitter ? juce::MouseCursor::UpDownResizeCursor
                    : kind == HitKind::aboutEmail ? juce::MouseCursor::PointingHandCursor
                                                  : juce::MouseCursor::NormalCursor);
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
        case HitKind::seqGrid: seqMouseUp(); break;
        default: break;
    }
    dragHit = {};
    refreshState();
}

void P800Editor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto hit = view.hitTest (toDesign (e.position), state);
    if (w.deltaY == 0.0f) return;
    if (hit.kind == HitKind::seqSwing || hit.kind == HitKind::seqGate || hit.kind == HitKind::seqBpm)
    {
        seqWheel (hit, w.deltaY * (w.isReversed ? -1.0f : 1.0f) > 0 ? 1 : -1);
        return;
    }
    if (hit.kind != HitKind::cell)
        return;
    dragAccum += w.deltaY * (w.isReversed ? -1.0f : 1.0f) * 8.0f;
    const int steps = (int) dragAccum;
    if (steps == 0)
        return;
    dragAccum -= (float) steps;
    state.selectedParam = hit.index;
    stepSelected (steps);
}

// ---- presets ------------------------------------------------------------------------------

juce::Rectangle<int> P800Editor::screenAreaFor (juce::Rectangle<float> design) const
{
    return localAreaToGlobal ((design * scale()).toNearestInt());
}

void P800Editor::showBankMenu()
{
    auto& pm = processor.getPresetManager();
    pm.refresh();
    const auto& banks = pm.getBanks();

    juce::PopupMenu menu;
    menu.addSectionHeader ("Banks");
    for (size_t i = 0; i < banks.size(); ++i)
    {
        const auto& b = banks[i];
        juce::String label = b.favourites ? kStar + " Favourites" : b.name;
        if (b.factory) label << "  (original programs)";
        label << "  [" << (int) b.presets.size() << "]";
        menu.addItem ((int) i + 1, label, true, (int) i == pm.getCurrentBankIndex());
    }
    menu.addSeparator();
    menu.addItem (90001, "New Bank...");
    menu.addItem (90002, "Delete Bank \"" + pm.getCurrentBank().name + "\"...", pm.getCurrentBank().isUser());
    menu.addSeparator();
    menu.addItem (90003, "Open Preset File...");
    menu.addItem (90004, "Show Preset Folder");

    juce::Component::SafePointer<P800Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (screenAreaFor (view.bankArea())),
        [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& m = safe->processor.getPresetManager();
            if (r == 90001)      safe->askNewBank();
            else if (r == 90002) safe->askDeleteBank();
            else if (r == 90003) safe->openPresetFile();
            else if (r == 90004) m.getRootDir().revealToUser();
            else
            {
                m.selectBank (r - 1);
                if (! m.getCurrentBank().presets.empty()) m.load (r - 1, 0); // choosing a bank plays its first preset
            }
            safe->refreshState();
        });
}

void P800Editor::showPresetMenu()
{
    auto& pm = processor.getPresetManager();
    pm.refresh();
    const auto& bank = pm.getCurrentBank();
    const auto current = pm.getCurrentKey();

    juce::PopupMenu menu;
    menu.addSectionHeader (bank.favourites ? kStar + " Favourites" : bank.name);
    for (size_t i = 0; i < bank.presets.size(); ++i)
    {
        const auto& p = bank.presets[i];
        const int number = p800::PresetManager::numberForIndex ((int) i);
        juce::String label;
        if (! bank.factory && number > 0) label << number << "  ";
        label << p.name;
        if (pm.isFavourite (p.key) && ! bank.favourites) label << "  " << kStar;
        menu.addItem ((int) i + 1, label, true, p.key == current);
    }
    if (bank.presets.empty())
        menu.addItem (-1, bank.favourites ? "(no favourites yet: press the star)" : "(empty bank: press SAVE)", false);
    menu.addSeparator();
    menu.addItem (90001, "Save...");
    menu.addItem (90002, "Delete \"" + pm.getCurrentName() + "\"...", pm.currentIsUser() && bank.isUser());

    const int bankIndex = pm.getCurrentBankIndex();
    juce::Component::SafePointer<P800Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (screenAreaFor (view.presetNameArea())),
        [safe, bankIndex] (int r)
        {
            if (safe == nullptr || r == 0) return;
            if (r == 90001)      safe->askSave();
            else if (r == 90002) safe->askDeletePreset();
            else if (r > 0)      safe->processor.getPresetManager().load (bankIndex, r - 1);
            safe->refreshState();
        });
}

void P800Editor::askSave()
{
    auto& pm = processor.getPresetManager();
    pm.refresh();
    auto banks = pm.getUserBankNames();
    if (banks.isEmpty()) banks.add (p800::PresetManager::defaultUserBank);
    const auto& cur = pm.getCurrentBank();
    const int bankIdx = juce::jmax (0, banks.indexOf (cur.isUser() ? cur.name : juce::String (p800::PresetManager::defaultUserBank)));
    // Suggest the current name for user presets; for a factory program, its name without the number.
    const auto suggestion = pm.currentIsUser() ? pm.getCurrentName()
                                               : pm.getCurrentName().fromFirstOccurrenceOf (" ", false, false);

    auto* w = new juce::AlertWindow ("Save Preset", "Save the current sound.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", suggestion, "Name:");
    w->addComboBox ("bank", banks, "Bank:");
    if (auto* box = w->getComboBoxComponent ("bank")) box->setSelectedItemIndex (bankIdx, juce::dontSendNotification);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<P800Editor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        const auto bank = w->getComboBoxComponent ("bank")->getText();
        if (name.isNotEmpty() && bank.isNotEmpty()) safe->saveTo (bank, name);
    }), true);
}

void P800Editor::saveTo (const juce::String& bank, const juce::String& name)
{
    auto& pm = processor.getPresetManager();
    if (! pm.presetExists (bank, name)) { pm.save (bank, name); refreshState(); return; }

    juce::Component::SafePointer<P800Editor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withTitle ("Replace Preset")
                                      .withMessage ("\"" + name + "\" already exists in " + bank + ". Replace it?")
                                      .withButton ("Replace")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe, bank, name] (int r)
                                  {
                                      if (r == 1 && safe != nullptr) { safe->processor.getPresetManager().save (bank, name); safe->refreshState(); }
                                  });
}

void P800Editor::askNewBank()
{
    auto* w = new juce::AlertWindow ("New Bank", "Name for the new bank:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", {});
    w->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<P800Editor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        if (! safe->processor.getPresetManager().createBank (name))
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withTitle ("New Bank")
                                              .withMessage ("Couldn't create \"" + name + "\". A bank with that name may already exist.")
                                              .withButton ("OK")
                                              .withAssociatedComponent (safe.getComponent()),
                                          nullptr);
        safe->refreshState();
    }), true);
}

void P800Editor::askDeletePreset()
{
    auto& pm = processor.getPresetManager();
    if (! pm.currentIsUser() || ! pm.getCurrentBank().isUser()) return;
    juce::Component::SafePointer<P800Editor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withTitle ("Delete Preset")
                                      .withMessage ("Delete \"" + pm.getCurrentName() + "\" from " + pm.getCurrentBank().name
                                                    + "? This can't be undone.")
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe] (int r)
                                  {
                                      if (r == 1 && safe != nullptr) { safe->processor.getPresetManager().deleteCurrentPreset(); safe->refreshState(); }
                                  });
}

void P800Editor::askDeleteBank()
{
    auto& pm = processor.getPresetManager();
    const auto& bank = pm.getCurrentBank();
    if (! bank.isUser()) return;
    const int index = pm.getCurrentBankIndex();
    const auto name = bank.name;
    const int count = (int) bank.presets.size();
    juce::Component::SafePointer<P800Editor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withTitle ("Delete Bank")
                                      .withMessage ("Delete the bank \"" + name + "\" and its " + juce::String (count)
                                                    + (count == 1 ? " preset" : " presets") + "? This can't be undone.")
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe, index] (int r)
                                  {
                                      if (r == 1 && safe != nullptr) { safe->processor.getPresetManager().deleteBank (index); safe->refreshState(); }
                                  });
}

void P800Editor::openPresetFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open EX-800 preset", processor.getPresetManager().getRootDir(),
                                                   juce::String ("*") + p800::PresetManager::extension);
    juce::Component::SafePointer<P800Editor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile()) { safe->processor.getPresetManager().loadFile (f); safe->refreshState(); }
                          });
}

// ---- sequencer screen ---------------------------------------------------------------------

bool P800Editor::seqMouseDown (const p800::Hit& hit, const juce::MouseEvent& e, juce::Point<float> d)
{
    using p800::SeqData;
    auto& q = state.seq;
    switch (hit.kind)
    {
        case HitKind::seqToggle: q.visible = ! q.visible; return true;
        case HitKind::seqPlay:   processor.setSeqRun (! processor.getSeqRun()); return true;
        case HitKind::seqLength:
        {
            const int len = hit.index == 1 ? 32 : 16;
            processor.editSeq ([len] (SeqData& s)
            {
                auto& p = s.pat();
                // Going to 32 with an empty second half: start it as a copy of the first.
                if (len == 32 && p.length == 16)
                {
                    bool emptyTail = true;
                    for (int i = 16; i < 32; ++i) emptyTail = emptyTail && p.steps[(size_t) i].empty();
                    if (emptyTail) for (int i = 0; i < 16; ++i) p.steps[(size_t) (i + 16)] = p.steps[(size_t) i];
                }
                p.length = len;
            });
            return true;
        }
        case HitKind::seqPattern:
        {
            const int idx = hit.index;
            processor.editSeq ([idx] (SeqData& s) { s.current = idx; });
            return true;
        }
        case HitKind::seqRate: showRateMenu(); return true;
        case HitKind::seqSwing: case HitKind::seqGate: case HitKind::seqBpm:
        {
            const auto& st = q.data.settings;
            seqDragValue = hit.kind == HitKind::seqSwing ? st.swing : hit.kind == HitKind::seqGate ? st.gate : st.bpm;
            dragStartY = d.y;
            if (hit.kind == HitKind::seqBpm && st.hostSync) return true; // the host sets the tempo
            return true;
        }
        case HitKind::seqSync:
        {
            const bool host = hit.index == 0;
            processor.editSeq ([host] (SeqData& s) { s.settings.hostSync = host; });
            return true;
        }
        case HitKind::seqOctUp:   q.lowNote = juce::jmin (96, q.lowNote + 12); return true;
        case HitKind::seqOctDown: q.lowNote = juce::jmax (0, q.lowNote - 12); return true;
        case HitKind::seqRec:     processor.setSeqRecording (! processor.getSeqRecording()); return true;
        case HitKind::seqCopy:
            if (! q.pasteArmed) { clipboard = q.data.pat(); q.pasteArmed = true; }
            else
            {
                const auto clip = clipboard;
                processor.editSeq ([clip] (SeqData& s) { s.pat() = clip; });
                q.pasteArmed = false;
            }
            return true;
        case HitKind::seqClear: askClearPattern(); return true;
        case HitKind::seqStepNumber: processor.setSeqSelectedStep (hit.index); return true;
        case HitKind::seqLaneTab: q.lane = hit.index; return true;
        case HitKind::seqSplitter: return true; // dragged in seqMouseDrag
        case HitKind::seqTie: case HitKind::seqRest:
        {
            // Step record: TIE stretches the notes ending here into this step; REST leaves it empty.
            const int sel = processor.getSeqSelectedStep(), len = q.data.pat().length;
            if (hit.kind == HitKind::seqTie)
                processor.editSeq ([sel] (SeqData& s) { s.tieInto (sel); });
            processor.setSeqSelectedStep ((sel + 1) % len);
            return true;
        }
        case HitKind::seqLockParam: showLockParamMenu(); return true;
        case HitKind::seqLane:
            setLaneValue (hit.index, d.y, e.mods.isPopupMenu() || e.getNumberOfClicks() > 1);
            return true;
        case HitKind::seqGrid:
        {
            const int step = hit.index, note = hit.note;
            processor.setSeqSelectedStep (step);
            int start = -1;
            const int idx = q.data.findNote (step, note, start);
            seqDragMoved = false;
            if (idx >= 0)
            {
                seqDragStart = start; seqDragIndex = idx; seqDragNew = false;
            }
            else
            {
                if (q.data.pat().steps[(size_t) step].noteCount() >= p800::kSeqMaxNotes)
                {
                    seqDragIndex = -1; // step is full (8 voices)
                    return true;
                }
                processor.editSeq ([step, note] (SeqData& s) { s.addNote (step, note); });
                state.seq.data = processor.getSeqData();
                lastSeqVersion = processor.getSeqVersion();
                seqDragStart = step;
                seqDragIndex = state.seq.data.findNote (step, note, start);
                seqDragNew = true;
            }
            return true;
        }
        default: return false;
    }
}

void P800Editor::seqMouseDrag (juce::Point<float> d)
{
    using p800::SeqData;
    const auto& q = state.seq;
    switch (dragHit.kind)
    {
        case HitKind::seqGrid:
        {
            if (seqDragIndex < 0) return;
            const auto grid = p800::PanelView::seqGridArea (q.laneHeight);
            const int length = q.data.pat().length;
            const int step = juce::jlimit (0, length - 1, (int) ((d.x - grid.getX()) / (grid.getWidth() / (float) length)));
            const int newLen = juce::jmax (1, step - seqDragStart + 1);
            const int start = seqDragStart, idx = seqDragIndex;
            if (newLen != (int) q.data.pat().steps[(size_t) start].notes[(size_t) idx].length)
            {
                seqDragMoved = true;
                processor.editSeq ([start, idx, newLen] (SeqData& s) { s.setNoteLength (start, idx, newLen); });
            }
            break;
        }
        case HitKind::seqLane:
        {
            const auto lane = p800::PanelView::seqLaneValues (q.laneHeight);
            const int length = q.data.pat().length;
            const int step = juce::jlimit (0, length - 1, (int) ((d.x - lane.getX()) / (lane.getWidth() / (float) length)));
            setLaneValue (step, d.y, false);
            break;
        }
        case HitKind::seqSplitter:
            processor.setSeqLaneHeight (p800::PanelView::clampLane (juce::roundToInt (772.0f - d.y)));
            break;
        case HitKind::seqSwing: case HitKind::seqGate: case HitKind::seqBpm:
        {
            const int delta = juce::roundToInt ((dragStartY - d.y) / 3.0f);
            const auto kind = dragHit.kind;
            const int v0 = seqDragValue;
            processor.editSeq ([kind, v0, delta] (SeqData& s)
            {
                if (kind == HitKind::seqSwing)      s.settings.swing = juce::jlimit (50, 75, v0 + delta);
                else if (kind == HitKind::seqGate)  s.settings.gate = juce::jlimit (5, 100, v0 + delta);
                else if (! s.settings.hostSync)     s.settings.bpm = juce::jlimit (30, 300, v0 + delta);
            });
            break;
        }
        default: break;
    }
    refreshState();
}

void P800Editor::seqMouseUp()
{
    // A click (no drag) on an existing note removes it.
    if (dragHit.kind == HitKind::seqGrid && seqDragIndex >= 0 && ! seqDragNew && ! seqDragMoved)
    {
        const int start = seqDragStart, idx = seqDragIndex;
        processor.editSeq ([start, idx] (p800::SeqData& s) { s.removeNote (start, idx); });
    }
    seqDragIndex = -1;
}

void P800Editor::seqWheel (const p800::Hit& hit, int delta)
{
    const auto kind = hit.kind;
    processor.editSeq ([kind, delta] (p800::SeqData& s)
    {
        if (kind == HitKind::seqSwing)      s.settings.swing = juce::jlimit (50, 75, s.settings.swing + delta);
        else if (kind == HitKind::seqGate)  s.settings.gate = juce::jlimit (5, 100, s.settings.gate + delta);
        else if (! s.settings.hostSync)     s.settings.bpm = juce::jlimit (30, 300, s.settings.bpm + delta);
    });
    refreshState();
}

void P800Editor::setLaneValue (int step, float y, bool reset)
{
    const auto lane = p800::PanelView::seqLaneValues (state.seq.laneHeight);
    const float f = juce::jlimit (0.0f, 1.0f, (lane.getBottom() - y) / lane.getHeight());
    const int which = state.seq.lane, param = state.seq.lockParam;
    processor.editSeq ([=] (p800::SeqData& s)
    {
        auto& st = s.pat().steps[(size_t) step];
        if (which == 0)      st.probability = (uint8_t) (reset ? 100 : juce::jlimit (0, 100, juce::roundToInt (f * 100.0f)));
        else if (which == 1) st.gate = (uint8_t) (reset ? 100 : juce::jlimit (1, 100, juce::roundToInt (f * 100.0f)));
        else if (reset)      s.clearLock (step, param);
        else
        {
            const auto spec = p800::lockSpec (param);
            s.setLock (step, param, spec.minValue + juce::roundToInt (f * (float) (spec.maxValue - spec.minValue)));
        }
    });
    refreshState();
}

void P800Editor::showRateMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Step length");
    for (int i = 0; i < p800::numSeqRates; ++i)
        menu.addItem (i + 1, p800::seqRates[i].name, true, state.seq.data.settings.rate == i);
    juce::Component::SafePointer<P800Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (screenAreaFor (view.seqRateArea())),
        [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->processor.editSeq ([r] (p800::SeqData& s) { s.settings.rate = r - 1; });
            safe->refreshState();
        });
}

void P800Editor::showLockParamMenu()
{
    const auto& d = state.seq.data;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Parameter to lock (" + juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f")) + " = has locks)");
    const char* groups[][2] = { { "DCO 1", "1" }, { "DCO 2", "2" }, { "Noise", "33" }, { "VCF / Chorus", "4" },
                                { "DEG 1", "5" }, { "DEG 2", "6" }, { "DEG 3", "7" }, { "MG", "8" } };
    for (auto& grp : groups)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < p800::numParams; ++i)
        {
            const auto num = juce::String (p800::params[i].number);
            const juce::String prefix (grp[1]);
            const bool inGroup = prefix == "33" ? num == "33"
                               : prefix == "2" ? (num.startsWith ("2") || num == "31" || num == "32")
                               : (num.startsWith (prefix) && num != "31" && num != "32" && num != "33");
            if (! inGroup) continue;
            juce::String label = num + "  " + p800::params[i].name;
            if (d.patternHasLocksFor (i)) label << "  " << juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f"));
            sub.addItem (i + 1, label, true, i == state.seq.lockParam);
        }
        menu.addSubMenu (grp[0], sub);
    }
    {
        // The other assignable controls: effect, volume, tune, joystick.
        juce::PopupMenu sub;
        for (int i = 0; i < p800::numLockExtras; ++i)
        {
            const int slot = p800::numParams + i;
            juce::String label = p800::lockExtras[i].name;
            if (d.patternHasLocksFor (slot)) label << "  " << juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f"));
            sub.addItem (slot + 1, label, true, slot == state.seq.lockParam);
        }
        menu.addSubMenu ("Effect / Performance", sub);
    }
    menu.addSeparator();
    menu.addItem (1000, "Clear locks for " + juce::String (p800::lockSpec (state.seq.lockParam).name) + " in this pattern",
                  d.patternHasLocksFor (state.seq.lockParam));
    juce::Component::SafePointer<P800Editor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (screenAreaFor (p800::PanelView::seqLockParamArea (state.seq.laneHeight))),
        [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r == 1000)
            {
                const int param = safe->state.seq.lockParam;
                safe->processor.editSeq ([param] (p800::SeqData& s) { s.clearLocksFor (param); });
            }
            else safe->state.seq.lockParam = r - 1;
            safe->refreshState();
            safe->repaint();
        });
}

void P800Editor::askClearPattern()
{
    const auto name = juce::String::charToString ((juce::juce_wchar) ('A' + state.seq.data.current));
    juce::Component::SafePointer<P800Editor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withTitle ("Clear Pattern")
                                      .withMessage ("Clear all notes and parameter locks in pattern " + name + "?")
                                      .withButton ("Clear")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe] (int r)
                                  {
                                      if (r == 1 && safe != nullptr) { safe->processor.editSeq ([] (p800::SeqData& s) { s.clearPattern(); }); safe->refreshState(); }
                                  });
}
