#include "LookAndFeel.h"

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
    const auto accent = accentOf (s);
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float trackWidth = juce::jmax (3.0f, radius * 0.12f);
    const float arcRadius = radius - trackWidth * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::panelEdge);
    g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (s.isEnabled() ? accent : accent.withSaturation (0.1f));
    g.strokePath (value, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob cap
    const float capR = arcRadius - trackWidth * 1.4f;
    juce::ColourGradient grad (juce::Colour (0xff3a3e48), centre.x, centre.y - capR,
                               juce::Colour (0xff1b1d22), centre.x, centre.y + capR, false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (capR * 2, capR * 2).withCentre (centre));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (juce::Rectangle<float> (capR * 2, capR * 2).withCentre (centre), 1.0f);

    // Pointer
    juce::Path pointer;
    const float pw = juce::jmax (2.0f, capR * 0.12f);
    pointer.addRoundedRectangle (-pw * 0.5f, -capR + 3.0f, pw, capR * 0.45f, pw * 0.5f);
    g.setColour (Colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));

    if (bipolar)
    {
        // Center detent marker
        const float a = (startAngle + endAngle) * 0.5f;
        const auto p = centre.getPointOnCircumference (radius + 1.0f, a);
        g.setColour (Colours::textDim);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (p));
    }
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = accentOf (b);
    const bool on = b.getToggleState();
    const float corner = juce::jmin (6.0f, r.getHeight() * 0.3f);

    auto base = on ? accent.withAlpha (0.88f) : Colours::control;
    if (over) base = base.brighter (0.08f);
    if (down) base = base.darker (0.15f);
    g.setColour (base);
    g.fillRoundedRectangle (r, corner);

    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (0.5f), corner, 2.0f);
    }
    else
    {
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r, corner, 1.0f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? juce::Colours::black.withAlpha (0.85f) : Colours::text.withAlpha (b.isEnabled() ? 0.9f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 2, 0.8f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return font (juce::jlimit (10.0f, 16.0f, (float) h * 0.42f));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (w, h).toFloat().reduced (1.0f);
    g.setColour (Colours::control);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (13.0f); }
juce::Font LookAndFeel::getLabelFont (juce::Label& l) { return font (juce::jmin (15.0f, (float) l.getHeight() * 0.75f)); }
} // namespace rdfx::ui
