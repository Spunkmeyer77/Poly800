// SPDX-License-Identifier: GPL-3.0-or-later
// Preset manager test: factory bank, user banks, save/load round trip, overwrite,
// hardware numbering (11..88), previous/next, favourites, delete preset and bank.
// Uses a temporary folder and a fake synth, so no host is needed.
#include "PresetManager.h"
#include <cstdio>

namespace
{
struct FakeSynth : p800::PresetTarget
{
    int cutoff = 0, chorusType = 0;
    juce::ValueTree captureSound() override
    {
        juce::ValueTree t ("Sound");
        t.setProperty ("p41", cutoff, nullptr);
        t.setProperty ("chorusType", chorusType, nullptr);
        return t;
    }
    void restoreSound (const juce::ValueTree& t) override
    {
        cutoff = (int) t.getProperty ("p41", -1);
        chorusType = (int) t.getProperty ("chorusType", -1);
    }
    void loadFactory (int index) override { cutoff = 100 + index; chorusType = 0; }
    int numFactory() const override { return 64; }
    juce::String factoryName (int index) const override { return index == 0 ? "Strings" : "Program " + juce::String (index); }
};

int fails = 0;
void check (bool ok, const char* what)
{
    std::printf ("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += ok ? 0 : 1;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI* none = nullptr; juce::ignoreUnused (none);
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("p800_preset_test_" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
    dir.deleteRecursively();
    {
        FakeSynth synth;
        p800::PresetManager pm (synth, dir);
        using PM = p800::PresetManager;

        check (PM::numberForIndex (0) == 11 && PM::numberForIndex (7) == 18 && PM::numberForIndex (8) == 21
               && PM::numberForIndex (63) == 88 && PM::numberForIndex (64) == -1, "numbering: 11..18, 21.. 88");
        check (PM::indexForNumber (24) == 11 && PM::indexForNumber (19) == -1 && PM::indexForNumber (90) == -1, "number to index");

        check (pm.getBanks().size() == 3 && pm.getBanks()[0].factory && pm.getBanks()[1].name == "User"
               && pm.getBanks().back().favourites, "banks: Factory, User, Favourites");
        check (pm.getBanks()[0].presets.size() == 64 && pm.getBanks()[0].presets[0].name == "11 Strings",
               "factory bank holds the 64 original programs");

        check (pm.load (0, 13) && synth.cutoff == 113 && pm.getCurrentNumber() == 26, "load factory 26");
        pm.step (+1);
        check (synth.cutoff == 114 && pm.getCurrentNumber() == 27, "next");
        pm.step (-2);
        check (synth.cutoff == 112, "previous");
        check (pm.loadByNumber (88) && synth.cutoff == 163, "keypad number 88 loads the last factory program");

        synth.cutoff = 42; synth.chorusType = 2;
        check (pm.save ("User", "Bright Pad") && pm.currentIsUser() && pm.getCurrentName() == "Bright Pad",
               "save into the User bank");
        check (pm.getCurrentBank().name == "User" && pm.getCurrentNumber() == 11, "saved preset is User 11");
        synth.cutoff = 0; synth.chorusType = 0;
        check (pm.load (pm.indexOfBank ("User"), 0) && synth.cutoff == 42 && synth.chorusType == 2,
               "load restores what was saved");

        check (pm.createBank ("Drexciya") && pm.getCurrentBank().name == "Drexciya" && pm.getCurrentBank().presets.empty(),
               "new bank is created, selected and empty");
        check (! pm.createBank ("Drexciya") && ! pm.createBank ("Factory") && ! pm.createBank ("  "),
               "duplicate, reserved or blank bank names are refused");
        synth.cutoff = 7;  pm.save ("Drexciya", "Aqua Bass");
        synth.cutoff = 8;  pm.save ("Drexciya", "Bubble Lead");
        check (pm.getUserBankNames().size() == 2 && pm.getCurrentBank().presets.size() == 2, "two presets in the new bank");
        check (pm.presetExists ("Drexciya", "Aqua Bass"), "presetExists");
        synth.cutoff = 9;  pm.save ("Drexciya", "Aqua Bass");
        check (pm.getCurrentBank().presets.size() == 2, "saving under an existing name overwrites it");
        pm.load (pm.getCurrentBankIndex(), 0);
        check (synth.cutoff == 9, "the overwritten preset has the new sound");
        pm.step (+1);
        check (synth.cutoff == 8 && pm.getCurrentName() == "Bubble Lead", "next stays inside the user bank");
        pm.step (+1);
        check (synth.cutoff == 9, "next wraps around in the bank");

        const auto key = pm.getCurrentKey();
        pm.toggleFavourite (key);
        pm.toggleFavourite ("F:0");
        check (pm.isFavourite (key) && pm.getBanks().back().presets.size() == 2, "favourites bank lists starred presets");
        {
            FakeSynth s2;
            p800::PresetManager again (s2, dir);
            check (again.isFavourite (key) && again.getBanks().size() == 4, "favourites and banks persist");
        }

        pm.setCurrentKey ("U:Drexciya/Bubble Lead");
        check (pm.getCurrentBank().name == "Drexciya" && pm.getCurrentName() == "Bubble Lead", "restoring a session key");

        pm.load (pm.indexOfBank ("Drexciya"), 0);
        check (pm.deleteCurrentPreset() && ! pm.presetExists ("Drexciya", "Aqua Bass") && ! pm.isFavourite (key),
               "delete preset removes the file and its star");
        pm.load (0, 0);
        check (! pm.deleteCurrentPreset(), "factory presets cannot be deleted");

        check (pm.deleteBank (pm.indexOfBank ("Drexciya")) && pm.indexOfBank ("Drexciya") < 0, "delete bank");
        check (! pm.deleteBank (0), "the factory bank cannot be deleted");
        check (pm.indexOfBank ("User") > 0, "the User bank is always there");

        const auto outside = dir.getSiblingFile ("p800_outside.p800preset");
        dir.getChildFile ("User").getChildFile ("Bright Pad.p800preset").copyFileTo (outside);
        synth.cutoff = 0;
        check (pm.loadFile (outside) && synth.cutoff == 42, "open a preset file from elsewhere");
        outside.deleteFile();
    }
    dir.deleteRecursively();
    std::printf (fails ? "\n%d failure(s)\n" : "\nall preset checks pass\n", fails);
    return fails ? 1 : 0;
}
