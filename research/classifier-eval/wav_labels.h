// File: research/classifier-eval/wav_labels.h
// Shared by research/classifier-eval and research/classifier-train: WAV reading (16-bit PCM or
// 32-bit float, any channel count -> mono), label files ("<sample> <type>[,<type>]"), set listing.
#pragma once

#include <plugin_dsp/DrumTypes.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace evalio
{
struct Label { std::int64_t sample; std::uint32_t types; };

inline bool readWav (const std::string& path, std::vector<float>& out, double& sr)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    char riff[12];
    f.read (riff, 12);
    int channels = 1, bits = 16, format = 1;
    std::uint32_t rate = 44100;
    while (f)
    {
        char id[4];
        std::uint32_t size = 0;
        f.read (id, 4);
        f.read (reinterpret_cast<char*> (&size), 4);
        if (! f) break;
        if (std::strncmp (id, "fmt ", 4) == 0)
        {
            std::vector<char> b (size);
            f.read (b.data(), size);
            format = static_cast<unsigned char> (b[0]) | (static_cast<unsigned char> (b[1]) << 8);
            channels = static_cast<unsigned char> (b[2]);
            std::memcpy (&rate, b.data() + 4, 4);
            bits = static_cast<unsigned char> (b[14]);
            if (format == 0xFFFE && size >= 26) format = static_cast<unsigned char> (b[24]);   // WAVE_FORMAT_EXTENSIBLE
        }
        else if (std::strncmp (id, "data", 4) == 0)
        {
            std::vector<char> b (size);
            f.read (b.data(), size);
            const std::size_t bytes = static_cast<std::size_t> (bits / 8), frames = size / (bytes * static_cast<std::size_t> (channels));
            out.assign (frames, 0.0f);
            for (std::size_t i = 0; i < frames; ++i)
            {
                float acc = 0.0f;
                for (int c = 0; c < channels; ++c)
                {
                    const char* p = b.data() + (i * static_cast<std::size_t> (channels) + static_cast<std::size_t> (c)) * bytes;
                    if (format == 3 && bits == 32) { float v; std::memcpy (&v, p, 4); acc += v; }
                    else if (bits == 16) { std::int16_t v; std::memcpy (&v, p, 2); acc += static_cast<float> (v) / 32768.0f; }
                    else if (bits == 24) { std::int32_t v = (static_cast<unsigned char> (p[0]) << 8) | (static_cast<unsigned char> (p[1]) << 16) | (static_cast<std::int32_t> (static_cast<signed char> (p[2])) << 24); acc += static_cast<float> (v) / 2147483648.0f; }
                }
                out[i] = acc / static_cast<float> (channels);
            }
        }
        else f.seekg (size + (size & 1u), std::ios::cur);
    }
    sr = rate;
    return ! out.empty();
}

inline plugin_dsp::DrumType parseType (const std::string& s)
{
    using plugin_dsp::DrumType;
    static const std::map<std::string, DrumType> m { { "kick", DrumType::kick }, { "snare", DrumType::snare },
        { "closedHat", DrumType::closedHat }, { "openHat", DrumType::openHat }, { "crash", DrumType::crash },
        { "ride", DrumType::ride }, { "rimshot", DrumType::rimshot }, { "clap", DrumType::clap }, { "misc", DrumType::misc } };
    auto it = m.find (s);
    return it == m.end() ? DrumType::misc : it->second;
}

inline std::vector<Label> readLabels (const std::string& path)
{
    std::vector<Label> labels;
    std::ifstream lf (path);
    std::string line;
    while (std::getline (lf, line))
    {
        std::istringstream is (line);
        Label l { 0, 0 };
        std::string types;
        is >> l.sample >> types;
        std::stringstream ts (types);
        std::string t;
        while (std::getline (ts, t, ',')) l.types |= plugin_dsp::bit (parseType (t));
        labels.push_back (l);
    }
    return labels;
}

inline std::vector<std::string> listSet (const std::string& dir)
{
    std::vector<std::string> names;   // std::filesystem, so the tools also build with Visual Studio
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator (dir, ec))
    {
        const std::string n = e.path().filename().string();
        if (n.size() > 4 && n.substr (n.size() - 4) == ".wav") names.push_back (n.substr (0, n.size() - 4));
    }
    std::sort (names.begin(), names.end());
    return names;
}
} // namespace evalio
