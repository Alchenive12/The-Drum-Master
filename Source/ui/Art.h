// File: Source/ui/Art.h
// All artwork of The Drum Master, drawn as vectors (docs/04-ui-concept.md "Art"). Each piece is one
// function, so a hand-made or generated image can replace it later without touching the logic.
// Original design: a porcelain mask with a stitched smile and a hairline crack, long bony hands,
// a wooden control bar, maroon velvet. Mood of the FNaF puppet, none of its design.
#pragma once

#include "Theme.h"
#include <plugin_dsp/DrumTypes.h>

#include <cmath>

namespace ui::art
{
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::Point;
using juce::Rectangle;
namespace c = colours;

constexpr float pi = 3.14159265358979f;

/** Dark stage: vignette, floor, velvet drapes at the sides and a valance at the top. */
inline void stage (Graphics& g, Rectangle<float> r, float flicker)
{
    g.fillAll (c::night);
    const auto centre = Point<float> (r.getCentreX(), r.getY() + r.getHeight() * 0.45f);
    juce::ColourGradient light (c::stageLight.brighter (0.05f * flicker), centre, c::night, { r.getX(), r.getBottom() }, true);
    g.setGradientFill (light);
    g.fillRect (r);

    // floor: planks fading into the dark
    const float floorY = r.getY() + r.getHeight() * 0.58f;
    g.setGradientFill (juce::ColourGradient (c::wood.darker (0.9f).withAlpha (0.0f), 0.0f, floorY, c::wood.darker (0.6f), 0.0f, r.getBottom(), false));
    g.fillRect (r.withTop (floorY));
    g.setColour (c::night.withAlpha (0.35f));
    for (int i = 1; i < 9; ++i)
    {
        const float y = floorY + (r.getBottom() - floorY) * std::pow (static_cast<float> (i) / 9.0f, 1.6f);
        g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getRight());
    }

    // side drapes with folds
    auto drape = [&g, &r] (bool left) {
        const float w = r.getWidth() * 0.075f;
        for (int f = 0; f < 5; ++f)
        {
            const float x = left ? r.getX() + static_cast<float> (f) * w / 5.0f : r.getRight() - static_cast<float> (f + 1) * w / 5.0f;
            const float wobble = static_cast<float> ((f * 37) % 11) * 0.8f;
            juce::ColourGradient fold (c::velvet.brighter (0.15f), x, 0.0f, c::velvetDark, x + w / 5.0f, 0.0f, false);
            fold.addColour (0.5, c::velvet);
            g.setGradientFill (fold);
            Path p;
            p.startNewSubPath (x, r.getY());
            p.lineTo (x + w / 5.0f, r.getY());
            p.lineTo (x + w / 5.0f + (left ? -wobble : wobble), r.getBottom());
            p.lineTo (x + (left ? -wobble : wobble), r.getBottom());
            p.closeSubPath();
            g.fillPath (p);
        }
    };
    drape (true);
    drape (false);

    // valance: scalloped velvet at the top with a gold fringe
    Path val;
    const float vh = r.getHeight() * 0.045f;
    val.startNewSubPath (r.getX(), r.getY());
    val.lineTo (r.getRight(), r.getY());
    const int scallops = 14;
    const float sw = r.getWidth() / static_cast<float> (scallops);
    for (int i = scallops; i > 0; --i)
    {
        const float x1 = r.getX() + sw * static_cast<float> (i), x0 = x1 - sw;
        val.quadraticTo ((x0 + x1) * 0.5f, r.getY() + vh * 1.7f, x0, r.getY() + vh);
    }
    val.closeSubPath();
    g.setGradientFill (juce::ColourGradient (c::velvet, 0.0f, r.getY(), c::velvetDark, 0.0f, r.getY() + vh * 1.6f, false));
    g.fillPath (val);
    g.setColour (c::goldDim);
    g.strokePath (val, juce::PathStrokeType (1.0f));
}

/** Late at night in a dark, bare room (grey, a little blue): wallpaper, a tall window letting in
    moonlight that falls across the floorboards, an old dusty drum set in the back corner, dust
    drifting in the light. `time` animates the dust, `flicker` dims everything for a moment. */
inline void room (Graphics& g, Rectangle<float> r, float flicker, float time)
{
    const float horizon = r.getY() + r.getHeight() * 0.6f;   // where the wall meets the floor
    const auto vp = Point<float> (r.getCentreX(), r.getY() + r.getHeight() * 0.42f);   // vanishing point

    // back wall: faded vertical wallpaper stripes, darker towards the corners
    g.setGradientFill (juce::ColourGradient (c::wall.brighter (0.08f * flicker), vp.x, vp.y, c::wallDark, r.getX(), r.getY(), true));
    g.fillRect (r.withBottom (horizon));
    for (int i = 0; i < 44; ++i)
    {
        const float x = r.getX() + r.getWidth() * static_cast<float> (i) / 44.0f;
        g.setColour (c::wallDark.withAlpha (i % 2 == 0 ? 0.25f : 0.1f));
        g.fillRect (Rectangle<float> (x, r.getY(), r.getWidth() / 88.0f, horizon - r.getY()));
    }
    for (int i = 0; i < 30; ++i)   // damp stains, fixed pattern
    {
        const float x = r.getX() + r.getWidth() * std::fmod (static_cast<float> (i) * 0.6180339f, 1.0f);
        const float y = r.getY() + (horizon - r.getY()) * std::fmod (static_cast<float> (i) * 0.4142135f + 0.1f, 1.0f);
        g.setColour (c::wallDark.withAlpha (0.12f));
        g.fillEllipse (Rectangle<float> (40.0f + static_cast<float> ((i * 37) % 60), 26.0f + static_cast<float> ((i * 53) % 40)).withCentre ({ x, y }));
    }

    // floor: boards running towards the vanishing point
    g.setGradientFill (juce::ColourGradient (c::floorBoard.darker (0.6f), 0.0f, horizon, c::floorBoard.brighter (0.05f), 0.0f, r.getBottom(), false));
    g.fillRect (r.withTop (horizon));
    g.setColour (c::wallDark.withAlpha (0.7f));
    for (int i = -14; i <= 14; ++i)
    {
        const float bx = vp.x + static_cast<float> (i) * r.getWidth() * 0.09f;
        const float t = (horizon - vp.y) / (r.getBottom() - vp.y);
        g.drawLine (vp.x + (bx - vp.x) * t, horizon, bx, r.getBottom(), 1.0f);
    }
    // baseboard
    g.setColour (c::wallDark);
    g.fillRect (Rectangle<float> (r.getX(), horizon - 8.0f, r.getWidth(), 8.0f));
    g.setColour (c::oldShell.withAlpha (0.5f));
    g.drawHorizontalLine (juce::roundToInt (horizon - 8.0f), r.getX(), r.getRight());

    // the window, high on the left, and its cold light across the floor
    const auto win = Rectangle<float> (r.getX() + r.getWidth() * 0.09f, r.getY() + r.getHeight() * 0.12f, r.getWidth() * 0.12f, r.getHeight() * 0.33f);
    g.setGradientFill (juce::ColourGradient (c::moon.withAlpha (0.55f * flicker), win.getCentreX(), win.getY(), c::moon.withAlpha (0.18f), win.getCentreX(), win.getBottom(), false));
    g.fillRect (win);
    g.setColour (c::wallDark);
    g.drawRect (win, 5.0f);
    g.fillRect (Rectangle<float> (win.getCentreX() - 2.5f, win.getY(), 5.0f, win.getHeight()));
    g.fillRect (Rectangle<float> (win.getX(), win.getCentreY() - 2.5f, win.getWidth(), 5.0f));
    g.setColour (c::wallDark.withAlpha (0.55f));   // a crack across one pane
    g.drawLine (win.getX() + win.getWidth() * 0.6f, win.getY() + 4.0f, win.getX() + win.getWidth() * 0.85f, win.getCentreY() - 3.0f, 1.0f);
    Path shaft;
    shaft.startNewSubPath (win.getX(), win.getY());
    shaft.lineTo (win.getRight(), win.getY());
    shaft.lineTo (win.getRight() + r.getWidth() * 0.26f, r.getBottom());
    shaft.lineTo (win.getX() + r.getWidth() * 0.06f, r.getBottom());
    shaft.closeSubPath();
    g.setGradientFill (juce::ColourGradient (c::moon.withAlpha (0.10f * flicker), win.getCentreX(), win.getBottom(), c::moon.withAlpha (0.0f), win.getCentreX() + r.getWidth() * 0.15f, r.getBottom(), false));
    g.fillPath (shaft);

    // dust drifting in the moonlight
    for (int i = 0; i < 40; ++i)
    {
        const float seed = static_cast<float> (i) * 12.9898f;
        const float fx = std::fmod (std::sin (seed) * 43758.5453f, 1.0f);
        const float fy = std::fmod (std::fabs (std::cos (seed * 1.3f) * 23421.631f) + time * 0.012f * (0.5f + std::fabs (fx)), 1.0f);
        const float y = win.getY() + (r.getBottom() - win.getY()) * fy;
        const float x = win.getX() + (y - win.getY()) * 0.3f + std::fabs (fx) * win.getWidth() * 1.8f + 6.0f * std::sin (time * 0.3f + seed);
        g.setColour (c::moon.withAlpha (0.25f + 0.2f * std::sin (time + seed)));
        g.fillEllipse (Rectangle<float> (1.6f, 1.6f).withCentre ({ x, y }));
    }
}

/** An old, dusty drum set standing in the dark back corner (base line = the floor under it). */
inline void oldDrumSet (Graphics& g, Point<float> base, float scale, float flicker)
{
    const auto col = [flicker] (Colour cc) { return cc.withMultipliedBrightness (0.55f + 0.25f * flicker); };
    auto stand = [&] (float x, float topY) {
        g.setColour (col (c::oldMetal).withAlpha (0.8f));
        g.drawLine (x, base.y, x, topY, 1.6f * scale);
        g.drawLine (x, base.y, x - 12.0f * scale, base.y + 4.0f * scale, 1.2f * scale);
        g.drawLine (x, base.y, x + 12.0f * scale, base.y + 4.0f * scale, 1.2f * scale);
    };
    auto disc = [&] (Point<float> cc, float w, float tilt) {
        const auto d = Rectangle<float> (w, w * tilt).withCentre (cc);
        g.setColour (col (c::oldMetal).withAlpha (0.9f));
        g.fillEllipse (d);
        g.setColour (col (c::wallDark));
        g.drawEllipse (d, 1.0f);
    };
    // cymbal stands behind
    stand (base.x - 70.0f * scale, base.y - 150.0f * scale);
    disc ({ base.x - 70.0f * scale, base.y - 150.0f * scale }, 78.0f * scale, 0.28f);
    stand (base.x + 95.0f * scale, base.y - 128.0f * scale);
    disc ({ base.x + 95.0f * scale, base.y - 128.0f * scale }, 64.0f * scale, 0.3f);
    // hi-hat on the left
    stand (base.x - 115.0f * scale, base.y - 88.0f * scale);
    disc ({ base.x - 115.0f * scale, base.y - 92.0f * scale }, 44.0f * scale, 0.22f);
    disc ({ base.x - 115.0f * scale, base.y - 86.0f * scale }, 44.0f * scale, 0.22f);
    // floor tom under a dust sheet
    Path sheet;
    const float fx = base.x + 70.0f * scale;
    sheet.startNewSubPath (fx - 30.0f * scale, base.y);
    sheet.quadraticTo (fx - 34.0f * scale, base.y - 50.0f * scale, fx - 10.0f * scale, base.y - 62.0f * scale);
    sheet.quadraticTo (fx + 14.0f * scale, base.y - 70.0f * scale, fx + 32.0f * scale, base.y - 52.0f * scale);
    sheet.quadraticTo (fx + 38.0f * scale, base.y - 20.0f * scale, fx + 34.0f * scale, base.y);
    sheet.closeSubPath();
    g.setColour (col (c::boneShadow).withAlpha (0.55f));
    g.fillPath (sheet);
    // bass drum and a rack tom on it
    const auto kick = Rectangle<float> (92.0f * scale, 92.0f * scale).withCentre ({ base.x, base.y - 46.0f * scale });
    g.setColour (col (c::oldShell));
    g.fillEllipse (kick);
    g.setColour (col (c::boneShadow).withAlpha (0.7f));
    g.fillEllipse (kick.reduced (9.0f * scale));
    g.setColour (col (c::oldShell).withAlpha (0.7f));
    g.drawEllipse (kick.reduced (28.0f * scale), 1.5f * scale);   // faded logo ring
    g.setColour (col (c::wallDark).withAlpha (0.5f));
    g.drawLine (kick.getX() + 20.0f * scale, kick.getY() + 18.0f * scale, kick.getCentreX() + 4.0f * scale, kick.getCentreY() + 10.0f * scale, 1.0f);   // a split in the head
    const auto tom = Rectangle<float> (46.0f * scale, 30.0f * scale).withCentre ({ base.x - 18.0f * scale, kick.getY() - 8.0f * scale });
    g.setColour (col (c::oldShell));
    g.fillRoundedRectangle (tom, 4.0f * scale);
    g.setColour (col (c::boneShadow).withAlpha (0.8f));
    g.fillEllipse (tom.withHeight (9.0f * scale).translated (0.0f, -4.0f * scale));
    // snare on its stand
    stand (base.x - 72.0f * scale, base.y - 48.0f * scale);
    const auto sn = Rectangle<float> (48.0f * scale, 18.0f * scale).withCentre ({ base.x - 72.0f * scale, base.y - 56.0f * scale });
    g.setColour (col (c::oldShell).brighter (0.1f));
    g.fillRect (sn);
    g.setColour (col (c::boneShadow));
    g.fillEllipse (sn.withHeight (8.0f * scale).translated (0.0f, -4.0f * scale));
}

/** The master's legs: very tall and skinny, from above the window down to the floor, standing in
    the room behind the strings. Pinstriped trousers, knobbly knees, bony ankles, long pointed shoes. */
inline void legs (Graphics& g, float centreX, float floorY, float sway, float scale)
{
    for (int side = -1; side <= 1; side += 2)
    {
        const float s = static_cast<float> (side);
        const float hipX = centreX + s * 30.0f * scale + sway;
        const auto knee = Point<float> (centreX + s * 92.0f * scale + sway * 0.5f, floorY * 0.5f);   // bowed outwards
        const auto ankle = Point<float> (centreX + s * 60.0f * scale, floorY - 16.0f * scale);
        const float thigh = 24.0f * scale, shin = 16.0f * scale;
        Path leg;   // trouser outline: hip -> knee -> ankle, tapering
        leg.startNewSubPath (hipX - thigh, -10.0f);
        leg.lineTo (hipX + thigh, -10.0f);
        leg.quadraticTo (knee.x + thigh * 0.8f, (knee.y - 10.0f) * 0.6f, knee.x + thigh * 0.55f, knee.y);
        leg.quadraticTo (ankle.x + shin * 0.9f, (knee.y + ankle.y) * 0.5f, ankle.x + shin * 0.45f, ankle.y);
        leg.lineTo (ankle.x - shin * 0.45f, ankle.y);
        leg.quadraticTo (ankle.x - shin * 0.9f, (knee.y + ankle.y) * 0.5f, knee.x - thigh * 0.55f, knee.y);
        leg.quadraticTo (knee.x - thigh * 0.8f, (knee.y - 10.0f) * 0.6f, hipX - thigh, -10.0f);
        leg.closeSubPath();
        g.setGradientFill (juce::ColourGradient (c::oldShell.darker (0.2f), knee.x - s * thigh, 0.0f, c::wallDark.darker (0.5f), knee.x + s * thigh, 0.0f, false));
        g.fillPath (leg);
        g.setColour (c::moon.withAlpha (0.28f));   // cold moonlit rim, so the silhouette reads
        g.strokePath (leg, juce::PathStrokeType (1.2f * scale));
        g.saveState();   // faint pinstripes
        g.reduceClipRegion (leg);
        g.setColour (c::oldMetal.withAlpha (0.18f));
        for (int k = -3; k <= 3; ++k)
        {
            const float o = static_cast<float> (k) * thigh * 0.28f;
            g.drawLine (hipX + o, -10.0f, knee.x + o * 0.7f, knee.y, 0.8f);
            g.drawLine (knee.x + o * 0.7f, knee.y, ankle.x + o * 0.4f, ankle.y, 0.8f);
        }
        g.restoreState();
        g.setColour (c::oldShell.withAlpha (0.6f));   // the knee pokes through the cloth
        g.fillEllipse (Rectangle<float> (thigh * 1.1f, thigh * 0.9f).withCentre (knee));
        // bony ankle, then the long pointed shoe pointing outwards
        g.setColour (c::boneShadow);
        g.fillRect (Rectangle<float> (shin * 0.55f, 10.0f * scale).withCentre (ankle.translated (0.0f, 6.0f * scale)));
        Path shoe;
        const float sy = floorY - 2.0f * scale;
        shoe.startNewSubPath (ankle.x - s * 12.0f * scale, sy);
        shoe.lineTo (ankle.x - s * 10.0f * scale, sy - 12.0f * scale);
        shoe.quadraticTo (ankle.x + s * 10.0f * scale, sy - 16.0f * scale, ankle.x + s * 26.0f * scale, sy - 8.0f * scale);
        shoe.quadraticTo (ankle.x + s * 52.0f * scale, sy - 6.0f * scale, ankle.x + s * 60.0f * scale, sy - 14.0f * scale);   // curled tip
        shoe.quadraticTo (ankle.x + s * 50.0f * scale, sy + 1.0f, ankle.x + s * 20.0f * scale, sy);
        shoe.closeSubPath();
        g.setColour (c::night);
        g.fillPath (shoe);
        g.setColour (c::oldMetal.withAlpha (0.35f));
        g.strokePath (shoe, juce::PathStrokeType (1.0f));
        g.setColour (c::night.withAlpha (0.5f));   // shadow on the floor
        g.fillEllipse (Rectangle<float> (70.0f * scale, 8.0f * scale).withCentre ({ ankle.x + s * 18.0f * scale, floorY + 2.0f * scale }));
    }
}

/** The master's porcelain mask, cut off by the top edge: only chin, cheeks and the stitched smile. */
inline void mask (Graphics& g, float cx, float visibleBottom, float width, float breathe)
{
    const float h = width * 1.25f;
    const auto face = Rectangle<float> (width, h).withCentre ({ cx, visibleBottom - h * 0.5f + breathe });
    g.setGradientFill (juce::ColourGradient (c::bone, cx - width * 0.2f, face.getBottom() - h * 0.35f,
                                             c::boneShadow.darker (0.4f), cx + width * 0.5f, face.getBottom(), true));
    g.fillEllipse (face);
    // hollow cheeks
    g.setColour (c::boneShadow.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (width * 0.22f, width * 0.12f).withCentre ({ cx - width * 0.27f, face.getBottom() - h * 0.26f }));
    g.fillEllipse (Rectangle<float> (width * 0.22f, width * 0.12f).withCentre ({ cx + width * 0.27f, face.getBottom() - h * 0.26f }));
    // stitched smile: too wide, turned up at the ends
    Path smile;
    const float sy = face.getBottom() - h * 0.17f;
    smile.startNewSubPath (cx - width * 0.3f, sy - width * 0.06f);
    smile.quadraticTo (cx, sy + width * 0.07f, cx + width * 0.3f, sy - width * 0.06f);
    g.setColour (c::night.withAlpha (0.85f));
    g.strokePath (smile, juce::PathStrokeType (juce::jmax (1.5f, width * 0.015f)));
    for (int i = -4; i <= 4; ++i)
    {
        const float t = 0.5f + static_cast<float> (i) / 10.0f;
        const auto p = smile.getPointAlongPath (smile.getLength() * t);
        g.drawLine (p.x, p.y - width * 0.02f, p.x, p.y + width * 0.02f, juce::jmax (1.0f, width * 0.008f));
    }
    // hairline crack from the chin
    Path crack;
    crack.startNewSubPath (cx + width * 0.08f, face.getBottom() - 1.0f);
    crack.lineTo (cx + width * 0.05f, face.getBottom() - h * 0.07f);
    crack.lineTo (cx + width * 0.11f, face.getBottom() - h * 0.12f);
    crack.lineTo (cx + width * 0.07f, face.getBottom() - h * 0.2f);
    g.setColour (c::night.withAlpha (0.6f));
    g.strokePath (crack, juce::PathStrokeType (1.0f));
}

/** One long bony hand gripping the bar from above; its dark sleeve reaches up and inwards to the
    master's body above the window (shoulderX), like a puppeteer leaning over the stage. */
inline void hand (Graphics& g, Point<float> grip, float size, bool left, float sway, float shoulderX)
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
    g.setGradientFill (juce::ColourGradient (c::night, shoulderX, 0.0f, c::oldShell.darker (0.25f), wrist.x, wrist.y, false));
    g.fillPath (sleeve);
    g.setColour (c::moon.withAlpha (0.18f));
    g.strokePath (sleeve, juce::PathStrokeType (1.0f));
    g.setColour (c::goldDim);   // frayed gold cuff
    g.drawLine (wrist.x - wristW, wrist.y, wrist.x + wristW, wrist.y, 2.0f);
    for (int i = 0; i < 5; ++i)
    {
        const float fx = wrist.x - wristW + wristW * 0.5f * static_cast<float> (i);
        g.drawLine (fx, wrist.y, fx + dir * 1.5f, wrist.y + 3.0f + static_cast<float> (i % 2) * 2.0f, 0.8f);
    }
    // palm: narrow and tapered, knuckles showing
    Path palm;
    const float pw = size * 0.44f, top = wrist.y - 1.0f, bottom = grip.y - size * 0.06f;
    palm.startNewSubPath (grip.x - pw * 0.42f, top);
    palm.lineTo (grip.x + pw * 0.42f, top);
    palm.quadraticTo (grip.x + pw * 0.62f, (top + bottom) * 0.5f, grip.x + pw * 0.5f, bottom);
    palm.lineTo (grip.x - pw * 0.5f, bottom);
    palm.quadraticTo (grip.x - pw * 0.62f, (top + bottom) * 0.5f, grip.x - pw * 0.42f, top);
    palm.closeSubPath();
    g.setGradientFill (juce::ColourGradient (c::bone, grip.x - dir * pw * 0.3f, top, c::boneShadow, grip.x + dir * pw * 0.5f, bottom, false));
    g.fillPath (palm);
    g.setColour (c::boneShadow.darker (0.3f).withAlpha (0.6f));   // tendons
    for (int t = 0; t < 3; ++t)
    {
        const float tx = grip.x - pw * 0.25f + pw * 0.25f * static_cast<float> (t);
        g.drawLine (tx, top + size * 0.05f, tx + dir * size * 0.01f, bottom - size * 0.04f, 0.8f);
    }
    // fingers: four long ones, two joints each, curling over the bar
    for (int f = 0; f < 4; ++f)
    {
        const float fx = grip.x - pw * 0.4f + pw * 0.8f * static_cast<float> (f) / 3.0f;
        const float len = size * (0.2f + (f == 1 || f == 2 ? 0.05f : 0.0f));
        const auto k1 = Point<float> (fx, bottom);
        const auto k2 = Point<float> (fx + dir * size * 0.015f, grip.y + len * 0.35f);
        const auto tip = Point<float> (fx - dir * size * 0.03f, grip.y + len * 0.8f);
        Path finger;
        finger.startNewSubPath (k1);
        finger.lineTo (k2);
        finger.lineTo (tip);
        g.setColour (c::bone);
        g.strokePath (finger, juce::PathStrokeType (size * 0.07f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c::boneShadow);
        for (auto k : { k1, k2 })
            g.fillEllipse (Rectangle<float> (size * 0.05f, size * 0.04f).withCentre (k));
        g.setColour (c::night.withAlpha (0.8f));   // dark, slightly long nails
        g.fillEllipse (Rectangle<float> (size * 0.045f, size * 0.035f).withCentre (tip.translated (0.0f, size * 0.01f)));
    }
    // thumb, wrapped around the bar from the other side
    Path thumb;
    thumb.startNewSubPath (grip.x - dir * pw * 0.45f, (top + bottom) * 0.55f);
    thumb.lineTo (grip.x - dir * size * 0.32f, grip.y - size * 0.02f);
    thumb.lineTo (grip.x - dir * size * 0.26f, grip.y + size * 0.1f);
    g.setColour (c::bone.darker (0.08f));
    g.strokePath (thumb, juce::PathStrokeType (size * 0.075f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

/** The wooden control bar the strings hang from. */
inline void controlBar (Graphics& g, float x1, float x2, float y, float thickness)
{
    const auto bar = Rectangle<float> (x1, y - thickness * 0.5f, x2 - x1, thickness);
    g.setGradientFill (juce::ColourGradient (c::woodLight, 0.0f, bar.getY(), c::wood.darker (0.3f), 0.0f, bar.getBottom(), false));
    g.fillRoundedRectangle (bar, thickness * 0.4f);
    g.setColour (c::night.withAlpha (0.4f));
    for (int i = 0; i < 6; ++i)   // grain
        g.drawHorizontalLine (juce::roundToInt (bar.getY() + thickness * (0.25f + 0.1f * static_cast<float> (i))), x1 + 4.0f + static_cast<float> ((i * 53) % 40), x2 - 4.0f - static_cast<float> ((i * 29) % 40));
    // short cross piece in the middle
    const auto cross = Rectangle<float> (thickness * 0.8f, thickness * 3.2f).withCentre (bar.getCentre());
    g.setGradientFill (juce::ColourGradient (c::woodLight, cross.getX(), 0.0f, c::wood.darker (0.3f), cross.getRight(), 0.0f, false));
    g.fillRoundedRectangle (cross, thickness * 0.3f);
}

// ------------------------------------------------------------------------------------------ drums
inline void cymbal (Graphics& g, Point<float> c0, float w, float tilt, Colour col)
{
    const auto disc = Rectangle<float> (w, w * tilt).withCentre (c0);
    g.setGradientFill (juce::ColourGradient (col.brighter (0.4f), disc.getX(), disc.getY(), col.darker (0.6f), disc.getRight(), disc.getBottom(), false));
    g.fillEllipse (disc);
    g.setColour (col.darker (0.8f));
    g.drawEllipse (disc, 1.0f);
    g.setColour (col.brighter (0.2f).withAlpha (0.6f));
    g.drawEllipse (disc.reduced (w * 0.18f, w * tilt * 0.18f), 0.8f);
    g.fillEllipse (Rectangle<float> (w * 0.16f, w * tilt * 0.3f).withCentre (c0.translated (0.0f, -w * tilt * 0.05f)));
}

inline void shell (Graphics& g, Rectangle<float> r, Colour col)
{
    // a drum seen slightly from above: top head ellipse + shell with tension rods
    const float eh = r.getHeight() * 0.32f;
    const auto top = r.withHeight (eh);
    g.setColour (col.darker (0.35f));
    g.fillRect (r.withTrimmedTop (eh * 0.5f).withTrimmedBottom (eh * 0.5f));
    g.fillEllipse (r.withTop (r.getBottom() - eh));
    g.setColour (c::gold.withAlpha (0.6f));
    for (int i = 0; i < 5; ++i)
    {
        const float x = r.getX() + r.getWidth() * (0.12f + 0.19f * static_cast<float> (i));
        g.drawLine (x, r.getY() + eh * 0.5f, x, r.getBottom() - eh * 0.5f, 1.2f);
    }
    g.setColour (c::bone.darker (0.1f));
    g.fillEllipse (top);
    g.setColour (col.darker (0.6f));
    g.drawEllipse (top, 1.2f);
}

/** The drum hanging from a string, drawn in a square of side `size` centred on `centre`. */
inline void drum (Graphics& g, plugin_dsp::DrumType t, Point<float> centre, float size)
{
    using plugin_dsp::DrumType;
    const auto r = Rectangle<float> (size, size).withCentre (centre);
    switch (t)
    {
        case DrumType::kick:
        {
            const auto head = r.reduced (size * 0.06f);
            g.setColour (c::velvet.darker (0.2f));
            g.fillEllipse (head);
            g.setColour (c::gold);
            g.drawEllipse (head, juce::jmax (2.0f, size * 0.05f));
            g.setColour (c::bone.withAlpha (0.9f));
            g.fillEllipse (head.reduced (size * 0.12f));
            g.setColour (c::velvet.darker (0.3f));
            g.drawEllipse (head.reduced (size * 0.3f), juce::jmax (1.0f, size * 0.03f));   // port hole
            break;
        }
        case DrumType::snare: shell (g, r.reduced (size * 0.05f, size * 0.25f), c::velvet); break;
        case DrumType::rimshot:
            shell (g, r.reduced (size * 0.05f, size * 0.25f), c::wood);
            g.setColour (c::bone);
            g.drawLine (r.getX() + size * 0.1f, r.getY() + size * 0.18f, r.getRight() - size * 0.05f, r.getY() + size * 0.45f, juce::jmax (2.0f, size * 0.05f));
            break;
        case DrumType::closedHat:
            cymbal (g, centre.translated (0.0f, -size * 0.04f), size * 0.9f, 0.22f, c::gold);
            cymbal (g, centre.translated (0.0f, size * 0.03f), size * 0.9f, 0.22f, c::goldDim);
            break;
        case DrumType::openHat:
            cymbal (g, centre.translated (0.0f, -size * 0.14f), size * 0.9f, 0.22f, c::gold);
            cymbal (g, centre.translated (0.0f, size * 0.14f), size * 0.9f, 0.22f, c::goldDim);
            break;
        case DrumType::crash: cymbal (g, centre, size * 1.05f, 0.35f, c::gold.brighter (0.1f)); break;
        case DrumType::ride:
            cymbal (g, centre, size * 1.05f, 0.3f, c::goldDim.brighter (0.2f));
            g.setColour (c::gold.brighter (0.3f));
            g.fillEllipse (Rectangle<float> (size * 0.26f, size * 0.12f).withCentre (centre.translated (0.0f, -size * 0.05f)));
            break;
        case DrumType::clap:
        {
            for (int side = -1; side <= 1; side += 2)
            {
                Path p;
                p.addRoundedRectangle (-size * 0.13f, -size * 0.32f, size * 0.26f, size * 0.62f, size * 0.12f);
                g.setColour (side < 0 ? c::bone : c::bone.darker (0.15f));
                g.fillPath (p, juce::AffineTransform::rotation (static_cast<float> (side) * 0.35f).translated (centre.x + static_cast<float> (side) * size * 0.12f, centre.y));
            }
            g.setColour (c::gold.withAlpha (0.8f));
            for (int i = 0; i < 3; ++i)
            {
                const float a = -0.6f + 0.6f * static_cast<float> (i);
                g.drawLine (centre.x + std::sin (a) * size * 0.34f, centre.y - std::cos (a) * size * 0.34f,
                            centre.x + std::sin (a) * size * 0.46f, centre.y - std::cos (a) * size * 0.46f, 1.5f);
            }
            break;
        }
        case DrumType::misc:
        {
            // a wooden tom block (toms are misc)
            const auto block = r.reduced (size * 0.12f, size * 0.24f);
            g.setGradientFill (juce::ColourGradient (c::woodLight, block.getX(), block.getY(), c::wood.darker (0.4f), block.getRight(), block.getBottom(), false));
            g.fillRoundedRectangle (block, size * 0.06f);
            g.setColour (c::night.withAlpha (0.5f));
            g.fillRoundedRectangle (block.withSizeKeepingCentre (block.getWidth() * 0.6f, block.getHeight() * 0.18f), size * 0.03f);
            break;
        }
    }
}

/** An empty string's tag, waiting for its type. */
inline void questionTag (Graphics& g, Point<float> centre, float size)
{
    const auto tag = Rectangle<float> (size * 0.5f, size * 0.62f).withCentre (centre);
    g.setColour (c::bone.darker (0.2f));
    g.fillRoundedRectangle (tag, size * 0.06f);
    g.setColour (c::night);
    g.setFont (serif (size * 0.45f, true));
    g.drawText ("?", tag, juce::Justification::centred);
}

// ------------------------------------------------------------------------------------ Shadow Play
/** The shape that stands for a drum type on the Shadow Play ring. */
inline void shadowShape (Graphics& g, plugin_dsp::DrumType t, Point<float> p, float s, Colour col)
{
    using plugin_dsp::DrumType;
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
        case DrumType::misc: g.setColour (col.withMultipliedAlpha (0.5f)); g.fillRect (Rectangle<float> (s * 0.5f, s * 0.5f).withCentre (p)); return;
    }
    g.fillPath (path);
}

// ---------------------------------------------------------------------------------------- curtain
/** Red velvet curtains: progress 0 = fully open (nothing drawn), 1 = closed over the whole area. */
inline void curtain (Graphics& g, Rectangle<float> r, float progress)
{
    if (progress <= 0.0f) return;
    const float half = r.getWidth() * 0.5f * juce::jlimit (0.0f, 1.0f, progress);
    for (int side = 0; side < 2; ++side)
    {
        const auto area = side == 0 ? r.withWidth (half) : r.withLeft (r.getRight() - half);
        const int folds = 9;
        for (int f = 0; f < folds; ++f)
        {
            const float fw = area.getWidth() / static_cast<float> (folds);
            const float x = area.getX() + fw * static_cast<float> (f);
            juce::ColourGradient fold (c::velvetDark, x, 0.0f, c::velvetDark, x + fw, 0.0f, false);
            fold.addColour (0.45, c::velvet.brighter (0.08f));
            g.setGradientFill (fold);
            g.fillRect (Rectangle<float> (x, area.getY(), fw + 0.5f, area.getHeight()));
        }
        g.setColour (c::gold);   // fringe at the meeting edge
        const float edge = side == 0 ? area.getRight() : area.getX();
        g.fillRect (Rectangle<float> (edge - 2.0f, area.getY(), 4.0f, area.getHeight()));
    }
    g.setColour (c::gold.withAlpha (0.8f));   // gold fringe at the bottom
    g.fillRect (r.withTop (r.getBottom() - 6.0f).withWidth (half));
    g.fillRect (r.withTop (r.getBottom() - 6.0f).withLeft (r.getRight() - half));
}
} // namespace ui::art
