// File: dsp/tests/test_drum_mapping.cpp
// Knobs -> processor settings: defaults are neutral everywhere, each knob only touches its own
// drum(s), group volumes stack on levels, macros stacked to the max stay bounded.
#include "dsp_test.h"
#include <plugin_dsp/DrumMapping.h>

namespace
{
using namespace plugin_dsp;

/** Bitmask of the drum types whose chain is not neutral. */
std::uint32_t touched (const DrumParams& p)
{
    const auto s = mapParams (p);
    std::uint32_t m = 0;
    for (std::size_t t = 0; t < numDrumTypes; ++t)
        if (! s[t].isNeutral()) m |= 1u << t;
    return m;
}
} // namespace

int main()
{
    // 1) A fresh instance is transparent.
    DSP_CHECK (touched (DrumParams {}) == 0u);

    // 2) Each knob touches only its own drum (or its quick-mix group).
    {
        const std::uint32_t snareGroup = bit (DrumType::snare) | bit (DrumType::rimshot) | bit (DrumType::clap);
        const std::uint32_t hatsGroup = bit (DrumType::closedHat) | bit (DrumType::openHat) | bit (DrumType::crash) | bit (DrumType::ride);
        struct Case { const char* name; void (*set) (DrumParams&); std::uint32_t expect; };
        const Case cases[] = {
            { "kick_top", [] (DrumParams& p) { p.kick.top = 3; }, bit (DrumType::kick) },
            { "kick_clip", [] (DrumParams& p) { p.kick.clip = 50; }, bit (DrumType::kick) },
            { "snare_rimify", [] (DrumParams& p) { p.snare.rimify = 50; }, bit (DrumType::snare) },
            { "chat_width", [] (DrumParams& p) { p.closedHat.width = 150; }, bit (DrumType::closedHat) },
            { "crash_lowcut", [] (DrumParams& p) { p.crash.lowCut = 300; }, bit (DrumType::crash) },
            { "ride_wash", [] (DrumParams& p) { p.ride.wash = -4; }, bit (DrumType::ride) },
            { "clap_comp", [] (DrumParams& p) { p.clap.comp = 40; }, bit (DrumType::clap) },
            { "misc_decay", [] (DrumParams& p) { p.misc.decay = -30; }, bit (DrumType::misc) },
            { "qm_kick_weight", [] (DrumParams& p) { p.quick.kickWeight = 50; }, bit (DrumType::kick) },
            { "qm_snare_volume", [] (DrumParams& p) { p.quick.snareVolume = -3; }, snareGroup },
            { "qm_snare_fat", [] (DrumParams& p) { p.quick.snareFat = 40; }, snareGroup },
            { "qm_hats_tame", [] (DrumParams& p) { p.quick.hatsTame = 60; }, hatsGroup },
            { "qm_hats_tone", [] (DrumParams& p) { p.quick.hatsTone = -60; }, hatsGroup },
            { "global_punch", [] (DrumParams& p) { p.global.punch = 50; }, allDrumTypes & ~bit (DrumType::misc) },
        };
        for (const auto& c : cases)
        {
            DrumParams p;
            c.set (p);
            const auto got = touched (p);
            if (got != c.expect) std::printf ("%s touched 0x%03x, expected 0x%03x\n", c.name, got, c.expect);
            DSP_CHECK (got == c.expect);
        }
    }

    // 3) Levels stack: drum level + group volume.
    {
        DrumParams p;
        p.clap.level = -2;
        p.quick.snareVolume = 5;
        DSP_CHECK_NEAR (mapParams (p)[index (DrumType::clap)].levelDb, 3.0, 1e-6);
    }

    // 4) All four macros at 100 % plus every related knob at the max: everything stays in range.
    {
        DrumParams p;
        p.snare.dubstepify = p.snare.popify = p.snare.rockify = p.snare.rimify = 100;
        p.snare.crack = p.snare.decay = p.snare.comp = p.snare.sat = 100;
        p.snare.body = 12;
        p.quick.snareFat = p.quick.snareSnap = 100;
        const auto s = mapParams (p)[index (DrumType::snare)];
        DSP_CHECK (s.attack <= 1.0f && s.decay <= 1.0f && s.comp <= 1.0f && s.sat <= 1.0f && s.clip <= 1.0f);
        for (const auto& e : s.eq) DSP_CHECK (e.gainDb >= -18.0f && e.gainDb <= 18.0f);
        // one macro alone is clearly audible, and the stack is less than the plain sum
        DrumParams one;
        one.snare.dubstepify = 100;
        const auto d = mapParams (one)[index (DrumType::snare)];
        DSP_CHECK (d.eq[3].gainDb > 3.0f && d.sat > 0.4f && d.clip > 0.4f && d.decay > 0.4f);
        DrumParams two = one;
        two.snare.rockify = 100;
        const auto dr = mapParams (two)[index (DrumType::snare)];
        DSP_CHECK (dr.eq[3].gainDb < 7.0f && dr.eq[3].gainDb > d.eq[3].gainDb);
    }

    // 5) Opposite macro moves cancel instead of stacking (Rimify cuts the body Dubstepify adds).
    {
        DrumParams p;
        p.snare.rimify = 100;
        DSP_CHECK (mapParams (p)[index (DrumType::snare)].eq[3].gainDb < -3.0f);
        DSP_CHECK (mapParams (p)[index (DrumType::snare)].decay < -0.3f);
    }
    DSP_TEST_END;
}
