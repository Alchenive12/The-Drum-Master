// File: dsp/include/plugin_dsp/DrumEngine.h
// The whole plugin DSP (docs/02-algorithm.md; decisions D-C10..D-C12 in docs/03-decisions-for-review.md).
//
//   input (mono sum) -> OnsetDetector -> HitClassifier -> HitTracker (gates on the output timeline)
//   (MIDI mode, docs/05-features.md F3: mapped notes -> HitClassifier as labelled hits, instead of the detector)
//   input (L, R) -> pre-delay -> DualResEstimator per channel -> 9 drum estimates ("stems")
//   stems -> DrumChannel per type -> changes, scaled by that type's gate
//   output = input delayed by D (50 ms) + sum of changes -> advanced section (HPF, LPF,
//            saturation, drive) -> global clip -> output gain -> dry/wet -> bypass crossfade
//
// Everything lines up on the output timeline tau = n - D. The estimator input is pre-delayed by
// D - (estimator latency) so the stems arrive exactly at tau. Hits are decided about 40-48 ms
// after their onset, just in time for the 2 ms pre-roll of their gate.
//
// Stems use one slowly changing spectral share per type and bin (D-C12): each type's long-term
// average hit spectrum (learned from this loop's confident hits, generic until then), weighted by
// how present that type is in the loop (a type never heard barely takes a share), against the others'.
// The shares of all enabled types sum to 1, so with every setting neutral the output is the
// input delayed by exactly D, bit for bit.
//
// prepare() allocates; process() never allocates, locks or does I/O.
#pragma once

#include "Biquad.h"
#include "DrumChannel.h"
#include "DrumMapping.h"
#include "DrumParams.h"
#include "DrumTypes.h"
#include "DualResEstimator.h"
#include "HitClassifier.h"
#include "HitTracker.h"
#include "OnsetDetector.h"
#include "PuppetShow.h"
#include "SpscQueue.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace plugin_dsp
{
/** MIDI mode: a note that named a drum, at a sample offset inside the block being processed. */
struct MidiHit
{
    std::size_t offset = 0;
    DrumType type = DrumType::misc;
};

/** One detected hit for the Shadow Play visualiser (sample = onset on the output timeline). */
struct ShadowEvent
{
    std::int64_t sample = 0;
    DrumType type = DrumType::misc;
    float levelDb = -120.0f, confidence = 0.0f;
};

class DrumEngine
{
public:
    static constexpr double lookaheadSeconds = 0.050;
    static constexpr std::size_t numBands = ClassifiedHit::numBands;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        lookahead = static_cast<std::int64_t> (std::lround (lookaheadSeconds * sr));
        detector.prepare (sr);
        classifier.prepare (sr);
        tracker.prepare (sr);
        for (auto& e : estimator) e.prepare (sr, numDrumTypes);
        preDelay = static_cast<std::size_t> (lookahead - estimator[0].latency());
        for (auto& ch : channels) ch.prepare (sr);

        preDelayLine.assign (2 * (preDelay + 1), 0.0f);
        dryDelayLine.assign (2 * (static_cast<std::size_t> (lookahead) + 1), 0.0f);

        binsLo = static_cast<std::size_t> (std::lround (sr / estimator[0].binHz (DualResEstimator::Band::low))) / 2 + 1;
        binsHi = static_cast<std::size_t> (std::lround (sr / estimator[0].binHz (DualResEstimator::Band::high))) / 2 + 1;
        shareLo.assign (numDrumTypes * binsLo, 0.0f);
        shareHi.assign (numDrumTypes * binsHi, 0.0f);

        smoothCoeff = static_cast<float> (std::exp (-1.0 / (0.005 * sr)));
        genericSpectra();
        reset();
        setParams (params);
    }

    void reset() noexcept
    {
        detector.reset();
        classifier.reset();
        classifier.resetLearning();
        tracker.reset();
        puppetShow.reset();
        for (auto& e : estimator) e.reset();
        for (auto& ch : channels) ch.reset();
        for (auto& ch : advFilters) for (auto& f : ch) f.reset();
        std::fill (preDelayLine.begin(), preDelayLine.end(), 0.0f);
        std::fill (dryDelayLine.begin(), dryDelayLine.end(), 0.0f);
        prePos = dryPos = 0;
        inputIndex = 0;
        lastMidiSample = -1;
        typePower = generic;
        typeCount.fill (0);
        heard.fill (0);
        updateShares();
        outGain = targetOutGain;
        wet = targetWet;
        bypassMix = targetBypass;
    }

    /** Latency to report to the host (constant, also when bypassed). */
    int latencySamples() const noexcept { return static_cast<int> (lookahead); }

    /** Once per block, before process(). */
    void setParams (const DrumParams& p) noexcept
    {
        params = p;
        mapped = mapParams (p);
        applyChannelSettings();
        const float s01 = std::clamp (p.global.sensitivity / 100.0f, 0.0f, 1.0f);
        detector.setSensitivity (s01);
        classifier.setSensitivity (s01);

        const auto& a = p.advanced;
        hpfOn = a.hpf > 20.5f;
        lpfOn = a.lpf < 19999.5f;
        for (auto& ch : advFilters)
        {
            if (hpfOn) ch[0].setCoeffs (BiquadCoeffs::highPass (sr, std::clamp (static_cast<double> (a.hpf), 20.0, 1000.0), 0.707));
            if (lpfOn) ch[1].setCoeffs (BiquadCoeffs::lowPass (sr, std::clamp (static_cast<double> (a.lpf), 1000.0, std::min (20000.0, 0.45 * sr)), 0.707));
        }
        advSat = std::clamp (a.sat / 100.0f, 0.0f, 1.0f);
        advSatDrive = 1.0f + 4.0f * advSat;
        advSatNorm = satRef / std::tanh (advSatDrive * satRef);
        advDrive = std::clamp (a.drive / 100.0f, 0.0f, 1.0f);
        advDriveK = 1.0f + 9.0f * advDrive;
        advDriveNorm = satRef / (std::tanh (advDriveK * (satRef + driveBias)) - std::tanh (advDriveK * driveBias));
        globalClip = std::clamp (p.global.clip / 100.0f, 0.0f, 1.0f);
        clipCeiling = dbToGain (-12.0f * globalClip);
        targetOutGain = dbToGain (std::clamp (p.global.outputDb, -24.0f, 12.0f));
        targetWet = std::clamp (p.global.mix / 100.0f, 0.0f, 1.0f);
        targetBypass = p.global.bypass ? 1.0f : 0.0f;
    }

    /** Manual mode / auto-detect result: which types may be detected (misc always). */
    void setAllowedTypes (std::uint32_t mask) noexcept
    {
        classifier.setAllowedTypes (mask);
        updateShares();
    }
    std::uint32_t allowedTypes() const noexcept { return classifier.allowedTypes(); }
    std::uint32_t presentTypes() const noexcept { return classifier.presentTypes(); }
    void resetLearning() noexcept
    {
        classifier.resetLearning();
        typePower = generic;
        typeCount.fill (0);
        heard.fill (0);
        updateShares();
    }

    /** Hits for the Shadow Play visualiser (read on the UI thread). */
    SpscQueue<ShadowEvent, 256>& shadowEvents() noexcept { return shadow; }

    /** MIDI mode (docs/05-features.md F3): while on and notes keep arriving, the notes say which drum hits
        and when; audio detection only takes over again after midiTimeoutSeconds without a note. */
    void setMidiMode (bool on) noexcept { midiMode = on; }
    /** True while MIDI notes, not audio detection, decide the hits. */
    bool midiDriving() const noexcept
    {
        return midiMode && lastMidiSample >= 0 && inputIndex - lastMidiSample < static_cast<std::int64_t> (midiTimeoutSeconds * sr);
    }
    static constexpr double midiTimeoutSeconds = 4.0;

    /** In-place processing of 1 or 2 channels. midiHits (sorted by offset) are this block's mapped notes. */
    void process (float* left, float* right, std::size_t numSamples, const MidiHit* midiHits = nullptr, std::size_t numMidiHits = 0) noexcept
    {
        if (spectraChanged)
        {
            updateShares();
            spectraChanged = false;
        }
        // The Puppet Show glides once per block; its gains go into each drum's reserved EQ slots.
        puppetShow.advance (params.puppetShow, static_cast<double> (numSamples) / sr);
        applyChannelSettings();
        const bool stereo = right != nullptr && right != left;
        std::array<float, numDrumTypes> stemL {}, stemR {};
        const std::size_t preLen = preDelay + 1, dryLen = static_cast<std::size_t> (lookahead) + 1;

        for (std::size_t i = 0; i < numSamples; ++i)
        {
            const float inL = left[i], inR = stereo ? right[i] : inL;
            const float mono = stereo ? 0.5f * (inL + inR) : inL;

            // --- detection on the undelayed input (MIDI mode: the notes, while they keep coming)
            for (; numMidiHits > 0 && midiHits->offset <= i; ++midiHits, --numMidiHits)
                if (midiMode)
                {
                    classifier.addMidiHit (inputIndex, midiHits->type);
                    lastMidiSample = inputIndex;
                }
            OnsetEvent ev;
            if (detector.processSample (mono, ev) && ! midiDriving()) classifier.addEvent (ev);
            classifier.pushSample (mono);
            ClassifiedHit hit;
            while (classifier.popClassified (hit)) onHit (hit);
            tracker.pushSample (mono);

            // --- estimation: pre-delay, then the stems come out on the output timeline
            preDelayLine[2 * prePos] = inL;
            preDelayLine[2 * prePos + 1] = inR;
            const std::size_t preRead = (prePos + 1) % preLen;
            prePos = preRead;
            auto share = [this] (DualResEstimator::Band band, std::size_t o, std::size_t k, std::size_t) {
                return band == DualResEstimator::Band::low ? shareLo[o * binsLo + k] : shareHi[o * binsHi + k];
            };
            estimator[0].processSample (preDelayLine[2 * preRead], stemL.data(), numDrumTypes, share);
            if (stereo) estimator[1].processSample (preDelayLine[2 * preRead + 1], stemR.data(), numDrumTypes, share);
            else stemR = stemL;

            // --- dry path delayed by exactly D
            dryDelayLine[2 * dryPos] = inL;
            dryDelayLine[2 * dryPos + 1] = inR;
            const std::size_t dryRead = (dryPos + 1) % dryLen;
            dryPos = dryRead;
            const float dryL = dryDelayLine[2 * dryRead], dryR = dryDelayLine[2 * dryRead + 1];

            // --- per-drum changes at tau
            const std::int64_t tau = inputIndex - lookahead;
            float yL = dryL, yR = dryR;
            for (std::size_t t = 0; t < numDrumTypes; ++t)
            {
                const auto type = static_cast<DrumType> (t);
                const float g = tracker.gate (type, tau);
                float dl = 0.0f, dr = 0.0f;
                channels[t].process (stemL[t], stemR[t], tracker.timeSinceOnset (type, tau), g, dl, dr);
                yL += dl;
                yR += dr;
            }
            ++inputIndex;

            // --- advanced + global section
            yL = advanced (yL, 0);
            yR = advanced (yR, 1);
            outGain = smoothCoeff * outGain + (1.0f - smoothCoeff) * targetOutGain;
            wet = smoothCoeff * wet + (1.0f - smoothCoeff) * targetWet;
            bypassMix = smoothCoeff * bypassMix + (1.0f - smoothCoeff) * targetBypass;
            if (std::fabs (bypassMix - targetBypass) < 1e-6f) bypassMix = targetBypass;
            if (std::fabs (wet - targetWet) < 1e-7f) wet = targetWet;
            if (std::fabs (outGain - targetOutGain) < 1e-7f) outGain = targetOutGain;
            yL = dryL + wet * (outGain * yL - dryL);
            yR = dryR + wet * (outGain * yR - dryR);
            left[i] = yL + bypassMix * (dryL - yL);
            if (stereo) right[i] = yR + bypassMix * (dryR - yR);
        }
    }

    /** The Puppet Show (meters: meterDb() is safe on the UI thread). */
    const PuppetShow& puppet() const noexcept { return puppetShow; }

    /** For tests: the tracker (gates, hit ages). */
    const HitTracker& hitTracker() const noexcept { return tracker; }
    /** For tests: current share of type t in low-band bin k / high-band bin k. */
    float shareAt (DualResEstimator::Band band, DrumType t, std::size_t k) const noexcept
    {
        return band == DualResEstimator::Band::low ? shareLo[index (t) * binsLo + k] : shareHi[index (t) * binsHi + k];
    }

private:
    static constexpr float satRef = 0.5f, driveBias = 0.15f;

    void onHit (const ClassifiedHit& h) noexcept
    {
        tracker.addHit (h);
        puppetShow.addHit (h);
        // Presence: every type this hit contains. Misc is also "the unexplained rest" of other
        // hits, so it only counts when it is the hit's main type.
        for (std::size_t u = 0; u < numDrumTypes; ++u)
            if ((u == index (DrumType::misc) ? h.primary == DrumType::misc : h.weight[u] >= 0.3f) && heard[u] < heardFull)
            {
                ++heard[u];
                spectraChanged = true;
            }
        ShadowEvent e;
        e.sample = h.sample;
        e.type = h.primary;
        e.levelDb = h.peakDb;
        e.confidence = h.confidence;
        shadow.push (e);

        // Learn this loop's spectrum of a type from its clear, single-type hits. Misc learns too, so
        // what lands there (toms, types switched off) doesn't keep its generic, bass-heavy spectrum.
        const std::size_t t = index (h.primary);
        if (h.weight[t] < 0.7f || h.confidence < 0.7f) return;
        for (std::size_t u = 0; u < numDrumTypes; ++u)
            if (u != t && u != index (DrumType::misc) && h.weight[u] >= 0.3f) return;
        float total = 0.0f;
        for (float v : h.rise) total += v;
        if (total <= 1e-12f) return;
        const float a = typeCount[t] < 8 ? 1.0f / static_cast<float> (typeCount[t] + 1) : 0.05f;
        for (std::size_t b = 0; b < numBands; ++b) typePower[t][b] += a * (h.rise[b] / total - typePower[t][b]);
        ++typeCount[t];
        spectraChanged = true;
    }

    /** Knob settings + The Puppet Show's current gains (slots 8-10) -> the drum chains. */
    void applyChannelSettings() noexcept
    {
        using PS = PuppetShow;
        auto settings = mapped;
        std::array<std::size_t, numDrumTypes> next {};
        next.fill (ChannelSettings::firstPuppetSlot);
        for (const auto& b : PS::bands)   // the table also drives the editor's curves
        {
            const auto kind = b.shape == PS::Shape::lowShelf ? EqSlot::Kind::lowShelf
                              : b.shape == PS::Shape::highShelf ? EqSlot::Kind::highShelf : EqSlot::Kind::bell;
            settings[index (b.drum)].eq[next[index (b.drum)]++] = { kind, b.freq, b.q, puppetShow.gainDb (b.measure) };
        }
        for (std::size_t t = 0; t < numDrumTypes; ++t) channels[t].setSettings (settings[t]);
    }

    float advanced (float y, std::size_t c) noexcept
    {
        if (hpfOn) y = advFilters[c][0].process (y);
        if (lpfOn) y = advFilters[c][1].process (y);
        if (advSat > 0.0f) y += advSat * (advSatNorm * std::tanh (advSatDrive * y) - y);
        if (advDrive > 0.0f)
            y += advDrive * (advDriveNorm * (std::tanh (advDriveK * (y + driveBias)) - std::tanh (advDriveK * driveBias)) - y);
        if (globalClip > 0.0f)
        {
            const float u = y / clipCeiling;
            y = u >= 1.5f ? clipCeiling : (u <= -1.5f ? -clipCeiling : clipCeiling * (u - (4.0f / 27.0f) * u * u * u));
        }
        return y;
    }

    /** Generic per-type spectra (power per band, sums to 1) until this loop teaches better. */
    void genericSpectra() noexcept
    {
        // Compressed magnitudes (power^1/4), bands 30,60,100,160,250,400,630,1k,1.6k,2.5k,4k,6.3k,10k-16k.
        const std::array<std::array<float, numBands>, numDrumTypes> c { {
            { 0.70f, 0.90f, 1.00f, 0.80f, 0.45f, 0.25f, 0.18f, 0.16f, 0.16f, 0.14f, 0.03f, 0.01f, 0.005f },   // kick
            { 0.10f, 0.15f, 0.35f, 0.90f, 1.00f, 0.80f, 0.65f, 0.60f, 0.60f, 0.60f, 0.58f, 0.52f, 0.45f },   // snare
            { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.10f, 0.15f, 0.25f, 0.40f, 0.70f, 1.00f, 0.90f },   // closed hat
            { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.12f, 0.20f, 0.30f, 0.45f, 0.75f, 1.00f, 0.90f },   // open hat
            { 0.00f, 0.00f, 0.05f, 0.10f, 0.15f, 0.25f, 0.40f, 0.60f, 0.80f, 0.95f, 1.00f, 0.95f, 0.85f },   // crash
            { 0.00f, 0.00f, 0.03f, 0.06f, 0.10f, 0.18f, 0.30f, 0.50f, 0.75f, 0.95f, 1.00f, 0.85f, 0.70f },   // ride
            { 0.05f, 0.05f, 0.10f, 0.20f, 0.35f, 0.60f, 0.85f, 1.00f, 0.95f, 0.75f, 0.55f, 0.40f, 0.25f },   // rimshot
            { 0.05f, 0.05f, 0.10f, 0.20f, 0.35f, 0.55f, 0.80f, 1.00f, 1.00f, 0.85f, 0.65f, 0.45f, 0.30f },   // clap
            { 0.50f, 0.60f, 0.70f, 0.75f, 0.75f, 0.70f, 0.60f, 0.50f, 0.45f, 0.40f, 0.35f, 0.30f, 0.25f },   // misc (toms, percussion)
        } };
        for (std::size_t t = 0; t < numDrumTypes; ++t)
        {
            float total = 0.0f;
            for (std::size_t b = 0; b < numBands; ++b)
            {
                const float m = c[t][b];
                generic[t][b] = m * m * m * m;
                total += generic[t][b];
            }
            for (auto& v : generic[t]) v /= total;
        }
    }

    /** Spectral power density of type t at frequency f: log-frequency interpolation between band
        centres of the per-band power divided by the band width. */
    float densityAt (std::size_t t, double f) const noexcept
    {
        auto centre = [] (std::size_t b) { return std::sqrt (HitClassifier::bandEdges[b] * HitClassifier::bandEdges[b + 1]); };
        auto dens = [this, t] (std::size_t b) {
            return static_cast<double> (typePower[t][b]) / (HitClassifier::bandEdges[b + 1] - HitClassifier::bandEdges[b]);
        };
        if (f <= centre (0)) return static_cast<float> (dens (0));
        if (f >= centre (numBands - 1)) return static_cast<float> (dens (numBands - 1));
        std::size_t b = 0;
        while (b + 1 < numBands && centre (b + 1) < f) ++b;
        const double x = std::log (f / centre (b)) / std::log (centre (b + 1) / centre (b));
        return static_cast<float> (dens (b) + x * (dens (b + 1) - dens (b)));
    }

    void updateShares() noexcept
    {
        const std::uint32_t allowed = classifier.allowedTypes();
        auto fill = [this, allowed] (std::vector<float>& table, std::size_t bins, double binHz) {
            for (std::size_t k = 0; k < bins; ++k)
            {
                const double f = std::max (20.0, static_cast<double> (k) * binHz);
                std::array<float, numDrumTypes> d {};
                float sum = 0.0f;
                for (std::size_t t = 0; t < numDrumTypes; ++t)
                {
                    const float presence = 0.02f + 0.98f * static_cast<float> (heard[t]) / static_cast<float> (heardFull);
                    d[t] = (allowed >> t) & 1u ? presence * densityAt (t, f) + 1e-12f : 0.0f;
                    sum += d[t];
                }
                for (std::size_t t = 0; t < numDrumTypes; ++t) table[t * bins + k] = sum > 0.0f ? d[t] / sum : 0.0f;
            }
        };
        fill (shareLo, binsLo, estimator[0].binHz (DualResEstimator::Band::low));
        fill (shareHi, binsHi, estimator[0].binHz (DualResEstimator::Band::high));
    }

    double sr = 48000.0;
    std::int64_t lookahead = 2400, inputIndex = 0, lastMidiSample = -1;
    bool midiMode = false;
    std::size_t preDelay = 0, prePos = 0, dryPos = 0, binsLo = 0, binsHi = 0;
    DrumParams params;
    std::array<ChannelSettings, numDrumTypes> mapped {};
    PuppetShow puppetShow;

    OnsetDetector detector;
    HitClassifier classifier;
    HitTracker tracker;
    std::array<DualResEstimator, 2> estimator;
    std::array<DrumChannel, numDrumTypes> channels;
    SpscQueue<ShadowEvent, 256> shadow;

    std::vector<float> preDelayLine, dryDelayLine, shareLo, shareHi;
    std::array<std::array<float, numBands>, numDrumTypes> generic {}, typePower {};
    std::array<int, numDrumTypes> typeCount {}, heard {};
    static constexpr int heardFull = 4;   // hits until a type takes its full share
    bool spectraChanged = false;

    std::array<std::array<Biquad, 2>, 2> advFilters {};
    bool hpfOn = false, lpfOn = false;
    float advSat = 0.0f, advSatDrive = 1.0f, advSatNorm = 1.0f, advDrive = 0.0f, advDriveK = 1.0f, advDriveNorm = 1.0f;
    float globalClip = 0.0f, clipCeiling = 1.0f;
    float smoothCoeff = 0.99f, outGain = 1.0f, targetOutGain = 1.0f, wet = 1.0f, targetWet = 1.0f, bypassMix = 0.0f, targetBypass = 0.0f;
};
} // namespace plugin_dsp
