// File: Source/Parameters.h
// The 97 host parameters of docs/02-parameters.md: one table drives both the JUCE parameter
// layout and the per-block copy into plugin_dsp::DrumParams, so the two can never drift apart.
// IDs are permanent (version hint 1). Header-only on purpose (no CMakeLists change needed).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <plugin_dsp/DrumParams.h>

#include <vector>

namespace params
{
using plugin_dsp::DrumParams;

struct Spec
{
    enum class Kind { real, toggle, choice };
    const char* id;
    const char* name;
    const char* group;   // parameter group (keeps FL's automation menu readable)
    Kind kind;
    float min, max, def;
    const char* unit;    // "dB", "%", "Hz" (real only)
    float skewCentre;    // > 0: logarithmic-feeling range centred here (frequencies)
    void (*apply) (DrumParams&, float);
};

constexpr int versionHint = 1;

inline const juce::StringArray& genreNames()
{
    static const juce::StringArray names { "Rock", "Pop", "EDM", "Metal", "Hip-hop" };
    return names;
}

// Shorthands for the table.
#define DM_DB(id, name, group, lo, hi, def, field) \
    { id, name, group, Spec::Kind::real, lo, hi, def, "dB", 0.0f, [] (DrumParams& p, float v) { p.field = v; } }
#define DM_PCT(id, name, group, lo, hi, def, field) \
    { id, name, group, Spec::Kind::real, lo, hi, def, "%", 0.0f, [] (DrumParams& p, float v) { p.field = v; } }
#define DM_HZ(id, name, group, lo, hi, def, centre, field) \
    { id, name, group, Spec::Kind::real, lo, hi, def, "Hz", centre, [] (DrumParams& p, float v) { p.field = v; } }
#define DM_BOOL(id, name, group, def, field) \
    { id, name, group, Spec::Kind::toggle, 0.0f, 1.0f, def, "", 0.0f, [] (DrumParams& p, float v) { p.field = v > 0.5f; } }

inline const std::vector<Spec>& table()
{
    static const std::vector<Spec> specs {
        // 1. Global
        DM_PCT ("global_mix", "Dry/Wet", "Global", 0, 100, 100, global.mix),
        DM_DB ("global_output", "Output", "Global", -24, 12, 0, global.outputDb),
        DM_PCT ("global_sensitivity", "Detection sensitivity", "Global", 0, 100, 50, global.sensitivity),
        DM_PCT ("global_punch", "Punch", "Global", 0, 100, 0, global.punch),
        DM_PCT ("global_clip", "Clip", "Global", 0, 100, 0, global.clip),
        DM_BOOL ("global_bypass", "Bypass", "Global", 0, global.bypass),

        // 2. Quick mix
        DM_DB ("qm_kick_volume", "Kick volume", "Quick mix", -24, 12, 0, quick.kickVolume),
        DM_PCT ("qm_kick_punch", "Kick Punch", "Quick mix", -100, 100, 0, quick.kickPunch),
        DM_PCT ("qm_kick_weight", "Kick Weight", "Quick mix", -100, 100, 0, quick.kickWeight),
        DM_DB ("qm_snare_volume", "Snare volume", "Quick mix", -24, 12, 0, quick.snareVolume),
        DM_PCT ("qm_snare_snap", "Snare Snap", "Quick mix", -100, 100, 0, quick.snareSnap),
        DM_PCT ("qm_snare_fat", "Snare Fat", "Quick mix", -100, 100, 0, quick.snareFat),
        DM_DB ("qm_hats_volume", "Hats volume", "Quick mix", -24, 12, 0, quick.hatsVolume),
        DM_PCT ("qm_hats_tone", "Hats Tone", "Quick mix", -100, 100, 0, quick.hatsTone),
        DM_PCT ("qm_hats_tame", "Hats Tame", "Quick mix", 0, 100, 0, quick.hatsTame),
        DM_BOOL ("qm_link", "Link", "Quick mix", 0, quick.link),

        // 3. Kick
        DM_DB ("kick_level", "Kick Level", "Kick", -24, 12, 0, kick.level),
        DM_DB ("kick_lowboost", "Kick Low-end boost", "Kick", 0, 12, 0, kick.lowBoost),
        DM_DB ("kick_top", "Kick Top", "Kick", -12, 12, 0, kick.top),
        DM_DB ("kick_bottom", "Kick Bottom", "Kick", -12, 12, 0, kick.bottom),
        DM_PCT ("kick_decay", "Kick Decay", "Kick", -100, 100, 0, kick.decay),
        DM_PCT ("kick_comp", "Kick Compression", "Kick", 0, 100, 0, kick.comp),
        DM_PCT ("kick_sat", "Kick Saturation", "Kick", 0, 100, 0, kick.sat),
        DM_PCT ("kick_clip", "Kick Clipping", "Kick", 0, 100, 0, kick.clip),
        DM_DB ("kick_eq_low", "Kick EQ low", "Kick", -12, 12, 0, kick.eqLow),
        DM_DB ("kick_eq_mid", "Kick EQ mid", "Kick", -12, 12, 0, kick.eqMid),
        DM_HZ ("kick_eq_midfreq", "Kick EQ mid freq", "Kick", 200, 5000, 1000, 1000, kick.eqMidFreq),
        DM_DB ("kick_eq_high", "Kick EQ high", "Kick", -12, 12, 0, kick.eqHigh),

        // Snare
        DM_DB ("snare_level", "Snare Level", "Snare", -24, 12, 0, snare.level),
        DM_PCT ("snare_crack", "Snare Crack", "Snare", -100, 100, 0, snare.crack),
        DM_DB ("snare_body", "Snare Body", "Snare", -12, 12, 0, snare.body),
        DM_PCT ("snare_decay", "Snare Decay", "Snare", -100, 100, 0, snare.decay),
        DM_PCT ("snare_comp", "Snare Compression", "Snare", 0, 100, 0, snare.comp),
        DM_PCT ("snare_sat", "Snare Saturation", "Snare", 0, 100, 0, snare.sat),
        DM_DB ("snare_eq_low", "Snare EQ low", "Snare", -12, 12, 0, snare.eqLow),
        DM_DB ("snare_eq_mid", "Snare EQ mid", "Snare", -12, 12, 0, snare.eqMid),
        DM_HZ ("snare_eq_midfreq", "Snare EQ mid freq", "Snare", 200, 5000, 1000, 1000, snare.eqMidFreq),
        DM_DB ("snare_eq_high", "Snare EQ high", "Snare", -12, 12, 0, snare.eqHigh),
        DM_PCT ("snare_dubstepify", "Dubstepify", "Snare", 0, 100, 0, snare.dubstepify),
        DM_PCT ("snare_popify", "Popify", "Snare", 0, 100, 0, snare.popify),
        DM_PCT ("snare_rockify", "Rockify", "Snare", 0, 100, 0, snare.rockify),
        DM_PCT ("snare_rimify", "Rimify", "Snare", 0, 100, 0, snare.rimify),

        // Closed hat
        DM_DB ("chat_level", "Closed hat Level", "Closed hat", -24, 12, 0, closedHat.level),
        DM_DB ("chat_tone", "Closed hat Tone", "Closed hat", -6, 6, 0, closedHat.tone),
        DM_PCT ("chat_tick", "Closed hat Tick", "Closed hat", -100, 100, 0, closedHat.tick),
        DM_PCT ("chat_decay", "Closed hat Decay", "Closed hat", -100, 100, 0, closedHat.decay),
        DM_PCT ("chat_deharsh", "Closed hat De-harsh", "Closed hat", 0, 100, 0, closedHat.deharsh),
        DM_PCT ("chat_width", "Closed hat Width", "Closed hat", 0, 200, 100, closedHat.width),

        // Open hat
        DM_DB ("ohat_level", "Open hat Level", "Open hat", -24, 12, 0, openHat.level),
        DM_DB ("ohat_tone", "Open hat Tone", "Open hat", -6, 6, 0, openHat.tone),
        DM_DB ("ohat_body", "Open hat Body", "Open hat", -12, 12, 0, openHat.body),
        DM_PCT ("ohat_decay", "Open hat Decay", "Open hat", -100, 100, 0, openHat.decay),
        DM_PCT ("ohat_width", "Open hat Width", "Open hat", 0, 200, 100, openHat.width),
        DM_PCT ("ohat_sat", "Open hat Saturation", "Open hat", 0, 100, 0, openHat.sat),
        DM_PCT ("ohat_deharsh", "Open hat De-harsh", "Open hat", 0, 100, 0, openHat.deharsh),

        // Crash
        DM_DB ("crash_level", "Crash Level", "Crash", -24, 12, 0, crash.level),
        DM_DB ("crash_tone", "Crash Tone", "Crash", -6, 6, 0, crash.tone),
        DM_DB ("crash_body", "Crash Body", "Crash", -12, 12, 0, crash.body),
        DM_PCT ("crash_decay", "Crash Decay", "Crash", -100, 100, 0, crash.decay),
        DM_PCT ("crash_width", "Crash Width", "Crash", 0, 200, 100, crash.width),
        DM_PCT ("crash_deharsh", "Crash De-harsh", "Crash", 0, 100, 0, crash.deharsh),
        DM_HZ ("crash_lowcut", "Crash Low cut", "Crash", 20, 800, 20, 150, crash.lowCut),

        // Ride
        DM_DB ("ride_level", "Ride Level", "Ride", -24, 12, 0, ride.level),
        DM_PCT ("ride_ping", "Ride Ping", "Ride", -100, 100, 0, ride.ping),
        DM_DB ("ride_bell", "Ride Bell", "Ride", -12, 12, 0, ride.bell),
        DM_DB ("ride_wash", "Ride Wash", "Ride", -12, 12, 0, ride.wash),
        DM_DB ("ride_tone", "Ride Tone", "Ride", -6, 6, 0, ride.tone),
        DM_PCT ("ride_width", "Ride Width", "Ride", 0, 200, 100, ride.width),

        // Rimshot
        DM_DB ("rim_level", "Rimshot Level", "Rimshot", -24, 12, 0, rim.level),
        DM_PCT ("rim_click", "Rimshot Click", "Rimshot", -100, 100, 0, rim.click),
        DM_DB ("rim_body", "Rimshot Body", "Rimshot", -12, 12, 0, rim.body),
        DM_DB ("rim_tone", "Rimshot Tone", "Rimshot", -6, 6, 0, rim.tone),
        DM_PCT ("rim_decay", "Rimshot Decay", "Rimshot", -100, 100, 0, rim.decay),
        DM_PCT ("rim_sat", "Rimshot Saturation", "Rimshot", 0, 100, 0, rim.sat),

        // Clap
        DM_DB ("clap_level", "Clap Level", "Clap", -24, 12, 0, clap.level),
        DM_PCT ("clap_attack", "Clap Attack", "Clap", -100, 100, 0, clap.attack),
        DM_DB ("clap_body", "Clap Body", "Clap", -12, 12, 0, clap.body),
        DM_DB ("clap_tone", "Clap Tone", "Clap", -6, 6, 0, clap.tone),
        DM_PCT ("clap_decay", "Clap Decay", "Clap", -100, 100, 0, clap.decay),
        DM_PCT ("clap_width", "Clap Width", "Clap", 0, 200, 100, clap.width),
        DM_PCT ("clap_comp", "Clap Compression", "Clap", 0, 100, 0, clap.comp),
        DM_PCT ("clap_sat", "Clap Saturation", "Clap", 0, 100, 0, clap.sat),

        // Misc
        DM_DB ("misc_level", "Misc Level", "Misc", -24, 12, 0, misc.level),
        DM_DB ("misc_tone", "Misc Tone", "Misc", -6, 6, 0, misc.tone),
        DM_PCT ("misc_attack", "Misc Attack", "Misc", -100, 100, 0, misc.attack),
        DM_PCT ("misc_decay", "Misc Decay", "Misc", -100, 100, 0, misc.decay),

        // 4. Advanced global
        DM_PCT ("adv_drive", "Distortion drive", "Advanced", 0, 100, 0, advanced.drive),
        DM_PCT ("adv_sat", "Saturation", "Advanced", 0, 100, 0, advanced.sat),
        DM_HZ ("adv_hpf", "High-pass filter", "Advanced", 20, 1000, 20, 150, advanced.hpf),
        DM_HZ ("adv_lpf", "Low-pass filter", "Advanced", 1000, 20000, 20000, 5000, advanced.lpf),

        // 4b. The Puppet Show (reserved; the DSP stage arrives with its phase 2)
        DM_BOOL ("ps_enabled", "The Puppet Show", "The Puppet Show", 0, puppetShow.enabled),
        { "ps_genre", "Genre", "The Puppet Show", Spec::Kind::choice, 0, 4, 0, "", 0.0f,
          [] (DrumParams& p, float v) { p.puppetShow.genre = static_cast<DrumParams::PuppetShow::Genre> (juce::jlimit (0, 4, juce::roundToInt (v))); } },
        DM_PCT ("ps_depth", "Depth", "The Puppet Show", 0, 100, 50, puppetShow.depth),
        DM_PCT ("ps_bass", "Bass boost", "The Puppet Show", -100, 100, 0, puppetShow.bass),
        DM_PCT ("ps_high", "High control", "The Puppet Show", -100, 100, 0, puppetShow.high),
        DM_PCT ("ps_mud", "Mud control", "The Puppet Show", -100, 100, 0, puppetShow.mud),
        DM_PCT ("ps_presence", "Mid presence", "The Puppet Show", -100, 100, 0, puppetShow.presence),
    };
    return specs;
}

#undef DM_DB
#undef DM_PCT
#undef DM_HZ
#undef DM_BOOL

/** The JUCE layout, grouped by scope. */
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    std::vector<std::unique_ptr<juce::AudioProcessorParameterGroup>> groups;
    auto groupFor = [&groups] (const char* name) -> juce::AudioProcessorParameterGroup& {
        for (auto& g : groups)
            if (g->getName() == name) return *g;
        const auto id = juce::String (name).toLowerCase().replaceCharacter (' ', '_');
        groups.push_back (std::make_unique<juce::AudioProcessorParameterGroup> (id, name, "|"));
        return *groups.back();
    };

    for (const auto& s : table())
    {
        const juce::ParameterID pid { s.id, versionHint };
        auto& group = groupFor (s.group);
        switch (s.kind)
        {
            case Spec::Kind::toggle:
                group.addChild (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f));
                break;
            case Spec::Kind::choice:
                group.addChild (std::make_unique<juce::AudioParameterChoice> (pid, s.name, genreNames(), juce::roundToInt (s.def)));
                break;
            case Spec::Kind::real:
            {
                juce::NormalisableRange<float> range (s.min, s.max, 0.0f);
                if (s.skewCentre > 0.0f) range.setSkewForCentre (s.skewCentre);
                group.addChild (std::make_unique<juce::AudioParameterFloat> (
                    pid, s.name, range, s.def, juce::AudioParameterFloatAttributes().withLabel (s.unit)));
                break;
            }
        }
    }
    for (auto& g : groups) layout.add (std::move (g));
    return layout;
}
} // namespace params
