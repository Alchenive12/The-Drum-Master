// File: dsp/include/plugin_dsp/DrumMapping.h
// Knobs -> processor settings: turns DrumParams (what the user sees) into one ChannelSettings per
// drum type (what DrumChannel runs). All the hidden curves live here, including the snare macros
// and the quick-mix character knobs. Their values are decision D-C11 in
// docs/03-decisions-for-review.md. Every knob at its default gives a neutral chain.
// Pure function, no allocation; call once per block.
#pragma once

#include "DrumChannel.h"
#include "DrumParams.h"
#include "DrumTypes.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace plugin_dsp
{
namespace mapping
{
inline float pct (float v) noexcept { return v / 100.0f; }   // -100..100 % -> -1..1 (0..100 -> 0..1)

/** Soft limit: ~linear for small x, never beyond +-limit (macro stacking stays sane). */
inline float soft (float x, float limit) noexcept { return limit * std::tanh (x / limit); }

inline EqSlot bell (float f, float q, float db) noexcept { return { EqSlot::Kind::bell, f, q, std::clamp (db, -18.0f, 18.0f) }; }
inline EqSlot lowShelf (float f, float db) noexcept { return { EqSlot::Kind::lowShelf, f, 0.707f, std::clamp (db, -18.0f, 18.0f) }; }
inline EqSlot highShelf (float f, float db) noexcept { return { EqSlot::Kind::highShelf, f, 0.707f, std::clamp (db, -18.0f, 18.0f) }; }
inline EqSlot highPass (float f) noexcept { return { EqSlot::Kind::highPass, f, 0.707f, 0.0f }; }

/** Tone = tilt around 1 kHz: the top end moves by +tone/2 dB, the bottom by -tone/2 (two slots). */
inline void tilt (ChannelSettings& s, std::size_t slot, float toneDb) noexcept
{
    s.eq[slot] = lowShelf (1000.0f, -0.5f * toneDb);
    s.eq[slot + 1] = highShelf (1000.0f, 0.5f * toneDb);
}

/** De-harsh: a cut in the harsh 5-9 kHz region, up to -8 dB. */
inline EqSlot deharsh (float amount01, float f = 7000.0f) noexcept { return bell (f, 1.0f, -8.0f * std::clamp (amount01, 0.0f, 1.0f)); }

inline float unit (float x) noexcept { return std::clamp (x, -1.0f, 1.0f); }
inline float amount (float x) noexcept { return std::clamp (x, 0.0f, 1.0f); }
} // namespace mapping

inline std::array<ChannelSettings, numDrumTypes> mapParams (const DrumParams& p) noexcept
{
    using namespace mapping;
    std::array<ChannelSettings, numDrumTypes> out {};
    const float globalPunch = 0.4f * amount (pct (p.global.punch));   // Punch on the whole bus: more attack on every hit

    // ---- Kick: quick mix Punch = attack (+ a little compression), Weight = sub/low shelf.
    {
        const auto& k = p.kick;
        const float punch = pct (p.quick.kickPunch), weight = pct (p.quick.kickWeight);
        ChannelSettings& s = out[index (DrumType::kick)];
        s.eq[0] = lowShelf (50.0f, k.lowBoost + 6.0f * weight);   // sub below ~60 Hz
        s.eq[1] = bell (90.0f, 0.9f, k.bottom);                   // body 60-150 Hz
        s.eq[2] = bell (3500.0f, 0.9f, k.top);                    // beater click
        s.eq[3] = lowShelf (100.0f, k.eqLow);
        s.eq[4] = bell (std::clamp (k.eqMidFreq, 200.0f, 5000.0f), 1.0f, k.eqMid);
        s.eq[5] = highShelf (8000.0f, k.eqHigh);
        s.attackMs = 12.0f;
        s.attack = unit (0.6f * punch + globalPunch);
        s.decay = unit (pct (k.decay));
        s.comp = amount (pct (k.comp) + 0.3f * std::max (0.0f, punch));
        s.compAttackMs = 8.0f;   // lets the beater through
        s.sat = amount (pct (k.sat));
        s.clip = amount (pct (k.clip));
        s.levelDb = k.level + p.quick.kickVolume;
    }

    // ---- Snare group character (quick mix): Snap = attack + air, Fat = body + longer tail.
    const float snap = pct (p.quick.snareSnap), fat = pct (p.quick.snareFat);

    // ---- Snare with the four blendable macros.
    {
        const auto& n = p.snare;
        const float dub = amount (pct (n.dubstepify)), pop = amount (pct (n.popify));
        const float rock = amount (pct (n.rockify)), rim = amount (pct (n.rimify));
        // Macro contributions add up, then a soft limit keeps all four stacked from going extreme.
        const float mBody = soft (4.0f * dub + 3.0f * rock - 5.0f * rim, 6.0f);   // ~200 Hz
        const float mLowMid = soft (-4.0f * pop, 6.0f);                          // 400 Hz mud
        const float mWood = soft (3.0f * rim, 6.0f);                             // 700 Hz wood
        const float mCrack = soft (4.0f * rim + 1.5f * dub, 6.0f);               // 3 kHz
        const float mAir = soft (3.0f * pop, 6.0f);                              // 8 kHz shelf
        const float mAttack = soft (0.3f * pop + 0.5f * rim + 0.4f * dub, 0.7f);
        const float mDecay = soft (-0.3f * pop + 0.4f * rock - 0.5f * rim + 0.6f * dub, 0.7f);
        const float mComp = soft (0.4f * pop + 0.25f * rock + 0.7f * dub, 0.8f);
        const float mSat = soft (0.3f * rock + 0.6f * dub, 0.8f);
        const float mClip = 0.5f * dub;

        ChannelSettings& s = out[index (DrumType::snare)];
        s.eq[0] = lowShelf (100.0f, n.eqLow);
        s.eq[1] = bell (std::clamp (n.eqMidFreq, 200.0f, 5000.0f), 1.0f, n.eqMid);
        s.eq[2] = highShelf (8000.0f, n.eqHigh + mAir + 3.0f * snap);
        s.eq[3] = bell (200.0f, 1.0f, n.body + mBody + 5.0f * fat);
        s.eq[4] = bell (400.0f, 1.2f, mLowMid);
        s.eq[5] = bell (700.0f, 1.4f, mWood);
        s.eq[6] = bell (3000.0f, 1.0f, mCrack);
        s.attackMs = 8.0f;
        s.attack = unit (pct (n.crack) + mAttack + 0.6f * snap + globalPunch);
        s.decay = unit (pct (n.decay) + mDecay + 0.3f * fat);
        s.comp = amount (pct (n.comp) + mComp);
        s.compAttackMs = 5.0f;
        s.sat = amount (pct (n.sat) + mSat + 0.2f * std::max (0.0f, fat));
        s.clip = amount (mClip);
        s.levelDb = n.level + p.quick.snareVolume;
    }

    // ---- Rimshot (snare group).
    {
        const auto& r = p.rim;
        ChannelSettings& s = out[index (DrumType::rimshot)];
        s.eq[0] = bell (400.0f, 1.0f, r.body + 3.0f * fat);
        tilt (s, 1, r.tone);
        s.eq[3] = highShelf (8000.0f, 2.0f * snap);
        s.attackMs = 5.0f;
        s.attack = unit (pct (r.click) + 0.4f * snap + globalPunch);
        s.decay = unit (pct (r.decay) + 0.2f * fat);
        s.sat = amount (pct (r.sat));
        s.levelDb = r.level + p.quick.snareVolume;
    }

    // ---- Clap (snare group).
    {
        const auto& c = p.clap;
        ChannelSettings& s = out[index (DrumType::clap)];
        s.eq[0] = bell (350.0f, 1.0f, c.body + 4.0f * fat);
        tilt (s, 1, c.tone);
        s.eq[3] = highShelf (8000.0f, 2.0f * snap);
        s.attackMs = 10.0f;   // spans the bursts
        s.attack = unit (pct (c.attack) + 0.4f * snap + globalPunch);
        s.decay = unit (pct (c.decay) + 0.3f * fat);
        s.comp = amount (pct (c.comp));
        s.compAttackMs = 3.0f;
        s.sat = amount (pct (c.sat));
        s.width = std::clamp (pct (c.width), 0.0f, 2.0f);
        s.levelDb = c.level + p.quick.snareVolume;
    }

    // ---- Hats group character (quick mix): Tone = tilt +-3 dB, Tame = de-harsh + shorter decay.
    const float hatsTone = 3.0f * pct (p.quick.hatsTone), tame = amount (pct (p.quick.hatsTame));

    {
        const auto& h = p.closedHat;
        ChannelSettings& s = out[index (DrumType::closedHat)];
        tilt (s, 0, h.tone + hatsTone);
        s.eq[2] = deharsh (pct (h.deharsh) + 0.5f * tame, 8000.0f);
        s.attackMs = 3.0f;
        s.attack = unit (pct (h.tick) + globalPunch);
        s.decay = unit (pct (h.decay) - 0.4f * tame);
        s.width = std::clamp (pct (h.width), 0.0f, 2.0f);
        s.levelDb = h.level + p.quick.hatsVolume;
    }
    {
        const auto& h = p.openHat;
        ChannelSettings& s = out[index (DrumType::openHat)];
        tilt (s, 0, h.tone + hatsTone);
        s.eq[2] = bell (900.0f, 0.8f, h.body);
        s.eq[3] = deharsh (pct (h.deharsh) + 0.5f * tame);
        s.attackMs = 5.0f;
        s.attack = unit (globalPunch);
        s.decay = unit (pct (h.decay) - 0.4f * tame);
        s.sat = amount (pct (h.sat));
        s.width = std::clamp (pct (h.width), 0.0f, 2.0f);
        s.levelDb = h.level + p.quick.hatsVolume;
    }
    {
        const auto& c = p.crash;
        ChannelSettings& s = out[index (DrumType::crash)];
        tilt (s, 0, c.tone + hatsTone);
        s.eq[2] = bell (700.0f, 0.8f, c.body);
        s.eq[3] = deharsh (pct (c.deharsh) + 0.5f * tame);
        s.eq[4] = highPass (std::clamp (c.lowCut, 20.0f, 800.0f));
        s.attackMs = 8.0f;
        s.attack = unit (globalPunch);
        s.decay = unit (pct (c.decay) - 0.4f * tame);
        s.width = std::clamp (pct (c.width), 0.0f, 2.0f);
        s.levelDb = c.level + p.quick.hatsVolume;
    }
    {
        const auto& r = p.ride;
        ChannelSettings& s = out[index (DrumType::ride)];
        tilt (s, 0, r.tone + hatsTone);
        s.eq[2] = bell (1200.0f, 1.4f, r.bell);
        s.eq[3] = deharsh (0.5f * tame);
        s.attackMs = 5.0f;
        s.attack = unit (pct (r.ping) + globalPunch);
        s.tailDb = std::clamp (r.wash, -12.0f, 12.0f);   // wash = the sustained shimmer after the ping
        s.decay = unit (-0.4f * tame);
        s.width = std::clamp (pct (r.width), 0.0f, 2.0f);
        s.levelDb = r.level + p.quick.hatsVolume;
    }

    // ---- Misc: deliberately small (uncertain hits must never sound bad).
    {
        const auto& m = p.misc;
        ChannelSettings& s = out[index (DrumType::misc)];
        tilt (s, 0, m.tone);
        s.attackMs = 8.0f;
        s.attack = unit (pct (m.attack));
        s.decay = unit (pct (m.decay));
        s.levelDb = m.level;
    }
    return out;
}
} // namespace plugin_dsp
