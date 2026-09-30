// File: research/engine-shootout/shootout2.cpp
//
// Shootout v2 — reruns the phase-2 engine comparison with the fixes the agents asked for in
// prompts/answers/03-*.md:
//   * SAME-BAND OVERLAPS (both agents, "the test that decides the design"):
//       kick + sustained bass with an 808-style glide, snare + layered clap,
//       open hat on top of a crash tail, closed hats everywhere.
//   * richer drums: kick with body mode + moving pitch + click, snare with 3 modes + 3.8 kHz ring.
//   * DUAL-RESOLUTION STFT (both agents' proposed fix): N=2048 below 1.5 kHz, N=256 above.
//   * short-window STFT (N=256, hop 64).
//   * a phase-insensitive spectral metric next to the L2 metric (both agents).
//   * onset jitter ±2 ms on the timing the engines get (Agent 1).
//   * two new tests: clap level (layered on snare) and kick "Top" EQ (1.5–5 kHz, shared with hats).
// Not tested (can't be, offline in the sandbox): real multitrack stems, stereo, listening tests.
//
// Engines
//   A    STFT gain mask,  N1024/hop128                    (Agents 1+2, prompt 02)
//   As   STFT gain mask,  N256/hop64                      (short window)
//   Ad   STFT gain mask,  dual-res (2048/256 low, 256/64 high, split at 1.5 kHz)
//   B    6 complementary linear-phase time bands + time-domain processors   (Claude)
//   C    hybrid: STFT soft-mask estimate (N1024/hop128) + time-domain processors
//   Cd   hybrid with the dual-resolution estimate
//   Cd1k same, but N1024/hop128 for the lows: the 2048 window alone is 43 ms of latency and does
//        not fit the 30 ms lookahead; 1024 (21 ms) does
// A-type engines can't create harmonics; for saturation Agent 1's "A' = A + time-domain
// nonlinearity" is exactly C, so A rows show "n/a" there.
//
// Build: g++ -O2 -std=c++17 shootout2.cpp -o shootout2 && ./shootout2

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

using cd  = std::complex<double>;
using Sig = std::vector<double>;

static constexpr double kPi = 3.14159265358979323846;
static constexpr double fs  = 48000.0;
static const double beat = 0.5;                       // 120 bpm
static const int    bars = 4;
static const size_t len  = size_t ((bars * 4 * beat + 1.5) * fs);
static const size_t lead = size_t (0.25 * fs);

// ================================================================ FFT + convolution
static void fft (std::vector<cd>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t l = 2; l <= n; l <<= 1)
    {
        const double ang = 2 * kPi / double (l) * (inverse ? 1 : -1);
        const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += l)
        {
            cd w (1);
            for (size_t j = 0; j < l / 2; ++j)
            {
                const cd u = a[i + j], v = a[i + j + l / 2] * w;
                a[i + j] = u + v; a[i + j + l / 2] = u - v; w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= double (n);
}

// Linear-phase FIR applied with its delay removed (output aligned with input).
static Sig filterAligned (const Sig& x, const Sig& h)
{
    const size_t L = h.size(), M = (L - 1) / 2, B = 8192, N = 16384;
    std::vector<cd> H (N);
    for (size_t i = 0; i < L; ++i) H[i] = h[i];
    fft (H, false);
    Sig y (x.size() + L - 1, 0.0);
    for (size_t s = 0; s < x.size(); s += B)
    {
        std::vector<cd> X (N);
        for (size_t i = 0; i < B && s + i < x.size(); ++i) X[i] = x[s + i];
        fft (X, false);
        for (size_t k = 0; k < N; ++k) X[k] *= H[k];
        fft (X, true);
        for (size_t i = 0; i < N && s + i < y.size(); ++i) y[s + i] += X[i].real();
    }
    return Sig (y.begin() + long (M), y.begin() + long (M + x.size()));
}

static const size_t kFirLen = 2047;
static Sig lowpass (double fc)
{
    Sig h (kFirLen);
    const double M = (kFirLen - 1) / 2.0, wc = 2.0 * fc / fs;
    double sum = 0;
    for (size_t n = 0; n < kFirLen; ++n)
    {
        const double k = double (n) - M;
        const double sinc = k == 0 ? wc : std::sin (kPi * wc * k) / (kPi * k);
        const double bl = 0.42 - 0.5 * std::cos (2 * kPi * n / (kFirLen - 1)) + 0.08 * std::cos (4 * kPi * n / (kFirLen - 1));
        h[n] = sinc * bl; sum += h[n];
    }
    for (auto& v : h) v /= sum;
    return h;
}
static const Sig& lp (double fc)
{
    static std::vector<std::pair<double, Sig>> cache;
    for (auto& c : cache) if (c.first == fc) return c.second;
    cache.emplace_back (fc, lowpass (fc));
    return cache.back().second;
}
// Zero-phase magnitude response of a symmetric FIR at frequency f.
static double zeroPhase (const Sig& h, double f)
{
    const double M = (h.size() - 1) / 2.0;
    double s = 0; for (size_t n = 0; n < h.size(); ++n) s += h[n] * std::cos (2 * kPi * f * (double (n) - M) / fs);
    return s;
}

static const double kXover[5] = { 120, 400, 1500, 5000, 10000 };
static std::vector<Sig> splitBands (const Sig& x)
{
    std::vector<Sig> l; for (double fc : kXover) l.push_back (filterAligned (x, lp (fc)));
    std::vector<Sig> b (6, Sig (x.size()));
    for (size_t i = 0; i < x.size(); ++i)
    {
        b[0][i] = l[0][i];
        for (size_t k = 1; k < 5; ++k) b[k][i] = l[k][i] - l[k - 1][i];
        b[5][i] = x[i] - l[4][i];
    }
    return b;
}
static void splitAt (const Sig& x, double fc, Sig& lo, Sig& hi)
{
    lo = filterAligned (x, lp (fc)); hi.resize (x.size());
    for (size_t i = 0; i < x.size(); ++i) hi[i] = x[i] - lo[i];
}

// ================================================================ stems
struct Stem { std::string name; Sig s; std::vector<size_t> onsets; };

static Sig noiseBuf (unsigned seed, double hpHz, double lpHz)
{
    Sig b (size_t (3.0 * fs));
    std::mt19937 rng (seed); std::normal_distribution<double> nd (0, 1);
    const double a = std::exp (-2 * kPi * hpHz / fs), c = lpHz > 0 ? std::exp (-2 * kPi * lpHz / fs) : 0.0;
    double x1[4] = {}, y1[4] = {}, l1 = 0, l2 = 0;
    for (auto& v : b)
    {
        double x = nd (rng);
        for (int p = 0; p < 4; ++p) { const double y = a * (y1[p] + x - x1[p]); x1[p] = x; y1[p] = y; x = y; }
        if (lpHz > 0) { l1 = c * l1 + (1 - c) * x; l2 = c * l2 + (1 - c) * l1; x = l2; }
        v = x;
    }
    return b;
}

static void add (Stem& st, double tBeats, double durS, const std::function<double(double)>& f)
{
    const size_t on = size_t (tBeats * beat * fs) + lead;
    st.onsets.push_back (on);
    for (size_t i = 0; i < size_t (durS * fs) && on + i < len; ++i) st.s[on + i] += f (double (i) / fs);
}

enum { KICK, BASS, SNARE, CLAP, CHAT, OHAT, CRASH, NSTEMS };

static std::vector<Stem> makeStems()
{
    std::vector<Stem> st (NSTEMS);
    const char* names[NSTEMS] = { "kick", "bass", "snare", "clap", "closed hat", "open hat", "crash" };
    for (int i = 0; i < NSTEMS; ++i) st[size_t (i)] = { names[i], Sig (len, 0.0), {} };
    unsigned seed = 11;
    for (int bar = 0; bar < bars; ++bar)
    {
        const double b0 = bar * 4.0;
        for (double kb : { 0.0, 1.75, 2.5 })
            add (st[KICK], b0 + kb, 0.7, [] (double t) {
                const double ph = 2 * kPi * (48.0 * t + 112.0 * 0.035 * (1 - std::exp (-t / 0.035)));   // 160→48 Hz
                return 0.75 * std::sin (ph) * std::exp (-t / 0.20)
                     + 0.20 * std::sin (2 * kPi * 118 * t) * std::exp (-t / 0.05)                        // body mode
                     + 0.22 * std::sin (2 * kPi * 3400 * t) * std::exp (-t / 0.003);                      // beater click
            });
        // Sustained bass under the kick; the last note of each bar glides like an 808.
        const double notes[4] = { 55.0, 49.0, 62.0, 52.0 };
        for (int n = 0; n < 4; ++n)
        {
            const double f0 = notes[(bar + n) % 4];
            const bool glide = n == 3;
            add (st[BASS], b0 + n, 0.49, [f0, glide] (double t) {
                const double f = glide ? f0 * (1.0 + 0.35 * std::exp (-t / 0.08)) : f0;
                const double ph = 2 * kPi * (glide ? (f0 * t + f0 * 0.35 * 0.08 * (1 - std::exp (-t / 0.08))) : f * t);
                const double env = std::min (1.0, t / 0.01) * std::min (1.0, (0.49 - t) / 0.02);
                return 0.30 * env * (std::sin (ph) + 0.3 * std::sin (2 * ph));
            });
        }
        for (double sb : { 1.0, 3.0 })
        {
            auto nz = std::make_shared<Sig> (noiseBuf (seed++, 1000, 0));
            add (st[SNARE], b0 + sb, 0.5, [nz] (double t) {
                return 0.30 * std::sin (2 * kPi * 185 * t) * std::exp (-t / 0.06)
                     + 0.18 * std::sin (2 * kPi * 330 * t) * std::exp (-t / 0.04)
                     + 0.12 * std::sin (2 * kPi * 450 * t) * std::exp (-t / 0.03)
                     + 0.06 * std::sin (2 * kPi * 3800 * t) * std::exp (-t / 0.08)
                     + 0.30 * (*nz)[size_t (t * fs)] * std::exp (-t / 0.12);
            });
        }
        // Clap layered on the beat-3 snare, 3 ms late: three bursts 7 ms apart + short tail.
        {
            auto nz = std::make_shared<Sig> (noiseBuf (seed++, 800, 4000));
            add (st[CLAP], b0 + 3.0 + 0.006, 0.3, [nz] (double t) {
                double e = 0;
                for (int k = 0; k < 3; ++k) { const double tk = t - 0.007 * k; if (tk >= 0) e += std::exp (-tk / 0.004); }
                e += 0.5 * std::exp (-t / 0.07);
                return 1.2 * (*nz)[size_t (t * fs)] * e;
            });
        }
        for (int e = 0; e < 8; ++e)
        {
            if (e == 7)
            {
                auto nz = std::make_shared<Sig> (noiseBuf (seed++, 7000, 0));
                add (st[OHAT], b0 + 3.5, 0.5, [nz] (double t) { return 0.25 * (*nz)[size_t (t * fs)] * std::exp (-t / 0.25); });
                continue;
            }
            auto nz = std::make_shared<Sig> (noiseBuf (seed++, 7000, 0));
            add (st[CHAT], b0 + 0.5 * e, 0.12, [nz] (double t) { return 0.28 * (*nz)[size_t (t * fs)] * std::exp (-t / 0.02); });
        }
        if (bar % 2 == 0)   // crash on beat 2 → its tail is under the open hat at 3.5
        {
            auto nz = std::make_shared<Sig> (noiseBuf (seed++, 2500, 0));
            add (st[CRASH], b0 + 2.0, 2.5, [nz] (double t) { return 0.22 * (*nz)[size_t (t * fs)] * std::exp (-t / 0.9) * std::min (1.0, t / 0.003); });
        }
    }
    return st;
}

// ================================================================ processors
static double db2lin (double db) { return std::pow (10.0, db / 20.0); }

static Sig gainCurve (const std::vector<size_t>& onsets, const std::function<double(double)>& g)
{
    Sig out (len, 1.0);
    for (size_t h = 0; h < onsets.size(); ++h)
    {
        const size_t end = h + 1 < onsets.size() ? onsets[h + 1] : len;
        for (size_t i = onsets[h]; i < end; ++i) out[i] = g (double (i - onsets[h]) / fs);
    }
    return out;
}

static Sig compressorGain (const Sig& x)
{
    const double att = std::exp (-1.0 / (0.001 * fs)), rel = std::exp (-1.0 / (0.060 * fs));
    Sig g (x.size()); double env = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double a = std::abs (x[i]);
        env = a > env ? att * env + (1 - att) * a : rel * env + (1 - rel) * a;
        const double l = 20 * std::log10 (env + 1e-12);
        g[i] = l > -20 ? db2lin ((-20 + (l + 20) / 4.0) - l) : 1.0;
    }
    return g;
}

struct Test
{
    std::string name; int target;
    enum Kind { Gain, Comp, Sat, TopEq } kind;
    std::function<double(double)> g;
};

static const double kTopGain = db2lin (6);
static Sig topBand (const Sig& x)   // 1.5–5 kHz, linear phase, aligned
{
    const Sig a = filterAligned (x, lp (5000)), b = filterAligned (x, lp (1500));
    Sig y (x.size()); for (size_t i = 0; i < x.size(); ++i) y[i] = a[i] - b[i];
    return y;
}

static Sig applyP (const Test& t, const Sig& x, const Sig& gc)
{
    Sig y (x.size());
    switch (t.kind)
    {
        case Test::Gain:  for (size_t i = 0; i < x.size(); ++i) y[i] = gc[i] * x[i]; break;
        case Test::Comp:  { const Sig g = compressorGain (x); for (size_t i = 0; i < x.size(); ++i) y[i] = g[i] * x[i]; } break;
        case Test::Sat:   for (size_t i = 0; i < x.size(); ++i) y[i] = std::tanh (3 * x[i]) / 3; break;
        case Test::TopEq: { const Sig b = topBand (x); for (size_t i = 0; i < x.size(); ++i) y[i] = x[i] + (kTopGain - 1) * b[i]; } break;
    }
    return y;
}

// ================================================================ STFT part (one resolution, one signal part)
struct StftPart
{
    size_t N, hop, F;
    Sig w;
    std::vector<std::vector<cd>> X;           // mixture frames
    std::vector<std::vector<double>> share;   // target share per frame/bin
    Sig est;                                  // soft-mask estimate of the target

    StftPart (size_t n, size_t h) : N (n), hop (h), F ((len + N) / h + 1), w (n)
    {
        for (size_t i = 0; i < N; ++i) w[i] = std::sqrt (0.5 - 0.5 * std::cos (2 * kPi * double (i) / double (N)));
    }
    double centre (size_t j) const { return double (j * hop) + N / 2.0 - double (N); }

    std::vector<std::vector<cd>> analyse (const Sig& x) const
    {
        std::vector<std::vector<cd>> fr (F, std::vector<cd> (N));
        for (size_t j = 0; j < F; ++j)
        {
            for (size_t i = 0; i < N; ++i)
            {
                const long p = long (j * hop + i) - long (N);
                fr[j][i] = (p >= 0 && p < long (len)) ? x[size_t (p)] * w[i] : 0.0;
            }
            fft (fr[j], false);
        }
        return fr;
    }
    Sig synth (std::vector<std::vector<cd>> fr) const
    {
        Sig y (len + 2 * N + hop, 0.0), nrm (y.size(), 0.0);
        for (size_t j = 0; j < F; ++j)
        {
            fft (fr[j], true);
            for (size_t i = 0; i < N && j * hop + i < y.size(); ++i) { y[j * hop + i] += fr[j][i].real() * w[i]; nrm[j * hop + i] += w[i] * w[i]; }
        }
        Sig out (len); for (size_t i = 0; i < len; ++i) out[i] = y[i + N] / nrm[i + N];
        return out;
    }

    void prepare (const Sig& mixPart, const std::vector<Sig>& stemParts, int target, bool oracle)
    {
        X = analyse (mixPart);
        std::vector<std::vector<std::vector<cd>>> S;
        for (auto& s : stemParts) S.push_back (analyse (s));
        std::vector<std::vector<double>> avg (S.size(), std::vector<double> (N, 0.0)), env (S.size(), std::vector<double> (F, 0.0));
        if (! oracle)
            for (size_t s = 0; s < S.size(); ++s)
            {
                double tot = 1e-30;
                for (size_t j = 0; j < F; ++j) for (size_t k = 0; k < N; ++k) { const double p = std::norm (S[s][j][k]); avg[s][k] += p; env[s][j] += p; tot += p; }
                for (auto& v : avg[s]) v /= tot;
            }
        share.assign (F, std::vector<double> (N));
        for (size_t j = 0; j < F; ++j)
            for (size_t k = 0; k < N; ++k)
            {
                double num = 0, den = 1e-20;
                for (size_t s = 0; s < S.size(); ++s)
                {
                    const double p = oracle ? std::norm (S[s][j][k]) : avg[s][k] * env[s][j];
                    den += p; if (int (s) == target) num = p;
                }
                share[j][k] = num / den;
            }
        auto m = X;
        for (size_t j = 0; j < F; ++j) for (size_t k = 0; k < N; ++k) m[j][k] *= share[j][k];
        est = synth (m);
    }

    // A-type output: Y = X (1 + a (G(k) g(n) - 1))
    Sig mask (const Sig& gTime, const std::function<double(double)>& Gfreq) const
    {
        auto Y = X;
        std::vector<double> G (N);
        for (size_t k = 0; k < N; ++k) { const double f = double (std::min (k, N - k)) * fs / double (N); G[k] = Gfreq ? Gfreq (f) : 1.0; }
        for (size_t j = 0; j < F; ++j)
        {
            const double gj = gTime[size_t (std::clamp (centre (j), 0.0, double (len - 1)))];
            for (size_t k = 0; k < N; ++k) Y[j][k] *= 1.0 + share[j][k] * (G[k] * gj - 1.0);
        }
        return synth (Y);
    }
};

// ================================================================ metrics
static double dB (double r) { return 10 * std::log10 (r + 1e-30); }
static double sq (const Sig& a, const Sig& b) { double s = 0; for (size_t i = 0; i < a.size(); ++i) s += (a[i] - b[i]) * (a[i] - b[i]); return s; }

struct Score { double l2, spec, pre; };
static Score score (const Sig& y, const Sig& ideal, const Sig& mix, const std::vector<size_t>& onsets)
{
    Score s;
    const double change = sq (ideal, mix);
    s.l2 = dB (sq (y, ideal) / change);
    double pre = 0;
    for (auto on : onsets) for (size_t i = on - size_t (0.025 * fs); i < on - size_t (0.001 * fs); ++i) pre += (y[i] - ideal[i]) * (y[i] - ideal[i]);
    s.pre = dB (pre / change);
    // phase-insensitive: magnitude differences on a 2048/512 STFT
    StftPart m (2048, 512);
    const auto Y = m.analyse (y), I = m.analyse (ideal), X = m.analyse (mix);
    double num = 0, den = 1e-30;
    for (size_t j = 0; j < Y.size(); ++j)
        for (size_t k = 0; k <= 1024; ++k)
        {
            const double yi = std::abs (Y[j][k]), ii = std::abs (I[j][k]), xi = std::abs (X[j][k]);
            num += (yi - ii) * (yi - ii); den += (ii - xi) * (ii - xi);
        }
    s.spec = dB (num / den);
    return s;
}

// ================================================================ main
int main()
{
    const auto stems = makeStems();
    Sig mix (len, 0.0);
    for (auto& s : stems) for (size_t i = 0; i < len; ++i) mix[i] += s.s[i];

    const std::vector<Test> tests = {
        { "T1 kick level -6 dB (+bass)",       KICK,  Test::Gain,  [] (double) { return db2lin (-6); } },
        { "T2 snare Crack +6 dB (+clap)",      SNARE, Test::Gain,  [] (double t) {
              if (t < 0.008) return db2lin (6);
              if (t < 0.025) return db2lin (6 * 0.5 * (1 + std::cos (kPi * (t - 0.008) / 0.017)));
              return 1.0; } },
        { "T3 open-hat Decay (+crash tail)",   OHAT,  Test::Gain,  [] (double t) { return t < 0.03 ? 1.0 : db2lin (-30 * std::min (1.0, (t - 0.03) / 0.12)); } },
        { "T4 snare Comp 4:1 (+clap)",         SNARE, Test::Comp,  nullptr },
        { "T5 kick Saturation (+bass)",        KICK,  Test::Sat,   nullptr },
        { "T6 clap level -6 dB (on snare)",    CLAP,  Test::Gain,  [] (double) { return db2lin (-6); } },
        { "T7 kick Top +6 dB (1.5-5k, hats)",  KICK,  Test::TopEq, nullptr },
    };

    // Band splits used by B and the dual-res engines (all linear, so parts of the mix = sum of parts of stems).
    std::vector<std::vector<Sig>> stemBands; for (auto& s : stems) stemBands.push_back (splitBands (s.s));
    const auto mixBands = splitBands (mix);
    Sig mixLo, mixHi; splitAt (mix, 1500, mixLo, mixHi);
    std::vector<Sig> stLo (NSTEMS), stHi (NSTEMS);
    for (size_t s = 0; s < NSTEMS; ++s) splitAt (stems[s].s, 1500, stLo[s], stHi[s]);
    std::vector<Sig> stemSigs; for (auto& s : stems) stemSigs.push_back (s.s);

    std::mt19937 rng (7); std::uniform_real_distribution<double> jit (-0.002, 0.002);

    struct Mode { bool oracle; bool jitter; const char* label; };
    const Mode modes[] = {
        { true,  false, "ORACLE shares, exact timing (upper bound)" },
        { false, false, "TEMPLATE shares, exact timing (realistic shares)" },
        { false, true,  "TEMPLATE shares, onset jitter +-2 ms (realistic shares + detection error)" },
    };
    const char* eng[7] = { "A 1024", "As 256", "Ad dual", "B bands", "C hybrid", "Cd hyb-dual", "Cd1k fits30ms" };

    for (const auto& md : modes)
    {
        std::printf ("=== %s ===\n", md.label);
        std::printf ("cells: L2 error / spectral error (dB, lower = better); '*' = smears >= -30 dB before the hit\n");
        std::printf ("%-34s", "Test");
        for (auto e : eng) std::printf (" | %-15s", e);
        std::printf ("\n");

        for (const auto& t : tests)
        {
            const auto& tgt = stems[size_t (t.target)];
            std::vector<size_t> engOn = tgt.onsets;
            if (md.jitter) for (auto& o : engOn) o = size_t (long (o) + long (jit (rng) * fs));
            const Sig gcTrue = t.g ? gainCurve (tgt.onsets, t.g) : Sig (len, 1.0);
            const Sig gcEng  = t.g ? gainCurve (engOn, t.g)      : Sig (len, 1.0);

            Sig ideal = mix;
            { const Sig p = applyP (t, tgt.s, gcTrue); for (size_t i = 0; i < len; ++i) ideal[i] += p[i] - tgt.s[i]; }

            auto timeDomain = [&] (const Sig& est) {
                const Sig p = applyP (t, est, gcEng);
                Sig y = mix; for (size_t i = 0; i < len; ++i) y[i] += p[i] - est[i];
                return y;
            };
            const std::function<double(double)> Gtop = [] (double f) {
                return 1.0 + (kTopGain - 1) * (zeroPhase (lp (5000), f) - zeroPhase (lp (1500), f));
            };
            auto maskOut = [&] (const StftPart& p, const Sig& estForComp) {
                if (t.kind == Test::Sat) return mix;   // a gain mask cannot make harmonics
                const Sig g = t.kind == Test::Comp ? compressorGain (estForComp) : gcEng;
                return p.mask (g, t.kind == Test::TopEq ? Gtop : nullptr);
            };

            Sig out[7];
            // A 1024 and C
            StftPart p1 (1024, 128); p1.prepare (mix, stemSigs, t.target, md.oracle);
            out[0] = maskOut (p1, p1.est);
            out[4] = timeDomain (p1.est);
            // As 256
            StftPart p2 (256, 64); p2.prepare (mix, stemSigs, t.target, md.oracle);
            out[1] = maskOut (p2, p2.est);
            // Ad / Cd dual resolution
            StftPart lo (2048, 256), hi (256, 64);
            lo.prepare (mixLo, stLo, t.target, md.oracle); hi.prepare (mixHi, stHi, t.target, md.oracle);
            Sig estD (len); for (size_t i = 0; i < len; ++i) estD[i] = lo.est[i] + hi.est[i];
            {
                const Sig a = maskOut (lo, estD), b = maskOut (hi, estD);
                out[2].resize (len);
                for (size_t i = 0; i < len; ++i) out[2][i] = t.kind == Test::Sat ? mix[i] : a[i] + b[i] - 0.0;
                if (t.kind != Test::Sat)   // each part's mask output already contains that part of the mix
                    for (size_t i = 0; i < len; ++i) out[2][i] = a[i] + b[i];
            }
            out[5] = timeDomain (estD);
            {
                StftPart lo1 (1024, 128);
                lo1.prepare (mixLo, stLo, t.target, md.oracle);
                Sig e1 (len); for (size_t i = 0; i < len; ++i) e1[i] = lo1.est[i] + hi.est[i];
                out[6] = timeDomain (e1);
            }
            // B bands
            {
                Sig est (len, 0.0);
                for (size_t b = 0; b < 6; ++b)
                {
                    std::vector<Sig> e (NSTEMS, Sig (len));
                    for (size_t s = 0; s < NSTEMS; ++s)
                    {
                        if (md.oracle) for (size_t i = 0; i < len; ++i) e[s][i] = stemBands[s][b][i] * stemBands[s][b][i];
                        else
                        {
                            double be = 0, te = 1e-30;
                            for (size_t i = 0; i < len; ++i) { be += stemBands[s][b][i] * stemBands[s][b][i]; te += stems[s].s[i] * stems[s].s[i]; }
                            for (size_t i = 0; i < len; ++i) e[s][i] = (be / te) * stems[s].s[i] * stems[s].s[i];
                        }
                        const double a = std::exp (-1.0 / (0.002 * fs));   // zero-phase 2 ms smoothing
                        for (size_t i = 1; i < len; ++i) e[s][i] = a * e[s][i - 1] + (1 - a) * e[s][i];
                        for (size_t i = len - 1; i-- > 0;) e[s][i] = a * e[s][i + 1] + (1 - a) * e[s][i];
                    }
                    for (size_t i = 0; i < len; ++i)
                    {
                        double den = 1e-20; for (auto& es : e) den += es[i];
                        est[i] += e[size_t (t.target)][i] / den * mixBands[b][i];
                    }
                }
                out[3] = timeDomain (est);
            }

            std::printf ("%-34s", t.name.c_str());
            for (int e = 0; e < 7; ++e)
            {
                const bool na = t.kind == Test::Sat && (e == 0 || e == 1 || e == 2);
                const Score s = score (out[e], ideal, mix, tgt.onsets);
                char cell[40];
                if (na) std::snprintf (cell, sizeof cell, "n/a");
                else    std::snprintf (cell, sizeof cell, "%5.1f/%5.1f%s", s.l2, s.spec, s.pre > -30 ? "*" : "");
                std::printf (" | %-15s", cell);
            }
            std::printf ("\n");
            std::fflush (stdout);
        }
        std::printf ("\n");
    }
    std::printf ("L2 error = energy(out - ideal) / energy(ideal - input). Spectral error = same on STFT magnitudes\n"
                 "(phase-insensitive). -10 dB = change 90%% right, -20 dB = 99%%. '*' = pre-echo: error energy in the\n"
                 "1-25 ms before target hits is above -30 dB of the intended change. n/a = a gain mask can't create\n"
                 "harmonics (A + time-domain nonlinearity = C).\n");
}
