#include "LookAndFeel.h"
#include "Hardware.h"

namespace rdfx::ui
{
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::background);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, Colours::control);
    setColour (juce::TextButton::textColourOffId, Colours::textDim);
    setColour (juce::TextButton::textColourOnId, Colours::text);
    setColour (juce::ComboBox::backgroundColourId, Colours::control);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::outlineColourId, Colours::panelEdge);
    setColour (juce::ComboBox::arrowColourId, Colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::beat.withAlpha (0.35f));
    setColour (juce::TextEditor::backgroundColourId, Colours::control);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::outlineColourId, Colours::panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::beat);
    setColour (juce::ListBox::backgroundColourId, Colours::panel);
    setColour (juce::ScrollBar::thumbColourId, Colours::panelEdge);
    setColour (juce::AlertWindow::backgroundColourId, Colours::panel);
    setColour (juce::AlertWindow::textColourId, Colours::text);
    setColour (juce::AlertWindow::outlineColourId, Colours::panelEdge);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colours::black);
    setColour (juce::TooltipWindow::textColourId, hw::Col::lcdText);
}

juce::Font LookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour LookAndFeel::accentOf (const juce::Component& c)
{
    const auto v = c.getProperties()["accent"];
    return v.isVoid() ? Colours::beat : juce::Colour ((juce::uint32) (juce::int64) v);
}

void LookAndFeel::setAccent (juce::Component& c, juce::Colour accent)
{
    c.getProperties().set ("accent", (juce::int64) accent.getARGB());
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const auto style = s.getProperties()["knob"].toString();
    const auto knobStyle = style == "encoder" ? hw::KnobStyle::Encoder : hw::KnobStyle::Matte;
    const int ticks = s.getProperties().getWithDefault ("ticks", 11);
    const bool detent = s.getProperties().getWithDefault ("detent", false);
    const float angle = startAngle + pos * (endAngle - startAngle);
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    bounds = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()),
                                           juce::jmin (bounds.getWidth(), bounds.getHeight()));
    hw::drawKnob (g, bounds, angle, knobStyle, hw::seedOf (s.getName()), startAngle, endAngle, ticks, detent);
    if (! s.isEnabled())
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (bounds.reduced (bounds.getWidth() * 0.1f));
    }
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto style = b.getProperties()["btn"].toString();
    const auto accent = accentOf (b);
    const bool on = b.getToggleState();

    if (style == "round")
    {
        const float glow = (float) (double) b.getProperties().getWithDefault ("glow", 0.0);
        hw::drawRoundButton (g, b.getLocalBounds().toFloat().reduced (2.0f), on, over, down, accent, glow);
        return;
    }

    if (style == "lcd")
    {
        const auto r = b.getLocalBounds().toFloat().reduced (8.0f);
        hw::drawLcdBezel (g, r);
        if (over)
        {
            g.setColour (hw::Col::lcdBlue.withAlpha (0.08f));
            g.fillRoundedRectangle (r, 3.0f);
        }
        return;
    }

    if (style == "flat")
    {
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const float corner = juce::jmin (6.0f, r.getHeight() * 0.3f);
        auto base = on ? accent.withAlpha (0.88f) : Colours::control;
        if (over) base = base.brighter (0.08f);
        if (down) base = base.darker (0.15f);
        g.setColour (base);
        g.fillRoundedRectangle (r, corner);
        g.setColour (on ? accent.withAlpha (0.35f) : Colours::panelEdge);
        g.drawRoundedRectangle (r, corner, 1.0f);
        return;
    }

    hw::drawRubberButton (g, b.getLocalBounds().toFloat().reduced (6.0f), on, over, down, accent,
                          hw::seedOf (b.getName() + b.getButtonText()));
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const auto style = b.getProperties()["btn"].toString();
    if (style == "round")
        return;

    if (style == "lcd")
    {
        const auto r = b.getLocalBounds().toFloat().reduced (14.0f, 8.0f);
        g.setFont (hw::lcdFont (17.0f));
        g.setColour (hw::Col::lcdText);
        g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        hw::drawGlass (g, b.getLocalBounds().toFloat().reduced (8.0f));
        return;
    }

    if (style == "flat")
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? juce::Colours::black.withAlpha (0.85f) : Colours::text.withAlpha (b.isEnabled() ? 0.9f : 0.4f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 2, 0.8f);
        return;
    }

    auto r = b.getLocalBounds().reduced (8).toFloat();
    if (down) r.translate (0.0f, 1.0f);
    const bool on = b.getToggleState();
    g.setFont (hw::printFont (juce::jlimit (9.0f, 14.0f, r.getHeight() * 0.42f)));
    // Lit keys get a dark legend (white on orange/cyan is below 3:1), unlit keys a white one
    g.setColour (on ? juce::Colours::white.withAlpha (0.35f) : juce::Colours::black.withAlpha (0.6f));
    g.drawFittedText (b.getButtonText(), r.translated (0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 2, 0.75f);
    g.setColour (on ? juce::Colour (0xff111214) : hw::Col::print.withAlpha (b.isEnabled() ? 0.88f : 0.35f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 2, 0.75f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return font (juce::jlimit (10.0f, 16.0f, (float) h * 0.42f));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (w, h).toFloat().reduced (3.0f);
    hw::drawLcdBezel (g, r.reduced (3.0f));
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (hw::Col::lcdBlue);
    g.fillPath (arrow);
    juce::ignoreUnused (box);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float, float, float,
                                    juce::Slider::SliderStyle, juce::Slider&)
{
    // Used for the BPM read-out: a small LCD that can be dragged
    hw::drawLcdBezel (g, juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f));
}

void LookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (hw::Col::lcdBlue.withAlpha (0.8f));
    g.drawRoundedRectangle (body.reduced (0.5f), 4.0f, 1.0f);
}

juce::Font LookAndFeel::getSliderPopupFont (juce::Slider&) { return hw::lcdFont (15.0f); }

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return hw::lcdFont (15.0f); }
juce::Font LookAndFeel::getLabelFont (juce::Label& l)
{
    if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr || dynamic_cast<juce::ComboBox*> (l.getParentComponent()) != nullptr)
        return hw::lcdFont (16.0f);
    return font (juce::jmin (15.0f, (float) l.getHeight() * 0.75f));
}
} // namespace rdfx::ui
