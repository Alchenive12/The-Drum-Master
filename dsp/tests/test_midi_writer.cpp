// File: dsp/tests/test_midi_writer.cpp
// MIDI write: which note each drum is written on, misc off by default and on a free note when asked,
// velocity from level, exact (unquantized) timing from the origin, time order.
#include "dsp_test.h"
#include <plugin_dsp/MidiWriter.h>

int main()
{
    using namespace plugin_dsp;
    const auto gm = MidiNoteMap::generalMidi();

    // 1) Notes: the usual GM note (kick 36 although 35 is also a kick), else the lowest mapped note.
    DSP_CHECK (writeNoteFor (gm, DrumType::kick) == 36);
    DSP_CHECK (writeNoteFor (gm, DrumType::snare) == 38);
    DSP_CHECK (writeNoteFor (gm, DrumType::closedHat) == 42);
    DSP_CHECK (writeNoteFor (gm, DrumType::openHat) == 46);
    DSP_CHECK (writeNoteFor (gm, DrumType::crash) == 49);
    DSP_CHECK (writeNoteFor (gm, DrumType::ride) == 51);
    DSP_CHECK (writeNoteFor (gm, DrumType::rimshot) == 37);
    DSP_CHECK (writeNoteFor (gm, DrumType::clap) == 39);
    auto custom = gm;
    custom.clear (DrumType::kick);
    custom.set (24, DrumType::kick);
    custom.set (30, DrumType::kick);
    custom.clear (DrumType::clap);
    DSP_CHECK (writeNoteFor (custom, DrumType::kick) == 24);
    DSP_CHECK (writeNoteFor (custom, DrumType::clap) == MidiNoteMap::none);

    // 2) Misc's free note: 34, unless the map uses it.
    DSP_CHECK (freeMiscNote (gm) == 34);
    custom.set (34, DrumType::snare);
    DSP_CHECK (freeMiscNote (custom) == 33);

    // 3) A take: exact timing from the origin, misc skipped by default, velocity by level per drum,
    //    sorted in time, drums without notes skipped.
    const std::vector<WrittenHit> take {
        { 8.51, DrumType::snare, -6.0f }, { 8.0, DrumType::kick, -3.0f }, { 9.003, DrumType::kick, -18.0f },
        { 8.25, DrumType::misc, -10.0f }, { 8.75, DrumType::clap, -9.0f }, { 9.5, DrumType::kick, -60.0f },
    };
    const auto notes = toNotes (take, gm, false, 8.0);
    DSP_CHECK (notes.size() == 5);
    if (notes.size() == 5)
    {
        DSP_CHECK (notes[0].note == 36 && notes[0].velocity == 127);
        DSP_CHECK_NEAR (notes[0].ppq, 0.0, 1e-12);
        DSP_CHECK (notes[1].note == 38 && notes[1].velocity == 127);   // the only snare: its loudest
        DSP_CHECK_NEAR (notes[1].ppq, 0.51, 1e-12);                    // not snapped to 0.5
        DSP_CHECK (notes[2].note == 39);
        DSP_CHECK (notes[3].note == 36 && notes[3].velocity == 64);    // 15 dB under the loudest kick
        DSP_CHECK_NEAR (notes[3].ppq, 1.003, 1e-12);
        DSP_CHECK (notes[4].velocity == 1);                            // 57 dB under: floor
    }
    const auto withMisc = toNotes (take, gm, true, 8.0);
    DSP_CHECK (withMisc.size() == 6);
    if (withMisc.size() == 6) DSP_CHECK (withMisc[1].note == 34 && withMisc[1].velocity == 127);
    DSP_CHECK (toNotes (take, custom, false, 8.0).size() == 4);   // custom map: no clap notes

    DSP_TEST_END;
}
