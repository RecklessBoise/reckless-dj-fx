#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Panels.h"
#include "ui/PresetBrowser.h"

/** Fixed-layout content (designed at kBaseWidth x kBaseHeight) scaled to any window size. */
class RecklessDJFXContent final : public juce::Component, private juce::ChangeListener
{
public:
    explicit RecklessDJFXContent (RecklessDJFXProcessor&);
    ~RecklessDJFXContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (float)> onScaleChosen;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showBrowser (bool show);
    void showSizeMenu();
    void updatePresetBar();

    RecklessDJFXProcessor& processor;
    rdfx::PresetManager& presets;

    juce::TextButton prevButton { juce::String::fromUTF8 ("\xe2\x97\x80") }, nextButton { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    juce::TextButton presetName, saveButton { "SAVE" }, browseButton { "PRESETS" }, sizeButton { "SIZE" }, tapButton { "TAP" };
    rdfx::ui::HeartButton heart;
    juce::ComboBox bpmMode;
    juce::Slider bpmSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bpmModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;

    rdfx::ui::ColorFxPanel colorPanel;
    rdfx::ui::BeatFxPanel beatPanel;
    rdfx::ui::PresetBrowser browser;
};

class RecklessDJFXEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr int kBaseWidth = 1100;
    static constexpr int kBaseHeight = 664;

    explicit RecklessDJFXEditor (RecklessDJFXProcessor&);
    ~RecklessDJFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RecklessDJFXProcessor& djfx;
    rdfx::ui::LookAndFeel lnf;
    RecklessDJFXContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessDJFXEditor)
};
