// File: Source/ui/Theme.h
// Colours and type of The Drum Master (docs/04-ui-concept.md, "Look").
// Moonlit horror: blue-black ink, cold moonlight, porcelain bone, and light that means something -
// ectoplasm green for things that are alive (listening, the playhead), one hue per drum type, blood
// red for danger (bypass, cutting a string) and warm limelight for The Puppet Show.
// Type: Instrument Serif for names and titles, Geist for the interface, Geist Mono for numbers
// (all SIL OFL 1.1, embedded from FontData.h, so the plugin looks the same on every machine).
#pragma once

#include "FontData.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <plugin_dsp/DrumTypes.h>

namespace ui
{
namespace colours
{
const juce::Colour ink        { 0xff05060a };   // the deepest blue-black
const juce::Colour night      { 0xff090b10 };
const juce::Colour deep       { 0xff0f1219 };
const juce::Colour slate      { 0xff1a1f29 };
const juce::Colour slateLight { 0xff2b323f };
const juce::Colour bone       { 0xffede6d8 };   // porcelain: text, the master
const juce::Colour boneDim    { 0xffa29d92 };   // secondary text
const juce::Colour boneFaint  { 0xff5f5d58 };   // tertiary text, hairlines
const juce::Colour moon       { 0xffb6cce6 };   // cold moonlight
const juce::Colour moonDeep   { 0xff34496a };
const juce::Colour ecto       { 0xff7ff5cf };   // alive: listening, playhead, the crack in the mask
const juce::Colour blood      { 0xffff3d57 };   // danger, bypass
const juce::Colour bloodDeep  { 0xff4d0813 };
const juce::Colour velvet     { 0xff6a0d1c };
const juce::Colour velvetDark { 0xff1a0307 };
const juce::Colour brass      { 0xffd2b682 };
const juce::Colour brassDim   { 0xff6d5b3d };
const juce::Colour limelight  { 0xffffc98c };   // The Puppet Show: boosts
const juce::Colour frost      { 0xff8cb6ff };   // The Puppet Show: cuts
const juce::Colour wood       { 0xff24180f };
const juce::Colour woodLight  { 0xff5b4331 };
const juce::Colour string     { 0xffd9d2c3 };
// the late-night room
const juce::Colour wall       { 0xff151a23 };
const juce::Colour wallDark   { 0xff07090d };
const juce::Colour floorBoard { 0xff11141a };
const juce::Colour oldMetal   { 0xff737d8b };
const juce::Colour oldShell   { 0xff232831 };
} // namespace colours

/** One hue per drum type: arcs in its deep edit, its glow on stage, its shape in Shadow Play. */
inline juce::Colour drumColour (plugin_dsp::DrumType t)
{
    using plugin_dsp::DrumType;
    switch (t)
    {
        case DrumType::kick:      return juce::Colour (0xffff4a5f);   // blood
        case DrumType::snare:     return juce::Colour (0xfff2e3c4);   // bone
        case DrumType::closedHat: return juce::Colour (0xff7ff5cf);   // ectoplasm
        case DrumType::openHat:   return juce::Colour (0xff5cd9ec);   // cold cyan
        case DrumType::crash:     return juce::Colour (0xffbba4ff);   // bruise violet
        case DrumType::ride:      return juce::Colour (0xff8db3ff);   // midnight blue
        case DrumType::rimshot:   return juce::Colour (0xffffb766);   // candle amber
        case DrumType::clap:      return juce::Colour (0xffff8db5);   // rose
        case DrumType::misc:      return juce::Colour (0xff9ea4ae);   // ash
    }
    return colours::bone;
}

// ------------------------------------------------------------------------------------------ type
/** The embedded typefaces. A DeletedAtShutdown singleton (not a function-local static), so they
    are released with JUCE's own shutdown, never after it when a host unloads the plugin. */
class Fonts final : private juce::DeletedAtShutdown
{
public:
    static const Fonts& get() { return *getInstance(); }

    juce::Typeface::Ptr display, displayItalic, sans, sansBold, mono;

    ~Fonts() override { clearSingletonInstance(); }
    Fonts()
        : display (make (fontdata::displayRegular, sizeof (fontdata::displayRegular))),
          displayItalic (make (fontdata::displayItalic, sizeof (fontdata::displayItalic))),
          sans (make (fontdata::sansRegular, sizeof (fontdata::sansRegular))),
          sansBold (make (fontdata::sansSemiBold, sizeof (fontdata::sansSemiBold))),
          mono (make (fontdata::monoMedium, sizeof (fontdata::monoMedium)))
    {
    }

    JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE (Fonts, false)

private:
    static juce::Typeface::Ptr make (const unsigned char* data, std::size_t size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, size);
    }
};

inline juce::Font makeFont (const juce::Typeface::Ptr& face, float height, float tracking = 0.0f)
{
    if (face == nullptr)   // never expected; keeps the editor usable if a typeface failed to load
        return juce::Font (juce::FontOptions (height)).withExtraKerningFactor (tracking);
    return juce::Font (juce::FontOptions (face).withHeight (height).withKerningFactor (tracking));
}

/** Instrument Serif: drum names, page titles. */
inline juce::Font display (float height) { return makeFont (Fonts::get().display, height, -0.01f); }
/** Instrument Serif Italic: whispers ("the master is listening..."). */
inline juce::Font whisper (float height) { return makeFont (Fonts::get().displayItalic, height); }
/** Geist: running text. */
inline juce::Font sans (float height, bool bold = false) { return makeFont (bold ? Fonts::get().sansBold : Fonts::get().sans, height); }
/** Geist SemiBold, tracked: small caps labels and buttons (pass upper-case text). */
inline juce::Font caps (float height, float tracking = 0.16f) { return makeFont (Fonts::get().sansBold, height, tracking); }
/** Geist Mono: values, readouts. */
inline juce::Font mono (float height) { return makeFont (Fonts::get().mono, height, 0.02f); }

inline float textWidth (const juce::Font& f, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (f, text);
}
} // namespace ui
