// File: Source/ui/Theme.h
// Colours, fonts and the knob / button look of The Drum Master (docs/04-ui-concept.md).
// Creepy theatre: near-black stage, maroon velvet, bone-white puppet master, dim gold text,
// a sickly green glow for things that are "alive".
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
namespace colours
{
const juce::Colour night      { 0xff0a090c };   // the darkest stage black
const juce::Colour stage      { 0xff16121a };
const juce::Colour stageLight { 0xff2a2230 };
const juce::Colour velvet     { 0xff5a0f16 };
const juce::Colour velvetDark { 0xff2a0509 };
const juce::Colour bone       { 0xffe6dcc6 };
const juce::Colour boneShadow { 0xff9d9180 };
const juce::Colour gold       { 0xffc9a45c };
const juce::Colour goldDim    { 0xff7d6636 };
const juce::Colour blood      { 0xffb01f2a };
const juce::Colour glow       { 0xff8fd18a };   // sickly green: meters, playhead, "listening"
const juce::Colour wood       { 0xff4a3322 };
const juce::Colour woodLight  { 0xff7a5a3c };
const juce::Colour string     { 0xffcfc5b0 };
} // namespace colours

inline juce::Font serif (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

/** Knobs: a dark disc with a bone rim and a bone pointer; the value arc glows blood red, from
    the centre for bipolar parameters. Buttons: engraved plaques. */
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, colours::night);
        setColour (juce::ComboBox::textColourId, colours::bone);
        setColour (juce::ComboBox::outlineColourId, colours::goldDim);
        setColour (juce::ComboBox::arrowColourId, colours::gold);
        setColour (juce::PopupMenu::backgroundColourId, colours::night);
        setColour (juce::PopupMenu::textColourId, colours::bone);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::velvet);
        setColour (juce::PopupMenu::highlightedTextColourId, colours::bone);
        setColour (juce::TooltipWindow::backgroundColourId, colours::night);
        setColour (juce::TooltipWindow::textColourId, colours::bone);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
    {
        const auto area = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float angle = start + pos * (end - start);

        // arc track and value (bipolar parameters light up from the top centre)
        const float arcR = r - 1.5f, thick = juce::jmax (2.0f, r * 0.12f);
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
        g.setColour (colours::night.brighter (0.15f));
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? start + (end - start) * static_cast<float> (s.valueToProportionOfLength (0.0)) : start;
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (s.isEnabled() ? colours::blood.brighter (s.isMouseOverOrDragging() ? 0.3f : 0.0f) : colours::goldDim);
        g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // the knob: dark disc, bone rim, subtle top light
        const float kr = r * 0.72f;
        const auto knob = juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (colours::stageLight.brighter (0.25f), c.x, c.y - kr,
                                                 colours::night, c.x, c.y + kr, false));
        g.fillEllipse (knob);
        g.setColour (colours::boneShadow);
        g.drawEllipse (knob, juce::jmax (1.0f, kr * 0.08f));

        // pointer: a bone needle
        juce::Path needle;
        needle.addRoundedRectangle (-kr * 0.07f, -kr * 0.95f, kr * 0.14f, kr * 0.62f, kr * 0.07f);
        g.setColour (colours::bone);
        g.fillPath (needle, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? colours::velvet : colours::night);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? colours::gold : (over ? colours::gold : colours::goldDim));
        g.drawRoundedRectangle (r, 3.0f, down ? 2.0f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
    {
        g.setFont (serif (juce::jmin (15.0f, static_cast<float> (b.getHeight()) * 0.5f), true));
        g.setColour (b.getToggleState() ? colours::bone : (over ? colours::bone : colours::gold));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return serif (15.0f, true); }
    juce::Font getPopupMenuFont() override { return serif (15.0f); }
};
} // namespace ui
