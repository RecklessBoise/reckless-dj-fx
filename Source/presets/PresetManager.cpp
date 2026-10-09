#include "PresetManager.h"

namespace rdfx
{
namespace
{
const juce::String factoryPrefix = "factory:";
const juce::String userPrefix = "user:";
// Parameters that are part of the UI workflow rather than the sound; presets leave them alone.
const juce::StringArray nonPresetParams { "bpmMode", "bpm", "centerLock" };
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, juce::File rootDirectory)
    : apvts (state), root (std::move (rootDirectory))
{
    rescan();
    loadFavorites();
}

juce::File PresetManager::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Reckless DJ FX");
}

juce::String PresetManager::sanitiseName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, 64).trim();
}

void PresetManager::rescan()
{
    userPresets.clear();
    const auto folder = getUserFolder();
    if (! folder.isDirectory())
        return;

    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + presetExtension))
    {
        if (auto xml = juce::parseXML (f))
            userPresets.add ({ xml->getStringAttribute ("name", f.getFileNameWithoutExtension()),
                               xml->getStringAttribute ("category", "User"), f });
    }

    std::sort (userPresets.begin(), userPresets.end(),
               [] (const UserPreset& a, const UserPreset& b) { return a.name.compareNatural (b.name) < 0; });
}

juce::Array<PresetManager::Info> PresetManager::list (Bank bank, const juce::String& search) const
{
    juce::Array<Info> result;
    auto matches = [&search] (const juce::String& name, const juce::String& category)
    {
        return search.isEmpty() || name.containsIgnoreCase (search) || category.containsIgnoreCase (search);
    };

    if (bank != Bank::User)
        for (const auto& p : getFactoryPresets())
        {
            const auto key = factoryPrefix + p.name;
            if ((bank != Bank::Liked || isLiked (key)) && matches (p.name, p.category))
                result.add ({ key, p.name, p.category, true, isLiked (key) });
        }

    if (bank != Bank::Factory)
        for (const auto& p : userPresets)
        {
            const auto key = userPrefix + p.name;
            if ((bank != Bank::Liked || isLiked (key)) && matches (p.name, p.category))
                result.add ({ key, p.name, p.category, false, isLiked (key) });
        }

    return result;
}

juce::String PresetManager::getCurrentName() const
{
    if (currentKey.startsWith (factoryPrefix)) return currentKey.fromFirstOccurrenceOf (factoryPrefix, false, false);
    if (currentKey.startsWith (userPrefix))    return currentKey.fromFirstOccurrenceOf (userPrefix, false, false);
    return "Init";
}

bool PresetManager::load (const juce::String& key)
{
    if (key.startsWith (factoryPrefix))
    {
        const auto name = key.fromFirstOccurrenceOf (factoryPrefix, false, false);
        for (const auto& p : getFactoryPresets())
            if (p.name == name)
            {
                applyFactory (p);
                currentKey = key;
                sendChangeMessage();
                return true;
            }
        return false;
    }

    if (key.startsWith (userPrefix))
    {
        const auto name = key.fromFirstOccurrenceOf (userPrefix, false, false);
        for (const auto& p : userPresets)
            if (p.name == name)
                if (auto xml = juce::parseXML (p.file))
                {
                    applyXml (*xml);
                    currentKey = key;
                    sendChangeMessage();
                    return true;
                }
    }
    return false;
}

void PresetManager::applyFactory (const FactoryPreset& preset)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (nonPresetParams.contains (rp->getParameterID()))
                continue;
            float norm = rp->getDefaultValue();
            for (const auto& [id, value] : preset.values)
                if (id == rp->getParameterID())
                    norm = rp->convertTo0to1 (value);
            rp->setValueNotifyingHost (norm);
        }
}

void PresetManager::applyXml (const juce::XmlElement& xml)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (nonPresetParams.contains (rp->getParameterID()))
                continue;
            float norm = rp->getDefaultValue();
            if (auto* e = xml.getChildByAttribute ("id", rp->getParameterID()))
                norm = rp->convertTo0to1 ((float) e->getDoubleAttribute ("value"));
            rp->setValueNotifyingHost (norm);
        }
}

std::unique_ptr<juce::XmlElement> PresetManager::captureState (const juce::String& name, const juce::String& category) const
{
    auto xml = std::make_unique<juce::XmlElement> ("RecklessDJFXPreset");
    xml->setAttribute ("name", name);
    xml->setAttribute ("category", category);
    xml->setAttribute ("version", 1);
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (nonPresetParams.contains (rp->getParameterID()))
                continue;
            auto* e = xml->createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", rp->convertFrom0to1 (rp->getValue()));
        }
    return xml;
}

bool PresetManager::userPresetExists (const juce::String& name) const
{
    return getUserFolder().getChildFile (sanitiseName (name) + presetExtension).existsAsFile();
}

juce::String PresetManager::saveUser (const juce::String& rawName, const juce::String& category, bool overwrite)
{
    const auto name = sanitiseName (rawName);
    if (name.isEmpty())
        return "Please enter a preset name.";

    const auto folder = getUserFolder();
    if (! folder.createDirectory())
        return "Cannot create folder " + folder.getFullPathName();

    const auto file = folder.getChildFile (name + presetExtension);
    if (file.existsAsFile() && ! overwrite)
        return "A preset named \"" + name + "\" already exists.";

    auto xml = captureState (name, category.isNotEmpty() ? category : juce::String ("User"));
    if (! xml->writeTo (file))
        return "Could not write " + file.getFullPathName();

    rescan();
    currentKey = userPrefix + name;
    sendChangeMessage();
    return {};
}

bool PresetManager::deleteUser (const juce::String& key)
{
    if (! key.startsWith (userPrefix))
        return false;
    const auto name = key.fromFirstOccurrenceOf (userPrefix, false, false);
    for (const auto& p : userPresets)
        if (p.name == name)
        {
            // Moves to the system trash so a mistaken delete can be recovered
            if (! p.file.moveToTrash())
                return false;
            liked.removeString (key);
            saveFavorites();
            if (currentKey == key) currentKey = {};
            rescan();
            sendChangeMessage();
            return true;
        }
    return false;
}

void PresetManager::setLiked (const juce::String& key, bool shouldBeLiked)
{
    if (shouldBeLiked == isLiked (key))
        return;
    if (shouldBeLiked) liked.add (key);
    else liked.removeString (key);
    saveFavorites();
    exportLikedBank(); // keep Banks/Liked.rdfxbank in sync with the likes
    sendChangeMessage();
}

bool PresetManager::step (Bank bank, int delta)
{
    const auto items = list (bank);
    if (items.isEmpty())
        return false;

    int index = -1;
    for (int i = 0; i < items.size(); ++i)
        if (items[i].key == currentKey) { index = i; break; }

    const int next = index < 0 ? (delta > 0 ? 0 : items.size() - 1)
                               : (index + delta + items.size()) % items.size();
    return load (items[next].key);
}

void PresetManager::loadFavorites()
{
    liked.clear();
    const auto file = root.getChildFile ("favorites.json");
    if (! file.existsAsFile())
        return;
    const auto json = juce::JSON::parse (file);
    if (auto* arr = json.getProperty ("liked", {}).getArray())
        for (const auto& v : *arr)
            liked.addIfNotAlreadyThere (v.toString());
}

void PresetManager::saveFavorites() const
{
    root.createDirectory();
    juce::Array<juce::var> arr;
    for (const auto& k : liked) arr.add (k);
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("liked", arr);
    root.getChildFile ("favorites.json").replaceWithText (juce::JSON::toString (juce::var (obj.release())));
}

juce::File PresetManager::exportLikedBank (juce::File target) const
{
    if (target == juce::File())
    {
        getBankFolder().createDirectory();
        target = getBankFolder().getChildFile (juce::String ("Liked") + bankExtension);
    }

    juce::XmlElement bank ("RecklessDJFXBank");
    bank.setAttribute ("name", "Liked");

    // Snapshot each liked preset by loading it into a scratch XML (without touching the live state)
    for (const auto& info : list (Bank::Liked))
    {
        auto* e = bank.createNewChildElement ("RecklessDJFXPreset");
        e->setAttribute ("name", info.name);
        e->setAttribute ("category", info.category);
        e->setAttribute ("version", 1);

        if (info.isFactory)
        {
            for (const auto& fp : getFactoryPresets())
                if (fp.name == info.name)
                    for (auto* p : apvts.processor.getParameters())
                        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                        {
                            if (nonPresetParams.contains (rp->getParameterID())) continue;
                            float value = rp->convertFrom0to1 (rp->getDefaultValue());
                            for (const auto& [id, v] : fp.values)
                                if (id == rp->getParameterID()) value = v;
                            auto* pe = e->createNewChildElement ("PARAM");
                            pe->setAttribute ("id", rp->getParameterID());
                            pe->setAttribute ("value", value);
                        }
        }
        else
        {
            for (const auto& up : userPresets)
                if (up.name == info.name)
                    if (auto xml = juce::parseXML (up.file))
                        for (auto* pe : xml->getChildWithTagNameIterator ("PARAM"))
                            e->addChildElement (new juce::XmlElement (*pe));
        }
    }

    return bank.writeTo (target) ? target : juce::File();
}

int PresetManager::importBank (const juce::File& bankFile)
{
    auto xml = juce::parseXML (bankFile);
    if (xml == nullptr || ! xml->hasTagName ("RecklessDJFXBank"))
        return 0;

    getUserFolder().createDirectory();
    int count = 0;
    for (auto* e : xml->getChildWithTagNameIterator ("RecklessDJFXPreset"))
    {
        auto name = sanitiseName (e->getStringAttribute ("name", "Imported"));
        auto file = getUserFolder().getChildFile (name + presetExtension);
        if (file.existsAsFile())
            file = getUserFolder().getNonexistentChildFile (name, presetExtension, false);
        juce::XmlElement copy (*e);
        copy.setAttribute ("name", file.getFileNameWithoutExtension());
        if (copy.writeTo (file))
            ++count;
    }
    rescan();
    sendChangeMessage();
    return count;
}
} // namespace rdfx
