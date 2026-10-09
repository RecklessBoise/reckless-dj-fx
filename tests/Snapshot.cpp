// Renders the plugin editor to PNG files (used for the README screenshots and visual checks).
//   RecklessDJFXSnapshot <output-dir>
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    RecklessDJFXProcessor proc;
    proc.getPresetManager().load ("factory:Spiral Into Space");

    auto save = [&] (juce::Component& c, const juce::String& name, float scale)
    {
        const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
        juce::PNGImageFormat png;
        juce::FileOutputStream out (outDir.getChildFile (name));
        out.setPosition (0);
        out.truncate();
        png.writeImageToStream (img, out);
        std::printf ("wrote %s (%dx%d)\n", outDir.getChildFile (name).getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (RecklessDJFXEditor::kBaseWidth, RecklessDJFXEditor::kBaseHeight);
        save (*editor, "screenshot.png", 1.0f);
        editor->setSize (RecklessDJFXEditor::kBaseWidth * 3 / 4, RecklessDJFXEditor::kBaseHeight * 3 / 4);
        save (*editor, "screenshot-75.png", 1.0f);
    }

    {
        RecklessDJFXContent content (proc);
        content.setSize (RecklessDJFXEditor::kBaseWidth, RecklessDJFXEditor::kBaseHeight);
        rdfx::ui::LookAndFeel lnf;
        content.setLookAndFeel (&lnf);
        // Open the preset browser as if the PRESETS button was clicked
        for (auto* child : content.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child))
                if (b->getButtonText() == "PRESETS" && b->onClick)
                    b->onClick();
        save (content, "screenshot-presets.png", 1.0f);
        content.setLookAndFeel (nullptr);
    }
    return 0;
}
