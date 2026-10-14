// File: dsp/include/plugin_dsp/MidiWriter.h
// MIDI write (docs/05-features.md F3b): turns the recorded hits into drum notes for a .mid file.
//   note:     the drum's usual General MIDI note if the map has it (kick 36, snare 38, ...), else the
//             drum's lowest mapped note; a drum without notes isn't written.
//   misc:     not written, unless asked; then on a note no default kit uses (34, or the next free one below).
//   velocity: from the hit's peak level, against the loudest hit of the same drum in the take
//             (that one is 127; 30 dB quieter is 1).
//   timing:   exactly as detected (no quantization), in quarter notes from `originPpq`.
#pragma once

#include "MidiMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace plugin_dsp
{
struct WrittenHit
{
    double ppq = 0.0;   // host position of the onset, in quarter notes
    DrumType type = DrumType::misc;
    float peakDb = -120.0f;
};

struct WrittenNote
{
    double ppq = 0.0;   // from the origin
    int note = 0, velocity = 0;
};

/** The note a drum is written on, or MidiNoteMap::none. */
inline int writeNoteFor (const MidiNoteMap& map, DrumType t) noexcept
{
    constexpr std::array<int, numDrumTypes> usual { 36, 38, 42, 46, 49, 51, 37, 39, 45 };   // DrumType order
    const auto want = static_cast<std::int8_t> (index (t));
    if (map.drum[static_cast<std::size_t> (usual[index (t)])] == want) return usual[index (t)];
    for (int n = 0; n < 128; ++n)
        if (map.drum[static_cast<std::size_t> (n)] == want) return n;
    return MidiNoteMap::none;
}

/** A note for misc that no default kit uses: 34 (just below the General MIDI drums), or the next
    note down that the map doesn't use either. */
inline int freeMiscNote (const MidiNoteMap& map) noexcept
{
    for (int n = 34; n >= 0; --n)
        if (map.drum[static_cast<std::size_t> (n)] == MidiNoteMap::none) return n;
    return MidiNoteMap::none;
}

inline std::vector<WrittenNote> toNotes (const std::vector<WrittenHit>& hits, const MidiNoteMap& map, bool includeMisc, double originPpq)
{
    std::array<float, numDrumTypes> loudest {};
    loudest.fill (-1000.0f);
    for (const auto& h : hits) loudest[index (h.type)] = std::max (loudest[index (h.type)], h.peakDb);

    std::vector<WrittenNote> out;
    for (const auto& h : hits)
    {
        const bool misc = h.type == DrumType::misc;
        if (misc && ! includeMisc) continue;
        const int note = misc ? freeMiscNote (map) : writeNoteFor (map, h.type);
        if (note == MidiNoteMap::none) continue;
        const float v = 127.0f + (h.peakDb - loudest[index (h.type)]) * (126.0f / 30.0f);
        out.push_back ({ h.ppq - originPpq, note, static_cast<int> (std::lround (std::clamp (v, 1.0f, 127.0f))) });
    }
    std::stable_sort (out.begin(), out.end(), [] (const WrittenNote& a, const WrittenNote& b) { return a.ppq < b.ppq; });
    return out;
}
} // namespace plugin_dsp
