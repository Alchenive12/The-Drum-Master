// File: dsp/tests/test_midi_map.cpp
// MIDI mode's note map: the General MIDI defaults, several notes per drum, clearing, and the text
// form used in sessions and preset files (round trip, bad input rejected).
#include "dsp_test.h"
#include <plugin_dsp/MidiMap.h>

int main()
{
    using namespace plugin_dsp;
    const auto gm = MidiNoteMap::generalMidi();

    // 1) General MIDI: kick C1 (36), snare D1 (38), hats, cymbals; toms are misc; others ignored.
    DSP_CHECK (gm.drum[36] == static_cast<std::int8_t> (index (DrumType::kick)));
    DSP_CHECK (gm.drum[35] == static_cast<std::int8_t> (index (DrumType::kick)));
    DSP_CHECK (gm.drum[38] == static_cast<std::int8_t> (index (DrumType::snare)));
    DSP_CHECK (gm.drum[37] == static_cast<std::int8_t> (index (DrumType::rimshot)));
    DSP_CHECK (gm.drum[39] == static_cast<std::int8_t> (index (DrumType::clap)));
    DSP_CHECK (gm.drum[42] == static_cast<std::int8_t> (index (DrumType::closedHat)));
    DSP_CHECK (gm.drum[46] == static_cast<std::int8_t> (index (DrumType::openHat)));
    DSP_CHECK (gm.drum[49] == static_cast<std::int8_t> (index (DrumType::crash)));
    DSP_CHECK (gm.drum[51] == static_cast<std::int8_t> (index (DrumType::ride)));
    DSP_CHECK (gm.drum[45] == static_cast<std::int8_t> (index (DrumType::misc)));
    DSP_CHECK (gm.drum[60] == MidiNoteMap::none);

    // 2) Edit: move the snare to 40 only, add a kick on 24, clear the ride.
    auto m = gm;
    m.clear (DrumType::snare);
    m.set (40, DrumType::snare);
    m.set (24, DrumType::kick);
    m.clear (DrumType::ride);
    DSP_CHECK (m.drum[38] == MidiNoteMap::none);
    DSP_CHECK (m.drum[40] == static_cast<std::int8_t> (index (DrumType::snare)));
    DSP_CHECK (m.drum[24] == static_cast<std::int8_t> (index (DrumType::kick)));
    DSP_CHECK (m.drum[51] == MidiNoteMap::none && m.drum[59] == MidiNoteMap::none);
    m.set (200, DrumType::kick);   // out of range: ignored
    m.clear (-3);

    // 3) Text round trip (names with spaces survive), empty map, bad input leaves the target alone.
    MidiNoteMap back;
    DSP_CHECK (MidiNoteMap::fromString (m.toString(), back));
    DSP_CHECK (back == m);
    DSP_CHECK (gm.toString().find ("42=closed_hat") != std::string::npos);
    MidiNoteMap empty = gm;
    DSP_CHECK (MidiNoteMap::fromString ("", empty));
    DSP_CHECK (empty == MidiNoteMap {});
    MidiNoteMap keep = gm;
    for (const char* bad : { "36=bongo", "128=kick", "x=kick", "36kick", "=kick", "1000=kick" })
    {
        DSP_CHECK (! MidiNoteMap::fromString (bad, keep));
        DSP_CHECK (keep == gm);
    }

    DSP_TEST_END;
}
