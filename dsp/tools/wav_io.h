// File: dsp/tools/wav_io.h
// Minimal WAV I/O for dsp/tools/bench.cpp (test tool only, not part of the plugin).
// Reads PCM 8/16/24/32-bit, 32/64-bit float, and WAVE_FORMAT_EXTENSIBLE, any channel count
// (mixed to mono). Resamples to the benchmark rate with linear interpolation (good enough for
// detection tests). Writes 16-bit mono PCM.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wav
{
struct Audio
{
    std::vector<float> mono;
    double sampleRate = 0.0;
    bool ok = false;
    std::string error;
};

inline std::uint32_t u32 (const unsigned char* p) { return static_cast<std::uint32_t> (p[0]) | (static_cast<std::uint32_t> (p[1]) << 8) | (static_cast<std::uint32_t> (p[2]) << 16) | (static_cast<std::uint32_t> (p[3]) << 24); }
inline std::uint16_t u16 (const unsigned char* p) { return static_cast<std::uint16_t> (p[0] | (p[1] << 8)); }

inline Audio read (const std::string& path)
{
    Audio a;
    std::FILE* f = std::fopen (path.c_str(), "rb");
    if (f == nullptr) { a.error = "cannot open"; return a; }
    std::vector<unsigned char> d;
    unsigned char buf[65536];
    std::size_t n;
    while ((n = std::fread (buf, 1, sizeof (buf), f)) > 0) d.insert (d.end(), buf, buf + n);
    std::fclose (f);
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0)
    { a.error = "not a RIFF/WAVE file"; return a; }

    int format = 0, channels = 0, bits = 0;
    double sr = 0.0;
    const unsigned char* data = nullptr;
    std::size_t dataLen = 0;
    for (std::size_t pos = 12; pos + 8 <= d.size();)
    {
        const std::uint32_t len = u32 (d.data() + pos + 4);
        const unsigned char* body = d.data() + pos + 8;
        const std::size_t avail = std::min<std::size_t> (len, d.size() - (pos + 8));
        if (std::memcmp (d.data() + pos, "fmt ", 4) == 0 && avail >= 16)
        {
            format = u16 (body);
            channels = u16 (body + 2);
            sr = static_cast<double> (u32 (body + 4));
            bits = u16 (body + 14);
            if (format == 0xFFFE && avail >= 26) format = u16 (body + 24);   // extensible: real format in the GUID
        }
        else if (std::memcmp (d.data() + pos, "data", 4) == 0)
        {
            data = body;
            dataLen = avail;
        }
        pos += 8 + len + (len & 1u);
    }
    if (data == nullptr || channels <= 0 || sr <= 0.0) { a.error = "missing fmt or data chunk"; return a; }
    if (! ((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (format == 3 && (bits == 32 || bits == 64))))
    { a.error = "unsupported format " + std::to_string (format) + "/" + std::to_string (bits) + " bit"; return a; }

    const std::size_t bytes = static_cast<std::size_t> (bits / 8), frame = bytes * static_cast<std::size_t> (channels);
    const std::size_t frames = dataLen / frame;
    a.mono.assign (frames, 0.0f);
    for (std::size_t i = 0; i < frames; ++i)
    {
        double sum = 0.0;
        for (int c = 0; c < channels; ++c)
        {
            const unsigned char* p = data + i * frame + static_cast<std::size_t> (c) * bytes;
            double v = 0.0;
            if (format == 3 && bits == 32) { float x; std::memcpy (&x, p, 4); v = x; }
            else if (format == 3) { double x; std::memcpy (&x, p, 8); v = x; }
            else if (bits == 8) v = (static_cast<int> (p[0]) - 128) / 128.0;
            else if (bits == 16) v = static_cast<std::int16_t> (u16 (p)) / 32768.0;
            else if (bits == 24) { std::int32_t x = static_cast<std::int32_t> ((static_cast<std::uint32_t> (p[0]) << 8) | (static_cast<std::uint32_t> (p[1]) << 16) | (static_cast<std::uint32_t> (p[2]) << 24)); v = (x >> 8) / 8388608.0; }
            else v = static_cast<std::int32_t> (u32 (p)) / 2147483648.0;
            sum += v;
        }
        a.mono[i] = static_cast<float> (sum / channels);
    }
    a.sampleRate = sr;
    a.ok = true;
    return a;
}

/** Linear-interpolation resample to `target` Hz (only used to bring all samples to one rate). */
inline std::vector<float> resample (const std::vector<float>& in, double from, double target)
{
    if (in.empty() || from == target) return in;
    const double ratio = from / target;
    const auto n = static_cast<std::size_t> (static_cast<double> (in.size()) / ratio);
    std::vector<float> out (n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double x = static_cast<double> (i) * ratio;
        const auto k = static_cast<std::size_t> (x);
        const double f = x - static_cast<double> (k);
        const float a = in[std::min (k, in.size() - 1)], b = in[std::min (k + 1, in.size() - 1)];
        out[i] = static_cast<float> (a + (b - a) * f);
    }
    return out;
}

inline bool write16 (const std::string& path, const std::vector<float>& mono, double sr)
{
    std::FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr) return false;
    const auto dataBytes = static_cast<std::uint32_t> (mono.size() * 2);
    auto w32 = [&] (std::uint32_t v) { unsigned char b[4] = { static_cast<unsigned char> (v), static_cast<unsigned char> (v >> 8), static_cast<unsigned char> (v >> 16), static_cast<unsigned char> (v >> 24) }; std::fwrite (b, 1, 4, f); };
    auto w16 = [&] (std::uint16_t v) { unsigned char b[2] = { static_cast<unsigned char> (v), static_cast<unsigned char> (v >> 8) }; std::fwrite (b, 1, 2, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (1); w32 (static_cast<std::uint32_t> (sr)); w32 (static_cast<std::uint32_t> (sr) * 2); w16 (2); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (float x : mono)
    {
        const float c = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
        w16 (static_cast<std::uint16_t> (static_cast<std::int16_t> (c * 32767.0f)));
    }
    std::fclose (f);
    return true;
}
} // namespace wav
