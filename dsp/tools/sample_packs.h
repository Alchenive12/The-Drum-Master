// File: dsp/tools/sample_packs.h
// Finds drum samples in sample-pack folders and infers their true label from the file name (then
// the folder names) for dsp/tools/bench.cpp. Test tool only, not part of the plugin.
//
// The label comes from keywords, checked in this order (first match wins):
//   open hat:  "open hat", "openhat", "open_hat", "open-hat", "ohh", " oh ", "_oh_", "oh_"/"_oh" at word edges
//   closed hat:"closed hat", "closedhat", "chh", "hihat", "hi-hat", "hi hat", "hat", "hh"
//   crash:     "crash", "cymbal", "cym"      ride: "ride"
//   rimshot:   "rimshot", "rim", "sidestick", "side stick", "stick"
//   clap:      "clap", "snap"                snare: "snare", "snr", "sd"
//   kick:      "kick", "kik", "bassdrum", "bd", "808"   (808s are kept as their own group in reports)
//   misc:      "tom", "perc", "shaker", "tamb", "cowbell", "conga", "bongo", "block", "clave"
// Loops (folder or name contains "loop", or longer than maxOneShotSeconds) are left out of one-shot
// tests; FX, vocals and anything without a keyword are skipped and listed as "unlabelled".
#pragma once

#include <plugin_dsp/DrumTypes.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

namespace packs
{
namespace fs = std::filesystem;

struct Sample
{
    std::string path, pack, group;     // pack = first folder under the root; group e.g. "kick" or "808"
    plugin_dsp::DrumType type = plugin_dsp::DrumType::misc;
    bool labelled = false, loop = false;
};

inline std::string lower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return s;
}

/** True if `word` occurs in `s` with no letters right before/after it (so "hat" matches "Hat_01"
    but not "that", and "sd" doesn't match "inside"). */
inline bool hasWord (const std::string& s, const std::string& word)
{
    for (std::size_t p = s.find (word); p != std::string::npos; p = s.find (word, p + 1))
    {
        const bool before = p == 0 || ! std::isalpha (static_cast<unsigned char> (s[p - 1]));
        const std::size_t e = p + word.size();
        const bool after = e >= s.size() || ! std::isalpha (static_cast<unsigned char> (s[e]));
        if (before && after) return true;
    }
    return false;
}

inline bool hasAny (const std::string& s, std::initializer_list<const char*> words, bool wholeWord)
{
    for (const char* w : words)
        if (wholeWord ? hasWord (s, w) : s.find (w) != std::string::npos) return true;
    return false;
}

/** Label from one piece of text (a file name or a folder name). Returns false if nothing matched. */
inline bool labelFrom (const std::string& raw, plugin_dsp::DrumType& type, std::string& group)
{
    using plugin_dsp::DrumType;
    const std::string s = lower (raw);
    // "Hi Hat Open", "HH Open 03": open, even though "open" doesn't come first
    if (s.find ("open") != std::string::npos && (hasAny (s, { "hihat", "hi-hat", "hi hat", "hi_hat" }, false) || hasAny (s, { "hat", "hats", "hh" }, true)))
    {
        type = DrumType::openHat;
        group = "open hat";
        return true;
    }
    struct Rule { DrumType t; const char* g; std::initializer_list<const char*> sub; std::initializer_list<const char*> word; };
    const Rule rules[] = {
        { DrumType::openHat,   "open hat",   { "open hat", "openhat", "open_hat", "open-hat", "open hh", "ohh" }, { "oh", "ohat" } },
        { DrumType::closedHat, "closed hat", { "closed hat", "closedhat", "closed_hat", "hihat", "hi-hat", "hi hat", "hi_hat" }, { "hat", "hats", "hh", "chh", "ch" } },
        { DrumType::crash,     "crash",      { "crash", "cymbal" }, { "cym" } },
        { DrumType::ride,      "ride",       { "ride" }, {} },
        { DrumType::rimshot,   "rimshot",    { "rimshot", "sidestick", "side stick", "side_stick" }, { "rim", "rims", "stick" } },
        { DrumType::clap,      "clap",       { "clap" }, { "snap", "snaps" } },
        { DrumType::snare,     "snare",      { "snare" }, { "snr", "sd" } },
        { DrumType::kick,      "808",        { "808" }, {} },
        { DrumType::kick,      "kick",       { "kick", "bassdrum", "bass drum" }, { "kik", "bd" } },
        { DrumType::misc,      "perc",       { "shaker", "tambourine", "cowbell", "conga", "bongo", "woodblock", "clave" }, { "tom", "toms", "perc", "percs", "tamb", "block" } },
    };
    for (const auto& r : rules)
        if (hasAny (s, r.sub, false) || hasAny (s, r.word, true))
        {
            type = r.t;
            group = r.g;
            return true;
        }
    return false;
}

inline std::vector<Sample> scan (const std::string& root)
{
    std::vector<Sample> out;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator (root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment (ec))
    {
        if (ec || ! it->is_regular_file()) continue;
        const auto& p = it->path();
        if (lower (p.extension().string()) != ".wav") continue;
        Sample s;
        s.path = p.string();
        const auto rel = fs::relative (p, root, ec);
        s.pack = rel.begin() != rel.end() && std::next (rel.begin()) != rel.end() ? rel.begin()->string() : std::string ("(root)");
        const std::string relLower = lower (rel.string());
        s.loop = relLower.find ("loop") != std::string::npos;
        // file name first, then folders from the innermost outwards
        s.labelled = labelFrom (p.stem().string(), s.type, s.group);
        for (auto parent = p.parent_path(); ! s.labelled && parent.has_relative_path() && parent != fs::path (root); parent = parent.parent_path())
            s.labelled = labelFrom (parent.filename().string(), s.type, s.group);
        out.push_back (s);
    }
    std::sort (out.begin(), out.end(), [] (const Sample& a, const Sample& b) { return a.path < b.path; });
    return out;
}
} // namespace packs
