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
                      || s.chorusType != state.chorusType || s.chorusSpeed != state.chorusSpeed
                      || s.bankName != state.bankName || s.presetName != state.presetName
                      || s.presetFavourite != state.presetFavourite || s.presetIsUser != state.presetIsUser;
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
    chooser = std::make_unique<juce::FileChooser> ("Open P-800 preset", processor.getPresetManager().getRootDir(),
                                                   juce::String ("*") + p800::PresetManager::extension);
    juce::Component::SafePointer<P800Editor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile()) { safe->processor.getPresetManager().loadFile (f); safe->refreshState(); }
                          });
}
