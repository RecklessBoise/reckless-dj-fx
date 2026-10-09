#include "Panels.h"
#include "PluginProcessor.h"
#include "dsp/BeatFx.h"

namespace rdfx::ui
{
using namespace rdfx::hw;

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

static void screws (juce::Graphics& g, juce::Rectangle<int> b, const juce::String& seed)
{
    const float m = 14.0f;
    const auto r = b.toFloat();
    drawScrew (g, { m, m }, 5.0f, seedOf (seed + "a"));
    drawScrew (g, { r.getWidth() - m, m }, 5.0f, seedOf (seed + "b"));
    drawScrew (g, { m, r.getHeight() - m }, 5.0f, seedOf (seed + "c"));
    drawScrew (g, { r.getWidth() - m, r.getHeight() - m }, 5.0f, seedOf (seed + "d"));
}

/** Prints a caption centred under a knob. */
static void caption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> knobBounds)
{
    drawPrinted (g, text, knobBounds.toFloat().withY ((float) knobBounds.getBottom() - 6.0f).withHeight (16.0f), 12.0f);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (APVTS& state, const char* paramID, const juce::StringArray& labels, int cols, juce::Colour led,
                              const char* gateParamID)
    : columns (cols)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setName (juce::String (paramID) + juce::String (i));
        LookAndFeel::setAccent (*b, led);
        b->onClick = [this, i]
        {
            if (onPick) onPick (i);
            else attachment->setValueAsCompleteGesture ((float) i);
        };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (
        *state.getParameter (paramID), [this] (float v) { selected = juce::roundToInt (v); updateLights(); });
    attachment->sendInitialUpdate();

    if (gateParamID != nullptr)
    {
        gateAttachment = std::make_unique<juce::ParameterAttachment> (
            *state.getParameter (gateParamID), [this] (float v) { gateOn = v > 0.5f; updateLights(); });
        gateAttachment->sendInitialUpdate();
    }
}

void ChoiceButtons::updateLights()
{
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (gateOn && i == selected, juce::dontSendNotification);
}

void ChoiceButtons::resized()
{
    const int rows = (buttons.size() + columns - 1) / columns;
    const float w = (float) getWidth() / (float) columns;
    const float h = (float) getHeight() / (float) rows;
    for (int i = 0; i < buttons.size(); ++i)
    {
        const int r = i / columns, c = i % columns;
        buttons[i]->setBounds (juce::Rectangle<float> ((float) c * w, (float) r * h, w, h).toNearestInt());
    }
}

//==============================================================================
Knob::Knob (APVTS& state, const char* paramID, const juce::String& name, const char* style, int ticks, bool detent,
            std::unique_ptr<juce::Slider> custom)
    : slider (custom != nullptr ? std::move (custom) : std::make_unique<juce::Slider>())
{
    slider->setName (name);
    slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider->setRotaryParameters (juce::degreesToRadians (-150.0f), juce::degreesToRadians (150.0f), true);
    slider->setMouseDragSensitivity (220);
    slider->setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    slider->setRepaintsOnMouseActivity (true);
    slider->getProperties().set ("knob", style);
    slider->getProperties().set ("ticks", ticks);
    slider->getProperties().set ("detent", detent);
    addAndMakeVisible (*slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, *slider);
}

void Knob::resized() { slider->setBounds (getLocalBounds()); }

void Knob::parentHierarchyChanged()
{
    // The value bubble lives in the scaled root component so it follows the editor size
    juce::Component* root = this;
    while (root->getParentComponent() != nullptr && ! root->getProperties().contains ("rdfxRoot"))
        root = root->getParentComponent();
    slider->setPopupDisplayEnabled (true, true, root->getProperties().contains ("rdfxRoot") ? root : nullptr, 900);
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
ToggleBtn::ToggleBtn (APVTS& state, const char* paramID, const juce::String& text, juce::Colour led, const char* style)
    : juce::TextButton (text)
{
    setName (paramID);
    setClickingTogglesState (true);
    LookAndFeel::setAccent (*this, led);
    getProperties().set ("btn", style);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, *this);
}

//==============================================================================
ColorFxPanel::ColorFxPanel (APVTS& state)
    : types (state, ParamID::colorType, colorFxNames(), 2, Col::colorLed, ParamID::colorOn),
      colorKnob (state, ParamID::colorAmt, "COLOR", "matte", 11, true, std::make_unique<CenterLockSlider> (state)),
      paramKnob (state, ParamID::colorParam, "PARAMETER", "matte", 11, false),
      onButton (state, ParamID::colorOn, "ON", Col::colorLed),
      lockButton (state, ParamID::centerLock, "CENTER LOCK", Col::colorLed)
{
    colorKnob.getSlider().setDoubleClickReturnValue (true, 0.0);
    paramKnob.getSlider().setDoubleClickReturnValue (true, 0.5);

    // Like the mixer: press a Color FX key to switch it on, press the lit key again to switch it off
    types.onPick = [&state] (int i)
    {
        const bool on = getParam (state, ParamID::colorOn) > 0.5f;
        if (on && (int) getParam (state, ParamID::colorType) == i)
        {
            setParam (state, ParamID::colorOn, 0.0f);
        }
        else
        {
            setParam (state, ParamID::colorType, (float) i);
            setParam (state, ParamID::colorOn, 1.0f);
        }
    };
    for (auto* c : std::initializer_list<juce::Component*> { &types, &colorKnob, &paramKnob, &onButton, &lockButton })
        addAndMakeVisible (c);
}

void ColorFxPanel::paint (juce::Graphics& g)
{
    screws (g, getLocalBounds(), "color");
    drawPrinted (g, "SOUND COLOR FX", { 30.0f, 14.0f, 200.0f, 22.0f }, 16.0f, juce::Justification::centredLeft);
    drawPrintedFrame (g, { 18.0f, 58.0f, 324.0f, 164.0f }, "SELECT");

    drawPrinted (g, "COLOR", colorKnob.getBounds().toFloat().withY ((float) colorKnob.getY() - 12.0f).withHeight (16.0f), 13.0f);
    drawPrinted (g, juce::String::fromUTF8 ("\xe2\x97\x84 LOW"), { 40.0f, (float) colorKnob.getBottom() - 18.0f, 90.0f, 16.0f }, 11.0f,
                 juce::Justification::centredLeft, 0.75f);
    drawPrinted (g, juce::String::fromUTF8 ("HI \xe2\x96\xba"), { 230.0f, (float) colorKnob.getBottom() - 18.0f, 90.0f, 16.0f }, 11.0f,
                 juce::Justification::centredRight, 0.75f);
    caption (g, "PARAMETER", paramKnob.getBounds());
    drawPrinted (g, "COLOR FX", onButton.getBounds().toFloat().withY ((float) onButton.getY() - 8.0f).withHeight (12.0f), 11.0f,
                 juce::Justification::centred, 0.75f);
}

void ColorFxPanel::resized()
{
    lockButton.setBounds (210, 4, 136, 44);
    types.setBounds (24, 66, 312, 152);
    colorKnob.setBounds (70, 248, 220, 220);
    paramKnob.setBounds (36, 470, 112, 112);
    onButton.setBounds (186, 494, 150, 64);
}

//==============================================================================
XPad::XPad (APVTS& s) : state (s) {}

juce::Rectangle<float> XPad::padArea() const { return getLocalBounds().toFloat().reduced (6.0f); }

int XPad::segmentAt (juce::Point<int> p) const
{
    const auto a = padArea();
    return juce::jlimit (0, kNumBeats - 1, (int) (((float) p.x - a.getX()) * kNumBeats / juce::jmax (1.0f, a.getWidth())));
}

void XPad::paint (juce::Graphics& g)
{
    const auto type = (BeatFxType) (int) getParam (state, ParamID::beatType);
    const int current = (int) getParam (state, ParamID::beatIdx);
    const bool sync = getParam (state, ParamID::beatSync) > 0.5f;
    const auto a = padArea();
    const float w = a.getWidth() / (float) kNumBeats;

    // Recess + glossy black glass
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (a.expanded (4.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d2025), a.getX(), a.getY(),
                                             juce::Colour (0xff050607), a.getX(), a.getBottom(), false));
    g.fillRoundedRectangle (a, 6.0f);

    for (int i = 0; i < kNumBeats; ++i)
    {
        auto cell = juce::Rectangle<float> (a.getX() + (float) i * w, a.getY(), w, a.getHeight()).reduced (2.0f, 6.0f);
        const bool isTouched = i == touched;
        const bool isCurrent = sync && i == current;

        const auto bar = juce::Rectangle<float> (cell.getWidth() * 0.6f, 3.0f).withCentre ({ cell.getCentreX(), cell.getBottom() - 6.0f });
        if (isTouched || isCurrent)
        {
            const auto col = Col::beatLed;
            g.setGradientFill (juce::ColourGradient (col.withAlpha (isTouched ? 0.5f : 0.22f), cell.getCentreX(), cell.getBottom(),
                                                     col.withAlpha (0.0f), cell.getCentreX(), cell.getY(), false));
            g.fillRoundedRectangle (cell, 4.0f);
            g.setColour (col.withAlpha (isTouched ? 1.0f : 0.75f));
            g.fillRoundedRectangle (bar, 1.5f);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (bar, 1.5f);
        }

        g.setFont (printFont (17.0f));
        g.setColour (isTouched ? juce::Colours::white : Col::print.withAlpha (isCurrent ? 1.0f : 0.7f));
        g.drawText (beatDisplayText (type, i), cell.withTrimmedBottom (8.0f).toNearestInt(), juce::Justification::centred);

        if (i > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawVerticalLine ((int) (a.getX() + (float) i * w), a.getY() + 10.0f, a.getBottom() - 10.0f);
        }
    }

    // Glass reflection
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), a.getX(), a.getY(),
                                             juce::Colours::transparentWhite, a.getX(), a.getCentreY(), false));
    g.fillRoundedRectangle (a.withHeight (a.getHeight() * 0.5f), 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (a.reduced (0.5f), 6.0f, 1.0f);
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
BeatScreen::BeatScreen (RecklessDJFXProcessor& p) : processor (p) {}

juce::Rectangle<float> BeatScreen::listArea() const
{
    auto s = screenArea();
    return { s.getX() + 6.0f, s.getY() + 34.0f, 150.0f, s.getHeight() - 64.0f };
}

void BeatScreen::paint (juce::Graphics& g)
{
    auto& st = processor.apvts;
    const auto type = (BeatFxType) (int) getParam (st, ParamID::beatType);
    const int idx = (int) getParam (st, ParamID::beatIdx);
    const bool sync = getParam (st, ParamID::beatSync) > 0.5f;
    const bool on = getParam (st, ParamID::beatOn) > 0.5f;
    const double bpm = processor.getCurrentBpm();
    const int mode = (int) getParam (st, ParamID::bpmMode);
    const float level = getParam (st, ParamID::level);
    const auto s = screenArea();

    drawLcdBezel (g, s);

    // Top bar: state, quantize, BPM
    auto top = s.withHeight (28.0f).reduced (8.0f, 4.0f);
    g.setColour (juce::Colour (0xff101823));
    g.fillRect (s.withHeight (28.0f).reduced (1.0f));
    g.setFont (lcdFont (14.0f));
    g.setColour (on ? Col::lcdAccent : Col::lcdBlue);
    g.drawText (on ? "BEAT FX  ON" : "BEAT FX", top, juce::Justification::centredLeft);
    g.setColour (Col::lcdText);
    g.setFont (lcdFont (19.0f));
    g.drawText (juce::String (bpm, 1), top.withTrimmedRight (52.0f), juce::Justification::centredRight);
    g.setFont (lcdFont (9.5f));
    g.setColour (Col::lcdBlue);
    g.drawFittedText (juce::String ("BPM\n") + juce::StringArray { "HOST", "TAP", "MANUAL" }[mode],
                      top.removeFromRight (48.0f).toNearestInt(), juce::Justification::centredRight, 2);
    if (getParam (st, ParamID::quantize) > 0.5f)
    {
        const auto q = juce::Rectangle<float> (s.getX() + 116.0f, s.getY() + 6.0f, 18.0f, 16.0f);
        g.setColour (Col::lcdAccent);
        g.fillRoundedRectangle (q, 2.0f);
        g.setColour (juce::Colours::black);
        g.setFont (lcdFont (12.0f));
        g.drawText ("Q", q, juce::Justification::centred);
    }

    // Effect list scrolling around the selected one
    const auto list = listArea();
    const float rowH = list.getHeight() / 5.0f;
    for (int k = -2; k <= 2; ++k)
    {
        const int t = (int) type + k;
        if (t < 0 || t >= kNumBeatFx) continue;
        auto row = juce::Rectangle<float> (list.getX(), list.getY() + (float) (k + 2) * rowH, list.getWidth(), rowH);
        if (k == 0)
        {
            g.setColour (Col::lcdAccent);
            g.fillRoundedRectangle (row.reduced (0.0f, 1.0f), 3.0f);
        }
        // Neighbours stay readable (>= 4.5:1 on the black screen): they are clickable
        g.setColour (k == 0 ? juce::Colours::white : Col::lcdText.withAlpha (std::abs (k) == 1 ? 0.78f : 0.62f));
        g.setFont (lcdFont (k == 0 ? 17.0f : 14.0f));
        g.drawText (beatFxNames()[t], row.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
    }

    // Beat value box
    auto right = juce::Rectangle<float> (list.getRight() + 10.0f, list.getY(), s.getRight() - list.getRight() - 18.0f, list.getHeight());
    g.setColour (juce::Colour (0xff0f141b));
    g.fillRoundedRectangle (right, 4.0f);
    g.setColour (Col::lcdBlue.withAlpha (0.35f));
    g.drawRoundedRectangle (right, 4.0f, 1.0f);
    g.setFont (lcdFont (12.0f));
    g.setColour (Col::lcdBlue);
    g.drawText (sync ? (isReverbFx (type) ? "SIZE" : "BEAT") : "TIME (FREE)", right.reduced (8.0f, 4.0f).withHeight (16.0f),
                juce::Justification::centredLeft);
    const double ms = sync ? TempoSync::beatsToSeconds (BeatFxEngine::effectiveBeats (type, idx), bpm) * 1000.0
                           : (double) getParam (st, ParamID::timeMs);
    g.setColour (Col::lcdText);
    g.setFont (lcdFont (50.0f));
    g.drawText (sync ? beatDisplayText (type, idx) : juce::String (juce::roundToInt (ms)), right.reduced (6.0f, 18.0f),
                juce::Justification::centred);
    g.setFont (lcdFont (13.0f, false));
    g.setColour (Col::lcdText.withAlpha (0.8f));
    auto info = right.reduced (8.0f, 4.0f).removeFromBottom (16.0f);
    g.drawText (juce::String (juce::roundToInt (ms)) + " ms", info, juce::Justification::centredLeft);
    g.drawText ("LEVEL " + juce::String (juce::roundToInt (level * 100.0f)) + "%", info, juce::Justification::centredRight);

    // Beat position bar (mirrors the X-PAD)
    auto barArea = juce::Rectangle<float> (s.getX() + 8.0f, s.getBottom() - 24.0f, s.getWidth() - 16.0f, 14.0f);
    const float cw = barArea.getWidth() / (float) kNumBeats;
    for (int i = 0; i < kNumBeats; ++i)
    {
        auto cell = juce::Rectangle<float> (barArea.getX() + (float) i * cw, barArea.getY(), cw, barArea.getHeight()).reduced (1.5f, 2.0f);
        g.setColour (sync && i == idx ? Col::lcdAccent : Col::lcdBlue.withAlpha (0.18f));
        g.fillRect (cell);
    }

    drawGlass (g, s);
}

void BeatScreen::mouseDown (const juce::MouseEvent& e)
{
    const auto list = listArea();
    if (! list.contains (e.position))
        return;
    const int k = (int) ((e.position.y - list.getY()) / (list.getHeight() / 5.0f)) - 2;
    const int t = juce::jlimit (0, kNumBeatFx - 1, (int) getParam (processor.apvts, ParamID::beatType) + k);
    setParam (processor.apvts, ParamID::beatType, (float) t);
}

void BeatScreen::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    wheelAccum += wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f);
    if (std::abs (wheelAccum) < 0.12f)
        return;
    const int step = wheelAccum > 0 ? -1 : 1;
    wheelAccum = 0.0f;
    const int t = juce::jlimit (0, kNumBeatFx - 1, (int) getParam (processor.apvts, ParamID::beatType) + step);
    setParam (processor.apvts, ParamID::beatType, (float) t);
}

//==============================================================================
BeatFxPanel::BeatFxPanel (RecklessDJFXProcessor& p)
    : processor (p), state (p.apvts),
      screen (p),
      selectKnob (state, ParamID::beatType, "FX SELECT", "encoder", kNumBeatFx, false),
      xpad (state),
      timeKnob (state, ParamID::timeMs, "TIME", "matte", 11, false),
      levelKnob (state, ParamID::level, "LEVEL/DEPTH", "matte", 11, false),
      outKnob (state, ParamID::outGain, "OUTPUT", "matte", 11, false),
      low (state, ParamID::fxLow, "LOW", Col::beatLed),
      mid (state, ParamID::fxMid, "MID", Col::beatLed),
      hi (state, ParamID::fxHi, "HI", Col::beatLed),
      quantize (state, ParamID::quantize, "QUANTIZE", Col::beatLed),
      tape (state, ParamID::tape, "X-PAD TAPE", Col::beatLed),
      sync (state, ParamID::beatSync, "BEAT SYNC", Col::beatLed),
      onButton (state, ParamID::beatOn, "ON/OFF", Col::onRing, "round")
{
    for (auto* c : std::initializer_list<juce::Component*> { &screen, &selectKnob, &beatDown, &beatUp, &xpad, &timeKnob, &levelKnob,
                                                             &outKnob, &low, &mid, &hi, &quantize, &tape, &sync, &onButton })
        addAndMakeVisible (c);

    beatDown.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
    beatUp.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    beatDown.setName ("beatDown");
    beatUp.setName ("beatUp");
    beatDown.onClick = [this] { stepBeat (-1); };
    beatUp.onClick = [this] { stepBeat (1); };

    // FX SELECT: stepped encoder, one detent per effect
    selectKnob.getSlider().setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    selectKnob.getSlider().setMouseDragSensitivity (300);

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
    // TIME only acts when BEAT SYNC is off: grey it out otherwise
    timeKnob.setAlpha (getParam (state, ParamID::beatSync) > 0.5f ? 0.45f : 1.0f);

    // ON/OFF ring pulses with the tempo, only while the effect is audible
    const bool engaged = processor.isBeatFxEngaged();
    if (engaged)
    {
        const double beatPhase = std::fmod (juce::Time::getMillisecondCounterHiRes() * processor.getCurrentBpm() / 60000.0, 1.0);
        onButton.getProperties().set ("glow", 1.0 - beatPhase);
        onButton.repaint();
    }
    else if ((double) onButton.getProperties()["glow"] != 0.0)
    {
        onButton.getProperties().set ("glow", 0.0);
        onButton.repaint();
    }

    // Screen and X-Pad: repaint only when something they show has changed
    juce::String now;
    for (auto* id : { ParamID::beatType, ParamID::beatIdx, ParamID::beatSync, ParamID::beatOn, ParamID::timeMs,
                      ParamID::level, ParamID::quantize, ParamID::bpmMode })
        now << getParam (state, id) << ";";
    now << juce::String (processor.getCurrentBpm(), 1) << ";" << (int) engaged;
    if (now != lastScreenState)
    {
        lastScreenState = now;
        screen.repaint();
        xpad.repaint();
    }
}

void BeatFxPanel::paint (juce::Graphics& g)
{
    screws (g, getLocalBounds(), "beat");
    drawPrinted (g, "BEAT FX", { 30.0f, 14.0f, 200.0f, 22.0f }, 16.0f, juce::Justification::centredLeft);

    drawPrintedFrame (g, { 424.0f, 34.0f, 310.0f, 150.0f }, "SELECT");
    drawPrintedFrame (g, { 424.0f, 204.0f, 310.0f, 70.0f }, "MODE");
    caption (g, juce::String::fromUTF8 ("\xe2\x97\x84 FX SELECT \xe2\x96\xba"), selectKnob.getBounds());
    drawPrinted (g, "BEAT", { (float) beatDown.getX(), (float) beatDown.getY() - 8.0f, (float) (beatUp.getRight() - beatDown.getX()), 12.0f },
                 11.0f, juce::Justification::centred, 0.8f);
    drawPrinted (g, "X-PAD", { 28.0f, (float) xpad.getY() - 14.0f, 100.0f, 14.0f }, 11.0f, juce::Justification::centredLeft, 0.8f);
    drawPrintedFrame (g, { 22.0f, 412.0f, 252.0f, 70.0f }, "FX FREQUENCY");
    caption (g, "OUTPUT", outKnob.getBounds());
    caption (g, "TIME", timeKnob.getBounds());
    caption (g, "LEVEL/DEPTH", levelKnob.getBounds());
    drawPrinted (g, "ON/OFF", onButton.getBounds().toFloat().withY ((float) onButton.getBottom() - 2.0f).withHeight (16.0f), 12.0f);
}

void BeatFxPanel::resized()
{
    screen.setBounds (22, 44, 392, 226);
    selectKnob.setBounds (432, 44, 128, 128);
    beatDown.setBounds (572, 54, 78, 64);
    beatUp.setBounds (648, 54, 78, 64);
    quantize.setBounds (572, 120, 154, 54);
    sync.setBounds (428, 212, 150, 56);
    tape.setBounds (576, 212, 150, 56);
    xpad.setBounds (22, 300, 704, 84);

    low.setBounds (28, 420, 82, 58);
    mid.setBounds (107, 420, 82, 58);
    hi.setBounds (186, 420, 82, 58);
    outKnob.setBounds (102, 490, 88, 88);

    timeKnob.setBounds (280, 396, 170, 170);
    levelKnob.setBounds (450, 406, 150, 150);
    onButton.setBounds (600, 418, 132, 132);
}
} // namespace rdfx::ui
