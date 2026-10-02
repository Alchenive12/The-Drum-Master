// File: Source/ui/LookAndFeel.h
// The look of every control. Components pick their variant through properties:
//   "accent" (int ARGB)  - the colour of a knob's arc, a lit button, a fader's fill
//   "style"              - buttons: "pill" (default), "round" (icon only), "danger", "switch", "ticket"
//   "icon"               - a line icon from fx::icon() drawn before the text
//   "genre"              - a ticket's genre icon (0..4)
//   "pulse"              - 0..1, breathes a lit button's glow (set by the editor's timer)
#pragma once

#include "Art3D.h"
#include "Fx.h"

namespace ui
{
inline juce::Colour accentOf (const juce::Component& c, juce::Colour fallback = colours::ecto)
{
    const auto& v = c.getProperties()["accent"];
    return v.isVoid() ? fallback : juce::Colour (static_cast<juce::uint32> (static_cast<juce::int64> (v)));
}

inline void setAccent (juce::Component& c, juce::Colour col)
{
    c.getProperties().set ("accent", static_cast<juce::int64> (col.getARGB()));
    c.repaint();
}

/** Where a rotary knob's parts sit inside its slider bounds (shared with the pages that attach
    strings to a knob's pointer). */
struct KnobGeometry
{
    juce::Point<float> centre;
    float radius = 0.0f, arcRadius = 0.0f, arcThickness = 0.0f, bodyRadius = 0.0f;
    bool big = false;

    explicit KnobGeometry (juce::Rectangle<float> area)
    {
        radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
        centre = area.getCentre();
        big = radius > 40.0f;
        arcThickness = juce::jmax (2.0f, radius * (big ? 0.05f : 0.075f));
        arcRadius = radius - arcThickness * 0.5f - (big ? radius * 0.13f : 0.0f);
        bodyRadius = arcRadius - arcThickness * 0.5f - radius * (big ? 0.1f : 0.13f);
    }
};

constexpr float knobStart = 225.0f * fx::pi / 180.0f, knobEnd = 495.0f * fx::pi / 180.0f;

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        using namespace juce;
        setColour (ComboBox::backgroundColourId, colours::night);
        setColour (ComboBox::textColourId, colours::bone);
        setColour (ComboBox::outlineColourId, colours::boneFaint);
        setColour (ComboBox::arrowColourId, colours::boneDim);
        setColour (PopupMenu::backgroundColourId, colours::night);
        setColour (PopupMenu::textColourId, colours::bone);
        setColour (PopupMenu::highlightedBackgroundColourId, colours::ecto.withAlpha (0.14f));
        setColour (PopupMenu::highlightedTextColourId, colours::ecto);
        setColour (TooltipWindow::backgroundColourId, colours::night);
        setColour (TooltipWindow::textColourId, colours::bone);
        setColour (TooltipWindow::outlineColourId, colours::boneFaint);
        setColour (Label::textColourId, colours::bone);
    }

    // ------------------------------------------------------------------------------- knobs
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
    {
        const KnobGeometry k (juce::Rectangle<int> (x, y, w, h).toFloat());
        const auto c = k.centre;
        const float angle = start + pos * (end - start);
        const auto accent = s.isEnabled() ? accentOf (s) : colours::boneFaint;
        const bool hot = s.isMouseOverOrDragging();
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? start + (end - start) * static_cast<float> (s.valueToProportionOfLength (0.0)) : start;
        const float lo = juce::jmin (from, angle), hi = juce::jmax (from, angle);

        // hero knobs: an LED ring of ticks outside the arc
        if (k.big)
        {
            const int n = 41;
            for (int i = 0; i < n; ++i)
            {
                const float a = start + (end - start) * static_cast<float> (i) / static_cast<float> (n - 1);
                const bool lit = a >= lo - 0.001f && a <= hi + 0.001f;
                const bool major = i % 10 == 0;
                const auto p1 = fx::polar (c, a, k.radius - 1.0f);
                const auto p2 = fx::polar (c, a, k.radius - k.radius * (major ? 0.09f : 0.055f));
                g.setColour (lit ? accent.withAlpha (hot ? 1.0f : 0.85f) : juce::Colours::white.withAlpha (major ? 0.22f : 0.09f));
                g.drawLine ({ p1, p2 }, major ? 1.6f : 1.2f);
            }
        }

        // arc track and value
        juce::Path track;
        track.addCentredArc (c.x, c.y, k.arcRadius, k.arcRadius, 0.0f, start, end, true);
        g.setColour (juce::Colours::white.withAlpha (0.075f));
        g.strokePath (track, juce::PathStrokeType (k.arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (hi - lo > 0.002f)
        {
            juce::Path value;
            value.addCentredArc (c.x, c.y, k.arcRadius, k.arcRadius, 0.0f, lo, hi, true);
            fx::glowStroke (g, value, accent, k.arcThickness, hot ? 1.3f : 0.8f);
        }
        if (bipolar)   // the centre notch
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre (fx::polar (c, from, k.arcRadius + k.arcThickness * 1.6f)));
        }

        // the body: the rendered 3D knob (filmstrip) inside v2's arc; the code-drawn cap is the fallback
        if (art3d::drawKnob (g, c, k.bodyRadius, pos, s.isEnabled()))
            return;
        // the body: a dark lathe-turned cap with a lit rim, sitting in its own shadow
        const float br = k.bodyRadius;
        const auto body = juce::Rectangle<float> (br * 2.0f, br * 2.0f).withCentre (c);
        fx::glowDot (g, c.translated (0.0f, br * 0.22f), br * 1.35f, juce::Colours::black.withAlpha (0.75f));
        juce::ColourGradient fill (colours::slateLight.brighter (hot ? 0.12f : 0.0f), c.x - br * 0.35f, c.y - br * 0.6f, colours::ink, c.x + br * 0.5f,
                                   c.y + br * 0.9f, true);
        g.setGradientFill (fill);
        g.fillEllipse (body);
        g.setColour (juce::Colours::white.withAlpha (0.025f));   // lathe rings
        for (float f : { 0.78f, 0.56f, 0.34f })
            g.drawEllipse (body.withSizeKeepingCentre (br * 2.0f * f, br * 2.0f * f), 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (hot ? 0.3f : 0.2f), c.x, c.y - br, juce::Colours::white.withAlpha (0.02f), c.x,
                                                 c.y + br, false));
        g.drawEllipse (body.reduced (0.5f), 1.0f);

        // the pointer: a bone needle, its tip catching the accent
        const auto p1 = fx::polar (c, angle, br * 0.3f), p2 = fx::polar (c, angle, br * 0.82f);
        g.setColour (hot ? juce::Colours::white : colours::bone);
        g.drawLine ({ p1, p2 }, juce::jmax (1.6f, br * 0.1f));
        fx::glowDot (g, p2, juce::jmax (3.0f, br * 0.2f), accent.withAlpha (hot ? 0.8f : 0.45f));
    }

    // ------------------------------------------------------------------------------- fader
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style,
                           juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0.0f, 0.0f, style, s);
            return;
        }
        const auto accent = accentOf (s);
        const float cx = static_cast<float> (x) + static_cast<float> (w) * 0.5f;
        const auto track = juce::Rectangle<float> (cx - 2.0f, static_cast<float> (y), 4.0f, static_cast<float> (h));
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (track, 2.0f);
        const float zero = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 ? static_cast<float> (s.getPositionOfValue (0.0)) : static_cast<float> (y + h);
        juce::Path fill;
        fill.startNewSubPath (cx, zero);
        fill.lineTo (cx, pos);
        if (std::abs (zero - pos) > 0.5f) fx::glowStroke (g, fill, accent, 3.0f, 0.9f, juce::PathStrokeType::butt);

        const bool hot = s.isMouseOverOrDragging();
        const auto thumb = juce::Rectangle<float> (juce::jmin (34.0f, static_cast<float> (w) - 4.0f), 14.0f).withCentre ({ cx, pos });
        fx::glowDot (g, thumb.getCentre().translated (0.0f, 4.0f), 22.0f, juce::Colours::black.withAlpha (0.7f));
        g.setGradientFill (juce::ColourGradient (colours::bone, 0.0f, thumb.getY(), colours::boneDim.darker (0.3f), 0.0f, thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 4.0f);
        g.setColour (hot ? accent : colours::ink.withAlpha (0.8f));
        g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() - 12.0f, 1.6f));
    }

    int getSliderThumbRadius (juce::Slider& s) override
    {
        return s.getSliderStyle() == juce::Slider::LinearVertical ? 9 : LookAndFeel_V4::getSliderThumbRadius (s);
    }

    // ------------------------------------------------------------------------------- buttons
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto style = b.getProperties()["style"].toString();
        const auto accent = accentOf (b, style == "danger" ? colours::blood : colours::ecto);
        const bool on = b.getToggleState();
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);

        if (style == "switch") return;   // drawn with its text
        if (style == "ticket")
        {
            drawTicket (g, b, r, accent, on, over, down);
            return;
        }
        // pill and danger buttons: the rendered brass plaque (icon-only round buttons keep v2's look)
        if (style != "round" && art3d::drawPlaque (g, b.getLocalBounds().toFloat(), (on || down) ? 2 : (over ? 1 : 0)))
            return;
        if (style == "round") r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));

        const float radius = r.getHeight() * 0.5f;
        const float pulse = static_cast<float> (b.getProperties().getWithDefault ("pulse", 1.0));
        if (on)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.05f * pulse * static_cast<float> (4 - i)));
                g.drawRoundedRectangle (r.expanded (static_cast<float> (i) * 1.6f), radius + static_cast<float> (i) * 1.6f, 1.6f);
            }
        }
        g.setColour (on ? accent.withAlpha (0.13f + 0.05f * pulse + (over ? 0.05f : 0.0f))
                        : juce::Colours::white.withAlpha ((over ? 0.075f : 0.04f) + (down ? 0.04f : 0.0f)));
        g.fillRoundedRectangle (r, radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.0f, r.getY(), juce::Colours::transparentWhite, 0.0f,
                                                 r.getCentreY(), false));
        g.fillRoundedRectangle (r, radius);
        const auto edge = on ? accent.withAlpha (0.8f) : (style == "danger" && over ? accent.withAlpha (0.7f) : juce::Colours::white.withAlpha (over ? 0.24f : 0.12f));
        g.setColour (edge);
        g.drawRoundedRectangle (r, radius, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
    {
        const auto style = b.getProperties()["style"].toString();
        const auto accent = accentOf (b, style == "danger" ? colours::blood : colours::ecto);
        const bool on = b.getToggleState();
        const auto iconName = b.getProperties()["icon"].toString();
        const auto text = b.getButtonText();
        auto r = b.getLocalBounds().toFloat();

        if (style == "switch")
        {
            drawSwitch (g, b, r, accent, on, over);
            return;
        }
        if (style == "ticket") return;   // drawn with its background
        if (style != "round" && art3d::plaqueAvailable())
            r = art3d::plaqueTextArea (r);   // keep the text off the plaque's screw ends

        const auto col = on ? accent.brighter (0.35f) : style == "danger" ? (over ? accent.brighter (0.2f) : accent.withAlpha (0.85f))
                                                                          : colours::bone.withAlpha (over ? 1.0f : 0.8f);
        const auto font = caps (juce::jmin (11.0f, r.getHeight() * 0.36f));
        const float iconSize = juce::jmin (13.0f, r.getHeight() * 0.42f);
        if (text.isEmpty())
        {
            fx::icon (g, iconName, juce::Rectangle<float> (iconSize, iconSize).withCentre (r.getCentre()), col);
            return;
        }
        const float tw = textWidth (font, text);
        const float iw = iconName.isEmpty() ? 0.0f : iconSize + 8.0f;
        float x0 = r.getCentreX() - (tw + iw) * 0.5f;
        if (iw > 0.0f)
        {
            auto iconCol = col;
            if (iconName == "listen" && on)
                iconCol = accent.withAlpha (0.45f + 0.55f * static_cast<float> (b.getProperties().getWithDefault ("pulse", 1.0)));
            fx::icon (g, iconName, juce::Rectangle<float> (iconSize, iconSize).withCentre ({ x0 + iconSize * 0.5f, r.getCentreY() }), iconCol);
            x0 += iw;
        }
        g.setFont (font);
        g.setColour (col);
        g.drawText (text, juce::Rectangle<float> (x0, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft, false);
    }

    // ------------------------------------------------------------------------------- combo box
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (w), static_cast<float> (h)).reduced (1.0f);
        const bool over = box.isMouseOver (true);
        g.setColour (juce::Colours::white.withAlpha (over ? 0.07f : 0.04f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (box.hasKeyboardFocus (true) || over ? accentOf (box).withAlpha (0.6f) : juce::Colours::white.withAlpha (0.13f));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
        juce::Path chevron;
        const float cx = r.getRight() - 18.0f, cy = r.getCentreY();
        chevron.startNewSubPath (cx - 5.0f, cy - 2.5f);
        chevron.lineTo (cx, cy + 2.5f);
        chevron.lineTo (cx + 5.0f, cy - 2.5f);
        g.setColour (over ? colours::bone : colours::boneDim);
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return caps (12.0f, 0.12f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 36, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawComboBoxTextWhenNothingSelected (juce::Graphics& g, juce::ComboBox& box, juce::Label& label) override
    {
        g.setColour (accentOf (box).withAlpha (0.8f));
        g.setFont (whisper (19.0f));
        g.drawFittedText (box.getTextWhenNothingSelected(), label.getBounds().reduced (6, 0), juce::Justification::centredLeft, 1);
    }

    // ------------------------------------------------------------------------------- menus
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (w), static_cast<float> (h));
        g.fillAll (colours::night);
        g.setGradientFill (juce::ColourGradient (colours::slate.withAlpha (0.6f), 0.0f, 0.0f, colours::ink, 0.0f, r.getBottom(), false));
        g.fillRect (r);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRect (r, 1.0f);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*) override
    {
        if (isSeparator)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }
        auto r = area.toFloat().reduced (4.0f, 1.0f);
        if (isHighlighted && isActive)
        {
            g.setColour (colours::ecto.withAlpha (0.12f));
            g.fillRoundedRectangle (r, 6.0f);
        }
        if (isTicked)
        {
            g.setColour (colours::ecto);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() }));
        }
        g.setFont (caps (11.5f, 0.12f));
        g.setColour (! isActive ? colours::boneFaint : isHighlighted ? colours::ecto : colours::bone.withAlpha (0.85f));
        g.drawText (text, r.withTrimmedLeft (24.0f), juce::Justification::centredLeft, true);
    }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int, int& w, int& h) override
    {
        h = isSeparator ? 9 : 30;
        w = juce::roundToInt (textWidth (caps (11.5f, 0.12f), text)) + 56;
    }

    juce::Font getPopupMenuFont() override { return caps (11.5f, 0.12f); }

    // ------------------------------------------------------------------------------- tooltip
    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (w), static_cast<float> (h));
        g.fillAll (colours::night);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawRect (r, 1.0f);
        g.setColour (colours::bone.withAlpha (0.9f));
        g.setFont (sans (13.0f));
        g.drawFittedText (text, r.reduced (10.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 3);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (380, juce::roundToInt (textWidth (sans (13.0f), text)) + 24);
        const int lines = juce::roundToInt (std::ceil (textWidth (sans (13.0f), text) / 356.0f));
        const int h = 12 + 17 * juce::jmax (1, lines);
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18, pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 18, w, h)
            .constrainedWithin (parent);
    }

    void drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging) override
    {
        g.setColour (juce::Colours::white.withAlpha (over || dragging ? 0.35f : 0.12f));
        for (int i = 1; i <= 2; ++i)
        {
            const float d = static_cast<float> (i) * 5.0f;
            g.drawLine (static_cast<float> (w) - d, static_cast<float> (h) - 2.0f, static_cast<float> (w) - 2.0f, static_cast<float> (h) - d, 1.2f);
        }
    }

private:
    /** THE SHOW IS ON / OFF: text on the left, a sliding switch on the right. */
    static void drawSwitch (juce::Graphics& g, juce::Button& b, juce::Rectangle<float> r, juce::Colour accent, bool on, bool over)
    {
        const auto trackR = juce::Rectangle<float> (40.0f, 22.0f).withCentre ({ r.getRight() - 24.0f, r.getCentreY() });
        g.setColour (on ? accent.withAlpha (0.3f) : juce::Colours::white.withAlpha (over ? 0.1f : 0.06f));
        g.fillRoundedRectangle (trackR, 11.0f);
        g.setColour (on ? accent.withAlpha (0.9f) : juce::Colours::white.withAlpha (0.18f));
        g.drawRoundedRectangle (trackR, 11.0f, 1.0f);
        const auto knob = juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ on ? trackR.getRight() - 11.0f : trackR.getX() + 11.0f, trackR.getCentreY() });
        if (on) fx::glowDot (g, knob.getCentre(), 16.0f, accent.withAlpha (0.6f));
        g.setColour (on ? juce::Colours::white : colours::boneDim);
        g.fillEllipse (knob);
        g.setFont (caps (10.5f));
        g.setColour (on ? accent.brighter (0.4f) : colours::bone.withAlpha (over ? 1.0f : 0.75f));
        g.drawText (b.getButtonText(), r.withRight (trackR.getX() - 10.0f), juce::Justification::centredRight, false);
    }

    /** The Puppet Show's genre tickets: a stub with notched sides, an icon and the name. */
    static void drawTicket (juce::Graphics& g, juce::Button& b, juce::Rectangle<float> r, juce::Colour accent, bool on, bool over, bool down)
    {
        juce::Path t;
        t.addRoundedRectangle (r, 8.0f);
        juce::Path clip;   // everything except two half-circle notches in the sides (even-odd)
        clip.addRectangle (r.expanded (10.0f));
        for (float nx : { r.getX(), r.getRight() })
            clip.addEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ nx, r.getCentreY() }));
        clip.setUsingNonZeroWinding (false);
        g.saveState();
        g.reduceClipRegion (clip);
        if (on)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.07f * static_cast<float> (4 - i)));
                g.strokePath (t, juce::PathStrokeType (static_cast<float> (i) * 3.0f));
            }
        g.setGradientFill (juce::ColourGradient (on ? accent.withAlpha (0.2f) : juce::Colours::white.withAlpha (over ? 0.075f : 0.04f), 0.0f, r.getY(),
                                                 on ? colours::bloodDeep.withAlpha (0.5f) : juce::Colours::white.withAlpha (0.015f), 0.0f, r.getBottom(), false));
        g.fillPath (t);
        g.setColour (on ? accent.withAlpha (0.85f) : juce::Colours::white.withAlpha (over ? 0.25f : 0.12f));
        g.strokePath (t, juce::PathStrokeType (down ? 1.6f : 1.0f));
        g.restoreState();
        // the perforation between the stub and the ticket
        g.setColour (on ? accent.withAlpha (0.4f) : juce::Colours::white.withAlpha (0.08f));
        for (float yy = r.getY() + 6.0f; yy < r.getBottom() - 4.0f; yy += 5.0f)
            g.fillRect (juce::Rectangle<float> (r.getX() + 38.0f, yy, 1.0f, 2.5f));
        const int genre = static_cast<int> (b.getProperties().getWithDefault ("genre", 0));
        const auto ink = on ? accent.brighter (0.4f) : colours::bone.withAlpha (over ? 0.95f : 0.7f);
        fx::genreIcon (g, genre, juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ r.getX() + 20.0f, r.getCentreY() }), ink);
        g.setFont (caps (11.5f, 0.2f));
        g.setColour (ink);
        g.drawText (b.getButtonText(), r.withTrimmedLeft (40.0f), juce::Justification::centred, false);
    }
};
} // namespace ui
