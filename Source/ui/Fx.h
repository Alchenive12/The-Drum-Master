// File: Source/ui/Fx.h
// Light and surface effects for the editor: glows, frosted glass, soft (blurred) light, film grain,
// and CachedLayer, which renders a static background once at the screen's real pixel density and
// then only blits it. Everything expensive (blur, grain, fog) lives in cached layers, so the 60 Hz
// animation only redraws what moves.
#pragma once

#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ui::fx
{
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::Point;
using juce::Rectangle;

constexpr float pi = 3.14159265358979f;

/** A point on a circle, in JUCE's rotary convention (0 = 12 o'clock, clockwise). */
inline Point<float> polar (Point<float> centre, float angle, float radius)
{
    return centre.translated (std::sin (angle) * radius, -std::cos (angle) * radius);
}

/** A stroke with a soft bloom around it (layered strokes, cheap enough to animate). */
inline void glowStroke (Graphics& g, const Path& p, Colour c, float width, float intensity = 1.0f,
                        juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::rounded)
{
    const float layers[3][2] = { { 5.0f, 0.05f }, { 3.0f, 0.10f }, { 1.8f, 0.20f } };
    for (const auto& l : layers)
    {
        g.setColour (c.withMultipliedAlpha (l[1] * intensity));
        g.strokePath (p, juce::PathStrokeType (width * l[0], juce::PathStrokeType::curved, cap));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, cap));
}

/** A radial light: colour at the centre fading to nothing at `radius`. */
inline void glowDot (Graphics& g, Point<float> centre, float radius, Colour c)
{
    if (radius <= 0.0f || c.getAlpha() == 0) return;
    juce::ColourGradient grad (c, centre, c.withAlpha (0.0f), centre.translated (radius, 0.0f), true);
    grad.addColour (0.35, c.withMultipliedAlpha (0.45f));
    g.setGradientFill (grad);
    g.fillEllipse (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
}

/** An elliptical radial light (for light pools on the floor). */
inline void glowEllipse (Graphics& g, Rectangle<float> r, Colour c)
{
    g.saveState();
    g.addTransform (juce::AffineTransform::scale (1.0f, r.getHeight() / juce::jmax (1.0f, r.getWidth()), r.getCentreX(), r.getCentreY()));
    glowDot (g, r.getCentre(), r.getWidth() * 0.5f, c);
    g.restoreState();
}

// ------------------------------------------------------------------------------------ pixels
/** Box blur, 3 passes (close to a gaussian), on an ARGB software image, in place. */
inline void boxBlur (juce::Image& img, int radius, int passes = 3)
{
    if (radius < 1 || img.isNull()) return;
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    const int w = bd.width, h = bd.height;
    if (w < 2 || h < 2 || bd.pixelStride != 4) return;
    std::vector<int> line (static_cast<std::size_t> (juce::jmax (w, h)) * 4u);
    auto blurLine = [&line, radius] (std::uint8_t* start, int count, int stride) {
        for (int i = 0; i < count; ++i)
            for (int ch = 0; ch < 4; ++ch)
                line[static_cast<std::size_t> (i * 4 + ch)] = start[i * stride + ch];
        const int div = radius * 2 + 1;
        auto at = [&line, count] (int i, int ch) { return line[static_cast<std::size_t> (juce::jlimit (0, count - 1, i) * 4 + ch)]; };
        for (int ch = 0; ch < 4; ++ch)
        {
            int sum = 0;
            for (int k = -radius; k <= radius; ++k) sum += at (k, ch);
            for (int i = 0; i < count; ++i)
            {
                start[i * stride + ch] = static_cast<std::uint8_t> (sum / div);
                sum += at (i + radius + 1, ch) - at (i - radius, ch);
            }
        }
    };
    for (int pass = 0; pass < passes; ++pass)
    {
        for (int y = 0; y < h; ++y) blurLine (bd.getLinePointer (y), w, bd.pixelStride);
        for (int x = 0; x < w; ++x) blurLine (bd.getPixelPointer (x, 0), h, bd.lineStride);
    }
}

/** Film grain on an opaque image: a fixed, fine luminance noise (amount ~ 0..20). */
inline void addGrain (juce::Image& img, float amount, int seed = 7)
{
    if (amount <= 0.0f || img.isNull()) return;
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    if (bd.pixelStride != 4) return;
    std::uint32_t state = static_cast<std::uint32_t> (seed) * 2654435761u + 1u;
    for (int y = 0; y < bd.height; ++y)
    {
        auto* px = bd.getLinePointer (y);
        for (int x = 0; x < bd.width; ++x, px += 4)
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;   // xorshift
            const float n = (static_cast<float> (state & 0xffffu) / 65535.0f - 0.5f) * 2.0f * amount;
            const int d = juce::roundToInt (n);
            const int a = px[3];
            for (int ch = 0; ch < 3; ++ch)   // premultiplied: keep each channel <= alpha
                px[ch] = static_cast<std::uint8_t> (juce::jlimit (0, a, static_cast<int> (px[ch]) + d));
        }
    }
}

/** Draw something soft: `fn` paints into a low-resolution buffer over `area`, which is blurred and
    scaled up into `g`. For light shafts, glows, fog and stains (only inside cached layers). */
template <typename Fn>
void soft (Graphics& g, Rectangle<float> area, float downscale, int blurRadius, Fn&& fn)
{
    const int w = juce::jmax (2, juce::roundToInt (area.getWidth() / downscale));
    const int h = juce::jmax (2, juce::roundToInt (area.getHeight() / downscale));
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    {
        Graphics sg (img);
        sg.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (1.0f / downscale));
        fn (sg);
    }
    boxBlur (img, blurRadius);
    g.saveState();
    g.setOpacity (1.0f);   // images are drawn with the current fill's opacity
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageTransformed (img, juce::AffineTransform::scale (area.getWidth() / static_cast<float> (w), area.getHeight() / static_cast<float> (h))
                                     .translated (area.getX(), area.getY()));
    g.restoreState();
}

/** While true (during page transitions), cached layers keep their image instead of re-rendering
    for the zoom's changing scale. Message thread only. */
inline bool& freezeCaches()
{
    static bool frozen = false;
    return frozen;
}

/** A static background rendered once at the real pixel density, then blitted. `painter` gets a
    Graphics in design units plus the target image and its scale (for frost()). */
class CachedLayer
{
public:
    float grain = 6.0f;   // film grain amount baked into the image

    template <typename Fn>
    void draw (Graphics& g, int w, int h, Fn&& painter)
    {
        float s = g.getInternalContext().getPhysicalPixelScaleFactor();
        s = juce::jlimit (1.0f, 3.0f, std::ceil (s * 4.0f) / 4.0f);
        if (image.isNull() || (! freezeCaches() && std::abs (s - scale) > 0.01f))
        {
            scale = s;
            juce::Image work (juce::Image::ARGB, juce::roundToInt (static_cast<float> (w) * s), juce::roundToInt (static_cast<float> (h) * s), true,
                              juce::SoftwareImageType());
            {
                Graphics ig (work);
                ig.addTransform (juce::AffineTransform::scale (s));
                painter (ig, work, s);
            }
            addGrain (work, grain);
            image = juce::NativeImageType().convert (work);
        }
        g.setOpacity (1.0f);
        g.drawImageTransformed (image, juce::AffineTransform::scale (static_cast<float> (w) / static_cast<float> (image.getWidth()),
                                                                     static_cast<float> (h) / static_cast<float> (image.getHeight())));
    }

    void invalidate() { image = {}; }

private:
    juce::Image image;
    float scale = 0.0f;
};

// ------------------------------------------------------------------------------------ surfaces
/** Frosted glass: blurs what is already painted under `r` (inside a CachedLayer painter). */
inline void frost (Graphics& g, juce::Image& target, float scale, Rectangle<float> r, float radius, int blurDesignPx = 10)
{
    const auto pr = (r * scale).getSmallestIntegerContainer().getIntersection (target.getBounds());
    if (pr.isEmpty()) return;
    auto sub = target.getClippedImage (pr).createCopy();
    boxBlur (sub, juce::jmax (1, juce::roundToInt (static_cast<float> (blurDesignPx) * scale / 2.0f)));
    g.saveState();
    Path clip;
    clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);
    g.setOpacity (1.0f);
    g.drawImageTransformed (sub, juce::AffineTransform::scale (1.0f / scale).translated (static_cast<float> (pr.getX()) / scale,
                                                                                        static_cast<float> (pr.getY()) / scale));
    g.restoreState();
}

/** A soft shadow under a rounded rectangle (blurred; inside cached layers). */
inline void softShadow (Graphics& g, Rectangle<float> r, float radius, float strength = 0.7f, float spread = 18.0f, float dy = 10.0f)
{
    soft (g, r.expanded (spread * 2.0f).translated (0.0f, dy), 2.0f, juce::roundToInt (spread / 4.0f), [&] (Graphics& sg) {
        sg.setColour (juce::Colours::black.withAlpha (strength));
        sg.fillRoundedRectangle (r.translated (0.0f, dy), radius);
    });
}

/** Dark glass: tinted body, a sheen on the top half, a hairline edge lit from above. */
inline void glass (Graphics& g, Rectangle<float> r, float radius, float opacity = 0.62f, Colour tint = colours::slate)
{
    g.setGradientFill (juce::ColourGradient (tint.withAlpha (opacity * 0.85f), 0.0f, r.getY(), colours::ink.withAlpha (juce::jmin (1.0f, opacity + 0.12f)), 0.0f,
                                             r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.045f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f,
                                             r.getY() + juce::jmin (60.0f, r.getHeight() * 0.5f), false));
    g.fillRoundedRectangle (r, radius);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.17f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.035f), 0.0f,
                                             r.getY() + juce::jmin (90.0f, r.getHeight()), false));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

/** Vignette: the edges of the frame sink into darkness. */
inline void vignette (Graphics& g, Rectangle<float> r, float strength = 0.85f)
{
    juce::ColourGradient v (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY() - r.getHeight() * 0.08f, colours::ink.withAlpha (strength),
                            r.getX() - r.getWidth() * 0.02f, r.getY() - r.getHeight() * 0.1f, true);
    v.addColour (0.55, juce::Colours::transparentBlack);
    v.addColour (0.8, colours::ink.withAlpha (strength * 0.45f));
    g.setGradientFill (v);
    g.fillRect (r);
}

// ------------------------------------------------------------------------------------ icons
/** Small line icons for buttons (drawn in `r`, stroke in `c`). */
inline void icon (Graphics& g, const juce::String& name, Rectangle<float> r, Colour c)
{
    const auto ctr = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const float lw = juce::jmax (1.2f, s * 0.11f);
    const juce::PathStrokeType stroke (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (c);
    Path p;
    if (name == "power")
    {
        p.addCentredArc (ctr.x, ctr.y + s * 0.05f, s * 0.38f, s * 0.38f, 0.0f, 0.75f, 2.0f * pi - 0.75f, true);
        p.startNewSubPath (ctr.x, ctr.y - s * 0.5f);
        p.lineTo (ctr.x, ctr.y - s * 0.02f);
        g.strokePath (p, stroke);
    }
    else if (name == "back")
    {
        p.startNewSubPath (ctr.x + s * 0.4f, ctr.y);
        p.lineTo (ctr.x - s * 0.4f, ctr.y);
        p.startNewSubPath (ctr.x - s * 0.08f, ctr.y - s * 0.32f);
        p.lineTo (ctr.x - s * 0.4f, ctr.y);
        p.lineTo (ctr.x - s * 0.08f, ctr.y + s * 0.32f);
        g.strokePath (p, stroke);
    }
    else if (name == "plus")
    {
        p.startNewSubPath (ctr.x - s * 0.36f, ctr.y);
        p.lineTo (ctr.x + s * 0.36f, ctr.y);
        p.startNewSubPath (ctr.x, ctr.y - s * 0.36f);
        p.lineTo (ctr.x, ctr.y + s * 0.36f);
        g.strokePath (p, stroke);
    }
    else if (name == "listen")   // a pulse: the record dot inside a ring
    {
        g.fillEllipse (Rectangle<float> (s * 0.34f, s * 0.34f).withCentre (ctr));
        g.drawEllipse (Rectangle<float> (s * 0.8f, s * 0.8f).withCentre (ctr), lw * 0.8f);
    }
    else if (name == "stage")   // a proscenium arch with its curtains
    {
        p.startNewSubPath (ctr.x - s * 0.45f, ctr.y + s * 0.42f);
        p.lineTo (ctr.x - s * 0.45f, ctr.y - s * 0.2f);
        p.quadraticTo (ctr.x, ctr.y - s * 0.62f, ctr.x + s * 0.45f, ctr.y - s * 0.2f);
        p.lineTo (ctr.x + s * 0.45f, ctr.y + s * 0.42f);
        p.startNewSubPath (ctr.x - s * 0.45f, ctr.y - s * 0.18f);
        p.quadraticTo (ctr.x - s * 0.18f, ctr.y, ctr.x - s * 0.22f, ctr.y + s * 0.42f);
        p.startNewSubPath (ctr.x + s * 0.45f, ctr.y - s * 0.18f);
        p.quadraticTo (ctr.x + s * 0.18f, ctr.y, ctr.x + s * 0.22f, ctr.y + s * 0.42f);
        g.strokePath (p, stroke);
    }
    else if (name == "scissors")
    {
        g.drawEllipse (Rectangle<float> (s * 0.3f, s * 0.3f).withCentre (ctr.translated (-s * 0.26f, s * 0.28f)), lw);
        g.drawEllipse (Rectangle<float> (s * 0.3f, s * 0.3f).withCentre (ctr.translated (s * 0.26f, s * 0.28f)), lw);
        p.startNewSubPath (ctr.x - s * 0.15f, ctr.y + s * 0.16f);
        p.lineTo (ctr.x + s * 0.3f, ctr.y - s * 0.45f);
        p.startNewSubPath (ctr.x + s * 0.15f, ctr.y + s * 0.16f);
        p.lineTo (ctr.x - s * 0.3f, ctr.y - s * 0.45f);
        g.strokePath (p, stroke);
    }
    else if (name == "link")
    {
        for (int side = -1; side <= 1; side += 2)
        {
            Path ring;
            ring.addRoundedRectangle (-s * 0.3f, -s * 0.16f, s * 0.6f, s * 0.32f, s * 0.16f);
            g.strokePath (ring, stroke, juce::AffineTransform::rotation (-0.6f).translated (ctr.x + static_cast<float> (side) * s * 0.17f,
                                                                                           ctr.y - static_cast<float> (side) * s * 0.12f));
        }
    }
    else if (name == "forget")   // a circular arrow
    {
        p.addCentredArc (ctr.x, ctr.y, s * 0.36f, s * 0.36f, 0.0f, 0.5f, 2.0f * pi - 0.2f, true);
        g.strokePath (p, stroke);
        const auto tip = polar (ctr, 0.5f, s * 0.36f);
        Path head;
        head.addTriangle (tip.x - s * 0.02f, tip.y - s * 0.2f, tip.x + s * 0.2f, tip.y + s * 0.02f, tip.x - s * 0.16f, tip.y + s * 0.1f);
        g.fillPath (head);
    }
    else if (name == "about")
    {
        g.setFont (whisper (s * 1.35f));
        g.drawText ("?", r.expanded (s), juce::Justification::centred);
    }
}

/** Genre icons for The Puppet Show's tickets: 0 Rock, 1 Pop, 2 EDM, 3 Metal, 4 Hip-hop. */
inline void genreIcon (Graphics& g, int genre, Rectangle<float> r, Colour c)
{
    const auto ctr = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const juce::PathStrokeType stroke (juce::jmax (1.3f, s * 0.07f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (c);
    Path p;
    switch (genre)
    {
        case 0:   // lightning
            p.startNewSubPath (ctr.x + s * 0.12f, ctr.y - s * 0.5f);
            p.lineTo (ctr.x - s * 0.22f, ctr.y + s * 0.05f);
            p.lineTo (ctr.x + s * 0.02f, ctr.y + s * 0.05f);
            p.lineTo (ctr.x - s * 0.12f, ctr.y + s * 0.5f);
            p.lineTo (ctr.x + s * 0.24f, ctr.y - s * 0.08f);
            p.lineTo (ctr.x + s * 0.0f, ctr.y - s * 0.08f);
            p.closeSubPath();
            g.strokePath (p, stroke);
            break;
        case 1:   // a four-point sparkle
            p.startNewSubPath (ctr.x, ctr.y - s * 0.5f);
            p.quadraticTo (ctr.x + s * 0.06f, ctr.y - s * 0.06f, ctr.x + s * 0.5f, ctr.y);
            p.quadraticTo (ctr.x + s * 0.06f, ctr.y + s * 0.06f, ctr.x, ctr.y + s * 0.5f);
            p.quadraticTo (ctr.x - s * 0.06f, ctr.y + s * 0.06f, ctr.x - s * 0.5f, ctr.y);
            p.quadraticTo (ctr.x - s * 0.06f, ctr.y - s * 0.06f, ctr.x, ctr.y - s * 0.5f);
            g.strokePath (p, stroke);
            break;
        case 2:   // a pulse
            p.startNewSubPath (ctr.x - s * 0.5f, ctr.y);
            p.lineTo (ctr.x - s * 0.24f, ctr.y);
            p.lineTo (ctr.x - s * 0.12f, ctr.y - s * 0.4f);
            p.lineTo (ctr.x + s * 0.04f, ctr.y + s * 0.4f);
            p.lineTo (ctr.x + s * 0.16f, ctr.y - s * 0.16f);
            p.lineTo (ctr.x + s * 0.24f, ctr.y);
            p.lineTo (ctr.x + s * 0.5f, ctr.y);
            g.strokePath (p, stroke);
            break;
        case 3:   // a jagged crown of spikes
            p.startNewSubPath (ctr.x - s * 0.5f, ctr.y + s * 0.34f);
            p.lineTo (ctr.x - s * 0.42f, ctr.y - s * 0.4f);
            p.lineTo (ctr.x - s * 0.2f, ctr.y);
            p.lineTo (ctr.x, ctr.y - s * 0.5f);
            p.lineTo (ctr.x + s * 0.2f, ctr.y);
            p.lineTo (ctr.x + s * 0.42f, ctr.y - s * 0.4f);
            p.lineTo (ctr.x + s * 0.5f, ctr.y + s * 0.34f);
            p.closeSubPath();
            g.strokePath (p, stroke);
            break;
        case 4:   // a record
        default:
            g.drawEllipse (Rectangle<float> (s * 0.96f, s * 0.96f).withCentre (ctr), stroke.getStrokeThickness());
            g.drawEllipse (Rectangle<float> (s * 0.6f, s * 0.6f).withCentre (ctr), stroke.getStrokeThickness() * 0.5f);
            g.fillEllipse (Rectangle<float> (s * 0.18f, s * 0.18f).withCentre (ctr));
            break;
    }
}
} // namespace ui::fx
