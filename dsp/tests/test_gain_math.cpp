// File: dsp/tests/test_gain_math.cpp
// Example test. Pattern: one file per DSP unit, checks at edge values, finite output, known results.
#include "dsp_test.h"
#include <plugin_dsp/GainMath.h>

int main()
{
    using namespace plugin_dsp;

    DSP_CHECK_NEAR (dbToGain (0.0f), 1.0f, 1e-6);
    DSP_CHECK_NEAR (dbToGain (-6.0206f), 0.5f, 1e-4);
    DSP_CHECK_NEAR (dbToGain (-200.0f), 0.0f, 0.0);
    DSP_CHECK_NEAR (gainToDb (1.0f), 0.0f, 1e-5);
    DSP_CHECK_NEAR (gainToDb (0.0f), -120.0f, 0.0);

    // Round trip
    for (float db = -60.0f; db <= 12.0f; db += 3.0f)
        DSP_CHECK_NEAR (gainToDb (dbToGain (db)), db, 1e-3);

    // Helper sanity: a unit sine reads ~1.0 at its own frequency (integer number of cycles)
    const auto s = dsptest::sine (1000.0, 48000.0, 4800);
    DSP_CHECK (dsptest::allFinite (s));
    DSP_CHECK_NEAR (dsptest::magnitudeAt (s, 1000.0, 48000.0), 1.0, 1e-3);

    DSP_TEST_END;
}
