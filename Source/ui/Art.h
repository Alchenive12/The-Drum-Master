// File: Source/ui/Art.h
// All artwork of The Drum Master, drawn as vectors (docs/04-ui-concept.md "Art"). Each piece is one
// function, so a hand-made or generated image can replace it later without touching the logic.
// Original design: a porcelain mask with a stitched smile and a glowing crack, long bony hands, an
// ebony control bar, a late-night room lit only by the moon. Mood of the FNaF puppet, none of its
// design. Functions marked (static) are meant for cached layers: they blur and are not cheap.
#pragma once

#include "Fx.h"

#include <array>
#include <cmath>
#include <vector>

namespace ui::art
{
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::Point;
using juce::Rectangle;
namespace c = colours;
using fx::pi;

/** A fixed pseudo-random number 0..1 for index i (art must look the same on every repaint). */
inline float hash01 (int i)
{
    auto x = static_cast<std::uint32_t> (i) * 2654435761u ^ 0x9e3779b9u;
    x ^= x >> 15; x *= 0x85ebca6bu; x ^= x >> 13; x *= 0xc2b2ae35u; x ^= x >> 16;
    return static_cast<float> (x & 0xffffffu) / static_cast<float> (0xffffffu);
}

// ------------------------------------------------------------------------------------ the room
/** Where the room's window is (the light shaft and the dust need it too). */
inline Rectangle<float> windowRect (Rectangle<float> r)
{
    return { r.getX() + r.getWidth() * 0.085f, r.getY() + r.getHeight() * 0.1f, r.getWidth() * 0.13f, r.getHeight() * 0.38f };
}

inline Path windowShape (Rectangle<float> win)
{
    const float rad = win.getWidth() * 0.5f;
    Path p;
    p.startNewSubPath (win.getX(), win.getBottom());
    p.lineTo (win.getX(), win.getY() + rad);
    p.addCentredArc (win.getCentreX(), win.getY() + rad, rad, rad, 0.0f, -pi * 0.5f, pi * 0.5f);
    p.lineTo (win.getRight(), win.getBottom());
    p.closeSubPath();
    return p;
}

/** Dead branches, grown from `from` (angle 0 = up). */
inline void branches (Graphics& g, Point<float> from, float angle, float len, float width, int depth, int& seed)
{
    const auto to = fx::polar (from, angle, len);
    Path p;
    p.startNewSubPath (from);
    p.quadraticTo (fx::polar (from, angle + (hash01 (seed++) - 0.5f) * 0.6f, len * 0.5f), to);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (depth <= 0) return;
    const int kids = 2 + (hash01 (seed++) > 0.55f ? 1 : 0);
    for (int k = 0; k < kids; ++k)
    {
        const float turn = (hash01 (seed++) - 0.5f) * 1.5f;
        const float shrink = 0.55f + 0.25f * hash01 (seed++);
        branches (g, to, angle + turn, len * shrink, juce::jmax (0.6f, width * 0.6f), depth - 1, seed);
    }
}

/** The shafts of moonlight from the window (static). */
inline void moonShaft (Graphics& g, Rectangle<float> r, float strength)
{
    const auto win = windowRect (r);
    fx::soft (g, r, 3.0f, 5, [&] (Graphics& sg) {
        for (int pane = 0; pane < 2; ++pane)   // two rays, with the mullion's shadow between them
        {
            const float x0 = pane == 0 ? win.getX() + 4.0f : win.getCentreX() + 4.0f;
            const float x1 = pane == 0 ? win.getCentreX() - 4.0f : win.getRight() - 4.0f;
            const float spread = r.getWidth() * 0.27f, drift = r.getWidth() * 0.06f;
            Path shaft;
            shaft.startNewSubPath (x0, win.getY() + win.getWidth() * 0.3f);
            shaft.lineTo (x1, win.getY() + win.getWidth() * 0.3f);
            shaft.lineTo (x1 + drift + spread * (x1 - win.getX()) / win.getWidth(), r.getBottom());
            shaft.lineTo (x0 + drift + spread * (x0 - win.getX()) / win.getWidth(), r.getBottom());
            shaft.closeSubPath();
            sg.setGradientFill (juce::ColourGradient (c::moon.withAlpha (0.36f * strength), 0.0f, win.getY(), c::moon.withAlpha (0.0f), 0.0f,
                                                      r.getBottom() + 40.0f, false));
            sg.fillPath (shaft);
        }
        // where it lands: a pool of moonlight on the boards, crossed by the mullions' shadow
        const float horizon = r.getY() + r.getHeight() * 0.6f;
        Path pool;
        const float px = win.getX() + r.getWidth() * 0.1f, pw = win.getWidth() * 1.7f;
        pool.startNewSubPath (px, horizon + 26.0f);
        pool.lineTo (px + pw * 0.8f, horizon + 26.0f);
        pool.lineTo (px + pw * 1.25f, r.getBottom());
        pool.lineTo (px + pw * 0.1f, r.getBottom());
        pool.closeSubPath();
        sg.setGradientFill (juce::ColourGradient (c::moon.withAlpha (0.2f * strength), 0.0f, horizon + 26.0f, c::moon.withAlpha (0.05f * strength), 0.0f,
                                                  r.getBottom(), false));
        sg.fillPath (pool);
        sg.setColour (c::ink.withAlpha (0.35f));
        Path cross;
        cross.startNewSubPath (px + pw * 0.42f, horizon + 26.0f);
        cross.lineTo (px + pw * 0.7f, r.getBottom());
        sg.strokePath (cross, juce::PathStrokeType (10.0f));
        sg.fillRect (Rectangle<float> (px, horizon + 95.0f, pw * 1.3f, 9.0f));
    });
}

/** Late at night in a bare room (static): faded wallpaper with damp stains, an arched window with a
    clouded moon behind dead branches, cold light across the floorboards, fog at the skirting, an
    empty portrait, cobwebs. */
inline void room (Graphics& g, Rectangle<float> r, float moonStrength = 1.0f)
{
    const float W = r.getWidth(), H = r.getHeight();
    const float horizon = r.getY() + H * 0.6f;
    const auto vp = Point<float> (r.getCentreX(), r.getY() + H * 0.42f);
    const auto wallArea = r.withBottom (horizon);

    // back wall
    g.setGradientFill (juce::ColourGradient (c::wall.brighter (0.12f), r.getX() + W * 0.3f, r.getY() + H * 0.36f, c::wallDark, r.getRight() + W * 0.2f, r.getY(), true));
    g.fillRect (wallArea);
    // wallpaper: paired pinstripes and a faint diamond damask
    for (int i = 0; i < 56; ++i)
    {
        const float x = r.getX() + W * static_cast<float> (i) / 56.0f;
        g.setColour (juce::Colours::white.withAlpha (0.022f));
        g.fillRect (Rectangle<float> (x, r.getY(), 1.0f, horizon - r.getY()));
        g.fillRect (Rectangle<float> (x + 4.0f, r.getY(), 1.0f, horizon - r.getY()));
    }
    for (int row = 0; row < 9; ++row)
        for (int col = 0; col < 20; ++col)
        {
            const float x = r.getX() + (static_cast<float> (col) + (row % 2 == 0 ? 0.5f : 0.0f)) * W / 19.6f;
            const float y = r.getY() + 30.0f + static_cast<float> (row) * 44.0f;
            Path d;
            d.addStar ({ x, y }, 4, 2.0f, 7.0f);
            g.setColour (c::moon.withAlpha (0.018f));
            g.fillPath (d);
        }
    // damp: stains, a tide line, water running down from the ceiling
    fx::soft (g, wallArea, 4.0f, 5, [&] (Graphics& sg) {
        for (int i = 0; i < 16; ++i)
        {
            const float x = r.getX() + W * hash01 (i * 3 + 1), y = r.getY() + (horizon - r.getY()) * (0.15f + 0.8f * hash01 (i * 3 + 2));
            const float s = 50.0f + 90.0f * hash01 (i * 3 + 3);
            sg.setColour (c::wallDark.withAlpha (0.35f));
            sg.fillEllipse (Rectangle<float> (s, s * 0.6f).withCentre ({ x, y }));
        }
        for (int i = 0; i < 7; ++i)
        {
            const float x = r.getX() + W * (0.3f + 0.65f * hash01 (100 + i));
            const float len = (horizon - r.getY()) * (0.2f + 0.5f * hash01 (200 + i));
            sg.setGradientFill (juce::ColourGradient (c::wallDark.withAlpha (0.55f), 0.0f, r.getY(), c::wallDark.withAlpha (0.0f), 0.0f, r.getY() + len, false));
            sg.fillRoundedRectangle (Rectangle<float> (x, r.getY(), 10.0f + 12.0f * hash01 (300 + i), len), 5.0f);
        }
        sg.setGradientFill (juce::ColourGradient (c::wallDark.withAlpha (0.0f), 0.0f, horizon - 110.0f, c::wallDark.withAlpha (0.5f), 0.0f, horizon, false));
        sg.fillRect (r.withTop (horizon - 110.0f).withBottom (horizon));
    });
    // the ceiling sinks into the dark
    g.setGradientFill (juce::ColourGradient (c::ink.withAlpha (0.92f), 0.0f, r.getY(), c::ink.withAlpha (0.0f), 0.0f, r.getY() + H * 0.3f, false));
    g.fillRect (wallArea);

    // the window: night sky, a clouded moon, dead branches
    const auto win = windowRect (r);
    const auto shape = windowShape (win);
    {
        g.saveState();
        g.reduceClipRegion (shape);
        g.setGradientFill (juce::ColourGradient (c::moonDeep.withMultipliedBrightness (0.9f), win.getX(), win.getY(), c::night, win.getRight(), win.getBottom(), false));
        g.fillRect (win);
        const auto moonPos = Point<float> (win.getX() + win.getWidth() * 0.64f, win.getY() + win.getWidth() * 0.46f);
        fx::glowDot (g, moonPos, win.getWidth() * 1.1f, c::moon.withAlpha (0.55f * moonStrength));
        g.setColour (c::moon.brighter (0.3f).withAlpha (0.95f * moonStrength));
        g.fillEllipse (Rectangle<float> (win.getWidth() * 0.26f, win.getWidth() * 0.26f).withCentre (moonPos));
        fx::soft (g, win, 2.0f, 3, [&] (Graphics& sg) {   // clouds drifting across it
            sg.setColour (c::night.withAlpha (0.8f));
            sg.fillEllipse (Rectangle<float> (win.getWidth() * 0.9f, 12.0f).withCentre (moonPos.translated (-6.0f, 8.0f)));
            sg.fillEllipse (Rectangle<float> (win.getWidth() * 0.7f, 9.0f).withCentre (moonPos.translated (20.0f, -14.0f)));
            sg.setColour (c::moon.withAlpha (0.2f));
            sg.fillEllipse (Rectangle<float> (win.getWidth() * 0.6f, 6.0f).withCentre (moonPos.translated (-10.0f, 3.0f)));
        });
        g.setColour (c::ink.withAlpha (0.92f));
        int seed = 11;
        branches (g, { win.getRight() + 6.0f, win.getBottom() + 12.0f }, -0.62f, win.getHeight() * 0.36f, 6.0f, 5, seed);
        seed = 57;
        branches (g, { win.getX() - 4.0f, win.getY() + win.getHeight() * 0.3f }, 1.1f, win.getWidth() * 0.4f, 3.0f, 4, seed);
        g.restoreState();
    }
    // frame, mullions, a cracked pane, the sill
    g.setColour (c::ink);
    g.strokePath (shape, juce::PathStrokeType (7.0f));
    g.fillRect (Rectangle<float> (win.getCentreX() - 2.0f, win.getY(), 4.0f, win.getHeight()));
    g.fillRect (Rectangle<float> (win.getX(), win.getY() + win.getWidth() * 0.5f - 2.0f, win.getWidth(), 4.0f));
    g.fillRect (Rectangle<float> (win.getX(), win.getY() + win.getHeight() * 0.68f - 2.0f, win.getWidth(), 4.0f));
    g.setColour (c::moon.withAlpha (0.18f));
    g.strokePath (shape, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (4.0f, 0.0f));
    {
        const auto hit = Point<float> (win.getX() + win.getWidth() * 0.26f, win.getY() + win.getHeight() * 0.8f);
        g.setColour (c::moon.withAlpha (0.4f));
        for (int i = 0; i < 6; ++i)
        {
            const float a = static_cast<float> (i) * 1.05f + 0.3f, len = 8.0f + 16.0f * hash01 (400 + i);
            g.drawLine ({ hit, fx::polar (hit, a, len) }, 0.7f);
        }
    }
    g.setGradientFill (juce::ColourGradient (c::oldShell.brighter (0.2f), 0.0f, win.getBottom(), c::ink, 0.0f, win.getBottom() + 9.0f, false));
    g.fillRect (Rectangle<float> (win.getX() - 10.0f, win.getBottom(), win.getWidth() + 20.0f, 9.0f));

    // an empty oval portrait, slightly crooked, something faint inside it
    {
        const auto pc = Point<float> (r.getX() + W * 0.905f, r.getY() + H * 0.215f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (0.07f, pc.x, pc.y));
        const auto oval = Rectangle<float> (74.0f, 96.0f).withCentre (pc);
        g.setColour (c::ink.withAlpha (0.6f));
        g.fillEllipse (oval.translated (6.0f, 8.0f).expanded (6.0f));
        g.setGradientFill (juce::ColourGradient (c::brassDim.withMultipliedBrightness (0.8f), oval.getX(), oval.getY(), c::ink, oval.getRight(), oval.getBottom(), false));
        g.fillEllipse (oval.expanded (7.0f));
        g.setGradientFill (juce::ColourGradient (c::slate, pc.x, pc.y - 20.0f, c::ink, pc.x, oval.getBottom(), true));
        g.fillEllipse (oval);
        g.setColour (c::slateLight.withAlpha (0.55f));   // head and shoulders, no face
        g.fillEllipse (Rectangle<float> (26.0f, 32.0f).withCentre (pc.translated (0.0f, -10.0f)));
        Path shoulders;
        shoulders.addCentredArc (pc.x, oval.getBottom() + 4.0f, 30.0f, 30.0f, 0.0f, -1.2f, 1.2f, true);
        g.fillPath (shoulders);
        g.setColour (c::bone.withAlpha (0.28f));   // ...but it is looking
        g.fillEllipse (Rectangle<float> (2.2f, 1.6f).withCentre (pc.translated (-5.0f, -12.0f)));
        g.fillEllipse (Rectangle<float> (2.2f, 1.6f).withCentre (pc.translated (5.0f, -12.0f)));
        g.setColour (c::brass.withAlpha (0.25f));
        g.drawEllipse (oval.expanded (7.0f), 1.0f);
        g.restoreState();
    }

    // floor: boards to the vanishing point, staggered seams, a cold sheen
    const auto floorArea = r.withTop (horizon);
    g.setGradientFill (juce::ColourGradient (c::floorBoard.darker (0.7f), 0.0f, horizon, c::floorBoard.brighter (0.12f), 0.0f, r.getBottom(), false));
    g.fillRect (floorArea);
    for (int i = -16; i <= 16; ++i)
    {
        const float bx = vp.x + static_cast<float> (i) * W * 0.085f;
        const float t = (horizon - vp.y) / (r.getBottom() - vp.y);
        const auto top = Point<float> (vp.x + (bx - vp.x) * t, horizon);
        g.setColour (c::ink.withAlpha (0.7f));
        g.drawLine ({ top, { bx, r.getBottom() } }, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.018f));
        g.drawLine ({ top.translated (1.5f, 0.0f), { bx + 2.0f, r.getBottom() } }, 1.0f);
        for (int s = 0; s < 3; ++s)   // board ends
        {
            const float f = 0.15f + 0.8f * hash01 (i * 7 + s + 900);
            const float y = horizon + (r.getBottom() - horizon) * f * f;
            const float xa = top.x + (bx - top.x) * (y - horizon) / (r.getBottom() - horizon);
            const float xb = xa + W * 0.085f * (0.2f + 0.8f * (y - horizon) / (r.getBottom() - horizon));
            g.setColour (c::ink.withAlpha (0.45f));
            g.drawLine (xa, y, xb, y, 1.0f);
        }
    }
    fx::soft (g, floorArea, 4.0f, 6, [&] (Graphics& sg) {
        sg.setColour (c::moon.withAlpha (0.06f));
        sg.fillEllipse (Rectangle<float> (W * 0.62f, H * 0.14f).withCentre ({ r.getCentreX(), horizon + H * 0.1f }));
    });
    // skirting board
    g.setColour (c::ink);
    g.fillRect (Rectangle<float> (r.getX(), horizon - 12.0f, W, 12.0f));
    g.setColour (c::moon.withAlpha (0.09f));
    g.fillRect (Rectangle<float> (r.getX(), horizon - 12.0f, W, 1.0f));

    moonShaft (g, r, moonStrength);

    // fog lying along the skirting
    fx::soft (g, r.withTop (horizon - 90.0f).withBottom (horizon + 70.0f), 4.0f, 6, [&] (Graphics& sg) {
        for (int i = 0; i < 12; ++i)
        {
            const float x = r.getX() + W * hash01 (500 + i);
            sg.setColour (c::moon.withAlpha (0.05f + 0.05f * hash01 (600 + i)));
            sg.fillEllipse (Rectangle<float> (180.0f + 200.0f * hash01 (700 + i), 34.0f).withCentre ({ x, horizon - 4.0f + 30.0f * hash01 (800 + i) }));
        }
    });

    // cobwebs in the top-right corner
    {
        const auto corner = Point<float> (r.getRight(), r.getY());
        g.setColour (c::moon.withAlpha (0.07f));
        std::array<Point<float>, 7> spokes {};
        for (std::size_t i = 0; i < spokes.size(); ++i)
        {
            const float a = pi + 0.12f + static_cast<float> (i) * 0.22f;
            spokes[i] = fx::polar (corner, a, 150.0f + 40.0f * hash01 (static_cast<int> (i) + 40));
            g.drawLine ({ corner, spokes[i] }, 0.8f);
        }
        for (int ring = 1; ring <= 6; ++ring)
        {
            const float f = static_cast<float> (ring) / 7.0f;
            Path web;
            for (std::size_t i = 0; i < spokes.size(); ++i)
            {
                const auto p = corner + (spokes[i] - corner) * f;
                if (i == 0) web.startNewSubPath (p);
                else
                {
                    const auto prev = corner + (spokes[i - 1] - corner) * f;
                    web.quadraticTo ((prev + p) * 0.5f + (corner - (prev + p) * 0.5f) * 0.08f, p);   // sagging threads
                }
            }
            g.strokePath (web, juce::PathStrokeType (0.7f));
        }
    }
}

/** An old dusty drum set in the dark back corner, rim-lit by the moon (static). */
inline void oldDrumSet (Graphics& g, Point<float> base, float scale)
{
    const auto body = c::oldShell.withMultipliedBrightness (0.8f);
    const auto rim = c::moon.withAlpha (0.35f);
    auto stand = [&] (float x, float topY) {
        g.setColour (c::oldMetal.withAlpha (0.45f));
        g.drawLine (x, base.y, x, topY, 1.4f * scale);
        g.drawLine (x, base.y, x - 12.0f * scale, base.y + 4.0f * scale, 1.2f * scale);
        g.drawLine (x, base.y, x + 12.0f * scale, base.y + 4.0f * scale, 1.2f * scale);
    };
    auto disc = [&] (Point<float> cc, float w, float tilt) {
        const auto d = Rectangle<float> (w, w * tilt).withCentre (cc);
        g.setGradientFill (juce::ColourGradient (Colour (0xff6e6450), d.getX(), d.getY(), c::ink, d.getRight(), d.getBottom(), false));
        g.fillEllipse (d);
        Path lit;
        lit.addCentredArc (cc.x, cc.y, w * 0.5f, w * tilt * 0.5f, 0.0f, -2.4f, -0.4f, true);
        g.setColour (rim);
        g.strokePath (lit, juce::PathStrokeType (1.0f));
    };
    fx::soft (g, Rectangle<float> (base.x - 200.0f * scale, base.y - 30.0f * scale, 400.0f * scale, 60.0f * scale), 2.0f, 4, [&] (Graphics& sg) {
        sg.setColour (juce::Colours::black.withAlpha (0.7f));
        sg.fillEllipse (Rectangle<float> (320.0f * scale, 22.0f * scale).withCentre (base.translated (0.0f, 2.0f)));
    });
    stand (base.x - 70.0f * scale, base.y - 150.0f * scale);
    disc ({ base.x - 70.0f * scale, base.y - 150.0f * scale }, 78.0f * scale, 0.26f);
    stand (base.x + 95.0f * scale, base.y - 128.0f * scale);
    disc ({ base.x + 95.0f * scale, base.y - 128.0f * scale }, 64.0f * scale, 0.3f);
    stand (base.x - 115.0f * scale, base.y - 88.0f * scale);
    disc ({ base.x - 115.0f * scale, base.y - 92.0f * scale }, 44.0f * scale, 0.22f);
    disc ({ base.x - 115.0f * scale, base.y - 86.0f * scale }, 44.0f * scale, 0.22f);
    // floor tom under a dust sheet
    Path sheet;
    const float fx0 = base.x + 70.0f * scale;
    sheet.startNewSubPath (fx0 - 32.0f * scale, base.y);
    sheet.quadraticTo (fx0 - 36.0f * scale, base.y - 50.0f * scale, fx0 - 10.0f * scale, base.y - 64.0f * scale);
    sheet.quadraticTo (fx0 + 14.0f * scale, base.y - 72.0f * scale, fx0 + 34.0f * scale, base.y - 52.0f * scale);
    sheet.quadraticTo (fx0 + 40.0f * scale, base.y - 20.0f * scale, fx0 + 38.0f * scale, base.y);
    sheet.quadraticTo (fx0, base.y - 6.0f * scale, fx0 - 32.0f * scale, base.y);
    sheet.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Colour (0xff595f69), fx0 - 30.0f * scale, base.y - 60.0f * scale, c::ink, fx0 + 40.0f * scale, base.y, false));
    g.fillPath (sheet);
    g.setColour (c::ink.withAlpha (0.4f));
    for (int i = 0; i < 3; ++i)   // folds
        g.drawLine (fx0 - 20.0f * scale + static_cast<float> (i) * 18.0f * scale, base.y - 58.0f * scale, fx0 - 24.0f * scale + static_cast<float> (i) * 22.0f * scale,
                    base.y - 2.0f, 1.0f);
    // the bass drum: a split head, a faded stitched-smile logo
    const auto kick = Rectangle<float> (92.0f * scale, 92.0f * scale).withCentre ({ base.x, base.y - 46.0f * scale });
    g.setColour (body);
    g.fillEllipse (kick);
    g.setGradientFill (juce::ColourGradient (Colour (0xff6f6a60), kick.getX(), kick.getY(), c::oldShell, kick.getRight(), kick.getBottom(), false));
    g.fillEllipse (kick.reduced (8.0f * scale));
    Path lit;
    lit.addCentredArc (kick.getCentreX(), kick.getCentreY(), kick.getWidth() * 0.5f, kick.getHeight() * 0.5f, 0.0f, -2.6f, -0.5f, true);
    g.setColour (rim);
    g.strokePath (lit, juce::PathStrokeType (1.2f));
    Path smile;
    smile.startNewSubPath (kick.getCentreX() - 22.0f * scale, kick.getCentreY() - 2.0f * scale);
    smile.quadraticTo (kick.getCentreX(), kick.getCentreY() + 12.0f * scale, kick.getCentreX() + 22.0f * scale, kick.getCentreY() - 2.0f * scale);
    g.setColour (c::bloodDeep.brighter (0.3f).withAlpha (0.7f));
    g.strokePath (smile, juce::PathStrokeType (1.4f));
    g.setColour (c::ink.withAlpha (0.7f));
    g.drawLine (kick.getX() + 18.0f * scale, kick.getY() + 20.0f * scale, kick.getCentreX() + 6.0f * scale, kick.getCentreY() + 14.0f * scale, 1.2f);
    // rack tom and snare
    const auto tom = Rectangle<float> (46.0f * scale, 28.0f * scale).withCentre ({ base.x - 18.0f * scale, kick.getY() - 8.0f * scale });
    g.setColour (body);
    g.fillRoundedRectangle (tom, 3.0f * scale);
    g.setColour (Colour (0xff5d5a54));
    g.fillEllipse (tom.withHeight (9.0f * scale).translated (0.0f, -4.0f * scale));
    stand (base.x - 72.0f * scale, base.y - 48.0f * scale);
    const auto sn = Rectangle<float> (48.0f * scale, 16.0f * scale).withCentre ({ base.x - 72.0f * scale, base.y - 56.0f * scale });
    g.setColour (body);
    g.fillRect (sn);
    g.setColour (Colour (0xff5d5a54));
    g.fillEllipse (sn.withHeight (8.0f * scale).translated (0.0f, -4.0f * scale));
    g.setColour (rim);
    g.drawLine (sn.getX(), sn.getY(), sn.getX(), sn.getBottom(), 1.0f);
}

/** Fog drifting along the floor and dust turning in the moonlight (live, cheap). `fog` is a soft
    white blob image made once by the page (see makeFogSprite). */
inline void roomLive (Graphics& g, Rectangle<float> r, float time, float flicker, const juce::Image& fog)
{
    const float horizon = r.getY() + r.getHeight() * 0.6f;
    if (fog.isValid())
    {
        for (int i = 0; i < 4; ++i)
        {
            const float speed = 6.0f + 4.0f * hash01 (i + 70);
            const float x = std::fmod (time * speed + static_cast<float> (i) * 420.0f, r.getWidth() + 700.0f) - 600.0f;
            const float y = horizon - 40.0f + static_cast<float> (i % 2) * 70.0f;
            g.setOpacity (0.1f + 0.05f * static_cast<float> (i % 3));
            g.drawImage (fog, Rectangle<float> (r.getX() + x, y, 620.0f, 110.0f), juce::RectanglePlacement::stretchToFit);
        }
        g.setOpacity (1.0f);
    }
    // dust in the shafts
    const auto win = windowRect (r);
    for (int i = 0; i < 64; ++i)
    {
        const float fx0 = hash01 (i * 5 + 1), speed = 0.006f + 0.012f * hash01 (i * 5 + 2);
        const float fy = std::fmod (hash01 (i * 5 + 3) + time * speed, 1.0f);
        const float y = win.getY() + win.getWidth() * 0.3f + (r.getBottom() - win.getY()) * fy;
        const float spread = (y - win.getY()) / (r.getBottom() - win.getY());
        const float x = win.getX() + r.getWidth() * 0.06f * spread + fx0 * (win.getWidth() + r.getWidth() * 0.27f * spread) + 8.0f * std::sin (time * 0.4f + static_cast<float> (i));
        const float tw = 0.5f + 0.5f * std::sin (time * (0.7f + hash01 (i * 5 + 4)) + static_cast<float> (i));
        const bool bokeh = i % 9 == 0;
        const float a = (bokeh ? 0.08f : 0.35f) * tw * (1.0f - spread * 0.6f) * flicker;
        if (bokeh) fx::glowDot (g, { x, y }, 5.0f, c::moon.withAlpha (a));
        else
        {
            g.setColour (c::moon.withAlpha (a));
            g.fillEllipse (Rectangle<float> (1.5f, 1.5f).withCentre ({ x, y }));
        }
    }
    if (flicker < 0.999f)   // a cloud passes the moon
    {
        g.setColour (c::ink.withAlpha ((1.0f - flicker) * 0.4f));
        g.fillRect (r);
    }
}

/** A soft fog blob, made once per page (then drawn stretched and faint by roomLive). */
inline juce::Image makeFogSprite()
{
    juce::Image img (juce::Image::ARGB, 256, 64, true, juce::SoftwareImageType());
    {
        Graphics g (img);
        for (int i = 0; i < 9; ++i)
        {
            g.setColour (c::moon.withAlpha (0.35f + 0.3f * hash01 (i + 20)));
            g.fillEllipse (Rectangle<float> (60.0f + 60.0f * hash01 (i + 30), 14.0f + 10.0f * hash01 (i + 40))
                               .withCentre ({ 40.0f + 176.0f * hash01 (i + 50), 26.0f + 12.0f * hash01 (i + 60) }));
        }
    }
    fx::boxBlur (img, 8);
    return juce::NativeImageType().convert (img);
}

// ------------------------------------------------------------------------------------ the master
/** The master's legs: very tall and skinny, standing in the room behind the strings. Pinstriped
    trousers lit from the window side, knobbly knees, long curled shoes, long moon shadows. */
inline void legs (Graphics& g, float centreX, float floorY, float sway, float scale)
{
    for (int side = -1; side <= 1; side += 2)   // shadows first, across the floor away from the window
    {
        const float s = static_cast<float> (side);
        const auto ankle = Point<float> (centreX + s * 60.0f * scale, floorY - 16.0f * scale);
        Path sh;
        sh.startNewSubPath (ankle.x - 12.0f, floorY);
        sh.lineTo (ankle.x + 10.0f, floorY - 2.0f);
        sh.lineTo (ankle.x + 250.0f * scale, floorY + 30.0f * scale);
        sh.lineTo (ankle.x + 215.0f * scale, floorY + 44.0f * scale);
        sh.closeSubPath();
        g.setGradientFill (juce::ColourGradient (c::ink.withAlpha (0.55f), ankle.x, floorY, c::ink.withAlpha (0.0f), ankle.x + 240.0f * scale, floorY + 36.0f, false));
        g.fillPath (sh);
    }
    for (int side = -1; side <= 1; side += 2)
    {
        const float s = static_cast<float> (side);
        const float hipX = centreX + s * 30.0f * scale + sway;
        const auto knee = Point<float> (centreX + s * 92.0f * scale + sway * 0.5f, floorY * 0.5f);
        const auto ankle = Point<float> (centreX + s * 60.0f * scale, floorY - 16.0f * scale);
        const float thigh = 24.0f * scale, shin = 16.0f * scale;
        Path leg;
        leg.startNewSubPath (hipX - thigh, -10.0f);
        leg.lineTo (hipX + thigh, -10.0f);
        leg.quadraticTo (knee.x + thigh * 0.8f, (knee.y - 10.0f) * 0.6f, knee.x + thigh * 0.55f, knee.y);
        leg.quadraticTo (ankle.x + shin * 0.9f, (knee.y + ankle.y) * 0.5f, ankle.x + shin * 0.45f, ankle.y);
        leg.lineTo (ankle.x - shin * 0.45f, ankle.y);
        leg.quadraticTo (ankle.x - shin * 0.9f, (knee.y + ankle.y) * 0.5f, knee.x - thigh * 0.55f, knee.y);
        leg.quadraticTo (knee.x - thigh * 0.8f, (knee.y - 10.0f) * 0.6f, hipX - thigh, -10.0f);
        leg.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Colour (0xff161a22), knee.x - thigh, 0.0f, c::ink, knee.x + thigh, 0.0f, false));
        g.fillPath (leg);
        g.saveState();
        g.reduceClipRegion (leg);
        g.setGradientFill (juce::ColourGradient (c::moon.withAlpha (0.2f), knee.x - thigh * 1.1f, 0.0f, c::moon.withAlpha (0.0f), knee.x - thigh * 0.1f, 0.0f, false));
        g.fillRect (leg.getBounds());
        g.setColour (c::oldMetal.withAlpha (0.1f));   // pinstripes
        for (int k = -3; k <= 3; ++k)
        {
            const float o = static_cast<float> (k) * thigh * 0.28f;
            g.drawLine (hipX + o, -10.0f, knee.x + o * 0.7f, knee.y, 0.8f);
            g.drawLine (knee.x + o * 0.7f, knee.y, ankle.x + o * 0.4f, ankle.y, 0.8f);
        }
        fx::glowDot (g, knee.translated (-thigh * 0.2f, 0.0f), thigh * 0.8f, c::moon.withAlpha (0.08f));   // the knee pokes through
        g.restoreState();
        g.setColour (c::moon.withAlpha (0.1f));
        g.strokePath (leg, juce::PathStrokeType (0.8f));
        // bony ankle, then the long pointed shoe with its curled tip
        g.setColour (c::boneDim.darker (0.4f));
        g.fillRect (Rectangle<float> (shin * 0.5f, 10.0f * scale).withCentre (ankle.translated (0.0f, 6.0f * scale)));
        Path shoe;
        const float sy = floorY - 2.0f * scale;
        shoe.startNewSubPath (ankle.x - s * 12.0f * scale, sy);
        shoe.lineTo (ankle.x - s * 10.0f * scale, sy - 12.0f * scale);
        shoe.quadraticTo (ankle.x + s * 10.0f * scale, sy - 16.0f * scale, ankle.x + s * 26.0f * scale, sy - 8.0f * scale);
        shoe.quadraticTo (ankle.x + s * 52.0f * scale, sy - 6.0f * scale, ankle.x + s * 62.0f * scale, sy - 16.0f * scale);
        shoe.quadraticTo (ankle.x + s * 64.0f * scale, sy - 22.0f * scale, ankle.x + s * 58.0f * scale, sy - 20.0f * scale);   // curl
        shoe.quadraticTo (ankle.x + s * 52.0f * scale, sy + 1.0f, ankle.x + s * 20.0f * scale, sy);
        shoe.closeSubPath();
        g.setColour (Colour (0xff050608));
        g.fillPath (shoe);
        Path gloss;
        gloss.startNewSubPath (ankle.x - s * 6.0f * scale, sy - 12.0f * scale);
        gloss.quadraticTo (ankle.x + s * 14.0f * scale, sy - 14.0f * scale, ankle.x + s * 28.0f * scale, sy - 8.5f * scale);
        g.setColour (c::moon.withAlpha (0.45f));
        g.strokePath (gloss, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

/** The master's porcelain mask, cut off by the top edge: chin, cheeks, the stitched smile, and a
    crack that glows from inside (`pulse` 0..1 flares it: the kick). */
inline void mask (Graphics& g, float cx, float visibleBottom, float width, float breathe, float pulse)
{
    const float h = width * 1.25f;
    const auto face = Rectangle<float> (width, h).withCentre ({ cx, visibleBottom - h * 0.5f + breathe });
    fx::glowDot (g, { cx, face.getBottom() - h * 0.15f }, width * 0.8f, c::ink.withAlpha (0.55f));   // it darkens the room around it
    juce::ColourGradient base (c::bone, cx - width * 0.24f, face.getBottom() - h * 0.32f, Colour (0xff5c5850), cx + width * 0.62f, face.getBottom() + h * 0.08f, true);
    base.addColour (0.5, Colour (0xffd3cab8));
    g.setGradientFill (base);
    g.fillEllipse (face);
    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    Path faceP;
    faceP.addEllipse (face);
    g.reduceClipRegion (faceP);
    g.setGradientFill (juce::ColourGradient (c::moon.withAlpha (0.28f), face.getX(), 0.0f, c::moon.withAlpha (0.0f), cx - width * 0.1f, 0.0f, false));
    g.fillRect (face);
    g.setGradientFill (juce::ColourGradient (c::ink.withAlpha (0.0f), 0.0f, face.getBottom() - h * 0.16f, c::ink.withAlpha (0.45f), 0.0f, face.getBottom(), false));
    g.fillRect (face);
    g.restoreState();
    // hollow cheeks and a cold highlight
    for (int side = -1; side <= 1; side += 2)
        fx::glowDot (g, { cx + static_cast<float> (side) * width * 0.28f, face.getBottom() - h * 0.25f }, width * 0.15f, Colour (0xff4a4640).withAlpha (0.4f));
    fx::glowDot (g, { cx - width * 0.3f, face.getBottom() - h * 0.34f }, width * 0.09f, juce::Colours::white.withAlpha (0.4f));
    // the stitched smile: too wide, turned up at the ends
    Path smile;
    const float sy = face.getBottom() - h * 0.17f;
    smile.startNewSubPath (cx - width * 0.31f, sy - width * 0.065f);
    smile.quadraticTo (cx, sy + width * 0.075f, cx + width * 0.31f, sy - width * 0.065f);
    g.setColour (Colour (0xff1a0a0e));
    g.strokePath (smile, juce::PathStrokeType (juce::jmax (1.6f, width * 0.014f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (int i = -5; i <= 5; ++i)
    {
        const float t = 0.5f + static_cast<float> (i) / 12.0f;
        const auto p = smile.getPointAlongPath (smile.getLength() * t);
        const float d = width * 0.022f;
        g.drawLine (p.x - d * 0.6f, p.y - d, p.x + d * 0.6f, p.y + d, juce::jmax (1.0f, width * 0.008f));
        g.drawLine (p.x + d * 0.6f, p.y - d, p.x - d * 0.6f, p.y + d, juce::jmax (1.0f, width * 0.008f));
    }
    for (int side = -1; side <= 1; side += 2)   // where the thread pulls, it has bled a little
        fx::glowDot (g, { cx + static_cast<float> (side) * width * 0.31f, sy - width * 0.065f }, width * 0.03f, c::blood.withAlpha (0.35f));
    // the crack, lit from inside
    Path crack;
    crack.startNewSubPath (cx + width * 0.09f, face.getBottom() - 1.0f);
    crack.lineTo (cx + width * 0.05f, face.getBottom() - h * 0.07f);
    crack.lineTo (cx + width * 0.12f, face.getBottom() - h * 0.12f);
    crack.lineTo (cx + width * 0.07f, face.getBottom() - h * 0.2f);
    crack.lineTo (cx + width * 0.1f, face.getBottom() - h * 0.3f);
    crack.startNewSubPath (cx + width * 0.12f, face.getBottom() - h * 0.12f);
    crack.lineTo (cx + width * 0.2f, face.getBottom() - h * 0.15f);
    crack.lineTo (cx + width * 0.23f, face.getBottom() - h * 0.21f);
    g.setColour (c::ink.withAlpha (0.8f));
    g.strokePath (crack, juce::PathStrokeType (1.6f));
    fx::glowStroke (g, crack, c::ecto.withAlpha (0.35f + 0.65f * pulse), 0.8f, 0.5f + 1.5f * pulse);
}

/** One long bony hand on the bar; its dark sleeve reaches up to the master's shoulder above the
    window. `curl` 1 = gripping the bar, 0 = fingers hanging open. Returns the four fingertips. */
inline std::array<Point<float>, 4> hand (Graphics& g, Point<float> grip, float size, bool left, float sway, float shoulderX, float curl = 1.0f)
{
    const float dir = left ? -1.0f : 1.0f;
    const auto wrist = Point<float> (grip.x, grip.y - size * 0.75f);
    Path sleeve;
    const float topW = size * 0.95f, wristW = size * 0.3f;
    sleeve.startNewSubPath (shoulderX - topW + sway, -20.0f);
    sleeve.lineTo (shoulderX + topW + sway, -20.0f);
    sleeve.quadraticTo (shoulderX + dir * size * 0.2f + sway, wrist.y * 0.45f, wrist.x + wristW, wrist.y);
    sleeve.lineTo (wrist.x - wristW, wrist.y);
    sleeve.quadraticTo (shoulderX - dir * size * 0.4f + sway, wrist.y * 0.4f, shoulderX - topW + sway, -20.0f);
    sleeve.closeSubPath();
    g.setGradientFill (juce::ColourGradient (c::ink, shoulderX, 0.0f, Colour (0xff181c24), wrist.x, wrist.y, false));
    g.fillPath (sleeve);
    g.setColour (c::moon.withAlpha (0.13f));
    g.strokePath (sleeve, juce::PathStrokeType (1.0f));
    // a frayed, yellowed shirt cuff
    const auto cuff = Rectangle<float> (wristW * 1.7f, size * 0.06f).withCentre (wrist.translated (0.0f, size * 0.01f));
    g.setGradientFill (juce::ColourGradient (Colour (0xffb9b09c), 0.0f, cuff.getY(), Colour (0xff5b554b), 0.0f, cuff.getBottom(), false));
    g.fillRoundedRectangle (cuff, size * 0.02f);
    g.setColour (Colour (0xffb9b09c).withAlpha (0.6f));
    for (int i = 0; i < 6; ++i)
    {
        const float tx = cuff.getX() + cuff.getWidth() * (0.1f + 0.16f * static_cast<float> (i));
        g.drawLine (tx, cuff.getBottom(), tx + dir * 1.0f, cuff.getBottom() + 2.0f + 2.0f * hash01 (i + 77), 0.7f);
    }
    // the palm: narrow at the wrist, bony at the knuckles
    Path palm;
    const float top = cuff.getBottom() - 1.0f, bottom = grip.y - size * 0.05f, ph = bottom - top;
    const float wTop = size * 0.3f, wKnuckle = size * 0.44f;
    palm.startNewSubPath (grip.x - wTop * 0.5f, top);
    palm.lineTo (grip.x + wTop * 0.5f, top);
    palm.cubicTo (grip.x + wTop * 0.62f, top + ph * 0.4f, grip.x + wKnuckle * 0.56f, bottom - ph * 0.3f, grip.x + wKnuckle * 0.5f, bottom);
    palm.quadraticTo (grip.x, bottom + size * 0.035f, grip.x - wKnuckle * 0.5f, bottom);
    palm.cubicTo (grip.x - wKnuckle * 0.56f, bottom - ph * 0.3f, grip.x - wTop * 0.62f, top + ph * 0.4f, grip.x - wTop * 0.5f, top);
    palm.closeSubPath();
    g.setGradientFill (juce::ColourGradient (c::bone.darker (0.1f), grip.x - wKnuckle * 0.6f, top, Colour (0xff4d4840), grip.x + wKnuckle * 0.6f, bottom, false));
    g.fillPath (palm);
    // the bones under the skin, fanning out to the knuckles
    for (int t = 0; t < 4; ++t)
    {
        const float kx = grip.x - wKnuckle * 0.36f + wKnuckle * 0.72f * static_cast<float> (t) / 3.0f;
        g.setColour (Colour (0xff3d3932).withAlpha (size > 100.0f ? 0.3f : 0.14f));
        g.drawLine (grip.x + (kx - grip.x) * 0.3f, top + ph * 0.3f, kx, bottom - ph * 0.12f, size * 0.01f);
    }
    // fingers: long and thin, three joints, curling over the bar (or hanging open)
    std::array<Point<float>, 4> tips {};
    for (int f = 0; f < 4; ++f)
    {
        const float fx0 = grip.x - wKnuckle * 0.36f + wKnuckle * 0.72f * static_cast<float> (f) / 3.0f;
        const float len = size * (0.3f + (f == 1 || f == 2 ? 0.07f : 0.0f)) * (1.35f - 0.35f * curl);
        const float splay = (static_cast<float> (f) - 1.5f) * size * 0.012f * (1.0f - curl);
        const auto k1 = Point<float> (fx0, bottom);
        const auto k2 = Point<float> (fx0 + dir * size * 0.02f * curl + splay, bottom + len * 0.42f);
        const auto k3 = Point<float> (fx0 + dir * size * 0.005f * curl + splay * 1.6f, bottom + len * 0.75f);
        const auto tip = Point<float> (fx0 - dir * size * 0.045f * curl + splay * 2.0f, bottom + len * (1.0f - 0.08f * curl));
        tips[static_cast<std::size_t> (f)] = tip;
        const Point<float> seg[4] = { k1, k2, k3, tip };
        for (int s = 0; s < 3; ++s)   // each bone a little thinner
        {
            const float w = size * (0.062f - 0.009f * static_cast<float> (s));
            g.setColour (c::bone.darker (0.04f + 0.05f * static_cast<float> (s)));
            g.drawLine ({ seg[s], seg[s + 1] }, w);
            g.setColour (Colour (0xff5f594f).withAlpha (0.55f));   // the shadow side
            g.drawLine ({ seg[s].translated (dir * w * 0.3f, 0.0f), seg[s + 1].translated (dir * w * 0.3f, 0.0f) }, w * 0.35f);
        }
        for (int s = 0; s < 3; ++s)   // knobbly joints
        {
            const float w = size * (0.068f - 0.01f * static_cast<float> (s));
            g.setColour (c::bone.darker (0.08f));
            g.fillEllipse (Rectangle<float> (w, w * 0.8f).withCentre (seg[s]));
            g.setColour (Colour (0xff5f594f).withAlpha (0.5f));
            g.drawLine (seg[s].x - w * 0.3f, seg[s].y, seg[s].x + w * 0.3f, seg[s].y, 0.6f);
        }
        g.setColour (Colour (0xff120c0e));   // long dark nails
        g.fillEllipse (Rectangle<float> (size * 0.04f, size * 0.05f).withCentre (tip.translated (0.0f, size * 0.014f)));
    }
    Path thumb;
    thumb.startNewSubPath (grip.x - dir * wTop * 0.45f, top + ph * 0.45f);
    thumb.lineTo (grip.x - dir * size * 0.28f, bottom - ph * 0.05f);
    thumb.lineTo (grip.x - dir * size * 0.25f, bottom + size * (0.08f + 0.06f * curl));
    g.setColour (c::bone.darker (0.12f));
    g.strokePath (thumb, juce::PathStrokeType (size * 0.06f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Colour (0xff120c0e));
    g.fillEllipse (Rectangle<float> (size * 0.04f, size * 0.045f).withCentre ({ grip.x - dir * size * 0.25f, bottom + size * (0.09f + 0.06f * curl) }));
    return tips;
}

/** The ebony control bar with brass caps; a screw eye under it for every string. */
inline void controlBar (Graphics& g, float x1, float x2, float y, float thickness, const std::vector<float>& eyes)
{
    const auto bar = Rectangle<float> (x1, y - thickness * 0.5f, x2 - x1, thickness);
    g.setColour (c::ink.withAlpha (0.5f));
    g.fillRoundedRectangle (bar.translated (0.0f, 4.0f), thickness * 0.4f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff3a2c22), 0.0f, bar.getY(), Colour (0xff0b0806), 0.0f, bar.getBottom(), false));
    g.fillRoundedRectangle (bar, thickness * 0.4f);
    g.setColour (c::moon.withAlpha (0.25f));
    g.drawHorizontalLine (juce::roundToInt (bar.getY() + 1.0f), x1 + 6.0f, x2 - 6.0f);
    g.setColour (c::ink.withAlpha (0.5f));
    for (int i = 0; i < 5; ++i)   // grain
        g.drawHorizontalLine (juce::roundToInt (bar.getY() + thickness * (0.35f + 0.12f * static_cast<float> (i))),
                              x1 + 8.0f + 40.0f * hash01 (i + 900), x2 - 8.0f - 40.0f * hash01 (i + 950));
    for (float ex : eyes)
    {
        g.setColour (c::brass.withAlpha (0.8f));
        g.drawEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ ex, bar.getBottom() + 3.0f }), 1.2f);
    }
    for (float cx : { x1 + 4.0f, x2 - 4.0f })   // brass caps
    {
        const auto cap = Rectangle<float> (10.0f, thickness + 4.0f).withCentre ({ cx, y });
        g.setGradientFill (juce::ColourGradient (c::brass, 0.0f, cap.getY(), c::brassDim.darker (0.5f), 0.0f, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);
    }
    const auto cross = Rectangle<float> (thickness * 0.85f, thickness * 3.4f).withCentre (bar.getCentre());
    g.setGradientFill (juce::ColourGradient (Colour (0xff3a2c22), cross.getX(), 0.0f, Colour (0xff0b0806), cross.getRight(), 0.0f, false));
    g.fillRoundedRectangle (cross, thickness * 0.3f);
    g.setColour (c::brass.withAlpha (0.7f));
    g.fillRect (cross.withHeight (2.0f).withY (cross.getY() + 4.0f));
    g.fillRect (cross.withHeight (2.0f).withY (cross.getBottom() - 6.0f));
}

// ------------------------------------------------------------------------------------------ drums
/** A tarnished cymbal seen at an angle: lathe grooves, a moonlit rim, a bell; it lights up on hits. */
inline void cymbal (Graphics& g, Point<float> c0, float w, float tilt, float glow, Colour glowCol, bool cracked = false, float bell = 0.16f)
{
    const auto disc = Rectangle<float> (w, w * tilt).withCentre (c0);
    const Colour metal (0xff9c8a5e);
    juce::ColourGradient grad (metal.brighter (0.35f), disc.getX(), disc.getY(), c::ink, disc.getRight(), disc.getBottom(), false);
    grad.addColour (0.45, metal.darker (0.35f));
    g.setGradientFill (grad);
    g.fillEllipse (disc);
    for (int i = 1; i < 6; ++i)
    {
        const float f = static_cast<float> (i) / 6.0f;
        g.setColour ((i % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.07f));
        g.drawEllipse (disc.withSizeKeepingCentre (w * f, w * tilt * f), 0.8f);
    }
    if (cracked)
    {
        g.setColour (c::ink.withAlpha (0.9f));
        g.drawLine (disc.getRight() - w * 0.06f, c0.y + w * tilt * 0.1f, c0.x + w * 0.22f, c0.y - w * tilt * 0.05f, 1.2f);
        g.drawLine (c0.x + w * 0.22f, c0.y - w * tilt * 0.05f, c0.x + w * 0.14f, c0.y + w * tilt * 0.08f, 1.0f);
    }
    const auto bellR = Rectangle<float> (w * bell * 1.6f, w * tilt * bell * 1.9f).withCentre (c0.translated (0.0f, -w * tilt * 0.08f));
    g.setGradientFill (juce::ColourGradient (metal.brighter (0.6f), bellR.getX(), bellR.getY(), metal.darker (0.6f), bellR.getRight(), bellR.getBottom(), false));
    g.fillEllipse (bellR);
    Path lit;
    lit.addCentredArc (c0.x, c0.y, w * 0.5f, w * tilt * 0.5f, 0.0f, -2.5f, -0.3f, true);
    g.setColour (c::moon.withAlpha (0.65f));
    g.strokePath (lit, juce::PathStrokeType (1.0f));
    g.setColour (c::ink.withAlpha (0.8f));
    g.drawEllipse (disc, 0.8f);
    if (glow > 0.01f)
    {
        g.setColour (glowCol.withAlpha (0.35f * glow));
        g.fillEllipse (disc);
        Path edge;
        edge.addEllipse (disc);
        fx::glowStroke (g, edge, glowCol.withAlpha (glow), 1.0f, glow);
    }
}

/** A drum shell seen slightly from above: lacquered body lit from the left, hoops, lugs, a worn head. */
inline void shell (Graphics& g, Rectangle<float> r, Colour lacquer, float glow, Colour glowCol)
{
    const float eh = r.getHeight() * 0.42f;
    const auto top = r.withHeight (eh);
    const auto bodyR = r.withTrimmedTop (eh * 0.5f).withTrimmedBottom (eh * 0.5f);
    juce::ColourGradient body (lacquer.darker (0.6f), r.getX(), 0.0f, lacquer.darker (0.95f), r.getRight(), 0.0f, false);
    body.addColour (0.22, lacquer.brighter (0.25f));
    body.addColour (0.5, lacquer.darker (0.3f));
    g.setGradientFill (body);
    g.fillRect (bodyR);
    g.fillEllipse (r.withTop (r.getBottom() - eh));
    g.setColour (c::brass.withAlpha (0.55f));   // lugs
    for (int i = 0; i < 6; ++i)
    {
        const float x = r.getX() + r.getWidth() * (0.1f + 0.16f * static_cast<float> (i));
        g.fillRoundedRectangle (Rectangle<float> (r.getWidth() * 0.035f, bodyR.getHeight() * 0.42f).withCentre ({ x, bodyR.getCentreY() + eh * 0.2f }), 1.0f);
    }
    Path bottomHoop;
    bottomHoop.addCentredArc (r.getCentreX(), r.getBottom() - eh * 0.5f, r.getWidth() * 0.5f, eh * 0.5f, 0.0f, pi * 0.5f, pi * 1.5f, true);
    g.setColour (c::oldMetal.withAlpha (0.6f));
    g.strokePath (bottomHoop, juce::PathStrokeType (1.4f));
    juce::ColourGradient head (c::bone, top.getX(), top.getY(), Colour (0xff8f8878), top.getRight(), top.getBottom(), false);
    g.setGradientFill (head);
    g.fillEllipse (top);
    fx::glowDot (g, top.getCentre().translated (-top.getWidth() * 0.08f, 0.0f), top.getHeight() * 0.35f, Colour (0xff5e584c).withAlpha (0.35f));   // stick-worn
    g.setColour (c::oldMetal.brighter (0.3f));
    g.drawEllipse (top, 1.6f);
    Path lit;
    lit.addCentredArc (top.getCentreX(), top.getCentreY(), top.getWidth() * 0.5f, top.getHeight() * 0.5f, 0.0f, -2.4f, -0.4f, true);
    g.setColour (c::moon.withAlpha (0.8f));
    g.strokePath (lit, juce::PathStrokeType (1.0f));
    if (glow > 0.01f)
    {
        g.setColour (glowCol.withAlpha (0.4f * glow));
        g.fillEllipse (top);
        Path edge;
        edge.addEllipse (top);
        fx::glowStroke (g, edge, glowCol.withAlpha (glow), 1.0f, glow);
    }
}

/** The drum hanging from a string, drawn in a square of side `size` centred on `centre`.
    `glow` 0..1 lights it in its own colour (a hit). */
inline void drum (Graphics& g, plugin_dsp::DrumType t, Point<float> centre, float size, float glow = 0.0f)
{
    using plugin_dsp::DrumType;
    const auto col = drumColour (t);
    const auto r = Rectangle<float> (size, size).withCentre (centre);
    if (glow > 0.01f) fx::glowDot (g, centre, size * 0.85f, col.withAlpha (0.3f * glow));
    switch (t)
    {
        case DrumType::kick:
        {
            const float R = size * 0.46f;
            const auto hoop = Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (centre);
            g.setGradientFill (juce::ColourGradient (Colour (0xff3b0d15), hoop.getX(), hoop.getY(), Colour (0xff080204), hoop.getRight(), hoop.getBottom(), false));
            g.fillEllipse (hoop);
            Path lit;
            lit.addCentredArc (centre.x, centre.y, R - 1.0f, R - 1.0f, 0.0f, -2.6f, -0.3f, true);
            g.setColour (c::moon.withAlpha (0.55f));
            g.strokePath (lit, juce::PathStrokeType (1.2f));
            g.setColour (c::brass.withAlpha (0.6f));   // claws
            for (int i = 0; i < 10; ++i)
            {
                const auto p = fx::polar (centre, static_cast<float> (i) * pi * 0.2f, R * 0.93f);
                g.fillEllipse (Rectangle<float> (size * 0.035f, size * 0.035f).withCentre (p));
            }
            const auto head = hoop.reduced (R * 0.13f);
            juce::ColourGradient hg (c::bone, head.getX() + head.getWidth() * 0.3f, head.getY() + head.getHeight() * 0.25f, Colour (0xff6c665b), head.getRight(),
                                     head.getBottom(), true);
            g.setGradientFill (hg);
            g.fillEllipse (head);
            if (glow > 0.01f)
            {
                g.setColour (col.withAlpha (0.25f * glow));
                g.fillEllipse (head);
            }
            // the logo: the master's stitched smile
            Path smile;
            smile.startNewSubPath (centre.x - R * 0.5f, centre.y - R * 0.08f);
            smile.quadraticTo (centre.x, centre.y + R * 0.32f, centre.x + R * 0.5f, centre.y - R * 0.08f);
            if (glow > 0.01f) fx::glowStroke (g, smile, col.withAlpha (glow), juce::jmax (1.2f, size * 0.022f), glow);
            g.setColour (c::bloodDeep.brighter (0.2f));
            g.strokePath (smile, juce::PathStrokeType (juce::jmax (1.2f, size * 0.022f)));
            for (int i = -3; i <= 3; ++i)
            {
                const auto p = smile.getPointAlongPath (smile.getLength() * (0.5f + static_cast<float> (i) / 8.0f));
                g.drawLine (p.x, p.y - size * 0.03f, p.x, p.y + size * 0.03f, juce::jmax (1.0f, size * 0.012f));
            }
            const auto port = Rectangle<float> (R * 0.34f, R * 0.34f).withCentre (centre.translated (R * 0.36f, R * 0.4f));
            g.setColour (c::ink);
            g.fillEllipse (port);
            g.setColour (c::oldMetal.withAlpha (0.6f));
            g.drawEllipse (port, 1.0f);
            break;
        }
        case DrumType::snare:
            shell (g, r.reduced (size * 0.04f, size * 0.24f), Colour (0xff5a1220), glow, col);
            break;
        case DrumType::rimshot:
        {
            shell (g, r.reduced (size * 0.06f, size * 0.26f), Colour (0xff4f3321), glow, col);
            Path stick;   // a drumstick across the rim
            stick.addRoundedRectangle (-size * 0.5f, -size * 0.022f, size, size * 0.044f, size * 0.022f);
            const auto tf = juce::AffineTransform::rotation (-0.32f).translated (centre.x + size * 0.02f, centre.y - size * 0.14f);
            g.setColour (c::ink.withAlpha (0.5f));
            g.fillPath (stick, tf.translated (2.0f, 3.0f));
            g.setGradientFill (juce::ColourGradient (Colour (0xffe0cfa8), centre.x, centre.y - size * 0.2f, Colour (0xff7a6446), centre.x, centre.y, false));
            g.fillPath (stick, tf);
            g.setColour (Colour (0xffe8dcc0));
            g.fillEllipse (Rectangle<float> (size * 0.07f, size * 0.06f).withCentre (Point<float> (size * 0.5f, 0.0f).transformedBy (tf)));
            break;
        }
        case DrumType::closedHat:
            g.setColour (c::oldMetal.withAlpha (0.7f));
            g.drawLine (centre.x, centre.y - size * 0.3f, centre.x, centre.y + size * 0.1f, 1.4f);
            cymbal (g, centre.translated (0.0f, size * 0.03f), size * 0.9f, 0.22f, glow * 0.6f, col);
            cymbal (g, centre.translated (0.0f, -size * 0.03f), size * 0.9f, 0.22f, glow, col);
            break;
        case DrumType::openHat:
            g.setColour (c::oldMetal.withAlpha (0.7f));
            g.drawLine (centre.x, centre.y - size * 0.35f, centre.x, centre.y + size * 0.2f, 1.4f);
            cymbal (g, centre.translated (0.0f, size * 0.14f), size * 0.9f, 0.22f, glow * 0.6f, col);
            cymbal (g, centre.translated (0.0f, -size * 0.14f), size * 0.9f, 0.22f, glow, col);
            break;
        case DrumType::crash: cymbal (g, centre, size * 1.08f, 0.34f, glow, col, true); break;
        case DrumType::ride:  cymbal (g, centre, size * 1.08f, 0.3f, glow, col, false, 0.24f); break;
        case DrumType::clap:
        {
            // two long hands pressed together
            for (int side = -1; side <= 1; side += 2)
            {
                const float s = static_cast<float> (side);
                Path h;
                h.startNewSubPath (0.0f, size * 0.36f);
                h.lineTo (s * size * 0.14f, size * 0.34f);
                h.quadraticTo (s * size * 0.2f, size * 0.05f, s * size * 0.13f, -size * 0.12f);
                for (int f = 0; f < 4; ++f)   // finger tips along the top
                {
                    const float fx0 = s * size * (0.12f - 0.035f * static_cast<float> (f));
                    const float ft = -size * (0.34f + (f == 1 || f == 2 ? 0.08f : 0.0f));
                    h.lineTo (fx0, ft);
                    h.lineTo (fx0 - s * size * 0.03f, ft + size * 0.02f);
                }
                h.lineTo (0.0f, -size * 0.3f);
                h.closeSubPath();
                const auto tf = juce::AffineTransform::rotation (s * 0.12f).translated (centre.x + s * size * 0.02f, centre.y);
                g.setGradientFill (juce::ColourGradient (side < 0 ? c::bone : c::bone.darker (0.25f), centre.x - size * 0.2f, centre.y - size * 0.3f,
                                                         Colour (0xff6e685c), centre.x + size * 0.2f, centre.y + size * 0.36f, false));
                g.fillPath (h, tf);
                g.setColour (Colour (0xff5d574e).withAlpha (0.6f));
                for (int f = 1; f < 4; ++f)
                {
                    const float fx0 = s * size * (0.12f - 0.035f * static_cast<float> (f)) + s * size * 0.016f;
                    g.drawLine (Point<float> (fx0, -size * 0.3f).transformedBy (tf).x, Point<float> (fx0, -size * 0.3f).transformedBy (tf).y,
                                Point<float> (fx0, -size * 0.05f).transformedBy (tf).x, Point<float> (fx0, -size * 0.05f).transformedBy (tf).y, 0.7f);
                }
            }
            g.setColour (col.withAlpha (0.35f + 0.65f * glow));   // the sound of it
            for (int i = 0; i < 3; ++i)
            {
                Path arc;
                const float rad = size * (0.46f + 0.07f * static_cast<float> (i));
                arc.addCentredArc (centre.x, centre.y, rad, rad, 0.0f, -0.5f, 0.5f, true);
                g.strokePath (arc, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            break;
        }
        case DrumType::misc:
        {
            // a wooden tom block (toms are misc) on an iron ring
            const auto block = r.reduced (size * 0.12f, size * 0.26f);
            g.setColour (c::oldMetal);
            g.drawEllipse (Rectangle<float> (size * 0.12f, size * 0.12f).withCentre ({ centre.x, block.getY() - size * 0.03f }), 1.4f);
            g.setGradientFill (juce::ColourGradient (Colour (0xff6c4e36), block.getX(), block.getY(), Colour (0xff1a110b), block.getRight(), block.getBottom(), false));
            g.fillRoundedRectangle (block, size * 0.06f);
            g.setColour (c::ink.withAlpha (0.35f));
            for (int i = 0; i < 4; ++i)   // grain
            {
                Path grain;
                const float y = block.getY() + block.getHeight() * (0.2f + 0.2f * static_cast<float> (i));
                grain.startNewSubPath (block.getX() + 3.0f, y);
                grain.quadraticTo (block.getCentreX(), y + (i % 2 == 0 ? 3.0f : -3.0f), block.getRight() - 3.0f, y);
                g.strokePath (grain, juce::PathStrokeType (0.8f));
            }
            g.setColour (c::ink.withAlpha (0.85f));
            g.fillRoundedRectangle (block.withSizeKeepingCentre (block.getWidth() * 0.62f, block.getHeight() * 0.16f), size * 0.03f);
            g.setColour (c::moon.withAlpha (0.5f));
            g.drawLine (block.getX() + 3.0f, block.getY() + 1.0f, block.getRight() - 6.0f, block.getY() + 1.0f, 1.0f);
            if (glow > 0.01f)
            {
                g.setColour (col.withAlpha (0.3f * glow));
                g.fillRoundedRectangle (block, size * 0.06f);
            }
            break;
        }
    }
}

/** An empty string's tag, waiting for its type: an old toe tag with a question mark. */
inline void questionTag (Graphics& g, Point<float> centre, float size, float glow = 0.0f)
{
    const auto tag = Rectangle<float> (size * 0.5f, size * 0.7f).withCentre (centre);
    const float cut = tag.getWidth() * 0.22f;
    Path p;
    p.startNewSubPath (tag.getX() + cut, tag.getY());
    p.lineTo (tag.getRight() - cut, tag.getY());
    p.lineTo (tag.getRight(), tag.getY() + cut);
    p.lineTo (tag.getRight(), tag.getBottom());
    p.lineTo (tag.getX(), tag.getBottom());
    p.lineTo (tag.getX(), tag.getY() + cut);
    p.closeSubPath();
    g.setColour (c::ink.withAlpha (0.5f));
    g.fillPath (p, juce::AffineTransform::translation (3.0f, 5.0f));
    g.setGradientFill (juce::ColourGradient (Colour (0xffd9cfb6), tag.getX(), tag.getY(), Colour (0xff867b66), tag.getRight(), tag.getBottom(), false));
    g.fillPath (p);
    fx::glowDot (g, tag.getCentre().translated (tag.getWidth() * 0.2f, tag.getHeight() * 0.25f), tag.getWidth() * 0.35f, Colour (0xff6b5436).withAlpha (0.35f));
    if (glow > 0.01f)
    {
        g.setColour (c::ecto.withAlpha (0.2f * glow));
        g.fillPath (p);
    }
    const auto hole = Rectangle<float> (size * 0.08f, size * 0.08f).withCentre ({ tag.getCentreX(), tag.getY() + size * 0.08f });
    g.setColour (c::brassDim);
    g.drawEllipse (hole.expanded (size * 0.018f), size * 0.022f);
    g.setColour (c::ink);
    g.fillEllipse (hole);
    g.setColour (c::ink.withAlpha (0.35f));
    for (int i = 0; i < 2; ++i)
        g.fillRect (Rectangle<float> (tag.getX() + tag.getWidth() * 0.18f, tag.getBottom() - size * (0.12f - 0.05f * static_cast<float> (i)), tag.getWidth() * 0.64f, 1.0f));
    g.setColour (c::ink.withAlpha (0.85f));
    g.setFont (whisper (size * 0.42f));
    g.drawText ("?", tag.withTrimmedTop (size * 0.08f).withTrimmedBottom (size * 0.12f), juce::Justification::centred, false);
}

// ------------------------------------------------------------------------------------ Shadow Play
/** The shape that stands for a drum type on the Shadow Play ring (`halo` 0..1: a fresh hit). */
inline void shadowShape (Graphics& g, plugin_dsp::DrumType t, Point<float> p, float s, Colour col, float halo = 0.0f)
{
    using plugin_dsp::DrumType;
    if (halo > 0.01f) fx::glowDot (g, p, s * 1.4f, col.withMultipliedAlpha (0.45f * halo));
    g.setColour (col);
    Path path;
    switch (t)
    {
        case DrumType::kick: g.fillEllipse (Rectangle<float> (s, s).withCentre (p)); return;
        case DrumType::snare: path.addTriangle (p.x, p.y - s * 0.6f, p.x - s * 0.55f, p.y + s * 0.4f, p.x + s * 0.55f, p.y + s * 0.4f); break;
        case DrumType::closedHat: g.fillEllipse (Rectangle<float> (s * 0.5f, s * 0.5f).withCentre (p)); return;
        case DrumType::openHat: g.drawEllipse (Rectangle<float> (s * 0.8f, s * 0.8f).withCentre (p), juce::jmax (1.2f, s * 0.12f)); return;
        case DrumType::crash: path.addStar (p, 8, s * 0.2f, s * 0.6f); break;
        case DrumType::ride:
            path.startNewSubPath (p.x, p.y - s * 0.55f);
            path.lineTo (p.x + s * 0.4f, p.y);
            path.lineTo (p.x, p.y + s * 0.55f);
            path.lineTo (p.x - s * 0.4f, p.y);
            path.closeSubPath();
            break;
        case DrumType::rimshot: g.drawLine (p.x - s * 0.45f, p.y + s * 0.3f, p.x + s * 0.45f, p.y - s * 0.3f, juce::jmax (1.5f, s * 0.14f)); return;
        case DrumType::clap:
            for (int i = 0; i < 3; ++i)
            {
                const float ox = (static_cast<float> (i) - 1.0f) * s * 0.38f, oy = (i == 1 ? -s * 0.2f : s * 0.1f);
                path.addTriangle (p.x + ox, p.y + oy - s * 0.25f, p.x + ox - s * 0.2f, p.y + oy + s * 0.15f, p.x + ox + s * 0.2f, p.y + oy + s * 0.15f);
            }
            break;
        case DrumType::misc:
            g.setColour (col.withMultipliedAlpha (0.6f));
            g.drawRect (Rectangle<float> (s * 0.5f, s * 0.5f).withCentre (p), juce::jmax (1.0f, s * 0.1f));
            return;
    }
    g.fillPath (path);
}

// ---------------------------------------------------------------------------------------- velvet
/** Heavy velvet folds filling `area` (light from the upper left). */
inline void velvet (Graphics& g, Rectangle<float> area, int folds, int seed, float brightness = 1.0f)
{
    if (area.getWidth() < 1.0f) return;
    const float fw = area.getWidth() / static_cast<float> (folds);
    for (int f = 0; f < folds; ++f)
    {
        const float x = area.getX() + fw * static_cast<float> (f);
        const float lift = (0.6f + 0.5f * hash01 (seed * 31 + f)) * brightness;
        juce::ColourGradient fold (c::velvetDark, x, 0.0f, c::velvetDark, x + fw, 0.0f, false);
        fold.addColour (0.32, c::velvet.withMultipliedBrightness (0.7f + 0.5f * lift));
        fold.addColour (0.45, c::velvet.brighter (0.25f * lift));
        fold.addColour (0.7, c::velvet.darker (0.4f));
        g.setGradientFill (fold);
        g.fillRect (Rectangle<float> (x, area.getY(), fw + 0.6f, area.getHeight()));
    }
    juce::ColourGradient fall (c::ink.withAlpha (0.7f), 0.0f, area.getY(), c::ink.withAlpha (0.0f), 0.0f, area.getY() + area.getHeight() * 0.35f, false);
    g.setGradientFill (fall);
    g.fillRect (area);
    g.setGradientFill (juce::ColourGradient (c::ink.withAlpha (0.0f), 0.0f, area.getBottom() - area.getHeight() * 0.3f, c::ink.withAlpha (0.65f), 0.0f,
                                             area.getBottom(), false));
    g.fillRect (area);
}

/** The scalloped valance across the top of a theatre, with a brass fringe. */
inline void valance (Graphics& g, Rectangle<float> r, float depth)
{
    Path val;
    val.startNewSubPath (r.getX(), r.getY());
    val.lineTo (r.getRight(), r.getY());
    const int scallops = 11;
    const float sw = r.getWidth() / static_cast<float> (scallops);
    val.lineTo (r.getRight(), r.getY() + depth);
    for (int i = scallops; i > 0; --i)
    {
        const float x1 = r.getX() + sw * static_cast<float> (i), x0 = x1 - sw;
        val.quadraticTo ((x0 + x1) * 0.5f, r.getY() + depth * 1.7f, x0, r.getY() + depth);
    }
    val.closeSubPath();
    g.setColour (c::ink.withAlpha (0.6f));
    g.fillPath (val, juce::AffineTransform::translation (0.0f, 6.0f));
    g.saveState();
    g.reduceClipRegion (val);
    velvet (g, r.withHeight (depth * 1.8f), 44, 3, 0.8f);
    g.restoreState();
    g.setColour (c::brass.withAlpha (0.55f));
    g.strokePath (val, juce::PathStrokeType (1.2f));
}

/** Red velvet curtains: progress 0 = fully open (nothing drawn), 1 = closed over the whole area. */
inline void curtain (Graphics& g, Rectangle<float> r, float progress)
{
    if (progress <= 0.0f) return;
    const float half = r.getWidth() * 0.5f * juce::jlimit (0.0f, 1.0f, progress);
    for (int side = 0; side < 2; ++side)
    {
        const auto area = side == 0 ? r.withWidth (half) : r.withLeft (r.getRight() - half);
        const float edge = side == 0 ? area.getRight() : area.getX();
        // the shadow it casts on what is still visible
        g.setGradientFill (juce::ColourGradient (c::ink.withAlpha (0.7f), edge, 0.0f, c::ink.withAlpha (0.0f), edge + (side == 0 ? 60.0f : -60.0f), 0.0f, false));
        g.fillRect (side == 0 ? Rectangle<float> (edge, r.getY(), 60.0f, r.getHeight()) : Rectangle<float> (edge - 60.0f, r.getY(), 60.0f, r.getHeight()));
        velvet (g, area, juce::jmax (2, juce::roundToInt (area.getWidth() / 46.0f)), side + 1);
        g.setGradientFill (juce::ColourGradient (c::brass.withAlpha (0.9f), 0.0f, r.getY(), c::brassDim, 0.0f, r.getBottom(), false));
        g.fillRect (Rectangle<float> (edge - 1.5f, area.getY(), 3.0f, area.getHeight()));
        g.setColour (c::ink.withAlpha (0.8f));
        g.fillRect (area.withTop (area.getBottom() - 18.0f));
        g.setColour (c::brass.withAlpha (0.6f));
        g.fillRect (area.withTop (area.getBottom() - 18.0f).withHeight (1.5f));
    }
    valance (g, r, 34.0f);
}

/** The Puppet Show's little theatre (static): black stage, haze, a spotlight, velvet wings,
    footlights along the front. */
inline void theatre (Graphics& g, Rectangle<float> r)
{
    const float W = r.getWidth(), H = r.getHeight();
    g.setGradientFill (juce::ColourGradient (Colour (0xff1c0a10), r.getCentreX(), H * 0.4f, c::ink, r.getX(), r.getY(), true));
    g.fillRect (r);
    // the floor of the stage
    const float floorY = r.getY() + H * 0.8f;
    g.setGradientFill (juce::ColourGradient (Colour (0xff120a08), 0.0f, floorY, Colour (0xff2a1a12), 0.0f, r.getBottom(), false));
    g.fillRect (r.withTop (floorY));
    g.setColour (c::ink.withAlpha (0.6f));
    for (int i = 0; i < 6; ++i)
    {
        const float y = floorY + (r.getBottom() - floorY) * std::pow (static_cast<float> (i + 1) / 7.0f, 1.4f);
        g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getRight());
    }
    // haze and the spotlight from above
    fx::soft (g, r, 4.0f, 8, [&] (Graphics& sg) {
        Path cone;
        cone.startNewSubPath (r.getCentreX() - 60.0f, r.getY() - 20.0f);
        cone.lineTo (r.getCentreX() + 60.0f, r.getY() - 20.0f);
        cone.lineTo (r.getCentreX() + W * 0.36f, floorY + 40.0f);
        cone.lineTo (r.getCentreX() - W * 0.36f, floorY + 40.0f);
        cone.closeSubPath();
        sg.setGradientFill (juce::ColourGradient (c::limelight.withAlpha (0.12f), 0.0f, r.getY(), c::limelight.withAlpha (0.03f), 0.0f, floorY, false));
        sg.fillPath (cone);
        sg.setColour (c::limelight.withAlpha (0.1f));
        sg.fillEllipse (Rectangle<float> (W * 0.66f, 70.0f).withCentre ({ r.getCentreX(), floorY + 30.0f }));
        for (int i = 0; i < 10; ++i)
        {
            sg.setColour (c::velvet.withAlpha (0.12f));
            sg.fillEllipse (Rectangle<float> (220.0f, 80.0f).withCentre ({ r.getX() + W * hash01 (i + 1000), r.getY() + H * (0.2f + 0.6f * hash01 (i + 1100)) }));
        }
    });
    // footlights
    for (int i = 0; i < 11; ++i)
    {
        const auto p = Point<float> (r.getX() + W * (0.08f + 0.84f * static_cast<float> (i) / 10.0f), r.getBottom() - 3.0f);
        fx::glowDot (g, p, 60.0f, c::limelight.withAlpha (0.14f));
        g.setColour (c::limelight.withAlpha (0.9f));
        g.fillEllipse (Rectangle<float> (7.0f, 4.0f).withCentre (p));
    }
    // velvet wings with their shadow on the stage
    const float wing = W * 0.09f;
    for (int side = 0; side < 2; ++side)
    {
        const auto area = side == 0 ? r.withWidth (wing) : r.withLeft (r.getRight() - wing);
        const float edge = side == 0 ? area.getRight() : area.getX();
        g.setGradientFill (juce::ColourGradient (c::ink.withAlpha (0.8f), edge, 0.0f, c::ink.withAlpha (0.0f), edge + (side == 0 ? 90.0f : -90.0f), 0.0f, false));
        g.fillRect (side == 0 ? Rectangle<float> (edge, r.getY(), 90.0f, H) : Rectangle<float> (edge - 90.0f, r.getY(), 90.0f, H));
        velvet (g, area, 4, side + 7);
    }
    valance (g, r, 30.0f);
    fx::vignette (g, r, 0.8f);
}
} // namespace ui::art
