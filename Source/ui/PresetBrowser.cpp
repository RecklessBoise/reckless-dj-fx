#include "PresetBrowser.h"
#include "Hardware.h"

namespace rdfx::ui
{
void drawHeart (juce::Graphics& g, juce::Rectangle<float> area, bool filled, juce::Colour colour)
{
    const auto r = area.reduced (area.getWidth() * 0.12f);
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    juce::Path p;
    p.startNewSubPath (x + w * 0.5f, y + h * 0.95f);
    p.cubicTo (x - w * 0.05f, y + h * 0.55f, x + w * 0.05f, y - h * 0.05f, x + w * 0.5f, y + h * 0.25f);
    p.cubicTo (x + w * 0.95f, y - h * 0.05f, x + w * 1.05f, y + h * 0.55f, x + w * 0.5f, y + h * 0.95f);
    p.closeSubPath();
    g.setColour (colour);
    if (filled) g.fillPath (p);
    else g.strokePath (p, juce::PathStrokeType (juce::jmax (1.2f, w * 0.09f)));
}

void HeartButton::paint (juce::Graphics& g)
{
    // A rubber key whose heart lights up when the current preset is liked
    const auto key = getLocalBounds().toFloat().reduced (6.0f);
    hw::drawRubberButton (g, key, false, isMouseOver(), isMouseButtonDown(), Colours::like, 0x4ea7);
    const float s = juce::jmin (key.getWidth(), key.getHeight()) * 0.62f;
    const auto heartArea = juce::Rectangle<float> (s, s).withCentre (key.getCentre());
    if (liked)
    {
        g.setGradientFill (juce::ColourGradient (Colours::like.withAlpha (0.55f), heartArea.getCentre(),
                                                 Colours::like.withAlpha (0.0f), heartArea.getCentre().translated (s, 0.0f), true));
        g.fillEllipse (heartArea.expanded (s * 0.4f));
    }
    drawHeart (g, heartArea, liked, liked ? Colours::like.brighter (0.2f) : hw::Col::print.withAlpha (isMouseOver() ? 0.95f : 0.7f));
}

void showSavePresetDialog (PresetManager& manager, juce::Component* parent)
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset (it will appear in the USER bank).",
                                     juce::MessageBoxIconType::NoIcon, parent);
    const auto current = manager.getCurrentName();
    w->addTextEditor ("name", current == "Init" ? juce::String ("My Preset") : current, "Name");
    w->addTextEditor ("category", "User", "Category");
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<juce::AlertWindow> safe (w);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, &manager] (int result)
    {
        if (result != 1 || safe == nullptr)
            return;
        const auto name = safe->getTextEditorContents ("name");
        const auto category = safe->getTextEditorContents ("category");

        auto doSave = [&manager, name, category]
        {
            const auto error = manager.saveUser (name, category, true);
            if (error.isNotEmpty())
                juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Save preset").withMessage (error).withButton ("OK"), nullptr);
        };

        if (manager.userPresetExists (name))
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withTitle ("Overwrite preset?")
                                              .withMessage ("\"" + PresetManager::sanitiseName (name) + "\" already exists. Replace it?")
                                              .withButton ("REPLACE")
                                              .withButton ("CANCEL"),
                                          [doSave] (int r) { if (r == 1) doSave(); });
        }
        else
        {
            doSave();
        }
    }), true);
}

//==============================================================================
PresetBrowser::PresetBrowser (PresetManager& m) : manager (m)
{
    for (auto* t : { &tabAll, &tabFactory, &tabUser, &tabLiked })
    {
        LookAndFeel::setAccent (*t, t == &tabLiked ? Colours::like : Colours::beat);
        addAndMakeVisible (t);
    }
    tabAll.onClick = [this] { setBank (PresetManager::Bank::All); };
    tabFactory.onClick = [this] { setBank (PresetManager::Bank::Factory); };
    tabUser.onClick = [this] { setBank (PresetManager::Bank::User); };
    tabLiked.onClick = [this] { setBank (PresetManager::Bank::Liked); };

    search.setTextToShowWhenEmpty ("Search presets or categories...", Colours::textDim);
    search.setFont (LookAndFeel::font (14.0f, false));
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (search);

    listBox.setRowHeight (30);
    listBox.setColour (juce::ListBox::outlineColourId, Colours::panelEdge);
    listBox.setOutlineThickness (1);
    addAndMakeVisible (listBox);

    saveButton.onClick = [this] { showSavePresetDialog (manager, this); };
    deleteButton.onClick = [this] { deleteSelected(); };
    exportButton.onClick = [this] { exportLiked(); };
    importButton.onClick = [this] { importBank(); };
    folderButton.onClick = [this]
    {
        manager.getUserFolder().createDirectory();
        manager.getUserFolder().startAsProcess();
    };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    LookAndFeel::setAccent (exportButton, Colours::like);
    for (auto* b : { &saveButton, &deleteButton, &exportButton, &importButton, &folderButton, &closeButton,
                     &tabAll, &tabFactory, &tabUser, &tabLiked })
    {
        b->getProperties().set ("btn", "flat");
        addAndMakeVisible (b);
    }

    countLabel.setColour (juce::Label::textColourId, Colours::textDim);
    countLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (countLabel);

    manager.addChangeListener (this);
    setBank (PresetManager::Bank::All);
}

PresetBrowser::~PresetBrowser() { manager.removeChangeListener (this); }

void PresetBrowser::setBank (PresetManager::Bank b)
{
    bank = b;
    tabAll.setToggleState (b == PresetManager::Bank::All, juce::dontSendNotification);
    tabFactory.setToggleState (b == PresetManager::Bank::Factory, juce::dontSendNotification);
    tabUser.setToggleState (b == PresetManager::Bank::User, juce::dontSendNotification);
    tabLiked.setToggleState (b == PresetManager::Bank::Liked, juce::dontSendNotification);
    refresh();
}

void PresetBrowser::refresh()
{
    items = manager.list (bank, search.getText().trim());
    tabLiked.setButtonText (juce::String::fromUTF8 ("\xe2\x99\xa5 LIKED (") + juce::String (manager.numLiked()) + ")");
    countLabel.setText (juce::String (items.size()) + " presets", juce::dontSendNotification);
    listBox.updateContent();

    for (int i = 0; i < items.size(); ++i)
        if (items[i].key == manager.getCurrentKey())
        {
            listBox.selectRow (i, true, true);
            break;
        }
    repaint();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
    g.setColour (Colours::text);
    g.setFont (LookAndFeel::font (20.0f));
    g.drawText ("PRESETS", 24, 16, 200, 30, juce::Justification::centredLeft);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    auto top = r.removeFromTop (34);
    top.removeFromLeft (130);
    closeButton.setBounds (top.removeFromRight (100));
    countLabel.setBounds (top.removeFromRight (140));
    const int tabW = 130;
    for (auto* t : { &tabAll, &tabFactory, &tabUser, &tabLiked })
    {
        t->setBounds (top.removeFromLeft (t == &tabLiked ? tabW + 30 : tabW).reduced (3, 0));
    }
    r.removeFromTop (10);
    search.setBounds (r.removeFromTop (32));
    r.removeFromTop (10);

    auto bottom = r.removeFromBottom (40);
    const int bw = (bottom.getWidth() - 5 * 8) / 6;
    for (auto* b : { &saveButton, &deleteButton, &exportButton, &importButton, &folderButton })
    {
        b->setBounds (bottom.removeFromLeft (bw));
        bottom.removeFromLeft (8);
    }
    r.removeFromBottom (10);
    listBox.setBounds (r);
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, items.size()))
        return;
    const auto& it = items.getReference (row);
    const bool current = it.key == manager.getCurrentKey();

    if (selected || current)
        g.fillAll (Colours::beat.withAlpha (current ? 0.28f : 0.14f));
    else if (row % 2 == 1)
        g.fillAll (Colours::control.withAlpha (0.4f));

    drawHeart (g, { 8.0f, 4.0f, (float) h - 8.0f, (float) h - 8.0f }, it.liked, it.liked ? Colours::like : Colours::textDim);

    g.setColour (Colours::text);
    g.setFont (LookAndFeel::font (14.0f, current));
    g.drawText (it.name, h + 14, 0, w / 2, h, juce::Justification::centredLeft);

    g.setColour (Colours::textDim);
    g.setFont (LookAndFeel::font (12.0f, false));
    g.drawText ((it.isFactory ? "FACTORY  /  " : "USER  /  ") + it.category.toUpperCase(), w / 2, 0, w / 2 - 12, h,
                juce::Justification::centredRight);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, items.size()))
        return;
    const auto key = items[row].key;
    if (e.x < listBox.getRowHeight() + 10)
        manager.toggleLiked (key); // heart column
    else
        manager.load (key);
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, items.size()) && manager.load (items[row].key) && onClose)
        onClose();
}

void PresetBrowser::returnKeyPressed (int row)
{
    if (juce::isPositiveAndBelow (row, items.size()))
        manager.load (items[row].key);
}

void PresetBrowser::deleteSelected()
{
    const int row = listBox.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, items.size()) || items[row].isFactory)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Delete preset")
                                          .withMessage ("Select one of your USER presets to delete (factory presets cannot be deleted).")
                                          .withButton ("OK"), nullptr);
        return;
    }
    const auto key = items[row].key;
    const auto name = items[row].name;
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Delete preset?")
                                      .withMessage ("Move \"" + name + "\" to the Trash?")
                                      .withButton ("DELETE").withButton ("CANCEL"),
                                  [this, key] (int r) { if (r == 1) manager.deleteUser (key); });
}

void PresetBrowser::exportLiked()
{
    if (manager.numLiked() == 0)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Liked bank")
                                          .withMessage ("Like some presets first (click the heart).").withButton ("OK"), nullptr);
        return;
    }
    manager.getBankFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Export liked bank",
                                                   manager.getBankFolder().getChildFile (juce::String ("Liked") + PresetManager::bankExtension),
                                                   juce::String ("*") + PresetManager::bankExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File())
                                  manager.exportLikedBank (f.withFileExtension (PresetManager::bankExtension));
                          });
}

void PresetBrowser::importBank()
{
    chooser = std::make_unique<juce::FileChooser> ("Import bank", manager.getBankFolder(),
                                                   juce::String ("*") + PresetManager::bankExtension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile())
                              {
                                  const int n = manager.importBank (f);
                                  setBank (PresetManager::Bank::User);
                                  juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Import bank")
                                                                    .withMessage (juce::String (n) + " presets imported into USER.")
                                                                    .withButton ("OK"), nullptr);
                              }
                          });
}
} // namespace rdfx::ui
