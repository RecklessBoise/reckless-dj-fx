#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "Hardware.h"

class RecklessDJFXProcessor;

namespace rdfx::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Sets a parameter from a real-world value as one undoable host gesture. */
void setParam (APVTS& state, const char* id, float realValue);
float getParam (const APVTS& state, const char* id);

/** A radio group of back-lit rubber keys bound to a choice parameter. */
class ChoiceButtons final : public juce::Component
{
public:
    ChoiceButtons (APVTS& state, const char* paramID, const juce::StringArray& labels, int columns, juce::Colour led);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns;
};

/** Hardware knob bound to a parameter. The caption is printed on the faceplate by the panel. */
class Knob final : public juce::Component
{
public:
    Knob (APVTS& state, const char* paramID, const juce::String& name, const char* style, int ticks, bool detent,
          std::unique_ptr<juce::Slider> customSlider = nullptr);
    void resized() override;
    void parentHierarchyChanged() override;
    juce::Slider& getSlider() { return *slider; }

private:
    std::unique_ptr<juce::Slider> slider;
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

/** Toggle key bound to a bool parameter. */
class ToggleBtn final : public juce::TextButton
{
public:
    ToggleBtn (APVTS& state, const char* paramID, const juce::String& text, juce::Colour led, const char* style = "rubber");

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
/** X-PAD: glossy touch strip. Touch a beat value to engage the Beat FX while held. */
class XPad final : public juce::Component
{
public:
    explicit XPad (APVTS& state);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> padArea() const;
    int segmentAt (juce::Point<int> p) const;
    APVTS& state;
    int touched = -1, savedIdx = 0;
    bool savedOn = false, savedSync = true;
};

/** Colour TFT-style screen: effect list, beat value, time, level and BPM. Click/scroll to pick an effect. */
class BeatScreen final : public juce::Component
{
public:
    explicit BeatScreen (RecklessDJFXProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> screenArea() const { return getLocalBounds().toFloat().reduced (8.0f); }
    juce::Rectangle<float> listArea() const;
    RecklessDJFXProcessor& processor;
    float wheelAccum = 0.0f;
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

    RecklessDJFXProcessor& processor;
    APVTS& state;
    BeatScreen screen;
    Knob selectKnob;
    juce::TextButton beatDown { "BEAT -" }, beatUp { "BEAT +" };
    XPad xpad;
    Knob timeKnob, levelKnob, outKnob;
    ToggleBtn low, mid, hi, quantize, tape, sync, onButton;
};
} // namespace rdfx::ui
