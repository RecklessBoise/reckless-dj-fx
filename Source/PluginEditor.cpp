#include "PluginEditor.h"

using namespace rdfx;
using namespace rdfx::ui;

RecklessDJFXContent::RecklessDJFXContent (RecklessDJFXProcessor& p)
    : processor (p), presets (p.getPresetManager()),
      colorPanel (p.apvts), beatPanel (p), browser (p.getPresetManager())
{
    prevButton.onClick = [this] { presets.step (browser.getBank(), -1); };
    nextButton.onClick = [this] { presets.step (browser.getBank(), 1); };
    presetName.onClick = [this] { showBrowser (true); };
    browseButton.onClick = [this] { showBrowser (! browser.isVisible()); };
    saveButton.onClick = [this] { showSavePresetDialog (presets, this); };
    heart.onClick = [this]
    {
        const auto key = presets.getCurrentKey();
        if (key.isNotEmpty())
            presets.toggleLiked (key);
    };
    sizeButton.onClick = [this] { showSizeMenu(); };
    tapButton.onClick = [this] { processor.tapTempo(); };
    LookAndFeel::setAccent (saveButton, Colours::like);

    bpmMode.addItemList ({ "HOST", "TAP", "MANUAL" }, 1);
    bpmModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamID::bpmMode, bpmMode);

    bpmSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 70, 30);
    bpmSlider.setColour (juce::Slider::trackColourId, Colours::control);
    bpmSlider.setTooltip ("Manual / tap BPM (drag up and down)");
    bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamID::bpm, bpmSlider);
    bpmSlider.onDragStart = [this] { setParam (processor.apvts, ParamID::bpmMode, (float) BpmMode::Manual); };

    for (auto* c : std::initializer_list<juce::Component*> { &prevButton, &nextButton, &presetName, &saveButton, &browseButton,
                                                             &sizeButton, &tapButton, &heart, &bpmMode, &bpmSlider,
                                                             &colorPanel, &beatPanel })
        addAndMakeVisible (c);

    addChildComponent (browser);
    browser.onClose = [this] { showBrowser (false); };

    presets.addChangeListener (this);
    updatePresetBar();
}

RecklessDJFXContent::~RecklessDJFXContent() { presets.removeChangeListener (this); }

void RecklessDJFXContent::changeListenerCallback (juce::ChangeBroadcaster*) { updatePresetBar(); }

void RecklessDJFXContent::updatePresetBar()
{
    const auto key = presets.getCurrentKey();
    presetName.setButtonText (key.isEmpty() ? juce::String ("Init") : presets.getCurrentName());
    heart.setLiked (key.isNotEmpty() && presets.isLiked (key));
    heart.setEnabled (key.isNotEmpty());
}

void RecklessDJFXContent::showBrowser (bool show)
{
    browser.setVisible (show);
    browseButton.setToggleState (show, juce::dontSendNotification);
    if (show)
    {
        browser.toFront (true);
        browser.refresh();
    }
}

void RecklessDJFXContent::showSizeMenu()
{
    juce::PopupMenu menu;
    const float scales[] = { 0.6f, 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    for (int i = 0; i < (int) std::size (scales); ++i)
        menu.addItem (i + 1, juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %", true,
                      std::abs (processor.editorScale - scales[i]) < 0.01f);
    menu.addSeparator();
    menu.addItem (100, "Tip: drag the bottom-right corner to resize freely", false, false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (sizeButton),
                        [this, scales] (int r)
                        {
                            if (r >= 1 && r <= (int) std::size (scales) && onScaleChosen)
                                onScaleChosen (scales[r - 1]);
                        });
}

void RecklessDJFXContent::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    // Header
    g.setColour (Colours::panel);
    g.fillRect (0, 0, getWidth(), 60);
    g.setColour (Colours::panelEdge);
    g.drawHorizontalLine (60, 0.0f, (float) getWidth());

    g.setFont (LookAndFeel::font (24.0f));
    g.setColour (Colours::text);
    g.drawText ("RECKLESS", 20, 0, 130, 60, juce::Justification::centredLeft);
    g.setColour (Colours::beat);
    g.drawText ("DJ FX", 140, 0, 90, 60, juce::Justification::centredLeft);

    g.setFont (LookAndFeel::font (10.0f, false));
    g.setColour (Colours::textDim);
    g.drawText ("BPM", 816, 4, 60, 12, juce::Justification::centred);
}

void RecklessDJFXContent::resized()
{
    // Header (designed at 1100 x 664)
    prevButton.setBounds (244, 13, 34, 34);
    presetName.setBounds (282, 13, 250, 34);
    nextButton.setBounds (536, 13, 34, 34);
    heart.setBounds (576, 13, 34, 34);
    saveButton.setBounds (616, 13, 64, 34);
    browseButton.setBounds (686, 13, 86, 34);

    bpmSlider.setBounds (816, 14, 60, 34);
    bpmMode.setBounds (882, 15, 92, 30);
    tapButton.setBounds (980, 13, 50, 34);
    sizeButton.setBounds (1036, 13, 50, 34);

    colorPanel.setBounds (16, 74, 330, 576);
    beatPanel.setBounds (362, 74, 722, 576);
    browser.setBounds (0, 61, getWidth(), getHeight() - 61);
}

//==============================================================================
RecklessDJFXEditor::RecklessDJFXEditor (RecklessDJFXProcessor& p)
    : AudioProcessorEditor (&p), djfx (p), content (p)
{
    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setSize (kBaseWidth, kBaseHeight);
    content.onScaleChosen = [this] (float s) { setSize (juce::roundToInt (kBaseWidth * s), juce::roundToInt (kBaseHeight * s)); };

    setResizable (true, true);
    setResizeLimits (kBaseWidth / 2, kBaseHeight / 2, kBaseWidth * 2, kBaseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);

    const float s = juce::jlimit (0.5f, 2.0f, djfx.editorScale);
    setSize (juce::roundToInt (kBaseWidth * s), juce::roundToInt (kBaseHeight * s));
}

RecklessDJFXEditor::~RecklessDJFXEditor()
{
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void RecklessDJFXEditor::paint (juce::Graphics& g) { g.fillAll (Colours::background); }

void RecklessDJFXEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    djfx.editorScale = scale;
}
