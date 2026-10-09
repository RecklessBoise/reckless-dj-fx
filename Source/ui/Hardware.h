#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Procedural "real hardware" rendering: brushed anodised faceplate with scratches and wear,
    knurled matte-black knobs, back-lit rubber buttons, LCD glass, screws and silk-screen print.
    Everything is drawn in code (no bitmaps shipped) and deterministic, so the wear looks the same every time.
*/
namespace rdfx::hw
{
namespace Col
{
    inline const juce::Colour faceplate  { 0xff1c1d20 };
    inline const juce::Colour faceplateHi { 0xff2a2c30 };
    inline const juce::Colour print      { 0xffe4e6ea };  // white silk-screen
    inline const juce::Colour printDim   { 0xff9aa0aa };
    inline const juce::Colour beatLed    { 0xffee7633 };  // orange back-light (softened)
    inline const juce::Colour colorLed   { 0xff4cbde8 };  // cyan back-light (softened)
    inline const juce::Colour onRing     { 0xff3d9bff };  // ON/OFF ring
    inline const juce::Colour like       { 0xffff3d6e };
    inline const juce::Colour lcdBg      { 0xff05080c };
    inline const juce::Colour lcdText    { 0xffeef6ff };
    inline const juce::Colour lcdAccent  { 0xffff4b2b };
    inline const juce::Colour lcdBlue    { 0xff41b8ff };
}

/** All knobs are matte black like the mixer; the encoder has a finer knurl and a dot pointer. */
enum class KnobStyle { Matte, Encoder };

/** Brushed dark metal with scratches, scuffs and edge wear. Cached per size. */
const juce::Image& faceplate (int width, int height, juce::uint32 seed);

/** Lighter, finer brushed aluminium strip (top bar). */
const juce::Image& brushedStrip (int width, int height);

void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, juce::uint32 seed);
void drawPanelSeam (juce::Graphics&, juce::Rectangle<float> area);
void drawPrinted (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification just = juce::Justification::centred, float alpha = 0.9f);
void drawPrintedFrame (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title);

/** Knob. `angle` in radians (0 = 12 o'clock). Ticks are the printed scale on the faceplate. */
void drawKnob (juce::Graphics&, juce::Rectangle<float> bounds, float angle, KnobStyle style, juce::uint32 seed,
               float startAngle, float endAngle, int numTicks, bool centreDetent, bool hover = false);

/** Back-lit rubber key. `led` is the light colour when on. */
void drawRubberButton (juce::Graphics&, juce::Rectangle<float> r, bool on, bool over, bool down, juce::Colour led,
                       juce::uint32 seed);

/** Large round ON/OFF key with chrome bezel and LED ring. */
void drawRoundButton (juce::Graphics&, juce::Rectangle<float> r, bool on, bool over, bool down, juce::Colour led,
                      float glow);

/** Small round indicator LED. */
void drawLed (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, bool on);

/** LCD bezel + black screen; call drawGlass() after the content to add the reflection. */
void drawLcdBezel (juce::Graphics&, juce::Rectangle<float> r);
void drawGlass (juce::Graphics&, juce::Rectangle<float> screen);

/** Fonts used for print and the LCD. */
juce::Font printFont (float height, bool bold = true);
juce::Font lcdFont (float height, bool bold = true);

juce::uint32 seedOf (const juce::String& s);
} // namespace rdfx::hw
