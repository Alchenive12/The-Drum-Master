// File: dsp/include/plugin_dsp/MidiMap.h
// MIDI mode (docs/05-features.md F3): which drum each MIDI note stands for. A note can name one drum
// or none (ignored); a drum can have any number of notes (kick on 35 and 36). The default is the
// General MIDI drum map; toms (41-50) go to misc. Saved as text, "36=kick 38=snare ...", in the
// session and in preset files (Source/PluginProcessor.cpp).
#pragma once

#include "DrumTypes.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <string>

namespace plugin_dsp
{
struct MidiNoteMap
{
    static constexpr int none = -1;
    std::array<std::int8_t, 128> drum {};   // drum type index per note, or none

    MidiNoteMap() { drum.fill (static_cast<std::int8_t> (none)); }

    void set (int note, DrumType t) noexcept { if (note >= 0 && note < 128) drum[static_cast<std::size_t> (note)] = static_cast<std::int8_t> (index (t)); }
    void clear (int note) noexcept { if (note >= 0 && note < 128) drum[static_cast<std::size_t> (note)] = static_cast<std::int8_t> (none); }
    /** Remove every note of a drum. */
    void clear (DrumType t) noexcept
    {
        for (auto& d : drum)
            if (d == static_cast<std::int8_t> (index (t))) d = static_cast<std::int8_t> (none);
    }

    static MidiNoteMap generalMidi()
    {
        MidiNoteMap m;
        const std::pair<int, DrumType> gm[] = {
            { 35, DrumType::kick },      { 36, DrumType::kick },                                     // acoustic bass drum, bass drum 1
            { 37, DrumType::rimshot },                                                                // side stick
            { 38, DrumType::snare },     { 40, DrumType::snare },                                    // acoustic, electric snare
            { 39, DrumType::clap },
            { 42, DrumType::closedHat }, { 44, DrumType::closedHat },                                // closed, pedal hi-hat
            { 46, DrumType::openHat },
            { 49, DrumType::crash },     { 57, DrumType::crash },     { 52, DrumType::crash }, { 55, DrumType::crash },   // crash 1/2, chinese, splash
            { 51, DrumType::ride },      { 59, DrumType::ride },      { 53, DrumType::ride },     // ride 1/2, ride bell
            { 41, DrumType::misc },      { 43, DrumType::misc },      { 45, DrumType::misc },     // toms
            { 47, DrumType::misc },      { 48, DrumType::misc },      { 50, DrumType::misc },
        };
        for (const auto& [note, t] : gm) m.set (note, t);
        return m;
    }

    bool operator== (const MidiNoteMap& o) const noexcept { return drum == o.drum; }
    bool operator!= (const MidiNoteMap& o) const noexcept { return drum != o.drum; }

    /** "36=kick 38=snare 42=closed_hat ..." (ascending notes; spaces in names become '_'). */
    std::string toString() const
    {
        std::string s;
        for (std::size_t n = 0; n < drum.size(); ++n)
        {
            if (drum[n] == static_cast<std::int8_t> (none)) continue;
            std::string name = drumTypeName (static_cast<DrumType> (drum[n]));
            for (auto& c : name) if (c == ' ') c = '_';
            if (! s.empty()) s += ' ';
            s += std::to_string (n) + '=' + name;
        }
        return s;
    }

    /** Parses toString(); false (and out untouched) on any bad entry. An empty string is an empty map. */
    static bool fromString (const std::string& text, MidiNoteMap& out)
    {
        MidiNoteMap m;
        std::istringstream in (text);
        std::string item;
        while (in >> item)
        {
            const auto eq = item.find ('=');
            if (eq == std::string::npos || eq == 0) return false;
            int note = 0;
            for (std::size_t i = 0; i < eq; ++i)
            {
                if (item[i] < '0' || item[i] > '9' || note > 127) return false;
                note = note * 10 + (item[i] - '0');
            }
            if (note > 127) return false;
            std::string name = item.substr (eq + 1);
            for (auto& c : name) if (c == '_') c = ' ';
            bool found = false;
            for (std::size_t t = 0; t < numDrumTypes && ! found; ++t)
                if (name == drumTypeName (static_cast<DrumType> (t)))
                {
                    m.set (note, static_cast<DrumType> (t));
                    found = true;
                }
            if (! found) return false;
        }
        out = m;
        return true;
    }
};
} // namespace plugin_dsp
