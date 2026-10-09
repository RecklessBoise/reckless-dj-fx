#include "PluginProcessor.h"

using namespace rdfx;

class PresetTests final : public juce::UnitTest
{
public:
    PresetTests() : juce::UnitTest ("Presets", "RecklessDJFX") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("RecklessDJFXTests-" + juce::String (juce::Random::getSystemRandom().nextInt()));
        root.deleteRecursively();

        RecklessDJFXProcessor proc;
        PresetManager pm (proc.apvts, root);

        beginTest ("factory bank is large and names are unique");
        const auto& factory = getFactoryPresets();
        expectGreaterOrEqual ((int) factory.size(), 250);
        juce::StringArray names;
        for (const auto& p : factory)
        {
            expect (! names.contains (p.name), "duplicate preset name: " + p.name);
            names.add (p.name);
            for (const auto& [id, value] : p.values)
                expect (proc.apvts.getParameter (id) != nullptr, "unknown parameter " + id + " in " + p.name);
        }

        beginTest ("loading a factory preset sets parameters");
        expect (pm.load ("factory:Echo 1/2"));
        expectEquals ((int) proc.apvts.getRawParameterValue (ParamID::beatType)->load(), (int) BeatFxType::Echo);
        expectEquals ((int) proc.apvts.getRawParameterValue (ParamID::beatIdx)->load(), 3);
        expect (proc.apvts.getRawParameterValue (ParamID::beatOn)->load() > 0.5f);

        beginTest ("user preset save / reload roundtrip");
        proc.apvts.getParameter (ParamID::level)->setValueNotifyingHost (0.33f);
        expectEquals (pm.saveUser ("My Test: Preset", "Tests"), juce::String());
        expect (pm.userPresetExists ("My Test: Preset"));
        const auto userKey = pm.getCurrentKey();
        expect (userKey.startsWith ("user:"));
        pm.load ("factory:Init");
        expectWithinAbsoluteError (proc.apvts.getRawParameterValue (ParamID::level)->load(), 0.5f, 1e-4f);
        expect (pm.load (userKey));
        expectWithinAbsoluteError (proc.apvts.getRawParameterValue (ParamID::level)->load(), 0.33f, 1e-3f);
        expectEquals (pm.list (PresetManager::Bank::User).size(), 1);
        expectEquals (pm.saveUser ("   "), juce::String ("Please enter a preset name."));

        beginTest ("likes persist and build the Liked bank");
        pm.setLiked ("factory:Spiral 1/2", true);
        pm.setLiked (userKey, true);
        expectEquals (pm.list (PresetManager::Bank::Liked).size(), 2);
        {
            PresetManager reloaded (proc.apvts, root);
            expect (reloaded.isLiked ("factory:Spiral 1/2"));
            expect (reloaded.isLiked (userKey));
        }
        const auto autoBank = pm.getBankFolder().getChildFile ("Liked.rdfxbank");
        expect (autoBank.existsAsFile(), "Liked bank is not written automatically");

        beginTest ("liked bank export / import");
        const auto bankFile = pm.exportLikedBank (root.getChildFile ("export.rdfxbank"));
        expect (bankFile.existsAsFile());
        const int imported = pm.importBank (bankFile);
        expectEquals (imported, 2);
        expectEquals (pm.list (PresetManager::Bank::User).size(), 3);

        beginTest ("search and prev/next");
        expect (pm.list (PresetManager::Bank::Factory, "roll").size() > 5);
        pm.load ("factory:Init");
        expect (pm.step (PresetManager::Bank::Liked, 1));
        expect (pm.isLiked (pm.getCurrentKey()));

        beginTest ("delete user preset removes its like");
        expect (pm.deleteUser (userKey));
        expect (! pm.isLiked (userKey));

        beginTest ("plugin state roundtrip keeps preset key and scale");
        proc.editorScale = 1.5f;
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        RecklessDJFXProcessor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        expectWithinAbsoluteError (other.editorScale, 1.5f, 1e-4f);

        root.deleteRecursively();
    }
};

static PresetTests presetTests;
