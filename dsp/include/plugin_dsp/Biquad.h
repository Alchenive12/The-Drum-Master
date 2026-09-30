// File: dsp/include/plugin_dsp/Biquad.h
// Second-order IIR filter (transposed direct form II) with RBJ "Audio EQ Cookbook" designs.
// Coefficients are computed in double, stored as float. No allocation anywhere.
#pragma once

#include <cmath>

namespace plugin_dsp
{
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;   // a0 normalised to 1

    static BiquadCoeffs identity() noexcept { return {}; }

    static BiquadCoeffs lowPass (double sr, double f, double q) noexcept { return design (Kind::lowPass, sr, f, q, 0.0); }
    static BiquadCoeffs highPass (double sr, double f, double q) noexcept { return design (Kind::highPass, sr, f, q, 0.0); }
    static BiquadCoeffs bandPass (double sr, double f, double q) noexcept { return design (Kind::bandPass, sr, f, q, 0.0); }
    static BiquadCoeffs peak (double sr, double f, double q, double gainDb) noexcept { return design (Kind::peak, sr, f, q, gainDb); }
    static BiquadCoeffs lowShelf (double sr, double f, double q, double gainDb) noexcept { return design (Kind::lowShelf, sr, f, q, gainDb); }
    static BiquadCoeffs highShelf (double sr, double f, double q, double gainDb) noexcept { return design (Kind::highShelf, sr, f, q, gainDb); }

    /** |H(f)| of these coefficients at frequency f (for tests and meters). */
    double magnitudeAt (double sr, double f) const noexcept
    {
        const double w = 2.0 * 3.14159265358979323846 * f / sr;
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double B0 = static_cast<double> (b0), B1 = static_cast<double> (b1), B2 = static_cast<double> (b2);
        const double A1 = static_cast<double> (a1), A2 = static_cast<double> (a2);
        const double nr = B0 + B1 * cr + B2 * c2r, ni = B1 * ci + B2 * c2i;
        const double dr = 1.0 + A1 * cr + A2 * c2r, di = A1 * ci + A2 * c2i;
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }

private:
    enum class Kind { lowPass, highPass, bandPass, peak, lowShelf, highShelf };

    static BiquadCoeffs design (Kind k, double sr, double f, double q, double gainDb) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        f = std::fmin (std::fmax (f, 1.0), 0.49 * sr);
        q = std::fmax (q, 0.05);
        const double w = 2.0 * pi * f / sr, cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        double nb0 = 1, nb1 = 0, nb2 = 0, na0 = 1, na1 = 0, na2 = 0;
        switch (k)
        {
            case Kind::lowPass:  nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = nb0; na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha; break;
            case Kind::highPass: nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = nb0; na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha; break;
            case Kind::bandPass: nb0 = alpha; nb1 = 0; nb2 = -alpha; na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha; break;
            case Kind::peak:
                nb0 = 1 + alpha * A; nb1 = -2 * cw; nb2 = 1 - alpha * A; na0 = 1 + alpha / A; na1 = -2 * cw; na2 = 1 - alpha / A; break;
            case Kind::lowShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                nb0 = A * ((A + 1) - (A - 1) * cw + s); nb1 = 2 * A * ((A - 1) - (A + 1) * cw); nb2 = A * ((A + 1) - (A - 1) * cw - s);
                na0 = (A + 1) + (A - 1) * cw + s; na1 = -2 * ((A - 1) + (A + 1) * cw); na2 = (A + 1) + (A - 1) * cw - s;
                break;
            }
            case Kind::highShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                nb0 = A * ((A + 1) + (A - 1) * cw + s); nb1 = -2 * A * ((A - 1) + (A + 1) * cw); nb2 = A * ((A + 1) + (A - 1) * cw - s);
                na0 = (A + 1) - (A - 1) * cw + s; na1 = 2 * ((A - 1) - (A + 1) * cw); na2 = (A + 1) - (A - 1) * cw - s;
                break;
            }
        }
        BiquadCoeffs c;
        c.b0 = static_cast<float> (nb0 / na0); c.b1 = static_cast<float> (nb1 / na0); c.b2 = static_cast<float> (nb2 / na0);
        c.a1 = static_cast<float> (na1 / na0); c.a2 = static_cast<float> (na2 / na0);
        return c;
    }
};

class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
    const BiquadCoeffs& coeffs() const noexcept { return k; }
    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        if (std::fabs (z1) < 1e-20f) z1 = 0.0f;   // flush denormals
        if (std::fabs (z2) < 1e-20f) z2 = 0.0f;
        return y;
    }

private:
    BiquadCoeffs k;
    float z1 = 0.0f, z2 = 0.0f;
};
} // namespace plugin_dsp
