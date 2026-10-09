#include "Hardware.h"
#include <map>

namespace rdfx::hw
{
namespace
{
constexpr float kLightDir = -juce::MathConstants<float>::pi * 0.25f; // light from the top-left

juce::Point<float> polar (juce::Point<float> c, float radius, float angle)
{
    return c.getPointOnCircumference (radius, angle);
}

/** Fast brushed-metal fill: per-row tone + long horizontal streaks + fine grain. */
void brush (juce::Image& img, float base, float rowVar, float streak, float grain, int streakLen, juce::Random& rng,
            juce::Colour tint)
{
    const int w = img.getWidth(), h = img.getHeight();
    juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
    std::vector<float> noise ((size_t) w + (size_t) streakLen), smooth ((size_t) w);
    float rowTone = 0.0f;

    for (int y = 0; y < h; ++y)
    {
        rowTone = rowTone * 0.6f + (rng.nextFloat() * 2.0f - 1.0f) * rowVar;
        for (auto& n : noise) n = rng.nextFloat() * 2.0f - 1.0f;
        float acc = 0.0f;
        for (int x = 0; x < streakLen; ++x) acc += noise[(size_t) x];
        for (int x = 0; x < w; ++x)
        {
            smooth[(size_t) x] = acc / (float) streakLen;
            acc += noise[(size_t) (x + streakLen)] - noise[(size_t) x];
        }

        for (int x = 0; x < w; ++x)
        {
            const float v = base + rowTone + smooth[(size_t) x] * streak * 4.0f + (rng.nextFloat() * 2.0f - 1.0f) * grain;
            const auto c = juce::jlimit (0.0f, 255.0f, v);
            const auto r = (juce::uint8) juce::jlimit (0.0f, 255.0f, c * tint.getFloatRed() * 1.0f);
            const auto g = (juce::uint8) juce::jlimit (0.0f, 255.0f, c * tint.getFloatGreen());
            const auto b = (juce::uint8) juce::jlimit (0.0f, 255.0f, c * tint.getFloatBlue());
            data.setPixelColour (x, y, juce::Colour (r, g, b));
        }
    }
}

void scratch (juce::Graphics& g, juce::Random& rng, juce::Rectangle<float> area, int count, float maxLen, float alphaMax,
              float widthScale)
{
    for (int i = 0; i < count; ++i)
    {
        const float x = area.getX() + rng.nextFloat() * area.getWidth();
        const float y = area.getY() + rng.nextFloat() * area.getHeight();
        const float len = (0.15f + rng.nextFloat()) * maxLen;
        // Mostly near-horizontal (along the grain) with some random strays
        const float ang = rng.nextFloat() < 0.65f ? (rng.nextFloat() - 0.5f) * 0.35f : rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float bend = (rng.nextFloat() - 0.5f) * len * 0.25f;
        const juce::Point<float> a (x, y), b (x + std::cos (ang) * len, y + std::sin (ang) * len);
        const auto mid = (a + b) * 0.5f + juce::Point<float> (-std::sin (ang), std::cos (ang)) * bend;

        juce::Path p;
        p.startNewSubPath (a);
        p.quadraticTo (mid, b);
        const float alpha = (0.25f + 0.75f * rng.nextFloat()) * alphaMax;
        const float width = (0.35f + rng.nextFloat() * 0.9f) * widthScale;
        g.setColour (juce::Colours::black.withAlpha (alpha * 0.9f));
        g.strokePath (p, juce::PathStrokeType (width), juce::AffineTransform::translation (0.0f, width));
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (width * 0.8f));
    }
}
} // namespace

juce::uint32 seedOf (const juce::String& s) { return (juce::uint32) s.hashCode(); }

juce::Font printFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Helvetica Neue", height, bold ? juce::Font::bold : juce::Font::plain))
        .withHorizontalScale (0.92f);
}

juce::Font lcdFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("DIN Alternate", height, bold ? juce::Font::bold : juce::Font::plain));
}

const juce::Image& faceplate (int width, int height, juce::uint32 seed)
{
    static std::map<std::tuple<int, int, juce::uint32>, juce::Image> cache;
    auto key = std::make_tuple (width, height, seed);
    if (auto it = cache.find (key); it != cache.end())
        return it->second;

    juce::Image img (juce::Image::RGB, width, height, false);
    juce::Random rng ((juce::int64) seed);
    brush (img, 33.0f, 1.6f, 2.4f, 2.2f, juce::jmax (8, width / 40), rng, juce::Colour (0xfff2f4ff));

    {
        juce::Graphics g (img);
        const auto all = img.getBounds().toFloat();
        const float s = (float) width / 1100.0f; // texture is generated at N x the base layout

        // Soft wear patches (hands, cases, cleaning)
        for (int i = 0; i < 26; ++i)
        {
            const auto c = juce::Point<float> (rng.nextFloat() * all.getWidth(), rng.nextFloat() * all.getHeight());
            const float rad = (40.0f + rng.nextFloat() * 140.0f) * s;
            juce::ColourGradient grad (juce::Colours::white.withAlpha (0.018f + rng.nextFloat() * 0.025f), c,
                                       juce::Colours::transparentWhite, c.translated (rad, 0), true);
            g.setGradientFill (grad);
            g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 1.3f).withCentre (c));
        }

        // Scratches: many fine, a few deep
        scratch (g, rng, all, 650, 70.0f * s, 0.085f, s);
        scratch (g, rng, all, 80, 160.0f * s, 0.15f, 1.3f * s);
        scratch (g, rng, all, 16, 260.0f * s, 0.20f, 1.6f * s);

        // Top light / bottom shade
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0, 0,
                                                 juce::Colours::black.withAlpha (0.22f), 0, all.getBottom(), false));
        g.fillRect (all);
    }

    return cache.emplace (key, img).first->second;
}

const juce::Image& brushedStrip (int width, int height)
{
    static std::map<std::pair<int, int>, juce::Image> cache;
    auto key = std::make_pair (width, height);
    if (auto it = cache.find (key); it != cache.end())
        return it->second;

    juce::Image img (juce::Image::RGB, width, height, false);
    juce::Random rng (7331);
    brush (img, 52.0f, 2.0f, 3.0f, 3.0f, juce::jmax (8, width / 25), rng, juce::Colour (0xfff4f6ff));
    {
        juce::Graphics g (img);
        const auto all = img.getBounds().toFloat();
        const float s = (float) width / 1100.0f;
        scratch (g, rng, all, 120, 60.0f * s, 0.07f, s);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, 0,
                                                 juce::Colours::black.withAlpha (0.30f), 0, all.getBottom(), false));
        g.fillRect (all);
    }
    return cache.emplace (key, img).first->second;
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, juce::uint32 seed)
{
    juce::Random rng ((juce::int64) seed);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.3f, r * 2.3f).withCentre (c.translated (0.0f, r * 0.15f)));
    juce::ColourGradient grad (juce::Colour (0xffb9bcc2), c.translated (-r * 0.6f, -r * 0.6f),
                               juce::Colour (0xff2f3135), c.translated (r * 0.7f, r * 0.8f), false);
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));

    const float rot = rng.nextFloat() * juce::MathConstants<float>::pi;
    for (int i = 0; i < 2; ++i)
    {
        juce::Path slot;
        slot.addRoundedRectangle (-r * 0.75f, -r * 0.13f, r * 1.5f, r * 0.26f, r * 0.1f);
        const auto t = juce::AffineTransform::rotation (rot + (float) i * juce::MathConstants<float>::halfPi).translated (c);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillPath (slot, t);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.strokePath (slot, juce::PathStrokeType (0.5f), t.translated (0.0f, 0.6f));
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 0.6f);
}

void drawPanelSeam (juce::Graphics& g, juce::Rectangle<float> a)
{
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillRect (a);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    if (a.getWidth() < a.getHeight())
        g.fillRect (a.getRight(), a.getY(), 1.0f, a.getHeight());
    else
        g.fillRect (a.getX(), a.getBottom(), a.getWidth(), 1.0f);
}

void drawPrinted (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification just, float alpha)
{
    g.setFont (printFont (height));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText (text, area.translated (0.0f, 1.0f), just, false);
    g.setColour (Col::print.withAlpha (alpha));
    g.drawText (text, area, just, false);
}

void drawPrintedFrame (juce::Graphics& g, juce::Rectangle<float> a, const juce::String& title)
{
    const auto font = printFont (11.0f);
    const float tw = title.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font, title) + 12.0f;
    const float x0 = a.getX() + 10.0f;
    juce::Path p;
    p.startNewSubPath (x0, a.getY());
    p.lineTo (a.getX() + 4.0f, a.getY());
    p.quadraticTo (a.getX(), a.getY(), a.getX(), a.getY() + 4.0f);
    p.lineTo (a.getX(), a.getBottom() - 4.0f);
    p.quadraticTo (a.getX(), a.getBottom(), a.getX() + 4.0f, a.getBottom());
    p.lineTo (a.getRight() - 4.0f, a.getBottom());
    p.quadraticTo (a.getRight(), a.getBottom(), a.getRight(), a.getBottom() - 4.0f);
    p.lineTo (a.getRight(), a.getY() + 4.0f);
    p.quadraticTo (a.getRight(), a.getY(), a.getRight() - 4.0f, a.getY());
    p.lineTo (x0 + tw, a.getY());
    g.setColour (Col::print.withAlpha (0.32f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    if (title.isNotEmpty())
        drawPrinted (g, title, { x0 + 6.0f, a.getY() - 7.0f, tw, 14.0f }, 11.0f, juce::Justification::centredLeft, 0.8f);
}

void drawKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float angle, KnobStyle style, juce::uint32 seed,
               float startAngle, float endAngle, int numTicks, bool centreDetent, bool hover)
{
    juce::Random rng ((juce::int64) seed);
    const auto c = bounds.getCentre();
    const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float r = numTicks > 0 ? R - juce::jmax (9.0f, R * 0.13f) : R - 4.0f;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Printed scale
    if (numTicks > 1)
    {
        for (int i = 0; i < numTicks; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (numTicks - 1);
            const bool major = i == 0 || i == numTicks - 1 || (centreDetent && i == numTicks / 2);
            g.setColour (Col::print.withAlpha (major ? 0.9f : 0.55f));
            g.drawLine ({ polar (c, R - (major ? 8.0f : 6.0f), a), polar (c, R - 1.0f, a) }, major ? 1.8f : 1.2f);
        }
    }

    // Hover: a faint ring tells the knob can be grabbed
    if (hover)
    {
        g.setColour (Col::print.withAlpha (0.22f));
        g.drawEllipse (juce::Rectangle<float> ((r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f).withCentre (c), 1.5f);
    }

    // Drop shadow
    for (int i = 0; i < 6; ++i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.11f));
        const float rr = r * (1.0f + 0.035f * (float) i);
        g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c.translated (r * 0.05f, r * 0.12f)));
    }

    // Skirt
    const bool fineKnurl = style == KnobStyle::Encoder;
    {
        juce::ColourGradient grad (juce::Colour (0xff34373c), c.translated (-r * 0.4f, -r * 0.5f),
                                   juce::Colour (0xff07080a),
                                   c.translated (r * 0.9f, r * 0.9f), true);
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    }

    // Knurling (rotates with the knob)
    const int ridges = fineKnurl ? 72 : 48;
    for (int k = 0; k < ridges; ++k)
    {
        const float th = angle + twoPi * (float) k / (float) ridges;
        const float lit = juce::jmax (0.0f, std::cos (th - kLightDir));
        g.setColour (juce::Colours::white.withAlpha (0.04f + 0.16f * lit));
        g.drawLine ({ polar (c, r * 0.80f, th), polar (c, r * 0.975f, th) }, juce::jmax (0.7f, r * 0.022f));
        const float th2 = th + twoPi * 0.5f / (float) ridges;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine ({ polar (c, r * 0.80f, th2), polar (c, r * 0.975f, th2) }, juce::jmax (0.7f, r * 0.02f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.0f);
    {
        juce::Path rim;
        rim.addCentredArc (c.x, c.y, r - 0.8f, r - 0.8f, 0.0f, -2.4f, 0.5f, true);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.strokePath (rim, juce::PathStrokeType (1.1f));
    }

    // Cap
    const float capR = style == KnobStyle::Encoder ? r * 0.62f : r * 0.74f;
    const auto capRect = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillEllipse (capRect.expanded (capR * 0.06f));

    // Matte black plastic cap: soft diffuse shading, no specular, fine grain
    {
        juce::ColourGradient grad (juce::Colour (0xff2c2e33), c.translated (-capR * 0.45f, -capR * 0.55f),
                                   juce::Colour (0xff111214), c.translated (capR * 0.9f, capR * 0.9f), true);
        g.setGradientFill (grad);
        g.fillEllipse (capRect);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), c.translated (-capR * 0.3f, -capR * 0.4f),
                                                 juce::Colours::transparentWhite, c.translated (capR * 0.6f, capR * 0.2f), true));
        g.fillEllipse (capRect);

        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (capRect);
        g.reduceClipRegion (clip);
        const int grains = (int) (capR * capR * 0.35f);
        for (int i = 0; i < grains; ++i)
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
            g.fillRect (capRect.getX() + rng.nextFloat() * capRect.getWidth(), capRect.getY() + rng.nextFloat() * capRect.getHeight(), 1.0f, 1.0f);
        }
    }

    // Wear: scratches on the cap (rotate with the knob)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (capRect);
        g.reduceClipRegion (clip);
        const auto rot = juce::AffineTransform::rotation (angle, c.x, c.y);
        const int n = 5 + rng.nextInt (6);
        for (int i = 0; i < n; ++i)
        {
            const float a = rng.nextFloat() * twoPi, d = rng.nextFloat() * capR * 0.9f;
            const auto p0 = polar (c, d, a);
            const float dir = rng.nextFloat() * twoPi, len = capR * (0.15f + rng.nextFloat() * 0.6f);
            juce::Path s;
            s.startNewSubPath (p0);
            s.quadraticTo (p0 + juce::Point<float> (std::cos (dir + 0.3f), std::sin (dir + 0.3f)) * len * 0.5f,
                           p0 + juce::Point<float> (std::cos (dir), std::sin (dir)) * len);
            g.setColour (juce::Colours::white.withAlpha (0.05f + rng.nextFloat() * 0.08f)); // faint scuffs on matte plastic
            g.strokePath (s, juce::PathStrokeType (0.5f + rng.nextFloat() * 0.5f), rot);
        }
    }

    // Pointer
    if (style == KnobStyle::Encoder)
    {
        g.setColour (Col::print);
        g.fillEllipse (juce::Rectangle<float> (capR * 0.22f, capR * 0.22f).withCentre (polar (c, capR * 0.68f, angle)));
    }
    else
    {
        const auto p0 = polar (c, capR * 0.30f, angle);
        const auto p1 = polar (c, r * 0.93f, angle);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawLine ({ p0, p1 }, juce::jmax (2.6f, r * 0.075f));
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawLine ({ p0, p1 }, juce::jmax (1.6f, r * 0.045f));
    }

    // Cap edge highlight
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    juce::Path edge;
    edge.addCentredArc (c.x, c.y, capR - 0.6f, capR - 0.6f, 0.0f, -2.3f, 0.2f, true);
    g.strokePath (edge, juce::PathStrokeType (0.9f));
}

void drawRubberButton (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over, bool down, juce::Colour led,
                       juce::uint32 seed)
{
    juce::Random rng ((juce::int64) seed);
    const float corner = juce::jmin (5.0f, r.getHeight() * 0.22f);

    // Hole in the faceplate
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (r.expanded (2.5f), corner + 2.0f);

    if (on)
    {
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (led.withAlpha (0.045f));
            g.fillRoundedRectangle (r.expanded (2.0f + (float) i * 1.4f), corner + (float) i * 2.0f);
        }
    }

    auto b = down ? r.translated (0.0f, 1.0f) : r;
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (led.brighter (0.35f), b.getX(), b.getY(),
                                                 led.darker (0.25f), b.getX(), b.getBottom(), false));
    }
    else
    {
        auto top = juce::Colour (0xff474a50), bottom = juce::Colour (0xff25272b);
        if (over) { top = top.brighter (0.12f); bottom = bottom.brighter (0.1f); }
        g.setGradientFill (juce::ColourGradient (top, b.getX(), b.getY(), bottom, b.getX(), b.getBottom(), false));
    }
    g.fillRoundedRectangle (b, corner);

    // Rubber matte texture + finger wear
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (b.toNearestInt());
        const int dots = (int) (b.getWidth() * b.getHeight() / 28.0f);
        for (int i = 0; i < dots; ++i)
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
            g.fillRect (b.getX() + rng.nextFloat() * b.getWidth(), b.getY() + rng.nextFloat() * b.getHeight(), 1.0f, 1.0f);
        }
        const auto wear = juce::Point<float> (b.getX() + b.getWidth() * (0.3f + 0.4f * rng.nextFloat()), b.getCentreY());
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.12f : 0.07f), wear,
                                                 juce::Colours::transparentWhite, wear.translated (b.getWidth() * 0.35f, 0), true));
        g.fillRect (b);
    }

    // Bevel
    g.setColour (juce::Colours::white.withAlpha (on ? 0.45f : 0.20f));
    g.drawLine (b.getX() + corner, b.getY() + 0.8f, b.getRight() - corner, b.getY() + 0.8f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawLine (b.getX() + corner, b.getBottom() - 0.6f, b.getRight() - corner, b.getBottom() - 0.6f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (b, corner, 0.8f);
}

void drawRoundButton (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over, bool down, juce::Colour led, float glow)
{
    const auto c = r.getCentre();
    const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;

    if (on)
        for (int i = 6; i >= 1; --i)
        {
            g.setColour (led.withAlpha (0.05f + 0.05f * glow));
            const float rr = R * 0.9f + (float) i * R * 0.025f;
            g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c));
        }

    // Chrome bezel
    const float bez = R * 0.86f;
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (juce::Rectangle<float> (bez * 2.08f, bez * 2.08f).withCentre (c.translated (0.0f, R * 0.04f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe1e4e8), c.translated (-bez * 0.7f, -bez * 0.7f),
                                             juce::Colour (0xff3c3f44), c.translated (bez * 0.7f, bez * 0.8f), false));
    g.fillEllipse (juce::Rectangle<float> (bez * 2.0f, bez * 2.0f).withCentre (c));

    // LED ring (translucent)
    const float ring = bez * 0.88f;
    g.setColour (on ? led.brighter (0.2f) : juce::Colour (0xff18222c));
    g.fillEllipse (juce::Rectangle<float> (ring * 2.0f, ring * 2.0f).withCentre (c));
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.6f), c.translated (0.0f, -ring),
                                                 led.withAlpha (0.0f), c, true));
        g.fillEllipse (juce::Rectangle<float> (ring * 2.0f, ring * 2.0f).withCentre (c));
    }

    // Rubber dome
    const float dome = ring * 0.80f;
    auto domeRect = juce::Rectangle<float> (dome * 2.0f, dome * 2.0f).withCentre (c.translated (0.0f, down ? 1.0f : 0.0f));
    g.setGradientFill (juce::ColourGradient (over ? juce::Colour (0xff4a4d53) : juce::Colour (0xff3c3f45),
                                             domeRect.getCentre().translated (-dome * 0.35f, -dome * 0.45f),
                                             juce::Colour (0xff0d0e10), domeRect.getCentre().translated (dome, dome), true));
    g.fillEllipse (domeRect);
    const auto spec = juce::Rectangle<float> (dome * 1.2f, dome * 0.6f).withCentre (domeRect.getCentre().translated (0.0f, -dome * 0.45f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), spec.getCentre(),
                                             juce::Colours::transparentWhite, juce::Point<float> (spec.getRight(), spec.getCentreY()), true));
    g.fillEllipse (spec);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (domeRect, 1.0f);
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool on)
{
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f), c, colour.withAlpha (0.0f), c.translated (r * 3.0f, 0), true));
        g.fillEllipse (juce::Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (c));
    }
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.5f, r * 2.5f).withCentre (c));
    g.setGradientFill (juce::ColourGradient (on ? colour.brighter (0.6f) : colour.withMultipliedBrightness (0.25f),
                                             c.translated (-r * 0.3f, -r * 0.3f),
                                             on ? colour : colour.withMultipliedBrightness (0.12f), c.translated (r, r), true));
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.25f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.7f, r * 0.5f).withCentre (c.translated (-r * 0.3f, -r * 0.4f)));
}

void drawLcdBezel (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto outer = r.expanded (7.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (outer.translated (0.0f, 2.0f).expanded (1.0f), 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e3034), outer.getX(), outer.getY(),
                                             juce::Colour (0xff0e0f11), outer.getX(), outer.getBottom(), false));
    g.fillRoundedRectangle (outer, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (outer.reduced (0.5f), 7.0f, 1.0f);
    g.setColour (Col::lcdBg);
    g.fillRoundedRectangle (r, 3.0f);
}

void drawGlass (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
    g.fillRoundedRectangle (r, 3.0f);
    juce::Path sheen;
    sheen.startNewSubPath (r.getX() + r.getWidth() * 0.55f, r.getY());
    sheen.lineTo (r.getX() + r.getWidth() * 0.75f, r.getY());
    sheen.lineTo (r.getX() + r.getWidth() * 0.45f, r.getBottom());
    sheen.lineTo (r.getX() + r.getWidth() * 0.25f, r.getBottom());
    sheen.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    g.fillPath (sheen);
    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 3.0f, 1.2f);
}
} // namespace rdfx::hw
