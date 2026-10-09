#include "PluginEditor.h"
#include "ui/Hardware.h"

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
    getProperties().set ("rdfxRoot", true);
    presetName.getProperties().set ("btn", "lcd");
    presetName.setTooltip ("Open the preset browser");
    for (auto* b : { &prevButton, &nextButton, &saveButton, &browseButton, &sizeButton, &tapButton })
        b->setName (b->getButtonText());
    bpmMode.setColour (juce::ComboBox::textColourId, hw::Col::lcdText);
    bpmSlider.setColour (juce::Slider::textBoxTextColourId, hw::Col::lcdText);

    bpmMode.addItemList ({ "HOST", "TAP", "MANUAL" }, 1);
    bpmModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamID::bpmMode, bpmMode);

    bpmSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 70, 30);
    bpmSlider.setColour (juce::Slider::trackColourId, Colours::control);
    bpmSlider.setTooltip ("Manual / tap BPM (drag up and down)");
    bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamID::bpm, bpmSlider);
    bpmSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " BPM"; };
    bpmSlider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    bpmSlider.updateText();
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
    constexpr float texScale = 2.0f; // textures are rendered at 2x so they stay sharp up to 200 %
    const int W = getWidth(), H = getHeight();

    // Top bar: brushed aluminium strip
    const auto& strip = hw::brushedStrip ((int) ((float) W * texScale), (int) (64.0f * texScale));
    g.drawImage (strip, juce::Rectangle<float> (0.0f, 0.0f, (float) W, 64.0f));

    // Faceplate: dark anodised metal, scratched and worn
    const auto& plate = hw::faceplate ((int) ((float) W * texScale), (int) ((float) (H - 64) * texScale), 0xA9A9A9);
    g.drawImage (plate, juce::Rectangle<float> (0.0f, 64.0f, (float) W, (float) (H - 64)));

    hw::drawPanelSeam (g, { 0.0f, 63.0f, (float) W, 2.0f });
    hw::drawPanelSeam (g, { 359.0f, 65.0f, 2.0f, (float) (H - 65) });

    // Logo, printed on the strip
    g.setFont (hw::printFont (25.0f));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawText ("RECKLESS", 20, 1, 140, 64, juce::Justification::centredLeft);
    g.setColour (hw::Col::print);
    g.drawText ("RECKLESS", 20, 0, 140, 64, juce::Justification::centredLeft);
    g.setColour (hw::Col::beatLed);
    g.drawText ("DJ FX", 146, 0, 80, 64, juce::Justification::centredLeft);
    g.setFont (hw::printFont (11.0f, false));
    g.setColour (hw::Col::print.withAlpha (0.82f));
    g.drawText ("PROFESSIONAL EFFECTS UNIT", 21, 42, 210, 14, juce::Justification::centredLeft);
}

void RecklessDJFXContent::resized()
{
    // Keys are 56 px boxes: 44 x 44 px visible rubber after the glow margin (comfortable click/touch targets)
    prevButton.setBounds (232, 4, 56, 56);
    presetName.setBounds (288, 4, 212, 56);
    nextButton.setBounds (500, 4, 56, 56);
    heart.setBounds (556, 4, 56, 56);
    saveButton.setBounds (612, 4, 72, 56);
    browseButton.setBounds (684, 4, 90, 56);

    bpmSlider.setBounds (776, 4, 104, 56);
    bpmMode.setBounds (880, 4, 92, 56);
    tapButton.setBounds (972, 4, 60, 56);
    sizeButton.setBounds (1032, 4, 62, 56);

    colorPanel.setBounds (0, 64, 360, 600);
    beatPanel.setBounds (360, 64, 740, 600);
    browser.setBounds (0, 64, getWidth(), getHeight() - 64);
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
