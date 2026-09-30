// File: research/engine-shootout/main.cpp
//
// Phase-2 experiment (not part of the plugin build): which processing domain applies per-drum
// settings to a MIXED drum loop most faithfully?
//
//   A  STFT gain mask   (Agents 1+2): Y(k,n) = X(k,n) * (1 + sum_t a_t(k,n) * (g_t(n) - 1))
//   B  6 time-domain bands (Claude): x_t ~ sum_b a_tb(n) * x_b(n),  y = x + P_t(x_t) - x_t
//   C  Hybrid: STFT soft-mask estimate x_t = ISTFT(a_t * X), then y = x + P_t(x_t) - x_t
//
// The loop is synthesised from separate stems, so the IDEAL result is known exactly:
//   ideal = mix - stem_t + P_t(stem_t)
// Every engine gets the same perfect hit timing (oracle onsets), so only the processing domain
// differs. Ownership shares a_t are computed two ways:
//   oracle   - from the real stems' energy per bin/band (upper bound: perfect knowledge)
//   template - from each stem's AVERAGE spectrum x its broadband envelope (what a detector with
//              templates could know; no per-moment spectral detail)
//
// Build: g++ -O2 -std=c++17 main.cpp -o shootout && ./shootout > RESULTS.txt

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

// ---------------------------------------------------------------- FFT (iterative radix-2)
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
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2 * kPi / double (len) * (inverse ? 1 : -1);
        const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            cd w (1);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const cd u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse)
        for (auto& x : a) x /= double (n);
}

// Linear convolution via FFT overlap-add; returns full length x.size() + h.size() - 1.
static Sig convolve (const Sig& x, const Sig& h)
{
    const size_t L = h.size(), B = 8192, N = 16384;
    std::vector<cd> H (N);
    for (size_t i = 0; i < L; ++i) H[i] = h[i];
    fft (H, false);
    Sig y (x.size() + L - 1, 0.0);
    for (size_t start = 0; start < x.size(); start += B)
    {
        std::vector<cd> X (N);
        for (size_t i = 0; i < B && start + i < x.size(); ++i) X[i] = x[start + i];
        fft (X, false);
        for (size_t k = 0; k < N; ++k) X[k] *= H[k];
        fft (X, true);
        for (size_t i = 0; i < N && start + i < y.size(); ++i) y[start + i] += X[i].real();
    }
    return y;
}

// ---------------------------------------------------------------- synthetic stems
struct Hit { size_t onset; };
struct Stem { std::string name; Sig s; std::vector<Hit> hits; };

static const double beat = 0.5;          // 120 bpm
static const int    bars = 4;
static const size_t len  = size_t ((bars * 4 * beat + 1.0) * fs);

static void addHit (Stem& st, double tBeats, const std::function<double(double)>& shape, double durS)
{
    const size_t on = size_t (tBeats * beat * fs) + size_t (0.25 * fs);   // 250 ms lead-in
    st.hits.push_back ({ on });
    for (size_t i = 0; i < size_t (durS * fs) && on + i < len; ++i)
        st.s[on + i] += shape (double (i) / fs);
}

// One-pole high-pass cascade (4 poles) on white noise, per hit, seeded for repeatability.
static std::function<double(double)> hpNoise (unsigned seed, double fc, double decayS, double amp)
{
    auto buf = std::make_shared<Sig> (size_t (2.0 * fs));
    std::mt19937 rng (seed);
    std::normal_distribution<double> nd (0.0, 1.0);
    const double a = std::exp (-2 * kPi * fc / fs);
    double x1[4] = {}, y1[4] = {};
    for (auto& v : *buf)
    {
        double x = nd (rng);
        for (int p = 0; p < 4; ++p) { const double y = a * (y1[p] + x - x1[p]); x1[p] = x; y1[p] = y; x = y; }
        v = x;
    }
    return [buf, decayS, amp] (double t) { return amp * (*buf)[size_t (t * fs)] * std::exp (-t / decayS); };
}

static std::vector<Stem> makeStems()
{
    Stem kick { "kick", Sig (len, 0.0), {} }, snare { "snare", Sig (len, 0.0), {} },
         chat { "closed hat", Sig (len, 0.0), {} }, ohat { "open hat", Sig (len, 0.0), {} };
    unsigned seed = 1;
    for (int bar = 0; bar < bars; ++bar)
    {
        const double b0 = bar * 4.0;
        for (double kb : { 0.0, 2.5 })
            addHit (kick, b0 + kb, [] (double t) {
                const double ph = 2 * kPi * (50.0 * t + 100.0 * 0.03 * (1 - std::exp (-t / 0.03)));
                return 0.8 * std::sin (ph) * std::exp (-t / 0.18) + 0.25 * std::exp (-t / 0.002) * std::sin (2 * kPi * 3000 * t);
            }, 0.6);
        for (double sb : { 1.0, 3.0 })
        {
            auto noise = hpNoise (seed++, 1000.0, 0.12, 0.35);
            addHit (snare, b0 + sb, [noise] (double t) {
                return 0.45 * std::sin (2 * kPi * 190 * t) * std::exp (-t / 0.06) + noise (t);
            }, 0.5);
        }
        for (int e = 0; e < 8; ++e)
        {
            if (e == 7) { addHit (ohat, b0 + 3.5, hpNoise (seed++, 7000.0, 0.25, 0.25), 0.5); continue; }
            addHit (chat, b0 + 0.5 * e, hpNoise (seed++, 7000.0, 0.02, 0.3), 0.12);
        }
    }
    return { kick, snare, chat, ohat };
}

// ---------------------------------------------------------------- processors P_t
// Gain curve from oracle onsets: g(tau) with tau = seconds since the latest onset of the target.
static Sig gainCurve (const Stem& st, const std::function<double(double)>& g)
{
    Sig out (len, 1.0);
    for (size_t h = 0; h < st.hits.size(); ++h)
    {
        const size_t on = st.hits[h].onset;
        const size_t end = h + 1 < st.hits.size() ? st.hits[h + 1].onset : len;
        for (size_t i = on; i < end; ++i) out[i] = g (double (i - on) / fs);
    }
    return out;
}

static double db2lin (double db) { return std::pow (10.0, db / 20.0); }

// Feed-forward compressor gain computed on the signal it will be applied to.
static Sig compressorGain (const Sig& x)
{
    const double att = std::exp (-1.0 / (0.001 * fs)), rel = std::exp (-1.0 / (0.060 * fs));
    const double thr = -20.0, ratio = 4.0;
    Sig g (x.size());
    double env = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double a = std::abs (x[i]);
        env = a > env ? att * env + (1 - att) * a : rel * env + (1 - rel) * a;
        const double l = 20.0 * std::log10 (env + 1e-12);
        g[i] = l > thr ? db2lin ((thr + (l - thr) / ratio) - l) : 1.0;
    }
    return g;
}

struct Test
{
    std::string name;
    int target;                  // stem index
    enum Kind { Gain, Comp, Sat } kind;
    std::function<double(double)> g;   // for Gain tests
};

// Apply P_t in the time domain. For Comp the detector runs on x itself.
static Sig applyP (const Test& t, const Sig& x, const Sig& gc)
{
    Sig y (x.size());
    if (t.kind == Test::Gain) for (size_t i = 0; i < x.size(); ++i) y[i] = gc[i] * x[i];
    if (t.kind == Test::Comp) { const Sig g = compressorGain (x); for (size_t i = 0; i < x.size(); ++i) y[i] = g[i] * x[i]; }
    if (t.kind == Test::Sat)  for (size_t i = 0; i < x.size(); ++i) y[i] = std::tanh (3.0 * x[i]) / 3.0;
    return y;
}

// ---------------------------------------------------------------- STFT (sqrt-Hann analysis + synthesis)
struct Stft
{
    size_t N, hop;
    Sig w;
    std::vector<std::vector<cd>> frames;   // [frame][bin]

    Stft (size_t n, size_t h) : N (n), hop (h), w (n)
    {
        for (size_t i = 0; i < N; ++i) w[i] = std::sqrt (0.5 - 0.5 * std::cos (2 * kPi * double (i) / double (N)));
    }
    size_t numFrames() const { return (len + 2 * N - N) / hop + 1; }
    double centre (size_t j) const { return double (j * hop) + N / 2.0 - double (N); }   // sample index in x

    void analyse (const Sig& x)
    {
        frames.assign (numFrames(), std::vector<cd> (N));
        for (size_t j = 0; j < frames.size(); ++j)
            for (size_t i = 0; i < N; ++i)
            {
                const long p = long (j * hop + i) - long (N);
                frames[j][i] = (p >= 0 && p < long (len)) ? x[size_t (p)] * w[i] : 0.0;
            }
        for (auto& f : frames) fft (f, false);
    }
    Sig synthesise (std::vector<std::vector<cd>> fr) const
    {
        Sig y (len + 2 * N, 0.0), norm (len + 2 * N, 0.0);
        for (size_t j = 0; j < fr.size(); ++j)
        {
            fft (fr[j], true);
            for (size_t i = 0; i < N && j * hop + i < y.size(); ++i)
            {
                y[j * hop + i] += fr[j][i].real() * w[i];
                norm[j * hop + i] += w[i] * w[i];
            }
        }
        Sig out (len);
        for (size_t i = 0; i < len; ++i) out[i] = y[i + N] / norm[i + N];
        return out;
    }
};

// ---------------------------------------------------------------- 6 complementary linear-phase bands
static const double kXover[5] = { 120, 400, 1500, 5000, 10000 };
static const size_t kFirLen = 2047;   // odd, delay 1023 samples (21.3 ms at 48 kHz)

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
        h[n] = sinc * bl;
        sum += h[n];
    }
    for (auto& v : h) v /= sum;
    return h;
}

static std::vector<Sig> splitBands (const Sig& x)
{
    static std::vector<Sig> lps;
    if (lps.empty()) for (double fc : kXover) lps.push_back (lowpass (fc));
    const size_t M = (kFirLen - 1) / 2;
    std::vector<Sig> lp;
    for (auto& h : lps)
    {
        const Sig full = convolve (x, h);
        lp.emplace_back (full.begin() + long (M), full.begin() + long (M + len));   // delay-compensated
    }
    std::vector<Sig> b (6, Sig (len));
    for (size_t i = 0; i < len; ++i)
    {
        b[0][i] = lp[0][i];
        for (int k = 1; k < 5; ++k) b[size_t (k)][i] = lp[size_t (k)][i] - lp[size_t (k - 1)][i];
        b[5][i] = x[i] - lp[4][i];
    }
    return b;
}

// Zero-phase one-pole smoothing (forward + backward) ~ what lookahead allows.
static void smooth (Sig& e, double tauS)
{
    const double a = std::exp (-1.0 / (tauS * fs));
    for (size_t i = 1; i < e.size(); ++i) e[i] = a * e[i - 1] + (1 - a) * e[i];
    for (size_t i = e.size() - 1; i-- > 0;) e[i] = a * e[i + 1] + (1 - a) * e[i];
}

// ---------------------------------------------------------------- metrics
static double sumSq (const Sig& a, const Sig& b)
{
    double s = 0; for (size_t i = 0; i < a.size(); ++i) s += (a[i] - b[i]) * (a[i] - b[i]); return s;
}
static double dB (double r) { return 10.0 * std::log10 (r + 1e-30); }

struct Score { double err, pre; };
static Score score (const Sig& y, const Sig& ideal, const Sig& mix, const Stem& target)
{
    const double change = sumSq (ideal, mix);
    double pre = 0;
    for (auto& h : target.hits)
        for (size_t i = h.onset - size_t (0.025 * fs); i < h.onset - size_t (0.001 * fs); ++i)
            pre += (y[i] - ideal[i]) * (y[i] - ideal[i]);
    return { dB (sumSq (y, ideal) / change), dB (pre / change) };
}

// ---------------------------------------------------------------- main
int main()
{
    const auto stems = makeStems();
    Sig mix (len, 0.0);
    for (auto& st : stems) for (size_t i = 0; i < len; ++i) mix[i] += st.s[i];

    // --- transparency of the two splitters
    {
        const auto b = splitBands (mix);
        double m = 0; for (size_t i = 0; i < len; ++i) { double s = 0; for (auto& bb : b) s += bb[i]; m = std::max (m, std::abs (s - mix[i])); }
        Stft st (1024, 128); st.analyse (mix);
        const Sig r = st.synthesise (st.frames);
        double m2 = 0; for (size_t i = 0; i < len; ++i) m2 = std::max (m2, std::abs (r[i] - mix[i]));
        std::printf ("TRANSPARENCY (all settings neutral): 6-band split max error %.1f dBFS, STFT resynthesis max error %.1f dBFS\n\n",
                     20 * std::log10 (m + 1e-30), 20 * std::log10 (m2 + 1e-30));
    }

    const std::vector<Test> tests = {
        { "T1 kick level -6 dB",               0, Test::Gain, [] (double) { return db2lin (-6); } },
        { "T2 snare Crack +6 dB (0-8 ms)",     1, Test::Gain, [] (double t) {
              if (t < 0.008) return db2lin (6);
              if (t < 0.025) return db2lin (6 * 0.5 * (1 + std::cos (kPi * (t - 0.008) / 0.017)));
              return 1.0; } },
        { "T3 open-hat Decay shorter",         3, Test::Gain, [] (double t) {
              if (t < 0.03) return 1.0;
              return db2lin (-30 * std::min (1.0, (t - 0.03) / 0.12)); } },
        { "T4 snare Compression 4:1",          1, Test::Comp, nullptr },
        { "T5 kick Saturation (tanh)",         0, Test::Sat,  nullptr },
    };

    // Precompute analysis of stems for shares.
    struct Cfg { size_t N, hop; std::string label; };
    const std::vector<Cfg> stftCfgs = { { 1024, 128, "A1: N1024 hop128" }, { 1024, 256, "A2: N1024 hop256" } };

    std::vector<std::vector<Sig>> stemBands;
    for (auto& st : stems) stemBands.push_back (splitBands (st.s));
    const auto mixBands = splitBands (mix);

    for (int mode = 0; mode < 2; ++mode)
    {
        const bool oracle = mode == 0;
        std::printf ("=== SHARES: %s ===\n", oracle ? "ORACLE (perfect per-moment knowledge, upper bound)"
                                                     : "TEMPLATE (average spectrum x envelope, realistic)");
        std::printf ("%-32s | %-26s | %-26s | %-26s | %-26s\n", "Test (error vs ideal, lower=better)",
                     "A STFT mask hop128", "A STFT mask hop256", "B 6 time bands", "C hybrid (STFT est.+P)");
        std::printf ("%-32s | %-26s | %-26s | %-26s | %-26s\n", "", "total / pre-echo dB", "total / pre-echo dB",
                     "total / pre-echo dB", "total / pre-echo dB");

        for (auto& t : tests)
        {
            const Stem& tgt = stems[size_t (t.target)];
            const Sig gc = t.kind == Test::Gain ? gainCurve (tgt, t.g) : Sig (len, 1.0);

            Sig ideal = mix;
            { const Sig p = applyP (t, tgt.s, gc); for (size_t i = 0; i < len; ++i) ideal[i] += p[i] - tgt.s[i]; }

            std::string cells[4];

            // ---- A (two hop sizes) and C (hop 128)
            Sig hybridEstimate;
            for (size_t c = 0; c < stftCfgs.size(); ++c)
            {
                Stft X (stftCfgs[c].N, stftCfgs[c].hop);
                X.analyse (mix);
                std::vector<Stft> S;
                for (auto& st : stems) { S.emplace_back (stftCfgs[c].N, stftCfgs[c].hop); S.back().analyse (st.s); }

                // share a_t(k,j)
                const size_t F = X.frames.size(), N = stftCfgs[c].N;
                std::vector<std::vector<double>> share (F, std::vector<double> (N));
                std::vector<std::vector<double>> avg (stems.size(), std::vector<double> (N, 0.0));
                std::vector<std::vector<double>> env (stems.size(), std::vector<double> (F, 0.0));
                if (! oracle)
                    for (size_t s = 0; s < stems.size(); ++s)
                    {
                        double tot = 0;
                        for (size_t j = 0; j < F; ++j)
                            for (size_t k = 0; k < N; ++k) { const double p = std::norm (S[s].frames[j][k]); avg[s][k] += p; env[s][j] += p; tot += p; }
                        for (auto& v : avg[s]) v /= tot;
                    }
                for (size_t j = 0; j < F; ++j)
                    for (size_t k = 0; k < N; ++k)
                    {
                        double num = 0, den = 1e-20;
                        for (size_t s = 0; s < stems.size(); ++s)
                        {
                            const double p = oracle ? std::norm (S[s].frames[j][k]) : avg[s][k] * env[s][j];
                            den += p;
                            if (int (s) == t.target) num = p;
                        }
                        share[j][k] = num / den;
                    }

                // C estimate (only for hop 128 config) and compressor gain source for A
                auto masked = X.frames;
                for (size_t j = 0; j < F; ++j) for (size_t k = 0; k < N; ++k) masked[j][k] *= share[j][k];
                const Sig est = X.synthesise (masked);
                if (c == 0) hybridEstimate = est;

                // A: per-frame gain g(n) sampled at the frame centre
                Sig g = gc;
                if (t.kind == Test::Comp) g = compressorGain (est);
                auto Y = X.frames;
                for (size_t j = 0; j < F; ++j)
                {
                    const double ctr = std::clamp (X.centre (j), 0.0, double (len - 1));
                    const double gj = t.kind == Test::Sat ? 1.0 : g[size_t (ctr)];   // Sat: a gain can't make harmonics
                    for (size_t k = 0; k < N; ++k) Y[j][k] *= 1.0 + share[j][k] * (gj - 1.0);
                }
                const Sig yA = X.synthesise (Y);
                const Score sc = score (yA, ideal, mix, tgt);
                char buf[64]; std::snprintf (buf, sizeof buf, "%6.1f / %6.1f%s", sc.err, sc.pre, t.kind == Test::Sat ? " (n/a)" : "");
                cells[c] = buf;
            }

            // ---- B: 6 time-domain bands
            {
                std::vector<Sig> a (6, Sig (len));
                for (int b = 0; b < 6; ++b)
                {
                    std::vector<Sig> e (stems.size(), Sig (len));
                    for (size_t s = 0; s < stems.size(); ++s)
                    {
                        if (oracle)
                            for (size_t i = 0; i < len; ++i) e[s][i] = stemBands[s][size_t (b)][i] * stemBands[s][size_t (b)][i];
                        else
                        {
                            double bandE = 0, totE = 0;
                            for (size_t i = 0; i < len; ++i) { bandE += stemBands[s][size_t (b)][i] * stemBands[s][size_t (b)][i]; totE += stems[s].s[i] * stems[s].s[i]; }
                            for (size_t i = 0; i < len; ++i) e[s][i] = (bandE / totE) * stems[s].s[i] * stems[s].s[i];
                        }
                        smooth (e[s], 0.002);
                    }
                    for (size_t i = 0; i < len; ++i)
                    {
                        double den = 1e-20; for (auto& es : e) den += es[i];
                        a[size_t (b)][i] = e[size_t (t.target)][i] / den;
                    }
                }
                Sig est (len, 0.0);
                for (int b = 0; b < 6; ++b) for (size_t i = 0; i < len; ++i) est[i] += a[size_t (b)][i] * mixBands[size_t (b)][i];
                const Sig p = applyP (t, est, gc);
                Sig yB = mix; for (size_t i = 0; i < len; ++i) yB[i] += p[i] - est[i];
                const Score sc = score (yB, ideal, mix, tgt);
                char buf[64]; std::snprintf (buf, sizeof buf, "%6.1f / %6.1f", sc.err, sc.pre); cells[2] = buf;
            }

            // ---- C: hybrid
            {
                const Sig p = applyP (t, hybridEstimate, gc);
                Sig yC = mix; for (size_t i = 0; i < len; ++i) yC[i] += p[i] - hybridEstimate[i];
                const Score sc = score (yC, ideal, mix, tgt);
                char buf[64]; std::snprintf (buf, sizeof buf, "%6.1f / %6.1f", sc.err, sc.pre); cells[3] = buf;
            }

            std::printf ("%-32s | %-26s | %-26s | %-26s | %-26s\n", t.name.c_str(), cells[0].c_str(), cells[1].c_str(), cells[2].c_str(), cells[3].c_str());
        }
        std::printf ("\n");
    }
    std::printf ("Error = energy(output - ideal) / energy(ideal - input). -20 dB means the intended change is 99%% right.\n"
                 "Pre-echo = the same error, counted only 1-25 ms BEFORE each target hit (smearing ahead of the transient).\n"
                 "(n/a) = an STFT gain mask cannot create saturation harmonics; the number shows the miss when it does nothing.\n");
}
