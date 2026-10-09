#include "Panels.h"
#include "PluginProcessor.h"
#include "dsp/BeatFx.h"

namespace rdfx::ui
{
void setParam (APVTS& state, const char* id, float realValue)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

float getParam (const APVTS& state, const char* id)
{
    if (auto* v = state.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

static void paintPanel (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent)
{
    auto r = bounds.toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (r.getX() + 16.0f, r.getY() + 14.0f, 4.0f, 16.0f, 2.0f);
    g.setFont (LookAndFeel::font (16.0f));
    g.setColour (Colours::text);
    g.drawText (title, (int) r.getX() + 28, (int) r.getY() + 10, 300, 24, juce::Justification::centredLeft);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (APVTS& state, const char* paramID, const juce::StringArray& labels, int cols, juce::Colour accent)
    : columns (cols)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        LookAndFeel::setAccent (*b, accent);
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (
        *state.getParameter (paramID),
        [this] (float v)
        {
            const int idx = juce::roundToInt (v);
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
        });
    attachment->sendInitialUpdate();
}

void ChoiceButtons::resized()
{
    const int rows = (buttons.size() + columns - 1) / columns;
    const float gap = 6.0f;
    const float w = ((float) getWidth() - gap * (float) (columns - 1)) / (float) columns;
    const float h = ((float) getHeight() - gap * (float) (rows - 1)) / (float) rows;
    for (int i = 0; i < buttons.size(); ++i)
    {
        const int r = i / columns, c = i % columns;
        buttons[i]->setBounds (juce::Rectangle<float> ((float) c * (w + gap), (float) r * (h + gap), w, h).toNearestInt());
    }
}

//==============================================================================
Knob::Knob (APVTS& state, const char* paramID, const juce::String& text, juce::Colour accent, std::unique_ptr<juce::Slider> custom)
    : slider (custom != nullptr ? std::move (custom) : std::make_unique<juce::Slider>())
{
    slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    slider->setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    LookAndFeel::setAccent (*slider, accent);
    addAndMakeVisible (*slider);

    caption.setText (text, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (caption);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, *slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (18));
    slider->setBounds (r);
}

//==============================================================================
double CenterLockSlider::snapValue (double attempted, DragMode mode)
{
    if (mode == notDragging || getParam (state, ParamID::centerLock) < 0.5f)
        return attempted;

    const double current = getValue();
    if (held)
    {
        if (std::abs (attempted) > 0.12)
        {
            held = false;
            return attempted;
        }
        return 0.0;
    }
    if ((current > 0.0 && attempted < 0.0) || (current < 0.0 && attempted > 0.0))
    {
        held = true; // stop exactly at the center, like the mixer's CENTER LOCK
        return 0.0;
    }
    return attempted;
}

//==============================================================================
ToggleBtn::ToggleBtn (APVTS& state, const char* paramID, const juce::String& text, juce::Colour accent)
    : juce::TextButton (text)
{
    setClickingTogglesState (true);
    LookAndFeel::setAccent (*this, accent);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, *this);
}

//==============================================================================
ColorFxPanel::ColorFxPanel (APVTS& state)
    : types (state, ParamID::colorType, colorFxNames(), 2, Colours::color),
      colorKnob (state, ParamID::colorAmt, "COLOR", Colours::color, std::make_unique<CenterLockSlider> (state)),
      paramKnob (state, ParamID::colorParam, "PARAMETER", Colours::color),
      onButton (state, ParamID::colorOn, "ON", Colours::color),
      lockButton (state, ParamID::centerLock, "CENTER LOCK", Colours::color)
{
    colorKnob.getSlider().setDoubleClickReturnValue (true, 0.0);
    paramKnob.getSlider().setDoubleClickReturnValue (true, 0.5);
    for (auto* c : std::initializer_list<juce::Component*> { &types, &colorKnob, &paramKnob, &onButton, &lockButton })
        addAndMakeVisible (c);
}

void ColorFxPanel::paint (juce::Graphics& g)
{
    paintPanel (g, getLocalBounds(), "SOUND COLOR FX", Colours::color);
    g.setFont (LookAndFeel::font (11.0f, false));
    g.setColour (Colours::textDim);
    g.drawText ("LOW / L", 40, 410, 100, 16, juce::Justification::centredLeft);
    g.drawText ("HI / R", getWidth() - 140, 410, 100, 16, juce::Justification::centredRight);
}

void ColorFxPanel::resized()
{
    types.setBounds (16, 46, getWidth() - 32, 132);
    colorKnob.setBounds (40, 192, getWidth() - 80, 228);
    paramKnob.setBounds (16, 432, 140, 130);
    onButton.setBounds (174, 448, getWidth() - 190, 48);
    lockButton.setBounds (174, 508, getWidth() - 190, 36);
}

//==============================================================================
XPad::XPad (APVTS& s) : state (s) {}

int XPad::segmentAt (juce::Point<int> p) const
{
    return juce::jlimit (0, kNumBeats - 1, p.x * kNumBeats / juce::jmax (1, getWidth()));
}

void XPad::paint (juce::Graphics& g)
{
    const auto type = (BeatFxType) (int) getParam (state, ParamID::beatType);
    const int current = (int) getParam (state, ParamID::beatIdx);
    const bool sync = getParam (state, ParamID::beatSync) > 0.5f;
    const float w = (float) getWidth() / (float) kNumBeats;

    auto bg = getLocalBounds().toFloat();
    g.setColour (Colours::lcd);
    g.fillRoundedRectangle (bg, 8.0f);

    for (int i = 0; i < kNumBeats; ++i)
    {
        auto cell = juce::Rectangle<float> ((float) i * w, 0.0f, w, (float) getHeight()).reduced (3.0f);
        const bool isTouched = i == touched;
        const bool isCurrent = sync && i == current;
        if (isTouched || isCurrent)
        {
            g.setColour (Colours::beat.withAlpha (isTouched ? 0.95f : 0.35f));
            g.fillRoundedRectangle (cell, 6.0f);
        }
        g.setColour (isTouched ? juce::Colours::black : Colours::text.withAlpha (0.85f));
        g.setFont (LookAndFeel::font (15.0f));
        g.drawText (beatDisplayText (type, i), cell.toNearestInt(), juce::Justification::centred);
        if (i > 0)
        {
            g.setColour (Colours::panelEdge);
            g.drawVerticalLine ((int) ((float) i * w), 8.0f, (float) getHeight() - 8.0f);
        }
    }
    g.setColour (Colours::beat.withAlpha (0.5f));
    g.drawRoundedRectangle (bg.reduced (0.5f), 8.0f, 1.0f);
}

void XPad::mouseDown (const juce::MouseEvent& e)
{
    savedIdx = (int) getParam (state, ParamID::beatIdx);
    savedOn = getParam (state, ParamID::beatOn) > 0.5f;
    savedSync = getParam (state, ParamID::beatSync) > 0.5f;
    touched = segmentAt (e.getPosition());
    setParam (state, ParamID::beatSync, 1.0f);
    setParam (state, ParamID::beatIdx, (float) touched);
    setParam (state, ParamID::beatOn, 1.0f);
    repaint();
}

void XPad::mouseDrag (const juce::MouseEvent& e)
{
    const int seg = segmentAt (e.getPosition());
    if (seg != touched)
    {
        touched = seg;
        setParam (state, ParamID::beatIdx, (float) touched);
        repaint();
    }
}

void XPad::mouseUp (const juce::MouseEvent&)
{
    // Momentary like the hardware pad: release restores the previous state
    setParam (state, ParamID::beatOn, savedOn ? 1.0f : 0.0f);
    setParam (state, ParamID::beatIdx, (float) savedIdx);
    setParam (state, ParamID::beatSync, savedSync ? 1.0f : 0.0f);
    touched = -1;
    repaint();
}

//==============================================================================
void BeatDisplay::paint (juce::Graphics& g)
{
    auto& st = processor.apvts;
    const auto type = (BeatFxType) (int) getParam (st, ParamID::beatType);
    const int idx = (int) getParam (st, ParamID::beatIdx);
    const bool sync = getParam (st, ParamID::beatSync) > 0.5f;
    const double bpm = processor.getCurrentBpm();
    const auto mode = (int) getParam (st, ParamID::bpmMode);
    const bool engaged = processor.isBeatFxEngaged();

    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::lcd);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colours::lcdText.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (Colours::lcdText);
    g.setFont (LookAndFeel::font (22.0f));
    g.drawText (beatFxNames()[(int) type], 16, 10, getWidth() - 32, 28, juce::Justification::centredLeft);

    const double ms = sync ? TempoSync::beatsToSeconds (BeatFxEngine::effectiveBeats (type, idx), bpm) * 1000.0
                           : (double) getParam (st, ParamID::timeMs);
    g.setFont (LookAndFeel::font (40.0f));
    g.drawText (sync ? beatDisplayText (type, idx) : juce::String ("FREE"), getWidth() - 196, 8, 180, 50,
                juce::Justification::centredRight);

    g.setFont (LookAndFeel::font (14.0f, false));
    g.setColour (Colours::lcdText.withAlpha (0.75f));
    juce::String info;
    info << (isReverbFx (type) ? juce::String ("SIZE ") + juce::String (sync ? (idx + 1) * 10 : (int) (ms / 40.0)) + "%"
                               : juce::String ("TIME ") + juce::String (juce::roundToInt (ms)) + " ms")
         << "     BPM " << juce::String (bpm, 1) << " " << juce::StringArray { "HOST", "TAP", "MANUAL" }[mode];
    if (getParam (st, ParamID::quantize) > 0.5f) info << "     QUANTIZE";
    g.drawText (info, 16, getHeight() - 34, getWidth() - 32, 22, juce::Justification::centredLeft);

    // Engaged LED
    const auto led = juce::Rectangle<float> (10.0f, 10.0f).withPosition ((float) getWidth() - 22.0f, (float) getHeight() - 28.0f);
    g.setColour (engaged ? Colours::beat : Colours::panelEdge);
    g.fillEllipse (led);
}

//==============================================================================
BeatFxPanel::BeatFxPanel (RecklessDJFXProcessor& p)
    : state (p.apvts),
      types (state, ParamID::beatType, beatFxNames(), 7, Colours::beat),
      display (p),
      xpad (state),
      timeKnob (state, ParamID::timeMs, "TIME", Colours::beat),
      levelKnob (state, ParamID::level, "LEVEL / DEPTH", Colours::beat),
      outKnob (state, ParamID::outGain, "OUTPUT", Colours::textDim),
      low (state, ParamID::fxLow, "LOW", Colours::beat),
      mid (state, ParamID::fxMid, "MID", Colours::beat),
      hi (state, ParamID::fxHi, "HI", Colours::beat),
      quantize (state, ParamID::quantize, "QUANTIZE", Colours::beat),
      tape (state, ParamID::tape, "X-PAD TAPE", Colours::beat),
      sync (state, ParamID::beatSync, "BEAT SYNC", Colours::beat),
      onButton (state, ParamID::beatOn, "ON / OFF", Colours::beat)
{
    for (auto* c : std::initializer_list<juce::Component*> { &types, &display, &beatDown, &beatUp, &xpad, &timeKnob, &levelKnob,
                                                             &outKnob, &low, &mid, &hi, &quantize, &tape, &sync, &onButton,
                                                             &freqCaption, &beatCaption, &xpadCaption })
        addAndMakeVisible (c);

    for (auto* l : { &freqCaption, &beatCaption, &xpadCaption })
    {
        l->setColour (juce::Label::textColourId, Colours::textDim);
        l->setFont (LookAndFeel::font (12.0f));
    }
    beatCaption.setJustificationType (juce::Justification::centred);

    beatDown.onClick = [this] { stepBeat (-1); };
    beatUp.onClick = [this] { stepBeat (1); };

    // Turning TIME switches to free (unsynced) time, like the mixer
    timeKnob.getSlider().onDragStart = [this] { setParam (state, ParamID::beatSync, 0.0f); };
    levelKnob.getSlider().setDoubleClickReturnValue (true, 0.5);
    outKnob.getSlider().setDoubleClickReturnValue (true, 0.0);

    startTimerHz (30);
}

void BeatFxPanel::stepBeat (int delta)
{
    const int idx = juce::jlimit (0, kNumBeats - 1, (int) getParam (state, ParamID::beatIdx) + delta);
    setParam (state, ParamID::beatSync, 1.0f);
    setParam (state, ParamID::beatIdx, (float) idx);
}

void BeatFxPanel::timerCallback()
{
    // TIME is only in charge when BEAT SYNC is off
    timeKnob.setAlpha (getParam (state, ParamID::beatSync) > 0.5f ? 0.5f : 1.0f);
    display.repaint();
    xpad.repaint();
}

void BeatFxPanel::paint (juce::Graphics& g)
{
    paintPanel (g, getLocalBounds(), "BEAT FX", Colours::beat);
}

void BeatFxPanel::resized()
{
    types.setBounds (16, 46, getWidth() - 32, 84);
    display.setBounds (16, 142, 450, 110);
    beatCaption.setBounds (482, 142, getWidth() - 498, 18);
    const int bw = (getWidth() - 498 - 8) / 2;
    beatDown.setBounds (482, 164, bw, 88);
    beatUp.setBounds (482 + bw + 8, 164, bw, 88);
    xpadCaption.setBounds (16, 262, 200, 18);
    xpad.setBounds (16, 282, getWidth() - 32, 70);

    timeKnob.setBounds (16, 368, 150, 160);
    levelKnob.setBounds (176, 368, 150, 160);

    const int x0 = 346, w = getWidth() - x0 - 16;
    freqCaption.setBounds (x0, 366, w, 18);
    const int third = (w - 16) / 3;
    low.setBounds (x0, 388, third, 40);
    mid.setBounds (x0 + third + 8, 388, third, 40);
    hi.setBounds (x0 + 2 * (third + 8), 388, third, 40);
    sync.setBounds (x0, 438, third, 36);
    quantize.setBounds (x0 + third + 8, 438, third, 36);
    tape.setBounds (x0 + 2 * (third + 8), 438, third, 36);
    onButton.setBounds (x0, 488, w - 130, 72);
    outKnob.setBounds (x0 + w - 120, 476, 120, 96);
}
} // namespace rdfx::ui
