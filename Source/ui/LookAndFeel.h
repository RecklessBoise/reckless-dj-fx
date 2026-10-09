#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rdfx::ui
{
namespace Colours
{
    inline const juce::Colour background { 0xff0d0e11 };
    inline const juce::Colour panel      { 0xff16181d };
    inline const juce::Colour panelEdge  { 0xff2a2d35 };
    inline const juce::Colour control    { 0xff23262e };
    inline const juce::Colour text       { 0xffe8e9ec };
    inline const juce::Colour textDim    { 0xff8a8f9b };
    inline const juce::Colour beat       { 0xffff5a1f }; // Beat FX accent (orange)
    inline const juce::Colour color      { 0xff1fc8ff }; // Sound Color FX accent (cyan)
    inline const juce::Colour like       { 0xffff3d6e };
    inline const juce::Colour lcd        { 0xff0a1416 };
    inline const juce::Colour lcdText    { 0xff7ff3ff };
}

/** Flat, dark, mixer-inspired look. Buttons use their `accent` property when toggled on. */
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;

    static juce::Colour accentOf (const juce::Component& c);
    static void setAccent (juce::Component& c, juce::Colour accent);
    static juce::Font font (float height, bool bold = true);
};
} // namespace rdfx::ui
