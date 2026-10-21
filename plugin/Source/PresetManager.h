// SPDX-License-Identifier: GPL-3.0-or-later
// EX-800 preset management: banks of presets, based on Polytwelve's PresetManager.
//
//  - "Factory" bank: the 64 original Poly-800 programs (11..88), built in, read-only.
//  - User banks: one folder each, holding *.p800preset files.
//  - "Favourites": every preset marked with the star, from any bank.
//
// User banks live in:
//   macOS    ~/Library/Audio/Presets/Ticco Ross/EX-800/<bank>
//   Windows  Documents\Ticco Ross\EX-800\Presets\<bank>
//   Linux    ~/.local/share/Ticco Ross/EX-800/Presets/<bank>
//
// Presets in a bank are numbered like the hardware: 11..18, 21..28 ... 88 (64 per
// number range; more are allowed but have no number). Message thread only, except
// noteFactoryProgram(), which a host may call from elsewhere.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <vector>

namespace p800
{
// What the preset manager loads into and saves from (the processor in the plugin).
struct PresetTarget
{
    virtual ~PresetTarget() = default;
    virtual juce::ValueTree captureSound() = 0;                 // a preset's content
    virtual void restoreSound (const juce::ValueTree&) = 0;
    virtual void loadFactory (int index) = 0;                   // 0..63
    virtual int numFactory() const = 0;
    virtual juce::String factoryName (int index) const = 0;     // "Strings"
};

class PresetManager : public juce::ChangeBroadcaster
{
public:
    static constexpr const char* extension = ".p800preset";
    static constexpr const char* defaultUserBank = "User";

    struct Preset
    {
        juce::String key;       // "F:<index>" or "U:<bank>/<name>"
        juce::String name;
        int factoryIndex = -1;
        juce::File file;
    };

    struct Bank
    {
        juce::String name;
        bool factory = false, favourites = false;
        juce::File dir;
        std::vector<Preset> presets;
        bool isUser() const { return ! factory && ! favourites; }
    };

    explicit PresetManager (PresetTarget&, juce::File rootDir = defaultRootDir());
    static juce::File defaultRootDir();
    juce::File getRootDir() const { return root; }

    void refresh();
    const std::vector<Bank>& getBanks() const { return banks; }
    int getCurrentBankIndex() const { return currentBank; }
    const Bank& getCurrentBank() const { return banks[(size_t) currentBank]; }
    void selectBank (int index);                     // shows the bank; loads nothing
    int indexOfBank (const juce::String& name) const;
    juce::StringArray getUserBankNames() const;

    // Current preset
    juce::String getCurrentKey() const;
    juce::String getCurrentName() const;             // "11 Strings" or the user name
    int getCurrentNumber() const;                    // 11..88 in the current bank, or -1
    bool currentIsUser() const { return getCurrentKey().startsWith ("U:"); }
    void setCurrentKey (const juce::String& key);    // restoring the plugin state

    // Loading
    bool load (int bankIndex, int presetIndex);
    bool loadByNumber (int number);                  // 11..88 within the current bank
    void step (int delta);                           // previous / next within the current bank
    bool loadFile (const juce::File&);               // any .p800preset
    void noteFactoryProgram (int index);             // host chose a factory program

    // Saving and organising
    bool save (const juce::String& bankName, const juce::String& name); // creates the bank if needed
    bool presetExists (const juce::String& bankName, const juce::String& name) const;
    bool createBank (const juce::String& name);
    bool deleteCurrentPreset();                      // user presets only
    bool deleteBank (int bankIndex);                 // user banks only, with their presets

    // Favourites
    bool isFavourite (const juce::String& key) const { return favourites.contains (key); }
    void toggleFavourite (const juce::String& key);

    static juce::String legalName (const juce::String&);
    static int numberForIndex (int index);           // 0 -> 11, 7 -> 18, 8 -> 21 ... 63 -> 88
    static int indexForNumber (int number);          // inverse, or -1

private:
    PresetTarget& target;
    juce::File root;
    std::vector<Bank> banks;
    juce::StringArray favourites;
    int currentBank = 0;
    juce::String currentKey { "F:0" };
    juce::String fileName;                           // name shown for a loadFile() preset
    mutable juce::CriticalSection lock;

    juce::File favouritesFile() const { return root.getChildFile ("favourites.txt"); }
    void loadFavourites();
    void saveFavourites() const;
    void rebuildFavouritesBank();
    int findInBank (int bankIndex, const juce::String& key) const;
    void followCurrentKey();                         // keep currentBank showing the current preset's bank if it is open

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
} // namespace p800
