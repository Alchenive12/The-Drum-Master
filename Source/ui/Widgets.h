// File: Source/ui/Widgets.h
// Knobs, the level fader and buttons bound to the parameters, plus the deep-edit knob groups per drum
// (docs/04-ui-concept.md). A knob shows its name, and its value while hovered or moved;
// double-click returns it to its default.
#pragma once

#include "LookAndFeel.h"

#include <functional>
#include <memory>
#include <vector>

namespace ui
{
inline juce::String formatValue (double v, const juce::String& unit, bool signedPct)
{
    const bool hz = unit == "Hz";
    juce::String s = hz && v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz"
                                       : juce::String (v, std::abs (v) < 10.0 && ! hz ? 1 : 0) + (unit.isEmpty() ? "" : " " + unit);
    if ((unit == "dB" || (unit == "%" && signedPct)) && v > 0.0) s = "+" + s;
    if (v < 0.0) s = juce::String::fromUTF8 ("\xe2\x88\x92") + s.substring (1);   // a real minus sign
    return s;
}

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::String labelText, juce::Colour accent = colours::ecto)
        : label (std::move (labelText))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (knobStart, knobEnd, true);
        slider.setMouseDragSensitivity (220);
        setAccent (slider, accent);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
        {
            slider.setDoubleClickReturnValue (true, static_cast<double> (p->convertFrom0to1 (p->getDefaultValue())));
            unit = p->getLabel();
        }
        slider.onValueChange = [this] {
            repaint();
            if (onValueChange) onValueChange();
        };
        slider.addMouseListener (this, false);
    }

    void setAccentColour (juce::Colour c) { setAccent (slider, c); repaint(); }

    void resized() override
    {
        auto r = getLocalBounds();
        slider.setBounds (r.removeFromTop (getHeight() - labelHeight()));
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().removeFromBottom (labelHeight()).toFloat();
        const bool showValue = slider.isMouseOverOrDragging();
        const float size = juce::jlimit (9.5f, 12.5f, static_cast<float> (getWidth()) * 0.105f);
        if (showValue)
        {
            g.setFont (mono (size + 1.0f));
            g.setColour (accentOf (slider).brighter (0.3f));
            g.drawText (formatValue (slider.getValue(), unit, slider.getMinimum() < 0.0), area, juce::Justification::centredTop, false);
        }
        else
        {
            g.setFont (caps (size, 0.14f));
            g.setColour (colours::boneDim);
            g.drawFittedText (label.toUpperCase(), area.toNearestInt(), juce::Justification::centredTop, 1, 0.8f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    /** Where the pointer's tip is, in this component's parent's coordinates. */
    juce::Point<float> pointerTip()
    {
        const KnobGeometry k (slider.getBounds().toFloat());
        const float p = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
        return fx::polar (k.centre, knobStart + p * (knobEnd - knobStart), k.bodyRadius * 0.82f) + getPosition().toFloat();
    }

    juce::Slider slider;
    std::function<void()> onValueChange;

private:
    int labelHeight() const { return juce::jmax (16, getHeight() / 6); }

    juce::String label, unit;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** A drum's level as a tall fader, with a ghost meter of its hits behind the track. */
class Fader final : public juce::Component
{
public:
    std::function<float()> meter;   // 0..1, the drum's last hit (decaying)

    Fader (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::Colour accent)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setAccent (slider, accent);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
        {
            slider.setDoubleClickReturnValue (true, static_cast<double> (p->convertFrom0to1 (p->getDefaultValue())));
            unit = p->getLabel();
        }
        slider.onValueChange = [this] { repaint(); };
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (56).withTrimmedBottom (14).withSizeKeepingCentre (44, getHeight() - 70)); }

    void paint (juce::Graphics& g) override
    {
        const auto accent = accentOf (slider);
        g.setFont (caps (10.0f, 0.2f));
        g.setColour (colours::boneDim);
        g.drawText ("LEVEL", getLocalBounds().withHeight (18), juce::Justification::centred, false);
        g.setFont (mono (15.0f));
        g.setColour (slider.isMouseOverOrDragging() ? accent.brighter (0.3f) : colours::bone);
        g.drawText (formatValue (slider.getValue(), unit, true), getLocalBounds().withY (20).withHeight (22), juce::Justification::centred, false);

        // scale: dB marks on both sides of the track
        const auto sb = slider.getBounds().toFloat();
        g.setFont (mono (9.5f));
        for (double db : { 12.0, 6.0, 0.0, -6.0, -12.0, -18.0, -24.0 })
        {
            const float y = sb.getY() + static_cast<float> (slider.getPositionOfValue (db));
            const bool zero = std::abs (db) < 0.01;
            g.setColour (juce::Colours::white.withAlpha (zero ? 0.35f : 0.13f));
            g.fillRect (juce::Rectangle<float> (sb.getCentreX() - 17.0f, y - 0.5f, zero ? 8.0f : 5.0f, 1.0f));
            g.fillRect (juce::Rectangle<float> (sb.getCentreX() + (zero ? 9.0f : 12.0f), y - 0.5f, zero ? 8.0f : 5.0f, 1.0f));
            g.setColour (zero ? colours::boneDim : colours::boneFaint);
            g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (sb.getRight() - 2.0f, y - 6.0f, 26.0f, 12.0f), juce::Justification::centredLeft,
                        false);
        }
        // the ghost meter: how hard this drum was last hit
        const float m = meter ? juce::jlimit (0.0f, 1.0f, meter()) : 0.0f;
        if (m > 0.01f)
        {
            const float bottom = sb.getBottom() - 9.0f, top = sb.getY() + 9.0f;
            const auto col = juce::Rectangle<float> (sb.getCentreX() - 9.5f, bottom - (bottom - top) * m * 0.8f, 3.0f, (bottom - top) * m * 0.8f);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.0f), 0.0f, col.getBottom(), accent.withAlpha (0.8f * m), 0.0f, col.getY(), false));
            g.fillRoundedRectangle (col, 1.5f);
        }
    }

    juce::Slider slider;

private:
    juce::String unit;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** A button that toggles a boolean parameter. */
class ParamToggle final : public juce::TextButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& buttonText)
        : juce::TextButton (buttonText)
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
    }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

inline juce::Button& styled (juce::Button& b, const char* style, const char* iconName = "", juce::Colour accent = colours::ecto)
{
    b.getProperties().set ("style", style);
    b.getProperties().set ("icon", iconName);
    setAccent (b, accent);
    return b;
}

// ------------------------------------------------------------------------ deep edit knob groups
struct KnobGroup
{
    const char* title;
    std::vector<std::pair<const char*, const char*>> knobs;   // parameter ID, short label
};

inline std::vector<KnobGroup> deepEditGroups (plugin_dsp::DrumType t)
{
    using plugin_dsp::DrumType;
    switch (t)
    {
        case DrumType::kick:
            return { { "WEIGHT", { { "kick_lowboost", "Low-end boost" }, { "kick_bottom", "Bottom" }, { "kick_top", "Top" } } },
                     { "DYNAMICS", { { "kick_decay", "Decay" }, { "kick_comp", "Compression" }, { "kick_sat", "Saturation" }, { "kick_clip", "Clipping" } } },
                     { "EQ", { { "kick_eq_low", "Low" }, { "kick_eq_mid", "Mid" }, { "kick_eq_midfreq", "Mid freq" }, { "kick_eq_high", "High" } } },
                     { "LEVEL", { { "kick_level", "Level" } } } };
        case DrumType::snare:
            return { { "CHARACTER", { { "snare_dubstepify", "Dubstepify" }, { "snare_popify", "Popify" }, { "snare_rockify", "Rockify" }, { "snare_rimify", "Rimify" } } },
                     { "BODY & CRACK", { { "snare_crack", "Crack" }, { "snare_body", "Body" }, { "snare_decay", "Decay" } } },
                     { "DYNAMICS", { { "snare_comp", "Compression" }, { "snare_sat", "Saturation" } } },
                     { "EQ", { { "snare_eq_low", "Low" }, { "snare_eq_mid", "Mid" }, { "snare_eq_midfreq", "Mid freq" }, { "snare_eq_high", "High" } } },
                     { "LEVEL", { { "snare_level", "Level" } } } };
        case DrumType::closedHat:
            return { { "SOUND", { { "chat_tone", "Tone" }, { "chat_tick", "Tick" }, { "chat_decay", "Decay" }, { "chat_deharsh", "De-harsh" }, { "chat_width", "Width" } } },
                     { "LEVEL", { { "chat_level", "Level" } } } };
        case DrumType::openHat:
            return { { "SOUND", { { "ohat_tone", "Tone" }, { "ohat_body", "Body" }, { "ohat_decay", "Decay" }, { "ohat_deharsh", "De-harsh" } } },
                     { "COLOUR & SPACE", { { "ohat_sat", "Saturation" }, { "ohat_width", "Width" } } },
                     { "LEVEL", { { "ohat_level", "Level" } } } };
        case DrumType::crash:
            return { { "SOUND", { { "crash_tone", "Tone" }, { "crash_body", "Body" }, { "crash_decay", "Decay" }, { "crash_deharsh", "De-harsh" } } },
                     { "SPACE", { { "crash_lowcut", "Low cut" }, { "crash_width", "Width" } } },
                     { "LEVEL", { { "crash_level", "Level" } } } };
        case DrumType::ride:
            return { { "SOUND", { { "ride_ping", "Ping" }, { "ride_bell", "Bell" }, { "ride_wash", "Wash" }, { "ride_tone", "Tone" } } },
                     { "SPACE", { { "ride_width", "Width" } } },
                     { "LEVEL", { { "ride_level", "Level" } } } };
        case DrumType::rimshot:
            return { { "SOUND", { { "rim_click", "Click" }, { "rim_body", "Body" }, { "rim_tone", "Tone" }, { "rim_decay", "Decay" } } },
                     { "COLOUR", { { "rim_sat", "Saturation" } } },
                     { "LEVEL", { { "rim_level", "Level" } } } };
        case DrumType::clap:
            return { { "SOUND", { { "clap_attack", "Attack" }, { "clap_body", "Body" }, { "clap_tone", "Tone" }, { "clap_decay", "Decay" } } },
                     { "SPACE & DYNAMICS", { { "clap_width", "Width" }, { "clap_comp", "Compression" }, { "clap_sat", "Saturation" } } },
                     { "LEVEL", { { "clap_level", "Level" } } } };
        case DrumType::misc:
            return { { "SOUND", { { "misc_tone", "Tone" }, { "misc_attack", "Attack" }, { "misc_decay", "Decay" } } },
                     { "LEVEL", { { "misc_level", "Level" } } } };
    }
    return {};
}

/** "KICK", "CLOSED HAT" ... (labels). */
inline juce::String typeTitle (plugin_dsp::DrumType t)
{
    return juce::String (plugin_dsp::drumTypeName (t)).toUpperCase();
}

/** "Kick", "Closed hat" ... (display names). */
inline juce::String typeName (plugin_dsp::DrumType t)
{
    const juce::String n (plugin_dsp::drumTypeName (t));
    return n.substring (0, 1).toUpperCase() + n.substring (1);
}

/** One whispered line per drum, under its name in the deep edit. */
inline juce::String typeWhisper (plugin_dsp::DrumType t)
{
    using plugin_dsp::DrumType;
    switch (t)
    {
        case DrumType::kick:      return "the heartbeat under the floor";
        case DrumType::snare:     return "a crack, like a bone";
        case DrumType::closedHat: return "the clock that never stops";
        case DrumType::openHat:   return "a long breath in the dark";
        case DrumType::crash:     return "the scream at the end of the hall";
        case DrumType::ride:      return "a bell, somewhere, at midnight";
        case DrumType::rimshot:   return "a knock at the door";
        case DrumType::clap:      return "applause from an empty room";
        case DrumType::misc:      return "something else is in the room";
    }
    return {};
}
} // namespace ui
