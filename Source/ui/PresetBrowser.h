#pragma once

#include "LookAndFeel.h"
#include "presets/PresetManager.h"

namespace rdfx::ui
{
void drawHeart (juce::Graphics& g, juce::Rectangle<float> area, bool filled, juce::Colour colour);

/** Small clickable heart used to like the current preset. */
class HeartButton final : public juce::Component
{
public:
    std::function<void()> onClick;
    void setLiked (bool l) { if (liked != l) { liked = l; repaint(); } }
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool liked = false;
};

/** Opens the "save preset" dialog. */
void showSavePresetDialog (PresetManager& manager, juce::Component* parent);

/** Overlay listing factory / user / liked presets with search, likes, delete, and bank import/export. */
class PresetBrowser final : public juce::Component,
                            private juce::ListBoxModel,
                            private juce::ChangeListener
{
public:
    explicit PresetBrowser (PresetManager& manager);
    ~PresetBrowser() override;

    std::function<void()> onClose;
    PresetManager::Bank getBank() const { return bank; }

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }

    void setBank (PresetManager::Bank b);
    void deleteSelected();
    void exportLiked();
    void importBank();

    PresetManager& manager;
    PresetManager::Bank bank = PresetManager::Bank::All;
    juce::Array<PresetManager::Info> items;

    juce::TextButton tabAll { "ALL" }, tabFactory { "FACTORY" }, tabUser { "USER" }, tabLiked { "LIKED" };
    juce::TextEditor search;
    juce::ListBox listBox { "presets", this };
    juce::TextButton saveButton { "SAVE AS..." }, deleteButton { "DELETE" }, exportButton { "EXPORT LIKED BANK" },
                     importButton { "IMPORT BANK" }, folderButton { "OPEN FOLDER" }, closeButton { "CLOSE" };
    juce::Label countLabel;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace rdfx::ui
