#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

namespace rdfx
{
/**
    Factory + user presets, likes (favourites) and the "Liked" bank.

    Storage (default root: ~/Music/Reckless DJ FX):
      Presets/<name>.rdfxpreset   user presets (XML)
      favorites.json              {"liked": ["factory:Echo 1/2", "user:My Preset", ...]}
      Banks/<name>.rdfxbank       exported banks (XML with several presets)

    All methods must be called from the message thread.
*/
class PresetManager : public juce::ChangeBroadcaster
{
public:
    enum class Bank { All, Factory, User, Liked };

    struct Info
    {
        juce::String key;      // "factory:<name>" or "user:<name>"
        juce::String name;
        juce::String category;
        bool isFactory = true;
        bool liked = false;
    };

    PresetManager (juce::AudioProcessorValueTreeState& state, juce::File rootDirectory = defaultRoot());

    static juce::File defaultRoot();
    static constexpr const char* presetExtension = ".rdfxpreset";
    static constexpr const char* bankExtension = ".rdfxbank";

    juce::Array<Info> list (Bank bank, const juce::String& search = {}) const;

    bool load (const juce::String& key);
    juce::String getCurrentKey() const { return currentKey; }
    juce::String getCurrentName() const;

    /** Saves the current state as a user preset. Returns an error message, or an empty string on success. */
    juce::String saveUser (const juce::String& name, const juce::String& category = "User", bool overwrite = true);
    bool deleteUser (const juce::String& key);
    bool userPresetExists (const juce::String& name) const;

    bool isLiked (const juce::String& key) const { return liked.contains (key); }
    void setLiked (const juce::String& key, bool shouldBeLiked);
    void toggleLiked (const juce::String& key) { setLiked (key, ! isLiked (key)); }
    int numLiked() const { return liked.size(); }

    /** Loads the next / previous preset of `bank` relative to the current one. */
    bool step (Bank bank, int delta);

    /** Writes every liked preset into one bank file (default: Banks/Liked.rdfxbank). */
    juce::File exportLikedBank (juce::File target = {}) const;
    /** Imports every preset of a bank file as user presets. Returns the number imported. */
    int importBank (const juce::File& bankFile);

    void rescan();
    juce::File getUserFolder() const { return root.getChildFile ("Presets"); }
    juce::File getBankFolder() const { return root.getChildFile ("Banks"); }

    /** Restores the "current preset" key after the host reloads the plugin state. */
    void setCurrentKeySilently (const juce::String& key) { currentKey = key; }

    static juce::String sanitiseName (const juce::String& name);

private:
    std::unique_ptr<juce::XmlElement> captureState (const juce::String& name, const juce::String& category) const;
    void applyXml (const juce::XmlElement& presetXml);
    void applyFactory (const FactoryPreset& preset);
    void loadFavorites();
    void saveFavorites() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::File root;
    juce::StringArray liked;
    struct UserPreset { juce::String name, category; juce::File file; };
    juce::Array<UserPreset> userPresets;
    juce::String currentKey;
};
} // namespace rdfx
