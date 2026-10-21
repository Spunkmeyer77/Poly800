// SPDX-License-Identifier: GPL-3.0-or-later
#include "PresetManager.h"

namespace p800
{
PresetManager::PresetManager (PresetTarget& t, juce::File rootDir) : target (t), root (std::move (rootDir))
{
    root.createDirectory();
    root.getChildFile (defaultUserBank).createDirectory();
    loadFavourites();
    refresh();
    currentBank = 0;
}

juce::File PresetManager::defaultRootDir()
{
    // EX-800 (named P-800 up to 0.5.2): presets saved under the old name are moved over once.
    auto dirFor = [] (const char* product)
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                 .getChildFile ("Library/Audio/Presets/Ticco Ross").getChildFile (product);
       #elif JUCE_WINDOWS
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Ticco Ross").getChildFile (product).getChildFile ("Presets");
       #else
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                 .getChildFile (".local/share/Ticco Ross").getChildFile (product).getChildFile ("Presets");
       #endif
    };
    const auto dir = dirFor ("EX-800"), old = dirFor ("P-800");
    if (! dir.exists() && old.isDirectory())
    {
        dir.getParentDirectory().createDirectory();
        if (! old.moveFileTo (dir))
            old.copyDirectoryTo (dir);
    }
    return dir;
}

juce::String PresetManager::legalName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, 64).trim();
}

int PresetManager::numberForIndex (int index)
{
    if (index < 0 || index >= 64) return -1;
    return (index / 8 + 1) * 10 + index % 8 + 1;
}

int PresetManager::indexForNumber (int number)
{
    const int tens = number / 10, ones = number % 10;
    if (tens < 1 || tens > 8 || ones < 1 || ones > 8) return -1;
    return (tens - 1) * 8 + (ones - 1);
}

// ---- banks ------------------------------------------------------------------------------

void PresetManager::refresh()
{
    const juce::ScopedLock sl (lock);
    const juce::String shownBank = banks.empty() ? juce::String() : banks[(size_t) currentBank].name;
    banks.clear();

    Bank factory;
    factory.name = "Factory";
    factory.factory = true;
    for (int i = 0; i < target.numFactory(); ++i)
    {
        Preset p;
        p.factoryIndex = i;
        p.key = "F:" + juce::String (i);
        p.name = juce::String (numberForIndex (i)) + " " + target.factoryName (i);
        factory.presets.push_back (p);
    }
    banks.push_back (std::move (factory));

    auto dirs = root.findChildFiles (juce::File::findDirectories, false, "*");
    std::sort (dirs.begin(), dirs.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    for (const auto& d : dirs)
    {
        Bank b;
        b.name = d.getFileName();
        b.dir = d;
        auto files = d.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
        std::sort (files.begin(), files.end(), [] (const juce::File& x, const juce::File& y)
                   { return x.getFileNameWithoutExtension().compareNatural (y.getFileNameWithoutExtension()) < 0; });
        for (const auto& f : files)
        {
            Preset p;
            p.file = f;
            p.name = f.getFileNameWithoutExtension();
            p.key = "U:" + b.name + "/" + p.name;
            b.presets.push_back (p);
        }
        banks.push_back (std::move (b));
    }

    Bank fav;
    fav.name = "Favourites";
    fav.favourites = true;
    banks.push_back (std::move (fav));
    rebuildFavouritesBank();

    currentBank = juce::jmax (0, indexOfBank (shownBank));
    sendChangeMessage();
}

void PresetManager::rebuildFavouritesBank()
{
    auto& fav = banks.back();
    fav.presets.clear();
    for (const auto& key : favourites)
        for (size_t b = 0; b + 1 < banks.size(); ++b)
            for (const auto& p : banks[b].presets)
                if (p.key == key)
                {
                    Preset copy = p;
                    if (banks[b].isUser()) copy.name = banks[b].name + " / " + p.name;
                    fav.presets.push_back (copy);
                }
}

int PresetManager::indexOfBank (const juce::String& name) const
{
    for (size_t i = 0; i < banks.size(); ++i)
        if (banks[i].name == name) return (int) i;
    return -1;
}

juce::StringArray PresetManager::getUserBankNames() const
{
    juce::StringArray names;
    for (const auto& b : banks)
        if (b.isUser()) names.add (b.name);
    return names;
}

void PresetManager::selectBank (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) banks.size())) return;
    currentBank = index;
    sendChangeMessage();
}

int PresetManager::findInBank (int bankIndex, const juce::String& key) const
{
    if (! juce::isPositiveAndBelow (bankIndex, (int) banks.size())) return -1;
    const auto& ps = banks[(size_t) bankIndex].presets;
    for (size_t i = 0; i < ps.size(); ++i)
        if (ps[i].key == key) return (int) i;
    return -1;
}

void PresetManager::followCurrentKey()
{
    if (findInBank (currentBank, currentKey) >= 0) return; // still visible where we are
    if (currentKey.startsWith ("F:"))
        currentBank = 0;
    else if (currentKey.startsWith ("U:"))
    {
        const int b = indexOfBank (currentKey.fromFirstOccurrenceOf (":", false, false).upToLastOccurrenceOf ("/", false, false));
        if (b >= 0) currentBank = b;
    }
}

// ---- current preset ---------------------------------------------------------------------

juce::String PresetManager::getCurrentKey() const
{
    const juce::ScopedLock sl (lock);
    return currentKey;
}

juce::String PresetManager::getCurrentName() const
{
    const juce::ScopedLock sl (lock);
    for (const auto& b : banks)
        for (const auto& p : b.presets)
            if (p.key == currentKey && ! b.favourites)
                return p.name;
    if (fileName.isNotEmpty()) return fileName;
    return currentKey.fromLastOccurrenceOf ("/", false, false).fromFirstOccurrenceOf (":", false, false);
}

int PresetManager::getCurrentNumber() const
{
    const juce::ScopedLock sl (lock);
    return numberForIndex (findInBank (currentBank, currentKey));
}

void PresetManager::setCurrentKey (const juce::String& key)
{
    {
        const juce::ScopedLock sl (lock);
        currentKey = key.isNotEmpty() ? key : juce::String ("F:0");
        fileName.clear();
        followCurrentKey();
    }
    sendChangeMessage();
}

void PresetManager::noteFactoryProgram (int index)
{
    {
        const juce::ScopedLock sl (lock);
        currentKey = "F:" + juce::String (index);
        fileName.clear();
        if (findInBank (currentBank, currentKey) < 0) currentBank = 0;
    }
    sendChangeMessage();
}

// ---- loading ----------------------------------------------------------------------------

bool PresetManager::load (int bankIndex, int presetIndex)
{
    if (! juce::isPositiveAndBelow (bankIndex, (int) banks.size())) return false;
    const auto& ps = banks[(size_t) bankIndex].presets;
    if (! juce::isPositiveAndBelow (presetIndex, (int) ps.size())) return false;
    const Preset p = ps[(size_t) presetIndex];
    currentBank = bankIndex;

    if (p.factoryIndex >= 0)
    {
        target.loadFactory (p.factoryIndex);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (p.file);
        if (xml == nullptr || ! xml->hasTagName ("P800Preset")) return false;
        if (auto* sound = xml->getChildByName ("Sound"))
            target.restoreSound (juce::ValueTree::fromXml (*sound));
        else
            return false;
    }
    {
        const juce::ScopedLock sl (lock);
        currentKey = p.key;
        fileName.clear();
    }
    sendChangeMessage();
    return true;
}

bool PresetManager::loadByNumber (int number)
{
    return load (currentBank, indexForNumber (number));
}

void PresetManager::step (int delta)
{
    const auto& ps = banks[(size_t) currentBank].presets;
    const int n = (int) ps.size();
    if (n == 0) return;
    const int cur = findInBank (currentBank, getCurrentKey());
    const int next = cur < 0 ? (delta > 0 ? 0 : n - 1) : ((cur + delta) % n + n) % n;
    load (currentBank, next);
}

bool PresetManager::loadFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("P800Preset")) return false;
    auto* sound = xml->getChildByName ("Sound");
    if (sound == nullptr) return false;
    target.restoreSound (juce::ValueTree::fromXml (*sound));
    {
        const juce::ScopedLock sl (lock);
        // A file from elsewhere: shown by name, kept in no bank until it is saved.
        currentKey = "U:" + file.getParentDirectory().getFileName() + "/" + file.getFileNameWithoutExtension();
        fileName = file.getFileNameWithoutExtension();
    }
    sendChangeMessage();
    return true;
}

// ---- saving and organising --------------------------------------------------------------

bool PresetManager::presetExists (const juce::String& bankName, const juce::String& name) const
{
    return root.getChildFile (legalName (bankName)).getChildFile (legalName (name) + extension).existsAsFile();
}

bool PresetManager::save (const juce::String& bankName, const juce::String& name)
{
    const auto bank = legalName (bankName), clean = legalName (name);
    if (bank.isEmpty() || clean.isEmpty()) return false;
    const auto dir = root.getChildFile (bank);
    if (! dir.createDirectory()) return false;

    juce::XmlElement xml ("P800Preset");
    xml.setAttribute ("name", clean);
    xml.setAttribute ("version", 1);
    auto sound = target.captureSound();
    if (auto s = sound.createXml())
    {
        s->setTagName ("Sound");
        xml.addChildElement (s.release());
    }
    if (! xml.writeTo (dir.getChildFile (clean + extension))) return false;

    {
        const juce::ScopedLock sl (lock);
        currentKey = "U:" + bank + "/" + clean;
        fileName.clear();
    }
    refresh();
    currentBank = juce::jmax (0, indexOfBank (bank));
    sendChangeMessage();
    return true;
}

bool PresetManager::createBank (const juce::String& name)
{
    const auto clean = legalName (name);
    if (clean.isEmpty() || clean == "Factory" || clean == "Favourites") return false;
    const auto dir = root.getChildFile (clean);
    if (dir.exists() || ! dir.createDirectory()) return false;
    refresh();
    currentBank = juce::jmax (0, indexOfBank (clean));
    sendChangeMessage();
    return true;
}

bool PresetManager::deleteCurrentPreset()
{
    const auto key = getCurrentKey();
    if (! key.startsWith ("U:")) return false;
    for (const auto& b : banks)
        if (b.isUser())
            for (const auto& p : b.presets)
                if (p.key == key)
                {
                    if (! p.file.deleteFile()) return false;
                    if (favourites.contains (key)) { favourites.removeString (key); saveFavourites(); }
                    refresh();
                    return true;
                }
    return false;
}

bool PresetManager::deleteBank (int bankIndex)
{
    if (! juce::isPositiveAndBelow (bankIndex, (int) banks.size()) || ! banks[(size_t) bankIndex].isUser()) return false;
    const auto prefix = "U:" + banks[(size_t) bankIndex].name + "/";
    if (! banks[(size_t) bankIndex].dir.deleteRecursively()) return false;
    for (int i = favourites.size(); --i >= 0;)
        if (favourites[i].startsWith (prefix)) favourites.remove (i);
    saveFavourites();
    root.getChildFile (defaultUserBank).createDirectory(); // there is always somewhere to save
    currentBank = 0;
    refresh();
    currentBank = 0;
    sendChangeMessage();
    return true;
}

// ---- favourites -------------------------------------------------------------------------

void PresetManager::toggleFavourite (const juce::String& key)
{
    if (key.isEmpty()) return;
    if (favourites.contains (key)) favourites.removeString (key);
    else favourites.add (key);
    saveFavourites();
    rebuildFavouritesBank();
    sendChangeMessage();
}

void PresetManager::loadFavourites()
{
    favourites.clear();
    if (favouritesFile().existsAsFile())
    {
        favourites.addLines (favouritesFile().loadFileAsString());
        favourites.removeEmptyStrings();
        favourites.removeDuplicates (false);
    }
}

void PresetManager::saveFavourites() const
{
    root.createDirectory();
    favouritesFile().replaceWithText (favourites.joinIntoString ("\n"));
}
} // namespace p800
