// File: dsp/include/plugin_dsp/DrumChannel.h
// The processor chain for one drum type (docs/02-algorithm.md §7, docs/03-decisions-for-review.md D-C10).
// It runs on that drum's stereo estimate x̂ (DualResEstimator) and returns the change it makes,
// already scaled by the hit gate (HitTracker):
//     delta = gate · (P(x̂) − x̂),        output = dry + Σ over types of delta
// so a neutral chain, or gate 0, adds exactly nothing.
//
// Chain: EQ (up to 11 filters: 8 for the knobs, 3 for The Puppet Show) -> compressor -> attack / tail shaping (by the hit's age) ->
// saturation -> soft clip -> level -> width. Filters and the compressor run all the time, so
// there's no click when a gate opens. No allocation.
#pragma once

#include "Biquad.h"
#include "GainMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace plugin_dsp
{
/** Float "is (almost) zero" without -Wfloat-equal. */
constexpr bool nearZero (float x) noexcept { return x < 1e-6f && x > -1e-6f; }

struct EqSlot
{
    enum class Kind : std::uint8_t { off, bell, lowShelf, highShelf, highPass };
    Kind kind = Kind::off;
    float freq = 1000.0f, q = 0.707f, gainDb = 0.0f;   // gainDb ignored by highPass (freq <= 20 Hz = off)
};

struct ChannelSettings
{
    static constexpr std::size_t numEq = 11;          // 0-7: the user's knobs (DrumMapping), 8-10: The Puppet Show
    static constexpr std::size_t firstPuppetSlot = 8;
    std::array<EqSlot, numEq> eq {};
    float levelDb = 0.0f;
    float attack = 0.0f;        // -1..1: -15 dB .. +9 dB over the attack window
    float attackMs = 8.0f;      // length of the attack window (full gain), then a same-length fade
    float decay = 0.0f;         // -1..1: < 0 faster tail (up to 0.3 dB/ms extra), > 0 up to +6 dB tail
    float tailDb = 0.0f;        // static tail gain after the attack window (ride Wash), dB
    float comp = 0.0f;          // 0..1
    float compAttackMs = 5.0f;
    float sat = 0.0f;           // 0..1 tanh saturation, level held around -6 dBFS (see saturate())
    float clip = 0.0f;          // 0..1 soft-clip ceiling 0 .. -12 dBFS
    float width = 1.0f;         // 0..2 side gain

    bool isNeutral() const noexcept
    {
        for (const auto& s : eq)
            if (s.kind == EqSlot::Kind::highPass ? s.freq > 20.0f : (s.kind != EqSlot::Kind::off && ! nearZero (s.gainDb)))
                return false;
        return nearZero (levelDb) && nearZero (attack) && nearZero (decay) && nearZero (tailDb) && comp <= 0.0f
               && sat <= 0.0f && clip <= 0.0f && nearZero (width - 1.0f);
    }
};

class DrumChannel
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        releaseCoeff = static_cast<float> (std::exp (-1.0 / (0.080 * sr)));
        reset();
    }

    void reset() noexcept
    {
        for (auto& ch : filters)
            for (auto& f : ch) f.reset();
        envelope = 0.0f;
        smoothedCompGain = dbToGain (makeupDb);
    }

    /** Call once per block (not per sample): designs filters and derived constants. */
    void setSettings (const ChannelSettings& s) noexcept
    {
        const bool compWasOff = settings.comp <= 0.0f;
        settings = s;
        neutral = s.isNeutral();
        for (std::size_t i = 0; i < ChannelSettings::numEq; ++i)
        {
            const EqSlot& e = s.eq[i];
            BiquadCoeffs c = BiquadCoeffs::identity();
            bool on = true;
            const double f = std::clamp (static_cast<double> (e.freq), 10.0, 0.45 * sr);
            switch (e.kind)
            {
                case EqSlot::Kind::off: on = false; break;
                case EqSlot::Kind::bell: on = ! nearZero (e.gainDb); if (on) c = BiquadCoeffs::peak (sr, f, static_cast<double> (e.q), static_cast<double> (e.gainDb)); break;
                case EqSlot::Kind::lowShelf: on = ! nearZero (e.gainDb); if (on) c = BiquadCoeffs::lowShelf (sr, f, static_cast<double> (e.q), static_cast<double> (e.gainDb)); break;
                case EqSlot::Kind::highShelf: on = ! nearZero (e.gainDb); if (on) c = BiquadCoeffs::highShelf (sr, f, static_cast<double> (e.q), static_cast<double> (e.gainDb)); break;
                case EqSlot::Kind::highPass: on = e.freq > 20.0f; if (on) c = BiquadCoeffs::highPass (sr, f, static_cast<double> (e.q)); break;
            }
            eqOn[i] = on;
            for (auto& ch : filters) ch[i].setCoeffs (c);
        }
        attackGain = s.attack >= 0.0f ? dbToGain (9.0f * s.attack) : dbToGain (15.0f * s.attack);
        attackLen = std::max<std::int64_t> (1, static_cast<std::int64_t> (static_cast<double> (s.attackMs) * 0.001 * sr));
        tailGainUp = dbToGain (6.0f * std::max (0.0f, s.decay) + s.tailDb);
        tailSlopeDbPerSample = static_cast<float> (0.3 * 1000.0 / sr) * std::max (0.0f, -s.decay);
        level = dbToGain (s.levelDb);
        // Compressor: threshold -6 .. -30 dBFS, ratio 1.5 .. 6, soft knee, half the static reduction back as make-up.
        thresholdDb = -6.0f - 24.0f * s.comp;
        ratio = 1.5f + 4.5f * s.comp;
        makeupDb = s.comp > 0.0f ? 0.5f * (-thresholdDb - 6.0f) * (1.0f - 1.0f / ratio) : 0.0f;
        compAttack = static_cast<float> (std::exp (-1.0 / (static_cast<double> (std::max (0.1f, s.compAttackMs)) * 0.001 * sr)));
        if (compWasOff) smoothedCompGain = dbToGain (makeupDb);   // start settled, no fade-in
        drive = 1.0f + 4.0f * s.sat;
        satNorm = satRef / std::tanh (drive * satRef);
        ceiling = dbToGain (-12.0f * s.clip);
    }

    /** One stereo sample of this drum's estimate. age = samples since the current hit's onset
        (-1 if none), gate = HitTracker gate (0..1). Writes the change to add to the dry signal. */
    void process (float xl, float xr, std::int64_t age, float gate, float& dl, float& dr) noexcept
    {
        if (neutral)
        {
            dl = dr = 0.0f;
            return;
        }
        float l = xl, r = xr;
        for (std::size_t i = 0; i < ChannelSettings::numEq; ++i)
            if (eqOn[i])
            {
                l = filters[0][i].process (l);
                r = filters[1][i].process (r);
            }

        if (settings.comp > 0.0f)
        {
            const float peak = std::max (std::fabs (l), std::fabs (r));
            envelope = peak > envelope ? peak : releaseCoeff * envelope + (1.0f - releaseCoeff) * peak;
            const float target = dbToGain (compGainDb (gainToDb (envelope)) + makeupDb);
            const float coeff = target < smoothedCompGain ? compAttack : releaseCoeff;
            smoothedCompGain = coeff * smoothedCompGain + (1.0f - coeff) * target;
            l *= smoothedCompGain;
            r *= smoothedCompGain;
        }

        const float shape = shapeGain (age);
        l *= shape;
        r *= shape;

        if (settings.sat > 0.0f)
        {
            l = saturate (l);
            r = saturate (r);
        }
        if (settings.clip > 0.0f)
        {
            l = softClip (l);
            r = softClip (r);
        }

        l *= level;
        r *= level;
        if (! nearZero (settings.width - 1.0f))
        {
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * settings.width;
            l = mid + side;
            r = mid - side;
        }
        dl = gate * (l - xl);
        dr = gate * (r - xr);
    }

    /** Static gain curve of the compressor (dB in -> dB change), exposed for tests. */
    float compGainDb (float inDb) const noexcept
    {
        constexpr float knee = 6.0f;
        const float over = inDb - thresholdDb;
        if (over <= -0.5f * knee) return 0.0f;
        if (over >= 0.5f * knee) return -over * (1.0f - 1.0f / ratio);
        const float x = over + 0.5f * knee;
        return -(1.0f - 1.0f / ratio) * x * x / (2.0f * knee);
    }

    /** Attack / tail gain at a hit age (exposed for tests). */
    float shapeGain (std::int64_t age) const noexcept
    {
        if (age < 0) return attackGain;   // pre-roll leads straight into the attack
        if (age < attackLen) return attackGain;
        if (age < 2 * attackLen)
        {
            const float x = static_cast<float> (age - attackLen) / static_cast<float> (attackLen);
            const float w = 0.5f + 0.5f * std::cos (3.14159265f * x);   // 1 -> 0
            return w * attackGain + (1.0f - w) * tailGainAt (age);
        }
        return tailGainAt (age);
    }

private:
    float tailGainAt (std::int64_t age) const noexcept
    {
        const auto since = static_cast<float> (std::max<std::int64_t> (0, age - attackLen));
        const float down = std::max (-60.0f, -tailSlopeDbPerSample * since);
        return tailGainUp * dbToGain (down);
    }

    /** tanh curve scaled so a -6 dBFS peak stays at -6 dBFS: quieter parts come up (thicker, more
        sustain), louder peaks are rounded off. Blended in by the amount, so 0+ is continuous. */
    float saturate (float x) const noexcept
    {
        return x + settings.sat * (satNorm * std::tanh (drive * x) - x);
    }

    float softClip (float x) const noexcept
    {
        const float u = x / ceiling;
        if (u >= 1.5f) return ceiling;
        if (u <= -1.5f) return -ceiling;
        return ceiling * (u - (4.0f / 27.0f) * u * u * u);   // cubic knee, slope 1 at 0, flat at +-1.5
    }

    double sr = 48000.0;
    ChannelSettings settings;
    bool neutral = true;
    std::array<bool, ChannelSettings::numEq> eqOn {};
    std::array<std::array<Biquad, ChannelSettings::numEq>, 2> filters {};
    float attackGain = 1.0f, tailGainUp = 1.0f, tailSlopeDbPerSample = 0.0f, level = 1.0f;
    std::int64_t attackLen = 384;
    float thresholdDb = -6.0f, ratio = 1.5f, makeupDb = 0.0f, compAttack = 0.9f, releaseCoeff = 0.999f;
    float envelope = 0.0f, smoothedCompGain = 1.0f, drive = 1.0f, satNorm = 1.0f, ceiling = 1.0f;
    static constexpr float satRef = 0.5f;
};
} // namespace plugin_dsp
