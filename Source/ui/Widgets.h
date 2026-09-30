// File: Source/ui/Widgets.h
// Knobs and buttons bound to the parameters, plus the deep-edit knob groups per drum
// (docs/04-ui-concept.md). A knob shows its name, and its value while hovered or moved;
// double-click returns it to its default.
#pragma once

#include "Theme.h"
#include <plugin_dsp/DrumTypes.h>

#include <memory>
#include <vector>

namespace ui
{
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::String labelText)
        : label (std::move (labelText))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
        {
            slider.setDoubleClickReturnValue (true, static_cast<double> (p->convertFrom0to1 (p->getDefaultValue())));
            unit = p->getLabel();
        }
        slider.onValueChange = [this] { repaint(); };
        slider.addMouseListener (this, false);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int labelH = juce::jmax (14, getHeight() / 5);
        slider.setBounds (r.removeFromTop (getHeight() - labelH));
    }

    void paint (juce::Graphics& g) override
    {
        const int labelH = juce::jmax (14, getHeight() / 5);
        const auto area = getLocalBounds().removeFromBottom (labelH);
        const bool showValue = slider.isMouseOverOrDragging();
        g.setFont (serif (juce::jmin (14.0f, static_cast<float> (labelH) * 0.9f), ! showValue));
        g.setColour (showValue ? colours::bone : colours::gold);
        g.drawFittedText (showValue ? valueText() : label, area, juce::Justification::centredTop, 1);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    juce::Slider slider;

private:
    juce::String valueText() const
    {
        const double v = slider.getValue();
        const bool hz = unit == "Hz";
        juce::String s = hz && v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz"
                                           : juce::String (v, std::abs (v) < 10.0 && ! hz ? 1 : 0) + (unit.isEmpty() ? "" : " " + unit);
        if ((unit == "dB" || (unit == "%" && slider.getMinimum() < 0.0)) && v > 0.0) s = "+" + s;
        return s;
    }

    juce::String label, unit;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** A plaque button that toggles a boolean parameter. */
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

inline juce::String typeTitle (plugin_dsp::DrumType t)
{
    return juce::String (plugin_dsp::drumTypeName (t)).toUpperCase();
}
} // namespace ui
