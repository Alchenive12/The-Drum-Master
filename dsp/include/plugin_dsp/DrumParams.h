// File: dsp/include/plugin_dsp/DrumParams.h
// Every host parameter of docs/02-parameters.md as plain values in the table's own units
// (dB, %, Hz; % knobs use the table's range, e.g. -100..100). The JUCE layer fills this struct from
// its parameters once per block; dsp/ never sees JUCE. Defaults = the table's defaults.
#pragma once

namespace plugin_dsp
{
struct DrumParams
{
    struct Global { float mix = 100, outputDb = 0, sensitivity = 50, punch = 0, clip = 0; bool bypass = false; };
    struct QuickMix
    {
        float kickVolume = 0, kickPunch = 0, kickWeight = 0;
        float snareVolume = 0, snareSnap = 0, snareFat = 0;
        float hatsVolume = 0, hatsTone = 0, hatsTame = 0;
        bool link = false;   // handled where the knobs move (JUCE layer), not in the DSP
    };
    struct Kick
    {
        float level = 0, lowBoost = 0, top = 0, bottom = 0, decay = 0, comp = 0, sat = 0, clip = 0;
        float eqLow = 0, eqMid = 0, eqMidFreq = 1000, eqHigh = 0;
    };
    struct Snare
    {
        float level = 0, crack = 0, body = 0, decay = 0, comp = 0, sat = 0;
        float eqLow = 0, eqMid = 0, eqMidFreq = 1000, eqHigh = 0;
        float dubstepify = 0, popify = 0, rockify = 0, rimify = 0;
    };
    struct ClosedHat { float level = 0, tone = 0, tick = 0, decay = 0, deharsh = 0, width = 100; };
    struct OpenHat { float level = 0, tone = 0, body = 0, decay = 0, width = 100, sat = 0, deharsh = 0; };
    struct Crash { float level = 0, tone = 0, body = 0, decay = 0, width = 100, deharsh = 0, lowCut = 20; };
    struct Ride { float level = 0, ping = 0, bell = 0, wash = 0, tone = 0, width = 100; };
    struct Rim { float level = 0, click = 0, body = 0, tone = 0, decay = 0, sat = 0; };
    struct Clap { float level = 0, attack = 0, body = 0, tone = 0, decay = 0, width = 100, comp = 0, sat = 0; };
    struct Misc { float level = 0, tone = 0, attack = 0, decay = 0; };
    struct Advanced { float drive = 0, sat = 0, hpf = 20, lpf = 20000; };
    struct PuppetShow
    {
        enum class Genre { rock, pop, edm, metal, hipHop };
        bool enabled = false;
        Genre genre = Genre::rock;
        float depth = 50, bass = 0, high = 0, mud = 0, presence = 0;
    };

    Global global;
    QuickMix quick;
    Kick kick;
    Snare snare;
    ClosedHat closedHat;
    OpenHat openHat;
    Crash crash;
    Ride ride;
    Rim rim;
    Clap clap;
    Misc misc;
    Advanced advanced;
    PuppetShow puppetShow;
};
} // namespace plugin_dsp
