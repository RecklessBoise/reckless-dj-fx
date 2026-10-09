#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

class RecklessDJFXProcessor;

namespace rdfx::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Sets a parameter from a real-world value as one undoable host gesture. */
void setParam (APVTS& state, const char* id, float realValue);
float getParam (const APVTS& state, const char* id);

/** A radio group of buttons bound to a choice parameter. */
class ChoiceButtons final : public juce::Component
{
public:
    ChoiceButtons (APVTS& state, const char* paramID, const juce::StringArray& labels, int columns, juce::Colour accent);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns;
};

/** Rotary knob with a caption, bound to a float parameter. */
class Knob final : public juce::Component
{
public:
    Knob (APVTS& state, const char* paramID, const juce::String& caption, juce::Colour accent,
          std::unique_ptr<juce::Slider> customSlider = nullptr);
    void resized() override;
    juce::Slider& getSlider() { return *slider; }

private:
    std::unique_ptr<juce::Slider> slider;
    juce::Label caption;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

/** Bipolar COLOR knob that stops at the center when CENTER LOCK is on. */
class CenterLockSlider final : public juce::Slider
{
public:
    explicit CenterLockSlider (APVTS& s) : state (s) {}
    double snapValue (double attempted, DragMode mode) override;
    void mouseDown (const juce::MouseEvent& e) override { held = false; juce::Slider::mouseDown (e); }

private:
    APVTS& state;
    bool held = false;
};

/** Toggle button bound to a bool parameter. */
class ToggleBtn final : public juce::TextButton
{
public:
    ToggleBtn (APVTS& state, const char* paramID, const juce::String& text, juce::Colour accent);

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
class ColorFxPanel final : public juce::Component
{
public:
    explicit ColorFxPanel (APVTS& state);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChoiceButtons types;
    Knob colorKnob, paramKnob;
    ToggleBtn onButton, lockButton;
};

//==============================================================================
/** X-PAD: touch a beat value to engage the Beat FX at that value while held. */
class XPad final : public juce::Component
{
public:
    XPad (APVTS& state);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int segmentAt (juce::Point<int> p) const;
    APVTS& state;
    int touched = -1, savedIdx = 0;
    bool savedOn = false, savedSync = true;
};

/** LCD style read-out: effect, beat value, time and BPM. */
class BeatDisplay final : public juce::Component
{
public:
    BeatDisplay (RecklessDJFXProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;

private:
    RecklessDJFXProcessor& processor;
};

class BeatFxPanel final : public juce::Component, private juce::Timer
{
public:
    explicit BeatFxPanel (RecklessDJFXProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepBeat (int delta);

    APVTS& state;
    ChoiceButtons types;
    BeatDisplay display;
    juce::TextButton beatDown { juce::String::fromUTF8 ("\xe2\x97\x80") }, beatUp { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    XPad xpad;
    Knob timeKnob, levelKnob, outKnob;
    ToggleBtn low, mid, hi, quantize, tape, sync, onButton;
    juce::Label freqCaption { {}, "FX FREQUENCY" }, beatCaption { {}, "BEAT" }, xpadCaption { {}, "X-PAD" };
};
} // namespace rdfx::ui
