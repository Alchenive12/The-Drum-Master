// File: Source/PluginEditor.h
// The Drum Master's editor (phase 4, docs/04-ui-concept.md): the puppet stage with its strings and
// the Shadow Play ring, the quick mix, the deep edit of one drum, The Puppet Show behind its
// curtain, the master's hands (advanced) and the about page.
//
// Everything is laid out in a fixed 1100 x 688 design space and scaled to the window, so the
// window can be resized freely at a fixed 16:10 shape. A 60 Hz timer runs the animations; it reads
// hits from the lock-free Shadow Play queue (this is its only reader), the band meters and the
// transport (atomics). UI actions reach the DSP only through the processor's thread-safe calls.
// Static scenery is rendered once per page into a cached image (ui/Fx.h, CachedLayer) at the
// screen's real pixel density; the timer only redraws what moves.
//
// DRUM_MASTER_DEMO=<stage|listen|kick|snare|new|show|curtain|advanced|about|shadow> (environment variable)
// makes the editor play made-up hits and open that page, for screenshots without audio. It never
// changes the sound; without the variable it does nothing.
#pragma once

#include "PluginProcessor.h"
#include "ui/Art.h"
#include "ui/Spectrum.h"
#include "ui/Widgets.h"

#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace ui
{
constexpr int baseW = 1100, baseH = 688;
using plugin_dsp::DrumType;

// ------------------------------------------------------------------------------------ live model
struct StageModel
{
    struct Slot
    {
        bool pending = false;          // an empty "?" string waiting for its type
        DrumType type = DrumType::misc;
        float jerk = 0.0f;             // 0..1, decays after a hit
        float drop = 1.0f;             // 0 = just appeared (falls in), 1 = arrived
    };
    struct Hit { DrumType type = DrumType::misc; float phase = 0.0f, level = 0.5f, confidence = 1.0f; double born = 0.0; };

    std::vector<Slot> slots;
    int pendingCount = 0;
    std::array<Hit, 384> hits {};
    std::size_t hitNext = 0, hitCount = 0;
    float playPhase = 0.0f;
    int beatsPerBar = 4;
    double now = 0.0;
    float sway = 0.0f, breathe = 0.0f, flicker = 1.0f;
    float pulse = 0.0f;   // the kick, for the crack in the mask
    float slack = 0.0f;   // 0..1: bypassed, the strings go slack
    bool listening = false;

    void addHit (const Hit& h)
    {
        hits[hitNext] = h;
        hitNext = (hitNext + 1) % hits.size();
        hitCount = juce::jmin (hitCount + 1, hits.size());
    }

    Slot* slotFor (DrumType t)
    {
        for (auto& s : slots)
            if (! s.pending && s.type == t) return &s;
        return nullptr;
    }
};

inline float easeOutBounce (float x)
{
    const float n = 7.5625f, d = 2.75f;
    if (x < 1.0f / d) return n * x * x;
    if (x < 2.0f / d) { x -= 1.5f / d; return n * x * x + 0.75f; }
    if (x < 2.5f / d) { x -= 2.25f / d; return n * x * x + 0.9375f; }
    x -= 2.625f / d;
    return n * x * x + 0.984375f;
}

inline juce::Point<float> onEllipse (juce::Point<float> centre, float rx, float ry, float angle)
{
    return centre.translated (std::sin (angle) * rx, -std::cos (angle) * ry);
}

/** The Shadow Play ring: a clock face in perspective, one bar around. A wake of light follows the
    playhead; hits stack up at their place in the bar and drift up and fade like smoke. */
inline void drawShadowRing (juce::Graphics& g, const StageModel& m, juce::Point<float> centre, float rx, float ry, float scale)
{
    using juce::Rectangle;
    auto ellipse = [&] (float f) { return Rectangle<float> (rx * 2.0f * f, ry * 2.0f * f).withCentre (centre); };
    fx::glowEllipse (g, ellipse (1.2f), colours::ecto.withAlpha (0.05f));

    const float head = m.playPhase * 2.0f * fx::pi;
    const int slices = 30;
    const float wake = 1.4f;
    for (int i = 0; i < slices; ++i)
    {
        const float f0 = static_cast<float> (i) / static_cast<float> (slices), f1 = static_cast<float> (i + 1) / static_cast<float> (slices);
        juce::Path seg;
        seg.addPieSegment (ellipse (1.0f), head - wake * f1, head - wake * f0, 0.0f);
        g.setColour (colours::ecto.withAlpha (0.13f * (1.0f - f0) * (1.0f - f0)));
        g.fillPath (seg);
    }
    for (float f : { 0.42f, 0.57f, 0.72f, 0.88f, 1.0f })
    {
        g.setColour (juce::Colours::white.withAlpha (f > 0.99f ? 0.2f : 0.055f));
        g.drawEllipse (ellipse (f), f > 0.99f ? 1.2f : 0.8f);
    }
    const int ticks = juce::jmax (1, m.beatsPerBar) * 4;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = static_cast<float> (i) / static_cast<float> (ticks) * 2.0f * fx::pi;
        const bool beat = i % 4 == 0;
        g.setColour (juce::Colours::white.withAlpha (i == 0 ? 0.6f : beat ? 0.32f : 0.12f));
        g.drawLine ({ onEllipse (centre, rx, ry, a), onEllipse (centre, rx * (beat ? 0.9f : 0.95f), ry * (beat ? 0.9f : 0.95f), a) }, beat ? 1.4f * scale : 1.0f);
    }
    fx::glowDot (g, onEllipse (centre, rx, ry, 0.0f), 7.0f * scale, colours::ecto.withAlpha (0.6f));

    auto radius = [] (DrumType t) {
        switch (t)
        {
            case DrumType::kick: return 0.42f;
            case DrumType::misc: return 0.57f;
            case DrumType::snare: case DrumType::clap: case DrumType::rimshot: return 0.72f;
            case DrumType::crash: case DrumType::ride: return 0.88f;
            case DrumType::closedHat: case DrumType::openHat: break;
        }
        return 1.0f;
    };
    for (std::size_t i = 0; i < m.hitCount; ++i)
    {
        const auto& h = m.hits[i];
        const float age = static_cast<float> (m.now - h.born);
        const float alpha = h.confidence * std::exp (-age / 7.0f);
        if (alpha < 0.02f) continue;
        const float a = h.phase * 2.0f * fx::pi, f = radius (h.type);
        const auto p = onEllipse (centre, rx * f, ry * f, a).translated (0.0f, -juce::jmin (5.0f, age * 1.2f) * scale);   // smoke rises a little
        const float size = (7.0f + 11.0f * h.level) * scale * (1.0f + 0.35f * std::exp (-age * 3.0f));   // fresh hits pop
        art::shadowShape (g, h.type, p, size, drumColour (h.type).withAlpha (juce::jlimit (0.0f, 1.0f, alpha)), std::exp (-age * 3.0f));
    }
    juce::Path hand;
    const auto tip = onEllipse (centre, rx * 1.04f, ry * 1.04f, head);
    hand.startNewSubPath (centre);
    hand.lineTo (tip);
    fx::glowStroke (g, hand, colours::ecto.withAlpha (0.85f), 1.1f * scale, 0.9f);
    fx::glowDot (g, tip, 9.0f * scale, colours::ecto.withAlpha (0.8f));
    g.setColour (juce::Colours::white);
    g.fillEllipse (Rectangle<float> (3.0f * scale, 3.0f * scale).withCentre (tip));
}

inline juce::TextButton& styleBack (juce::TextButton& b)
{
    b.setButtonText ("STAGE");
    styled (b, "pill", "back");
    return b;
}

/** "The Drum Master": an italic "The", then the name. */
inline void drawWordmark (juce::Graphics& g, juce::Point<float> at, float size, juce::Justification just = juce::Justification::centredLeft)
{
    const auto a = whisper (size), b = display (size);
    const float wa = textWidth (a, "The "), wb = textWidth (b, "Drum Master");
    float x = at.x;
    if (just.testFlags (juce::Justification::horizontallyCentred)) x -= (wa + wb) * 0.5f;
    g.setFont (a);
    g.setColour (colours::boneDim);
    g.drawText ("The ", juce::Rectangle<float> (x, at.y - size, wa + 4.0f, size * 1.4f), juce::Justification::centredLeft, false);
    g.setFont (b);
    g.setColour (colours::bone);
    g.drawText ("Drum Master", juce::Rectangle<float> (x + wa, at.y - size, wb + 4.0f, size * 1.4f), juce::Justification::centredLeft, false);
}

/** A small section label: coloured dot, tracked caps. */
inline void drawOverline (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Colour dot, juce::Justification just = juce::Justification::centredLeft)
{
    const auto f = caps (9.5f, 0.22f);
    const float w = textWidth (f, text) + 12.0f;
    float x = r.getX();
    if (just.testFlags (juce::Justification::horizontallyCentred)) x = r.getCentreX() - w * 0.5f;
    fx::glowDot (g, { x + 3.0f, r.getCentreY() }, 5.0f, dot.withAlpha (0.5f));
    g.setColour (dot);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x + 3.0f, r.getCentreY() }));
    g.setFont (f);
    g.setColour (colours::boneDim);
    g.drawText (text, juce::Rectangle<float> (x + 12.0f, r.getY(), w, r.getHeight()), juce::Justification::centredLeft, false);
}

// ------------------------------------------------------------------------------------ the stage
class StagePage final : public juce::Component
{
public:
    std::function<void (int)> onOpenSlot;      // slot index
    std::function<void()> onPuppet, onShow, onAbout, onShadow, onAddString, onListen;

    StagePage (DrumMasterProcessor& p, StageModel& m) : proc (p), model (m)
    {
        auto& s = proc.apvts;
        styled (listen, "pill", "listen", colours::ecto);
        styled (addString, "pill", "plus");
        styled (show, "pill", "stage", colours::limelight);
        styled (about, "round", "about");
        styled (bypass, "round", "power", colours::blood);
        styled (link, "pill", "link", colours::ecto);
        for (auto* b : { &listen, &addString, &show, &about })
            addAndMakeVisible (*b);
        addAndMakeVisible (bypass);
        listen.setTooltip ("Auto detect: clears the strings and lets them drop in as the master hears each drum");
        addString.setTooltip ("Hang a new string by hand and choose what hangs on it");
        show.setTooltip ("The Puppet Show: genre-aware smart EQ, behind the curtain");
        about.setTooltip ("About The Drum Master");
        bypass.setTooltip ("Bypass (the 50 ms latency stays, so nothing jumps)");
        listen.onClick = [this] { if (onListen) onListen(); };
        addString.onClick = [this] { if (onAddString) onAddString(); };
        show.onClick = [this] { if (onShow) onShow(); };
        about.onClick = [this] { if (onAbout) onAbout(); };

        const char* quick[3][3][2] = { { { "qm_kick_volume", "Volume" }, { "qm_kick_punch", "Punch" }, { "qm_kick_weight", "Weight" } },
                                       { { "qm_snare_volume", "Volume" }, { "qm_snare_snap", "Snap" }, { "qm_snare_fat", "Fat" } },
                                       { { "qm_hats_volume", "Volume" }, { "qm_hats_tone", "Tone" }, { "qm_hats_tame", "Tame" } } };
        const juce::Colour groupColour[3] = { drumColour (DrumType::kick), drumColour (DrumType::snare), drumColour (DrumType::closedHat) };
        for (int grp = 0; grp < 3; ++grp)
            for (int k = 0; k < 3; ++k)
                knobs.push_back (std::make_unique<Knob> (s, quick[grp][k][0], quick[grp][k][1], groupColour[grp]));
        const char* globals[5][2] = { { "global_mix", "Dry/Wet" }, { "global_output", "Output" }, { "global_sensitivity", "Sensitivity" },
                                      { "global_punch", "Punch" }, { "global_clip", "Clip" } };
        for (auto& gl : globals)
            knobs.push_back (std::make_unique<Knob> (s, gl[0], gl[1], colours::moon));
        for (auto& k : knobs) addAndMakeVisible (*k);
        addAndMakeVisible (link);
        link.setTooltip ("Link: the three group volumes keep their dB offsets");
    }

    void resized() override
    {
        listen.setBounds (654, 14, 100, 32);
        addString.setBounds (762, 14, 104, 32);
        show.setBounds (874, 14, 128, 32);
        bypass.setBounds (1010, 14, 32, 32);
        about.setBounds (1048, 14, 32, 32);
        for (int grp = 0; grp < 3; ++grp)
            for (int k = 0; k < 3; ++k)
                knobs[static_cast<std::size_t> (grp * 3 + k)]->setBounds (groupX[grp] + k * 66, 562, 64, 98);
        link.setBounds (688, 598, 68, 28);
        for (int k = 0; k < 5; ++k)
            knobs[static_cast<std::size_t> (9 + k)]->setBounds (782 + k * 58, 566, 56, 92);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        background.draw (g, baseW, baseH, [this] (juce::Graphics& ig, juce::Image& img, float s) { paintBackground (ig, img, s); });
        if (fog.isNull()) fog = art::makeFogSprite();
        art::roomLive (g, r.withBottom (530.0f), static_cast<float> (model.now), model.flicker, fog);
        art::legs (g, 550.0f, 452.0f, model.sway * 1.5f, 1.0f);   // the master is very tall: his legs stand in the room

        layout();
        // the master: the bar, his hands wrapped over it, arms reaching down, the mask above
        art::controlBar (g, barX1, barX2, barY + model.sway * 0.3f, 13.0f, xs);
        art::hand (g, { barX1 + 18.0f, barY }, 74.0f, true, model.sway, 400.0f);
        art::hand (g, { barX2 - 18.0f, barY }, 74.0f, false, -model.sway, 700.0f);
        const bool puppetHot = hover == Target::puppet;
        art::mask (g, 550.0f, 92.0f + model.breathe, 190.0f, model.breathe, juce::jmax (model.pulse, puppetHot ? 0.45f : 0.0f));

        paintTitle (g);
        if (puppetHot)
        {
            g.setFont (caps (9.0f, 0.24f));
            g.setColour (colours::ecto.withAlpha (0.85f));
            g.drawText ("THE MASTER'S HANDS  -  CLICK", juce::Rectangle<float> (400.0f, barY - 34.0f, 300.0f, 14.0f), juce::Justification::centred, false);
        }

        // the drums' faint shadows on the floor, smaller as a drum jerks up
        for (std::size_t i = 0; i < model.slots.size(); ++i)
        {
            const auto& s = model.slots[i];
            const float lift = juce::jlimit (0.0f, 1.0f, (318.0f - drumY (s)) / 190.0f);
            fx::glowEllipse (g, juce::Rectangle<float> (70.0f * (1.0f - 0.4f * lift), 12.0f).withCentre ({ xs[i] + 18.0f, 432.0f }),
                             colours::ink.withAlpha (0.55f * (1.0f - lift) * s.drop));
            if (s.jerk > 0.02f && ! s.pending)   // a hit lights the boards under it
                fx::glowEllipse (g, juce::Rectangle<float> (150.0f, 26.0f).withCentre ({ xs[i], 434.0f }), drumColour (s.type).withAlpha (0.3f * s.jerk));
        }

        // strings and their drums
        for (std::size_t i = 0; i < model.slots.size(); ++i)
        {
            const auto& s = model.slots[i];
            const float x = xs[i];
            const float y = drumY (s);
            const bool hot = hoverSlot == static_cast<int> (i) && hover == Target::slot;
            const auto col = s.pending ? colours::ecto : drumColour (s.type);
            juce::Path str;
            const float twitch = s.jerk * 5.0f * std::sin (static_cast<float> (model.now) * 46.0f + static_cast<float> (i));
            str.startNewSubPath (x, barY + 9.0f);
            str.quadraticTo (x + model.sway * 0.6f + twitch + model.slack * 10.0f, (barY + y) * 0.5f + model.slack * 16.0f, x, y - 30.0f);
            if (s.jerk > 0.05f || hot)
                fx::glowStroke (g, str, col.withAlpha (juce::jmax (hot ? 0.5f : 0.0f, s.jerk)), 1.0f, juce::jmax (hot ? 0.5f : 0.0f, s.jerk));
            g.setColour (colours::string.withAlpha (0.7f).interpolatedWith (col, juce::jmin (1.0f, s.jerk * 1.5f)));
            g.strokePath (str, juce::PathStrokeType (1.1f));
            if (s.pending) art::questionTag (g, { x, y }, 64.0f, hot ? 1.0f : 0.5f + 0.5f * std::sin (static_cast<float> (model.now) * 3.0f));
            else art::drum (g, s.type, { x, y }, 64.0f, juce::jmax (s.jerk, hot ? 0.35f : 0.0f));
            const auto label = s.pending ? juce::String ("CHOOSE") : typeTitle (s.type);
            g.setFont (caps (9.5f, 0.2f));
            g.setColour ((hot || s.jerk > 0.3f ? col : colours::boneDim).withMultipliedAlpha (s.drop));
            g.drawText (label, juce::Rectangle<float> (x - 60.0f, y + 38.0f, 120.0f, 14.0f), juce::Justification::centred, false);
        }
        if (model.slots.empty())
        {
            const juce::String dots = juce::String ("...").substring (0, 1 + static_cast<int> (std::fmod (model.now * 1.5, 3.0)));
            g.setFont (whisper (28.0f));
            g.setColour (colours::bone.withAlpha (0.75f));
            g.drawText (model.listening ? "Play your loop" + dots : juce::String ("The stage is empty"), juce::Rectangle<float> (250.0f, 238.0f, 600.0f, 36.0f),
                        juce::Justification::centred, false);
            g.setFont (caps (9.5f, 0.2f));
            g.setColour (colours::boneDim);
            g.drawText (model.listening ? "THE MASTER IS LISTENING" : "PRESS LISTEN, OR + STRING TO HANG ONE", juce::Rectangle<float> (250.0f, 276.0f, 600.0f, 16.0f),
                        juce::Justification::centred, false);
        }

        drawShadowRing (g, model, ringCentre, ringRx, ringRy, 1.0f);
        if (hover == Target::ring)
        {
            g.setFont (caps (9.0f, 0.22f));
            g.setColour (colours::ecto.withAlpha (0.8f));
            g.drawText ("SHADOW PLAY  -  CLICK TO ENLARGE", juce::Rectangle<float> (400.0f, 508.0f, 300.0f, 14.0f), juce::Justification::centred, false);
        }

        // the link, lit between the three groups
        const bool linked = link.getToggleState();
        for (float x : { 250.0f, 466.0f })
        {
            if (linked) fx::glowDot (g, { x, 606.0f }, 12.0f, colours::ecto.withAlpha (0.35f));
            fx::icon (g, "link", juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ x, 606.0f }), linked ? colours::ecto : colours::boneFaint);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int slot = -1;
        hover = target (e.position, &slot);
        hoverSlot = slot;
        setMouseCursor (hover != Target::none ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hover = Target::none; }

    void mouseUp (const juce::MouseEvent& e) override
    {
        int slot = -1;
        switch (target (e.position, &slot))
        {
            case Target::slot: if (onOpenSlot) onOpenSlot (slot); break;
            case Target::puppet: if (onPuppet) onPuppet(); break;
            case Target::ring: if (onShadow) onShadow(); break;
            case Target::none: break;
        }
    }

    /** Called by the editor's timer: the scene above the dock moves every frame; the dock's knobs
        repaint themselves, and the link glyphs only when LINK changes. */
    void tick()
    {
        repaint (0, 0, baseW, 528);
        if (link.getToggleState() != lastLinked)
        {
            lastLinked = link.getToggleState();
            repaint (230, 590, 260, 32);
        }
    }

    /** Where a slot's drum hangs (for the zoom origin). */
    juce::Point<float> slotPosition (int i)
    {
        layout();
        if (i < 0 || i >= static_cast<int> (xs.size())) return { 550.0f, 300.0f };
        return { xs[static_cast<std::size_t> (i)], drumY (model.slots[static_cast<std::size_t> (i)]) };
    }

    juce::TextButton listen { "LISTEN" }, addString { "STRING" }, show { "PUPPET SHOW" }, about { "" };

private:
    enum class Target { none, slot, puppet, ring };

    void paintBackground (juce::Graphics& g, juce::Image& img, float scale)
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (baseW), static_cast<float> (baseH));
        art::room (g, r);
        art::oldDrumSet (g, { 975.0f, 406.0f }, 0.68f);
        fx::vignette (g, r, 0.75f);
        g.setGradientFill (juce::ColourGradient (colours::ink.withAlpha (0.85f), 0.0f, 0.0f, colours::ink.withAlpha (0.0f), 0.0f, 70.0f, false));   // top bar
        g.fillRect (r.withHeight (70.0f));

        // the quick-mix dock: frosted dark glass
        const auto dock = juce::Rectangle<float> (20.0f, 530.0f, 1060.0f, 148.0f);
        fx::softShadow (g, dock, 18.0f, 0.8f, 20.0f, 8.0f);
        fx::frost (g, img, scale, dock, 18.0f, 14);
        fx::glass (g, dock, 18.0f, 0.62f);
        const char* titles[] = { "KICK", "SNARE", "HATS" };
        const DrumType types[] = { DrumType::kick, DrumType::snare, DrumType::closedHat };
        for (int grp = 0; grp < 3; ++grp)
            drawOverline (g, titles[grp], juce::Rectangle<float> (static_cast<float> (groupX[grp]), 540.0f, 196.0f, 16.0f), drumColour (types[grp]),
                          juce::Justification::centred);
        drawOverline (g, "GLOBAL", juce::Rectangle<float> (782.0f, 540.0f, 290.0f, 16.0f), colours::moon, juce::Justification::centred);
        juce::ColourGradient line (juce::Colours::white.withAlpha (0.0f), 0.0f, 548.0f, juce::Colours::white.withAlpha (0.0f), 0.0f, 664.0f, false);
        line.addColour (0.5, juce::Colours::white.withAlpha (0.1f));
        g.setGradientFill (line);
        for (float x : { 250.0f, 466.0f, 766.0f })   // hairline dividers between the groups
            g.fillRect (juce::Rectangle<float> (x, 548.0f, 1.0f, 116.0f));
    }

    void paintTitle (juce::Graphics& g)
    {
        drawWordmark (g, { 24.0f, 40.0f }, 30.0f);
        juce::String status;
        juce::Colour dot = colours::boneFaint;
        if (model.slack > 0.5f) { status = "BYPASSED"; dot = colours::blood; }
        else if (model.listening) { status = "LISTENING"; dot = colours::ecto.withAlpha (0.4f + 0.6f * (0.5f + 0.5f * std::sin (static_cast<float> (model.now) * 4.0f))); }
        else
        {
            int n = 0;
            for (auto& s : model.slots) if (! s.pending) ++n;
            status = juce::String (n) + (n == 1 ? " STRING" : " STRINGS");
            dot = colours::moon;
        }
        g.setColour (dot);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ 27.0f, 55.0f }));
        if (model.listening || model.slack > 0.5f) fx::glowDot (g, { 27.0f, 55.0f }, 8.0f, dot.withAlpha (0.5f));
        g.setFont (caps (9.0f, 0.24f));
        g.setColour (colours::boneDim);
        g.drawText (status, juce::Rectangle<float> (36.0f, 48.0f, 240.0f, 14.0f), juce::Justification::centredLeft, false);
    }

    void layout()
    {
        const auto n = model.slots.size();
        const float spacing = n > 0 ? juce::jmin (112.0f, 880.0f / static_cast<float> (n)) : 0.0f;
        xs.resize (n);
        for (std::size_t i = 0; i < n; ++i)
            xs[i] = 550.0f + (static_cast<float> (i) - 0.5f * static_cast<float> (n - 1)) * spacing;
        const float half = n > 0 ? 0.5f * static_cast<float> (n - 1) * spacing + 60.0f : 90.0f;
        barX1 = 550.0f - juce::jmax (120.0f, half);
        barX2 = 550.0f + juce::jmax (120.0f, half);
    }

    float drumY (const StageModel::Slot& s) const
    {
        return 318.0f - s.jerk * 26.0f - (1.0f - easeOutBounce (s.drop)) * 190.0f + model.slack * 22.0f;
    }

    Target target (juce::Point<float> p, int* slotOut = nullptr)
    {
        layout();
        for (std::size_t i = 0; i < xs.size(); ++i)
        {
            const float y = drumY (model.slots[i]);
            const bool onDrum = p.getDistanceFrom ({ xs[i], y }) < 40.0f;
            const bool onString = std::abs (p.x - xs[i]) < 12.0f && p.y > barY + 8.0f && p.y < y;
            if (onDrum || onString)
            {
                if (slotOut) *slotOut = static_cast<int> (i);
                return Target::slot;
            }
        }
        const bool onBar = p.y > barY - 90.0f && p.y < barY + 12.0f && p.x > barX1 - 40.0f && p.x < barX2 + 40.0f && p.y > 50.0f;
        const bool onMask = p.getDistanceFrom ({ 550.0f, 40.0f }) < 95.0f && p.y > 50.0f;
        if (onBar || onMask) return Target::puppet;
        const auto d = (p - ringCentre);
        if ((d.x * d.x) / (ringRx * ringRx) + (d.y * d.y) / (ringRy * ringRy) < 1.1f) return Target::ring;
        return Target::none;
    }

    DrumMasterProcessor& proc;
    StageModel& model;
    std::vector<std::unique_ptr<Knob>> knobs;
    ParamToggle link { proc.apvts, "qm_link", "LINK" };
    ParamToggle bypass { proc.apvts, "global_bypass", "" };
    fx::CachedLayer background;
    juce::Image fog;
    std::vector<float> xs;
    Target hover = Target::none;
    int hoverSlot = -1;
    bool lastLinked = false;
    float barX1 = 430.0f, barX2 = 670.0f;
    const float barY = 168.0f;
    const int groupX[3] = { 44, 260, 476 };
    const juce::Point<float> ringCentre { 550.0f, 470.0f };
    const float ringRx = 250.0f, ringRy = 50.0f;
};

// ------------------------------------------------------------------------------- deep edit
class DeepEditPage final : public juce::Component
{
public:
    std::function<void()> onBack;
    std::function<void (DrumType)> onChooseType;   // a pending string got its type
    std::function<void (DrumType)> onChangeType;   // an existing string changes type
    std::function<void()> onCut;

    DeepEditPage (DrumMasterProcessor& p, StageModel& m) : proc (p), model (m)
    {
        addAndMakeVisible (styleBack (back));
        back.onClick = [this] { if (onBack) onBack(); };
        addAndMakeVisible (typeBox);
        for (std::size_t t = 0; t < plugin_dsp::numDrumTypes; ++t)
            typeBox.addItem (juce::String (plugin_dsp::drumTypeName (static_cast<DrumType> (t))).toUpperCase(), static_cast<int> (t) + 1);
        typeBox.setTextWhenNothingSelected (juce::String::fromUTF8 ("Choose\xe2\x80\xa6"));
        typeBox.onChange = [this] {
            const int id = typeBox.getSelectedId();
            if (id <= 0 || updating) return;
            const auto t = static_cast<DrumType> (id - 1);
            if (pending) { if (onChooseType) onChooseType (t); }
            else if (t != type) { if (onChangeType) onChangeType (t); }
        };
        addAndMakeVisible (styled (cut, "danger", "scissors", colours::blood));
        cut.setTooltip ("Cut this string: the drum goes back to Misc (neutral)");
        cut.onClick = [this] { if (onCut) onCut(); };
    }

    void show (bool isPending, DrumType t)
    {
        pending = isPending;
        type = t;
        updating = true;
        if (pending) typeBox.setSelectedId (0, juce::dontSendNotification);
        else typeBox.setSelectedId (static_cast<int> (t) + 1, juce::dontSendNotification);
        updating = false;
        setAccent (typeBox, pending ? colours::ecto : drumColour (t));
        knobs.clear();
        cards.clear();
        fader.reset();
        if (! pending)
        {
            const auto accent = drumColour (t);
            std::vector<KnobGroup> groups;
            for (auto& gr : deepEditGroups (t))
            {
                if (juce::String (gr.title) == "LEVEL")
                {
                    fader = std::make_unique<Fader> (proc.apvts, gr.knobs.front().first, accent);
                    fader->meter = [this] {
                        for (auto& s : model.slots)
                            if (! s.pending && s.type == type) return s.jerk;
                        return 0.0f;
                    };
                    addAndMakeVisible (*fader);
                    fader->setBounds (levelCard.reduced (8, 14).withTrimmedTop (4));
                }
                else groups.push_back (gr);
            }
            layoutCards (groups, accent);
        }
        background.invalidate();
        repaint();
    }

    bool isPending() const noexcept { return pending; }
    DrumType currentType() const noexcept { return type; }

    /** Called by the editor's timer while this page is visible. */
    void tick()
    {
        repaint (0, 0, 440, 540);
        if (fader != nullptr) fader->repaint();
    }

    void resized() override
    {
        back.setBounds (20, 16, 104, 32);
        typeBox.setBounds (40, 578, 222, 40);
        cut.setBounds (270, 578, 132, 40);
    }

    void paint (juce::Graphics& g) override
    {
        background.draw (g, baseW, baseH, [this] (juce::Graphics& ig, juce::Image& img, float s) { paintBackground (ig, img, s); });

        const auto col = pending ? colours::ecto : drumColour (type);
        float jerk = 0.0f;
        int index = 0, count = 0;
        for (auto& s : model.slots)
        {
            ++count;
            if (s.pending == pending && (pending || s.type == type)) { jerk = s.jerk; index = count; }
        }
        g.setFont (mono (11.0f));
        g.setColour (colours::boneFaint);
        g.drawText (juce::String::formatted ("STRING %02d / %02d", juce::jmax (1, index), juce::jmax (1, count)), juce::Rectangle<float> (250.0f, 16.0f, 160.0f, 32.0f),
                    juce::Justification::centredRight, false);

        // the drum, still on its string, under the spotlight
        const float cx = 216.0f, y = 250.0f - jerk * 30.0f;
        fx::glowEllipse (g, juce::Rectangle<float> (220.0f * (1.0f - 0.25f * jerk), 34.0f).withCentre ({ cx, 396.0f }), colours::ink.withAlpha (0.8f));
        if (jerk > 0.01f) fx::glowEllipse (g, juce::Rectangle<float> (300.0f, 60.0f).withCentre ({ cx, 396.0f }), col.withAlpha (0.25f * jerk));
        juce::Path str;
        str.startNewSubPath (cx, 0.0f);
        str.lineTo (cx, y - 70.0f);
        if (jerk > 0.05f) fx::glowStroke (g, str, col.withAlpha (jerk), 1.2f, jerk);
        g.setColour (colours::string.withAlpha (0.8f));
        g.strokePath (str, juce::PathStrokeType (1.3f));
        if (pending) art::questionTag (g, { cx, y }, 190.0f, 0.4f + 0.3f * std::sin (static_cast<float> (model.now) * 2.5f));
        else art::drum (g, type, { cx, y }, 180.0f, jerk);

        g.setFont (display (66.0f));
        g.setColour (colours::bone);
        g.drawText (pending ? juce::String ("A new string") : typeName (type), juce::Rectangle<float> (20.0f, 410.0f, 392.0f, 66.0f), juce::Justification::centred, false);
        g.setFont (whisper (20.0f));
        g.setColour (col.withAlpha (0.75f));
        g.drawText (pending ? juce::String ("what hangs here?") : typeWhisper (type), juce::Rectangle<float> (20.0f, 474.0f, 392.0f, 26.0f), juce::Justification::centred,
                    false);
    }

private:
    struct Card { juce::String title; juce::Rectangle<float> r; };

    void layoutCards (const std::vector<KnobGroup>& groups, juce::Colour accent)
    {
        // pack the groups into rows of at most five knobs; cards share a row by knob count
        std::vector<std::vector<const KnobGroup*>> rows;
        int inRow = 0;
        for (auto& gr : groups)
        {
            const int n = static_cast<int> (gr.knobs.size());
            if (rows.empty() || inRow + n > 5) { rows.emplace_back(); inRow = 0; }
            rows.back().push_back (&gr);
            inRow += n;
        }
        const float gap = 14.0f;
        const auto nRows = static_cast<float> (rows.size());
        const float rowH = juce::jmin (206.0f, (gridArea.getHeight() - gap * (nRows - 1.0f)) / juce::jmax (1.0f, nRows));
        float y = gridArea.getY() + (gridArea.getHeight() - (rowH * nRows + gap * (nRows - 1.0f))) * 0.5f;
        for (auto& row : rows)
        {
            int total = 0;
            for (auto* gr : row) total += static_cast<int> (gr->knobs.size());
            const float unit = (gridArea.getWidth() - gap * static_cast<float> (row.size() - 1)) / static_cast<float> (total);
            float x = gridArea.getX();
            for (auto* gr : row)
            {
                const auto n = static_cast<int> (gr->knobs.size());
                const auto card = juce::Rectangle<float> (x, y, unit * static_cast<float> (n), rowH);
                cards.push_back ({ gr->title, card });
                const auto knobArea = card.reduced (10.0f, 0.0f).withTrimmedTop (40.0f).withTrimmedBottom (12.0f);
                const float cell = knobArea.getWidth() / static_cast<float> (n);
                const float kw = juce::jmin (cell, 108.0f), kh = juce::jmin (knobArea.getHeight(), kw * 1.22f);
                for (int k = 0; k < n; ++k)
                {
                    const auto& kd = gr->knobs[static_cast<std::size_t> (k)];
                    auto knob = std::make_unique<Knob> (proc.apvts, kd.first, kd.second, accent);
                    knob->setBounds (juce::Rectangle<float> (kw, kh)
                                         .withCentre ({ knobArea.getX() + cell * (static_cast<float> (k) + 0.5f), knobArea.getCentreY() })
                                         .toNearestInt());
                    addAndMakeVisible (*knob);
                    knobs.push_back (std::move (knob));
                }
                x += unit * static_cast<float> (n) + gap;
            }
            y += rowH + gap;
        }
    }

    void paintBackground (juce::Graphics& g, juce::Image& img, float scale)
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (baseW), static_cast<float> (baseH));
        const auto col = pending ? colours::ecto : drumColour (type);
        g.fillAll (colours::ink);
        g.setGradientFill (juce::ColourGradient (colours::deep, 760.0f, 300.0f, colours::ink, 1100.0f, 700.0f, true));
        g.fillRect (r.withLeft (440.0f));
        // a huge ghost of the name behind the controls
        g.setFont (display (300.0f));
        g.setColour (juce::Colours::white.withAlpha (0.022f));
        g.drawText (pending ? juce::String ("?") : typeName (type), juce::Rectangle<float> (470.0f, 360.0f, 900.0f, 330.0f), juce::Justification::centredLeft, false);
        // the spotlight from above onto the drum
        fx::soft (g, r.withRight (440.0f), 4.0f, 7, [&] (juce::Graphics& sg) {
            juce::Path cone;
            cone.startNewSubPath (186.0f, -20.0f);
            cone.lineTo (246.0f, -20.0f);
            cone.lineTo (370.0f, 400.0f);
            cone.lineTo (62.0f, 400.0f);
            cone.closeSubPath();
            sg.setGradientFill (juce::ColourGradient (colours::moon.withAlpha (0.3f), 0.0f, 0.0f, colours::moon.withAlpha (0.07f), 0.0f, 400.0f, false));
            sg.fillPath (cone);
            sg.setColour (colours::moon.withAlpha (0.22f));
            sg.fillEllipse (juce::Rectangle<float> (330.0f, 70.0f).withCentre ({ 216.0f, 398.0f }));
            sg.setColour (col.withAlpha (0.08f));
            sg.fillEllipse (juce::Rectangle<float> (420.0f, 140.0f).withCentre ({ 216.0f, 410.0f }));
        });
        fx::vignette (g, r, 0.7f);
        juce::ColourGradient line (juce::Colours::white.withAlpha (0.0f), 0.0f, 20.0f, juce::Colours::white.withAlpha (0.0f), 0.0f, 668.0f, false);
        line.addColour (0.5, juce::Colours::white.withAlpha (0.1f));
        g.setGradientFill (line);
        g.fillRect (juce::Rectangle<float> (440.0f, 20.0f, 1.0f, 648.0f));

        drawOverline (g, "THIS STRING IS A", juce::Rectangle<float> (40.0f, 552.0f, 300.0f, 16.0f), col);

        if (pending)
        {
            for (int i = 0; i < 3; ++i)   // skeleton cards: the knobs will appear here
            {
                const auto card = juce::Rectangle<float> (gridArea.getX(), gridArea.getY() + 30.0f + static_cast<float> (i) * 196.0f, gridArea.getWidth() + 120.0f, 176.0f);
                juce::Path outline;
                outline.addRoundedRectangle (card, 16.0f);
                juce::Path dashed;
                const float dashes[] = { 4.0f, 6.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.fillPath (dashed);
            }
            g.setFont (whisper (32.0f));
            g.setColour (colours::bone.withAlpha (0.85f));
            g.drawText (juce::String::fromUTF8 ("Choose what hangs on this string\xe2\x80\xa6"), juce::Rectangle<float> (460.0f, 300.0f, 616.0f, 40.0f),
                        juce::Justification::centred, false);
            g.setFont (caps (9.5f, 0.22f));
            g.setColour (colours::boneDim);
            g.drawText ("ITS KNOBS APPEAR HERE", juce::Rectangle<float> (460.0f, 344.0f, 616.0f, 16.0f), juce::Justification::centred, false);
            return;
        }
        for (const auto& card : cards)
        {
            fx::softShadow (g, card.r, 16.0f, 0.6f, 14.0f, 6.0f);
            fx::glass (g, card.r, 16.0f, 0.55f);
            drawOverline (g, card.title, juce::Rectangle<float> (card.r.getX() + 16.0f, card.r.getY() + 14.0f, card.r.getWidth() - 32.0f, 14.0f), col);
        }
        fx::softShadow (g, levelCard.toFloat(), 16.0f, 0.6f, 14.0f, 6.0f);
        fx::glass (g, levelCard.toFloat(), 16.0f, 0.55f);
        juce::ignoreUnused (img, scale);
    }

    DrumMasterProcessor& proc;
    StageModel& model;
    bool pending = false, updating = false;
    DrumType type = DrumType::kick;
    juce::TextButton back, cut { "CUT STRING" };
    juce::ComboBox typeBox;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<Fader> fader;
    std::vector<Card> cards;
    fx::CachedLayer background;
    const juce::Rectangle<float> gridArea { 460.0f, 24.0f, 496.0f, 640.0f };
    const juce::Rectangle<int> levelCard { 970, 24, 108, 640 };
};

// ------------------------------------------------------------------------- The Puppet Show
/** The Puppet Show: genre tickets, Depth and four trims around a live EQ graph in the manner of
    Gullfoss / Guardian Angel. Behind: the bus spectrum before (ghost line) and after (filled) the
    plugin. In front: the show's EQ for each drum, drawn from the same band table the engine uses
    (plugin_dsp::PuppetShow::bands) with the live gains, in the drum's colour, flaring when that
    drum is hit; and a handle per measure, warm when it boosts, cold when it cuts. */
class ShowPage final : public juce::Component
{
public:
    std::function<void()> onBack;
    std::function<float (int)> meterValue;           // dB, measure index 0..4
    std::function<bool (float*, float*)> readScope;  // Spectrum::fftSize samples before / after; false: no audio (demo)
    std::function<double()> sampleRate;

    ShowPage (DrumMasterProcessor& p, StageModel& m) : proc (p), model (m), scopeIn (Spectrum::fftSize), scopeOut (Spectrum::fftSize)
    {
        auto& s = proc.apvts;
        addAndMakeVisible (styleBack (back));
        back.onClick = [this] { if (onBack) onBack(); };
        styled (lever, "switch", "", colours::limelight);
        lever.setTooltip ("The Puppet Show listens all the time; this turns its EQ on");
        addAndMakeVisible (lever);
        genre = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter ("ps_genre"));
        const char* names[] = { "ROCK", "POP", "EDM", "METAL", "HIP-HOP" };
        for (int i = 0; i < 5; ++i)
        {
            auto& b = genres[static_cast<std::size_t> (i)];
            b.setButtonText (names[i]);
            styled (b, "ticket", "", colours::limelight);
            b.getProperties().set ("genre", i);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (1);
            b.onClick = [this, i] {
                if (genre != nullptr)
                {
                    genre->beginChangeGesture();
                    genre->setValueNotifyingHost (genre->convertTo0to1 (static_cast<float> (i)));
                    genre->endChangeGesture();
                }
            };
            addAndMakeVisible (b);
        }
        depth = std::make_unique<Knob> (s, "ps_depth", "Depth", colours::limelight);
        addAndMakeVisible (*depth);
        const char* ks[4][2] = { { "ps_bass", "Bass boost" }, { "ps_mud", "Mud control" }, { "ps_presence", "Mid presence" }, { "ps_high", "High control" } };
        for (auto& k : ks)
        {
            knobs.push_back (std::make_unique<Knob> (s, k[0], k[1], colours::limelight));
            addAndMakeVisible (*knobs.back());
        }
    }

    /** Screenshot demo: show the switch on without touching the parameter (no notification, so
        the attachment never hears of it and the sound can't change). */
    void showDemoOn() { lever.setToggleState (true, juce::dontSendNotification); }

    /** Called by the editor's timer (30 fps) while the page is visible. */
    void refresh()
    {
        lever.setButtonText (lever.getToggleState() ? "THE SHOW IS ON" : "THE SHOW IS OFF");
        if (genre != nullptr)
        {
            const int idx = genre->getIndex();
            for (int i = 0; i < 5; ++i)
                genres[static_cast<std::size_t> (i)].setToggleState (i == idx, juce::dontSendNotification);
        }
        for (std::size_t i = 0; i < gains.size(); ++i)
        {
            const float v = meterValue ? juce::jlimit (-12.0f, 12.0f, meterValue (static_cast<int> (i))) : 0.0f;
            gains[i] += (v - gains[i]) * 0.4f;
        }
        const double sr = sampleRate ? juce::jmax (8000.0, sampleRate()) : 48000.0;
        updateCurves (sr);
        if (readScope && readScope (scopeIn.data(), scopeOut.data())) spectrum.analyse (scopeIn.data(), scopeOut.data(), sr);
        else demoSpectrum();
        repaint (graph.toNearestInt().expanded (6));
        repaint (marquee.toNearestInt().expanded (30));
    }

    void resized() override
    {
        back.setBounds (20, 16, 104, 32);
        lever.setBounds (880, 16, 200, 32);
        for (int i = 0; i < 5; ++i)
            genres[static_cast<std::size_t> (i)].setBounds (190 + i * 146, 118, 134, 46);
        depth->setBounds (466, 466, 168, 200);
        const int kx[4] = { 172, 316, 674, 818 };
        for (int i = 0; i < 4; ++i)
            knobs[static_cast<std::size_t> (i)]->setBounds (kx[i], 498, 110, 128);
    }

    void paint (juce::Graphics& g) override
    {
        background.draw (g, baseW, baseH, [this] (juce::Graphics& ig, juce::Image& img, float s) { paintBackground (ig, img, s); });
        paintMarquee (g);
        paintGraph (g);
    }

private:
    using PS = plugin_dsp::PuppetShow;
    struct Group { const char* name; DrumType drum; std::array<DrumType, 4> members; std::size_t numMembers; };
    static constexpr std::array<Group, 5> groups { {
        { "KICK", DrumType::kick, { DrumType::kick, DrumType::kick, DrumType::kick, DrumType::kick }, 1 },
        { "SNARE", DrumType::snare, { DrumType::snare, DrumType::snare, DrumType::snare, DrumType::snare }, 1 },
        { "CLAP", DrumType::clap, { DrumType::clap, DrumType::clap, DrumType::clap, DrumType::clap }, 1 },
        { "TOMS", DrumType::misc, { DrumType::misc, DrumType::misc, DrumType::misc, DrumType::misc }, 1 },
        { "HATS & CYMBALS", DrumType::closedHat, { DrumType::closedHat, DrumType::openHat, DrumType::crash, DrumType::ride }, 4 },
    } };

    // ---------------------------------------------------------------- geometry of the graph
    juce::Rectangle<float> plot() const { return graph.withTrimmedLeft (54.0f).withTrimmedRight (22.0f).withTrimmedTop (44.0f).withTrimmedBottom (30.0f); }
    float xOf (float hz) const { const auto p = plot(); return p.getX() + p.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); }
    float yEq (float db) const { const auto p = plot(); return p.getCentreY() - juce::jlimit (-rangeDb, rangeDb, db) / rangeDb * p.getHeight() * 0.5f; }
    float ySpec (float db) const { const auto p = plot(); return juce::jmap (juce::jlimit (-90.0f, 0.0f, db), -90.0f, 0.0f, p.getBottom(), p.getY()); }

    /** Design the show's bands once per frame from the engine's band table and the live gains. */
    void updateCurves (double sr)
    {
        for (std::size_t i = 0; i < PS::bands.size(); ++i)
        {
            const auto& b = PS::bands[i];
            const double gDb = static_cast<double> (gains[static_cast<std::size_t> (b.measure)]);
            bandOn[i] = std::abs (gDb) >= 0.01;
            if (! bandOn[i]) continue;
            const double f = static_cast<double> (b.freq), q = static_cast<double> (b.q);
            bandCoeffs[i] = b.shape == PS::Shape::lowShelf    ? plugin_dsp::BiquadCoeffs::lowShelf (sr, f, q, gDb)
                            : b.shape == PS::Shape::highShelf ? plugin_dsp::BiquadCoeffs::highShelf (sr, f, q, gDb)
                                                              : plugin_dsp::BiquadCoeffs::peak (sr, f, q, gDb);
        }
        curveSr = sr;
        for (std::size_t gi = 0; gi < groups.size(); ++gi)
            for (std::size_t i = 0; i < curveSteps; ++i)
                curves[gi][i] = curveDb (groups[gi].drum, Spectrum::frequency (static_cast<float> (i) / static_cast<float> (curveSteps - 1)));
    }

    /** The show's EQ for one drum at frequency f (dB), from this frame's bands. */
    float curveDb (DrumType drum, float f) const
    {
        double mag = 1.0;
        for (std::size_t i = 0; i < PS::bands.size(); ++i)
            if (bandOn[i] && PS::bands[i].drum == drum) mag *= bandCoeffs[i].magnitudeAt (curveSr, static_cast<double> (f));
        return static_cast<float> (20.0 * std::log10 (juce::jmax (1.0e-6, mag)));
    }

    float jerkOf (const Group& gr) const
    {
        float j = 0.0f;
        for (const auto& s : model.slots)
            for (std::size_t k = 0; k < gr.numMembers; ++k)
                if (! s.pending && s.type == gr.members[k]) j = juce::jmax (j, s.jerk);
        return j;
    }

    // ---------------------------------------------------------------- painting
    void paintBackground (juce::Graphics& g, juce::Image& img, float scale)
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (baseW), static_cast<float> (baseH));
        art::theatre (g, r);
        fx::softShadow (g, marquee, 12.0f, 0.9f, 22.0f, 10.0f);
        fx::softShadow (g, graph, 18.0f, 0.85f, 22.0f, 10.0f);
        fx::frost (g, img, scale, graph, 18.0f, 14);
        fx::glass (g, graph, 18.0f, 0.78f, juce::Colour (0xff160b0f));
        drawOverline (g, "LIVE  -  THE SHOW'S EQ, DRUM BY DRUM", juce::Rectangle<float> (graph.getX() + 20.0f, graph.getY() + 16.0f, 400.0f, 14.0f), colours::limelight);

        // legend: one colour per curve, then the spectrum
        float x = graph.getRight() - 24.0f;
        g.setFont (caps (8.5f, 0.18f));
        auto legend = [&g, &x, this] (const juce::String& name, juce::Colour c, bool line) {
            const float w = textWidth (caps (8.5f, 0.18f), name);
            x -= w;
            g.setColour (colours::boneDim);
            g.drawText (name, juce::Rectangle<float> (x, graph.getY() + 16.0f, w + 2.0f, 14.0f), juce::Justification::centredLeft, false);
            x -= 12.0f;
            g.setColour (c);
            if (line) g.fillRect (juce::Rectangle<float> (x - 2.0f, graph.getY() + 22.5f, 10.0f, 1.5f));
            else g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x + 3.0f, graph.getY() + 23.0f }));
            x -= 16.0f;
        };
        legend ("AFTER", colours::limelight.withAlpha (0.6f), true);
        legend ("BEFORE", colours::bone.withAlpha (0.35f), true);
        x -= 10.0f;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it)
            legend (it->name, drumColour (it->drum), false);

        // the grid: frequencies and dB
        const auto p = plot();
        g.setFont (mono (9.5f));
        const std::pair<float, const char*> freqs[] = { { 30.0f, "30" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
                                                        { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" } };
        for (const auto& [hz, label] : freqs)
        {
            const float fx0 = xOf (hz);
            g.setColour (juce::Colours::white.withAlpha (hz > 999.0f && hz < 1001.0f ? 0.08f : 0.045f));
            g.fillRect (juce::Rectangle<float> (fx0, p.getY(), 1.0f, p.getHeight()));
            g.setColour (colours::boneFaint);
            g.drawText (label, juce::Rectangle<float> (fx0 - 20.0f, p.getBottom() + 8.0f, 40.0f, 12.0f), juce::Justification::centred, false);
        }
        for (int db : { 12, 6, 0, -6, -12 })
        {
            const float y = yEq (static_cast<float> (db));
            const float dashes[] = { 2.0f, 5.0f };
            juce::Path line, dashed;
            line.startNewSubPath (p.getX(), y);
            line.lineTo (p.getRight(), y);
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dashes, 2);
            g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.22f : 0.06f));
            g.fillPath (dashed);
            g.setColour (db == 0 ? colours::boneDim : colours::boneFaint);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (graph.getX() + 10.0f, y - 7.0f, 34.0f, 14.0f), juce::Justification::centredRight,
                        false);
        }
    }

    void paintMarquee (juce::Graphics& g)
    {
        const bool on = lever.getToggleState();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0710), 0.0f, marquee.getY(), juce::Colour (0xff12030a), 0.0f, marquee.getBottom(), false));
        g.fillRoundedRectangle (marquee, 12.0f);
        g.setColour (colours::brass.withAlpha (0.8f));
        g.drawRoundedRectangle (marquee, 12.0f, 1.5f);
        g.drawRoundedRectangle (marquee.reduced (5.0f), 9.0f, 0.6f);
        const int bulbs = 26;
        const int step = static_cast<int> (juce::Time::getMillisecondCounter() / 180);
        for (int i = 0; i < bulbs; ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (bulbs);
            const auto inner = marquee.reduced (10.0f, 0.0f);
            const auto p = t < 0.5f ? juce::Point<float> (inner.getX() + inner.getWidth() * t * 2.0f, marquee.getY() + 8.0f)
                                    : juce::Point<float> (inner.getRight() - inner.getWidth() * (t - 0.5f) * 2.0f, marquee.getBottom() - 8.0f);
            const bool lit = (i + step) % 3 != 0;
            if (lit) fx::glowDot (g, p, 9.0f, colours::limelight.withAlpha (on ? 0.6f : 0.3f));
            g.setColour (lit ? colours::limelight.brighter (0.3f).withAlpha (on ? 1.0f : 0.7f) : colours::brassDim.darker (0.5f));
            g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (p));
        }
        const auto a = whisper (38.0f), b = display (38.0f);
        const float wa = textWidth (a, "The "), wb = textWidth (b, "Puppet Show");
        const float x0 = marquee.getCentreX() - (wa + wb) * 0.5f;
        g.setFont (a);
        g.setColour (colours::limelight.withAlpha (0.8f));
        g.drawText ("The ", juce::Rectangle<float> (x0, marquee.getY() + 10.0f, wa + 4.0f, marquee.getHeight() - 20.0f), juce::Justification::centredLeft, false);
        g.setFont (b);
        g.setColour (colours::bone);
        g.drawText ("Puppet Show", juce::Rectangle<float> (x0 + wa, marquee.getY() + 10.0f, wb + 4.0f, marquee.getHeight() - 20.0f), juce::Justification::centredLeft, false);
    }

    void paintGraph (juce::Graphics& g)
    {
        const bool on = lever.getToggleState();
        const auto p = plot();
        g.saveState();
        g.reduceClipRegion (p.expanded (2.0f, 0.0f).toNearestInt());

        // the spectrum: before as a ghost, after filled
        juce::Path before, after;
        for (std::size_t i = 0; i < Spectrum::numPoints; ++i)
        {
            const float x = p.getX() + p.getWidth() * Spectrum::position (i);
            const auto pb = juce::Point<float> (x, ySpec (spectrum.before[i])), pa = juce::Point<float> (x, ySpec (spectrum.after[i]));
            if (i == 0) { before.startNewSubPath (pb); after.startNewSubPath (pa); }
            else { before.lineTo (pb); after.lineTo (pa); }
        }
        juce::Path afterFill (after);
        afterFill.lineTo (p.getRight(), p.getBottom());
        afterFill.lineTo (p.getX(), p.getBottom());
        afterFill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::limelight.withAlpha (0.16f), 0.0f, p.getY(), colours::limelight.withAlpha (0.015f), 0.0f, p.getBottom(), false));
        g.fillPath (afterFill);
        g.setColour (colours::limelight.withAlpha (0.32f));
        g.strokePath (after, juce::PathStrokeType (1.1f));
        g.setColour (colours::bone.withAlpha (0.2f));
        g.strokePath (before, juce::PathStrokeType (0.9f));

        // the show's EQ, one curve per drum
        const float zeroY = yEq (0.0f);
        for (std::size_t gi = 0; gi < groups.size(); ++gi)
        {
            const auto& gr = groups[gi];
            juce::Path curve;
            float peak = 0.0f;
            for (std::size_t i = 0; i < curveSteps; ++i)
            {
                const float t = static_cast<float> (i) / static_cast<float> (curveSteps - 1);
                const float db = curves[gi][i];
                peak = juce::jmax (peak, std::abs (db));
                const auto pt = juce::Point<float> (p.getX() + p.getWidth() * t, yEq (db));
                if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
            }
            if (peak < 0.05f) continue;
            const auto col = drumColour (gr.drum);
            const float hit = jerkOf (gr);
            juce::Path fill (curve);
            fill.lineTo (p.getRight(), zeroY);
            fill.lineTo (p.getX(), zeroY);
            fill.closeSubPath();
            g.setColour (col.withAlpha ((on ? 0.07f : 0.03f) + 0.14f * hit));
            g.fillPath (fill);
            fx::glowStroke (g, curve, col.withAlpha ((on ? 0.6f : 0.3f) + 0.4f * hit), 1.5f, 0.5f + 1.0f * hit);
        }

        // a handle per measure: warm when it boosts, cold when it cuts
        struct Handle { const char* name; PS::Measure m; float hz; DrumType on; int side; };   // side: label -1 left, 1 right, 0 above/below
        const Handle handles[] = { { "KICK SUB", PS::kickSub, 60.0f, DrumType::kick, 0 }, { "MUD", PS::mud, 320.0f, DrumType::snare, 0 },
                                   { "PRESENCE", PS::presence, 3000.0f, DrumType::snare, -1 }, { "KICK CLICK", PS::kickClick, 3500.0f, DrumType::kick, 1 },
                                   { "AIR", PS::air, 11000.0f, DrumType::closedHat, 0 } };
        for (const auto& h : handles)
        {
            const float gDb = gains[static_cast<std::size_t> (h.m)];
            const auto pt = juce::Point<float> (xOf (h.hz), yEq (curveDb (h.on, h.hz)));
            const bool active = std::abs (gDb) >= 0.1f;
            const auto col = ! active ? colours::boneDim : gDb > 0.0f ? colours::limelight : colours::frost;
            if (active) fx::glowDot (g, pt, 14.0f, col.withAlpha (on ? 0.55f : 0.3f));
            g.setColour (col.withAlpha (active ? 1.0f : 0.5f));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
            g.setColour (juce::Colours::white.withAlpha (active ? 0.85f : 0.3f));
            g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre (pt.translated (-0.8f, -0.8f)));
            const juce::String text = juce::String (h.name) + "  " + formatValue (static_cast<double> (gDb), "", true);
            g.setFont (caps (8.5f, 0.14f));
            const float w = textWidth (caps (8.5f, 0.14f), text) + 4.0f;
            juce::Rectangle<float> box (w, 12.0f);
            if (h.side < 0) box = box.withPosition (pt.x - w - 9.0f, pt.y - 6.0f);
            else if (h.side > 0) box = box.withPosition (pt.x + 9.0f, pt.y - 6.0f);
            else box = box.withCentre (pt.translated (0.0f, gDb < -0.05f ? 15.0f : -15.0f));
            g.setColour (col.withAlpha (active ? 0.95f : 0.55f));
            g.drawText (text, box, juce::Justification::centred, false);
        }
        g.restoreState();

        // status, top right of the plot
        g.setFont (caps (8.5f, 0.22f));
        g.setColour (on ? colours::limelight : colours::boneFaint);
        g.drawText (on ? "CORRECTING" : "OFF  -  LISTENING ONLY", juce::Rectangle<float> (p.getRight() - 220.0f, p.getY() + 4.0f, 214.0f, 12.0f), juce::Justification::centredRight,
                    false);
        if (! on)
        {
            g.setFont (whisper (19.0f));
            g.setColour (colours::boneDim.withAlpha (0.8f));
            g.drawText ("pick a genre, set the depth, press play", juce::Rectangle<float> (p.getX(), p.getY() + 4.0f, 400.0f, 22.0f), juce::Justification::centredLeft,
                        false);
        }
    }

    /** No audio in the screenshot demo: a made-up drum bus whose kick, snare and hats swell with
        the demo's hits, and an "after" with the show's average correction on top. */
    void demoSpectrum()
    {
        const auto t = static_cast<float> (model.now);
        float kick = 0.0f, snare = 0.0f, hats = 0.0f;
        for (const auto& s : model.slots)
        {
            if (s.type == DrumType::kick) kick = s.jerk;
            if (s.type == DrumType::snare || s.type == DrumType::clap) snare = juce::jmax (snare, s.jerk);
            if (s.type == DrumType::closedHat || s.type == DrumType::openHat || s.type == DrumType::ride) hats = juce::jmax (hats, s.jerk);
        }
        auto bump = [] (float f, float centre, float width) { const float d = std::log2 (f / centre) / width; return std::exp (-d * d); };
        auto base = [=] (float f) {
            const float wobble = 1.5f * std::sin (std::log2 (f) * 7.0f + t * 1.3f) + 0.8f * std::sin (std::log2 (f) * 17.0f - t * 2.1f);
            return -44.0f - 3.0f * std::log2 (f / 1000.0f) + 20.0f * bump (f, 60.0f, 0.6f) * (0.55f + 0.45f * kick) + 9.0f * bump (f, 220.0f, 0.8f) * (0.5f + 0.5f * snare)
                   + 10.0f * bump (f, 3000.0f, 1.0f) * (0.4f + 0.6f * snare) + 12.0f * bump (f, 9000.0f, 0.9f) * (0.5f + 0.5f * hats) + wobble
                   - 30.0f * (f > 15000.0f ? std::log2 (f / 15000.0f) : 0.0f);
        };
        spectrum.synthesise (base, false);
        spectrum.synthesise ([&, this] (float f) {
            const float pos = std::log (f / 20.0f) / std::log (1000.0f) * static_cast<float> (curveSteps - 1);
            const auto i = static_cast<std::size_t> (juce::jlimit (0.0f, static_cast<float> (curveSteps - 1), pos));
            float eq = 0.0f;
            for (const auto& c : curves) eq += c[i];
            return base (f) + eq * 0.4f;
        }, true);
    }

    DrumMasterProcessor& proc;
    StageModel& model;
    juce::TextButton back;
    ParamToggle lever { proc.apvts, "ps_enabled", "THE SHOW IS OFF" };
    juce::AudioParameterChoice* genre = nullptr;
    std::array<juce::TextButton, 5> genres;
    std::unique_ptr<Knob> depth;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::array<float, PS::numMeasures> gains {};
    static constexpr std::size_t curveSteps = 161;
    std::array<plugin_dsp::BiquadCoeffs, PS::bands.size()> bandCoeffs {};
    std::array<bool, PS::bands.size()> bandOn {};
    std::array<std::array<float, curveSteps>, groups.size()> curves {};
    double curveSr = 48000.0;
    Spectrum spectrum;
    std::vector<float> scopeIn, scopeOut;
    fx::CachedLayer background;
    static constexpr float rangeDb = 12.0f;
    const juce::Rectangle<float> marquee { 330.0f, 36.0f, 440.0f, 68.0f };
    const juce::Rectangle<float> graph { 110.0f, 176.0f, 880.0f, 282.0f };
};

// ---------------------------------------------------------------------- the master's hands
class AdvancedPage final : public juce::Component
{
public:
    std::function<void()> onBack, onForget;
    float sway = 0.0f, breathe = 0.0f, time = 0.0f, flicker = 1.0f;

    explicit AdvancedPage (DrumMasterProcessor& p) : proc (p)
    {
        addAndMakeVisible (styleBack (back));
        back.onClick = [this] { if (onBack) onBack(); };
        addAndMakeVisible (styled (forget, "pill", "forget", colours::ecto));
        forget.setTooltip ("Forget what was learned about this loop (which drums are present and how they sound)");
        forget.onClick = [this] {
            if (onForget) onForget();
            forget.setButtonText ("FORGOTTEN");
        };
        const char* ks[4][2] = { { "adv_drive", "Distortion" }, { "adv_sat", "Saturation" }, { "adv_hpf", "High-pass" }, { "adv_lpf", "Low-pass" } };
        for (auto& k : ks)
        {
            knobs.push_back (std::make_unique<Knob> (proc.apvts, k[0], k[1], colours::ecto));
            addAndMakeVisible (*knobs.back());
        }
        strings.setInterceptsMouseClicks (false, false);
        strings.page = this;
        addAndMakeVisible (strings);
    }

    void resized() override
    {
        back.setBounds (20, 16, 104, 32);
        const int kx[4] = { 176, 368, 592, 784 }, ky[4] = { 388, 420, 420, 388 };
        for (int i = 0; i < 4; ++i)
            knobs[static_cast<std::size_t> (i)]->setBounds (kx[i], ky[i], 140, 170);
        forget.setBounds (430, 620, 240, 38);
        strings.setBounds (getLocalBounds());
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        background.draw (g, baseW, baseH, [r] (juce::Graphics& ig, juce::Image&, float) {
            art::room (ig, r, 0.75f);
            ig.setColour (colours::ink.withAlpha (0.45f));
            ig.fillRect (r);
            fx::vignette (ig, r, 0.9f);
            ig.setFont (whisper (19.0f));
            ig.setColour (colours::boneDim);
            ig.drawText ("on the whole bus, after every drum", juce::Rectangle<float> (250.0f, 336.0f, 600.0f, 24.0f), juce::Justification::centred, false);
        });
        if (fog.isNull()) fog = art::makeFogSprite();
        art::roomLive (g, r, time, flicker, fog);
        tipsL = art::hand (g, { 316.0f, 214.0f }, 150.0f, true, sway, 430.0f, 0.25f);
        tipsR = art::hand (g, { 784.0f, 214.0f }, 150.0f, false, -sway, 670.0f, 0.25f);
        art::mask (g, 550.0f, 112.0f + breathe, 240.0f, breathe, 0.2f);
        g.setFont (display (54.0f));
        g.setColour (colours::bone);
        g.drawText ("The Master's Hands", juce::Rectangle<float> (250.0f, 276.0f, 600.0f, 60.0f), juce::Justification::centred, false);
    }

private:
    /** Strings from the master's fingertips to each knob's pointer: he turns them. */
    struct Strings final : public juce::Component
    {
        AdvancedPage* page = nullptr;
        void paint (juce::Graphics& g) override
        {
            if (page == nullptr) return;
            const std::array<juce::Point<float>, 4> from { page->tipsL[0], page->tipsL[3], page->tipsR[0], page->tipsR[3] };
            for (std::size_t i = 0; i < 4; ++i)
            {
                auto* k = page->knobs[i].get();
                const auto to = k->pointerTip();
                const bool hot = k->slider.isMouseOverOrDragging();
                juce::Path s;
                s.startNewSubPath (from[i]);
                s.quadraticTo ((from[i].x + to.x) * 0.5f + page->sway * 2.0f, (from[i].y + to.y) * 0.5f + 10.0f, to.x, to.y);
                if (hot) fx::glowStroke (g, s, colours::ecto.withAlpha (0.8f), 1.0f, 0.8f);
                g.setColour (colours::string.withAlpha (hot ? 1.0f : 0.6f));
                g.strokePath (s, juce::PathStrokeType (1.0f));
                g.setColour (colours::brass);
                g.drawEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (to), 1.2f);
            }
        }
    };

    DrumMasterProcessor& proc;
    juce::TextButton back, forget { "FORGET THIS LOOP" };
    std::vector<std::unique_ptr<Knob>> knobs;
    Strings strings;
    std::array<juce::Point<float>, 4> tipsL {}, tipsR {};
    fx::CachedLayer background;
    juce::Image fog;
};

// ------------------------------------------------------------------------------------ about
class AboutPage final : public juce::Component
{
public:
    std::function<void()> onBack;
    AboutPage()
    {
        addAndMakeVisible (styleBack (back));
        back.onClick = [this] { if (onBack) onBack(); };
    }
    void resized() override { back.setBounds (20, 16, 104, 32); }
    void paint (juce::Graphics& g) override
    {
        background.draw (g, baseW, baseH, [] (juce::Graphics& ig, juce::Image& img, float s) {
            const auto r = juce::Rectangle<float> (static_cast<float> (baseW), static_cast<float> (baseH));
            art::room (ig, r, 0.8f);
            art::oldDrumSet (ig, { 975.0f, 406.0f }, 0.68f);
            fx::vignette (ig, r, 0.9f);
            const auto card = juce::Rectangle<float> (230.0f, 64.0f, 640.0f, 568.0f);
            fx::softShadow (ig, card, 22.0f, 0.9f, 24.0f, 10.0f);
            fx::frost (ig, img, s, card, 22.0f, 18);
            fx::glass (ig, card, 22.0f, 0.72f);

            drawOverline (ig, "ABOUT", juce::Rectangle<float> (card.getX(), card.getY() + 34.0f, card.getWidth(), 14.0f), colours::ecto, juce::Justification::centred);
            drawWordmark (ig, { card.getCentreX(), card.getY() + 110.0f }, 58.0f, juce::Justification::centred);
            const juce::String version = juce::String ("VERSION ") + JucePlugin_VersionString;
            ig.setFont (mono (11.0f));
            const float vw = textWidth (mono (11.0f), version) + 26.0f;
            const auto pill = juce::Rectangle<float> (vw, 24.0f).withCentre ({ card.getCentreX(), card.getY() + 146.0f });
            ig.setColour (juce::Colours::white.withAlpha (0.05f));
            ig.fillRoundedRectangle (pill, 12.0f);
            ig.setColour (juce::Colours::white.withAlpha (0.14f));
            ig.drawRoundedRectangle (pill, 12.0f, 1.0f);
            ig.setColour (colours::boneDim);
            ig.drawText (version, pill, juce::Justification::centred, false);

            struct Section { const char* title; juce::Colour dot; const char* text; };
            const Section sections[] = {
                { "HOW IT HEARS", drumColour (DrumType::kick),
                  "Every drum is detected 50 ms ahead and shaped only in its own frequency range.\n"
                  "Misc hits stay neutral: a wrong guess should never sound bad." },
                { "TRAINING DATA", drumColour (DrumType::closedHat),
                  "The hit classifier was trained in part on the Groove MIDI Dataset (Gillick, Roberts, Engel, Tsao and Bamman, 2019) "
                  "and the Expanded Groove MIDI Dataset (Callender, Hawthorne and Engel, 2020), Magenta, Google - both CC BY 4.0, "
                  "and on our own synthetic drum loops." },
                { "TYPE", drumColour (DrumType::crash),
                  "Instrument Serif (The Instrument Serif Project Authors), Geist and Geist Mono (The Geist Project Authors), "
                  "all under the SIL Open Font License 1.1." },
            };
            float y = card.getY() + 190.0f;
            for (const auto& sec : sections)
            {
                drawOverline (ig, sec.title, juce::Rectangle<float> (card.getX() + 56.0f, y, 400.0f, 14.0f), sec.dot);
                ig.setFont (sans (14.0f));
                ig.setColour (colours::bone.withAlpha (0.78f));
                ig.drawFittedText (sec.text, juce::Rectangle<float> (card.getX() + 56.0f, y + 22.0f, card.getWidth() - 112.0f, 70.0f).toNearestInt(),
                                   juce::Justification::topLeft, 4, 1.0f);
                y += 110.0f;
            }
            ig.setFont (whisper (18.0f));
            ig.setColour (colours::boneDim);
            ig.drawText ("made away from home, late at night", juce::Rectangle<float> (card.getX(), card.getBottom() - 50.0f, card.getWidth(), 24.0f),
                         juce::Justification::centred, false);
        });
    }

private:
    juce::TextButton back;
    fx::CachedLayer background;
};

/** Red velvet curtains over everything (The Puppet Show's entrance). */
class CurtainOverlay final : public juce::Component
{
public:
    float progress = 0.0f;
    CurtainOverlay() { setInterceptsMouseClicks (true, false); }
    void paint (juce::Graphics& g) override { art::curtain (g, getLocalBounds().toFloat(), progress); }
};

/** Shadow Play enlarged to the whole window; a click closes it. */
class ShadowOverlay final : public juce::Component
{
public:
    std::function<void()> onClose;
    explicit ShadowOverlay (StageModel& m) : model (m) {}
    void paint (juce::Graphics& g) override
    {
        background.draw (g, baseW, baseH, [] (juce::Graphics& ig, juce::Image&, float) {
            const auto r = juce::Rectangle<float> (static_cast<float> (baseW), static_cast<float> (baseH));
            ig.fillAll (colours::ink);
            fx::soft (ig, r, 4.0f, 10, [&] (juce::Graphics& sg) {
                sg.setColour (colours::ecto.withAlpha (0.05f));
                sg.fillEllipse (juce::Rectangle<float> (1000.0f, 520.0f).withCentre ({ 550.0f, 372.0f }));
                sg.setColour (colours::moon.withAlpha (0.05f));
                for (int i = 0; i < 8; ++i)
                    sg.fillEllipse (juce::Rectangle<float> (300.0f, 60.0f).withCentre ({ 1100.0f * art::hash01 (i + 3000), 600.0f + 80.0f * art::hash01 (i + 3100) }));
            });
            fx::vignette (ig, r, 0.9f);
            ig.setFont (display (42.0f));
            ig.setColour (colours::bone);
            ig.drawText ("Shadow Play", juce::Rectangle<float> (0.0f, 14.0f, r.getWidth(), 48.0f), juce::Justification::centred, false);
            ig.setFont (whisper (18.0f));
            ig.setColour (colours::boneDim);
            ig.drawText (juce::String::fromUTF8 ("what the master hears \xe2\x80\x94 one bar, hits fade like smoke \xe2\x80\x94 click to close"),
                         juce::Rectangle<float> (0.0f, 60.0f, r.getWidth(), 24.0f), juce::Justification::centred, false);
            const DrumType types[] = { DrumType::kick, DrumType::snare, DrumType::closedHat, DrumType::openHat, DrumType::crash,
                                       DrumType::ride, DrumType::rimshot, DrumType::clap, DrumType::misc };
            for (int i = 0; i < 9; ++i)   // legend chips
            {
                const auto chip = juce::Rectangle<float> (46.0f + static_cast<float> (i) * 113.0f, 632.0f, 104.0f, 30.0f);
                ig.setColour (juce::Colours::white.withAlpha (0.04f));
                ig.fillRoundedRectangle (chip, 15.0f);
                ig.setColour (juce::Colours::white.withAlpha (0.1f));
                ig.drawRoundedRectangle (chip, 15.0f, 1.0f);
                art::shadowShape (ig, types[i], { chip.getX() + 18.0f, chip.getCentreY() }, 13.0f, drumColour (types[i]), 0.6f);
                ig.setFont (caps (9.0f, 0.16f));
                ig.setColour (colours::bone.withAlpha (0.8f));
                ig.drawText (typeTitle (types[i]), chip.withTrimmedLeft (32.0f), juce::Justification::centredLeft, false);
            }
        });
        drawShadowRing (g, model, { 550.0f, 360.0f }, 440.0f, 226.0f, 2.1f);
    }
    void mouseUp (const juce::MouseEvent&) override { if (onClose) onClose(); }

private:
    StageModel& model;
    fx::CachedLayer background;
};
} // namespace ui

// ============================================================================== the editor
class DrumMasterEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrumMasterEditor (DrumMasterProcessor& p)
        : juce::AudioProcessorEditor (p), proc (p), stage (p, model), deep (p, model), show (p, model), advanced (p)
    {
        setLookAndFeel (&lnf);
        addAndMakeVisible (content);
        content.setBounds (0, 0, ui::baseW, ui::baseH);
        for (juce::Component* c : { static_cast<juce::Component*> (&stage), static_cast<juce::Component*> (&deep), static_cast<juce::Component*> (&show),
                                    static_cast<juce::Component*> (&advanced), static_cast<juce::Component*> (&about),
                                    static_cast<juce::Component*> (&shadow), static_cast<juce::Component*> (&curtain) })
        {
            content.addChildComponent (c);
            c->setBounds (0, 0, ui::baseW, ui::baseH);
        }
        stage.setVisible (true);
        wire();

        demo = juce::SystemStats::getEnvironmentVariable ("DRUM_MASTER_DEMO", {});
        if (demo.isNotEmpty()) startDemo();
        else restorePage();

        setResizable (true, true);
        setResizeLimits (900, 563, 1600, 1000);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio (static_cast<double> (ui::baseW) / static_cast<double> (ui::baseH));
        setSize (ui::baseW, ui::baseH);
        lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        startTimerHz (60);
    }

    ~DrumMasterEditor() override
    {
        stopTimer();
        ui::fx::freezeCaches() = false;
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override { g.fillAll (ui::colours::ink); }

    void resized() override
    {
        content.setTransform (juce::AffineTransform::scale (static_cast<float> (getWidth()) / static_cast<float> (ui::baseW)));
    }

private:
    enum class Page { stage, deep, show, advanced, about };
    enum class Anim { none, zoomIn, zoomOut, curtainTo, fadeTo };

    void wire()
    {
        using ui::DrumType;
        stage.onOpenSlot = [this] (int i) { openSlot (i); };
        stage.onPuppet = [this] { fadeTo (Page::advanced); };
        stage.onAbout = [this] { fadeTo (Page::about); };
        stage.onShow = [this] { curtainTo (Page::show); };
        stage.onShadow = [this] { shadow.setVisible (true); };
        stage.onListen = [this] { if (demo.isEmpty()) proc.armAutoDetect(); model.pendingCount = 0; };
        stage.onAddString = [this] {
            ++model.pendingCount;
            syncSlots();
            openSlot (static_cast<int> (model.slots.size()) - 1);
        };
        shadow.onClose = [this] { shadow.setVisible (false); };
        deep.onBack = [this] { zoomOut(); };
        deep.onChooseType = [this] (DrumType t) {
            if (model.pendingCount > 0) --model.pendingCount;
            if (demo.isEmpty()) proc.addString (t); else demoTypes |= plugin_dsp::bit (t);
            syncSlots();
            deep.show (false, t);
            proc.setUiPage (plugin_dsp::drumTypeName (t));
        };
        deep.onChangeType = [this] (DrumType t) {
            const auto old = deep.currentType();
            if (demo.isEmpty()) { proc.removeString (old); proc.addString (t); }
            else demoTypes = (demoTypes & ~plugin_dsp::bit (old)) | plugin_dsp::bit (t);
            syncSlots();
            deep.show (false, t);
            proc.setUiPage (plugin_dsp::drumTypeName (t));
        };
        deep.onCut = [this] {
            if (deep.isPending()) { if (model.pendingCount > 0) --model.pendingCount; }
            else if (demo.isEmpty()) proc.removeString (deep.currentType());
            else demoTypes &= ~plugin_dsp::bit (deep.currentType());
            zoomOut();
        };
        show.onBack = [this] { curtainTo (Page::stage); };
        show.readScope = [this] (float* in, float* out) {
            if (demo.isNotEmpty()) return false;
            proc.copyScope (in, out, ui::Spectrum::fftSize);
            return true;
        };
        show.sampleRate = [this] { return proc.getTransport().sampleRate; };
        show.meterValue = [this] (int i) {
            if (demo.isNotEmpty()) return 4.5f * std::sin (static_cast<float> (model.now) * 0.7f + static_cast<float> (i) * 1.3f);
            return proc.puppetShow().meterDb (static_cast<plugin_dsp::PuppetShow::Measure> (i));
        };
        advanced.onBack = [this] { fadeTo (Page::stage); };
        advanced.onForget = [this] { proc.forgetLoop(); };
        about.onBack = [this] { fadeTo (Page::stage); };
    }

    juce::Component& pageComponent (Page p)
    {
        switch (p)
        {
            case Page::stage: return stage;
            case Page::deep: return deep;
            case Page::show: return show;
            case Page::advanced: return advanced;
            case Page::about: return about;
        }
        return stage;
    }

    void setPageNow (Page p)
    {
        for (auto q : { Page::stage, Page::deep, Page::show, Page::advanced, Page::about })
        {
            auto& c = pageComponent (q);
            c.setVisible (q == p);
            c.setAlpha (1.0f);
            c.setTransform ({});
        }
        page = p;
        const char* names[] = { "main", "deep", "show", "advanced", "about" };
        if (p != Page::deep) proc.setUiPage (names[static_cast<int> (p)]);
    }

    void openSlot (int i)
    {
        syncSlots();
        if (i < 0 || i >= static_cast<int> (model.slots.size())) return;
        const auto& s = model.slots[static_cast<std::size_t> (i)];
        deep.show (s.pending, s.type);
        if (! s.pending) proc.setUiPage (plugin_dsp::drumTypeName (s.type));
        zoomOrigin = stage.slotPosition (i);
        start (Anim::zoomIn, Page::deep, 0.34);
    }

    void zoomOut() { start (Anim::zoomOut, Page::stage, 0.3); }
    void fadeTo (Page p) { start (Anim::fadeTo, p, 0.24); }
    void curtainTo (Page p) { start (Anim::curtainTo, p, 1.0); }

    void start (Anim a, Page target, double seconds)
    {
        anim = a;
        animTarget = target;
        animT = 0.0;
        animDuration = seconds;
        switched = false;
        ui::fx::freezeCaches() = a == Anim::zoomIn || a == Anim::zoomOut;
        auto& to = pageComponent (target);
        if (a == Anim::zoomIn || a == Anim::fadeTo)
        {
            to.setAlpha (0.0f);
            to.setVisible (true);
            to.toFront (false);
            shadow.toFront (false);
            curtain.toFront (false);
        }
        if (a == Anim::zoomOut)
        {
            stage.setVisible (true);
            stage.setAlpha (0.0f);
        }
        if (a == Anim::curtainTo)
        {
            curtain.setVisible (true);
            curtain.toFront (false);
        }
    }

    void stepAnimation (double dt)
    {
        if (anim == Anim::none) return;
        animT = juce::jmin (1.0, animT + dt / animDuration);
        const auto t = static_cast<float> (animT);
        const float e = t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f;   // ease in-out cubic
        switch (anim)
        {
            case Anim::zoomIn:
                stage.setTransform (juce::AffineTransform::scale (1.0f + 2.2f * e, 1.0f + 2.2f * e, zoomOrigin.x, zoomOrigin.y));
                stage.setAlpha (1.0f - e);
                deep.setAlpha (e);
                break;
            case Anim::zoomOut:
                stage.setTransform (juce::AffineTransform::scale (3.2f - 2.2f * e, 3.2f - 2.2f * e, zoomOrigin.x, zoomOrigin.y));
                stage.setAlpha (e);
                pageComponent (page).setAlpha (1.0f - e);
                break;
            case Anim::fadeTo:
                pageComponent (animTarget).setAlpha (e);
                break;
            case Anim::curtainTo:
            {
                // close in ~0.42 s (ease in), switch the page behind, open in ~0.58 s (ease out)
                const float closeT = juce::jmin (1.0f, t / 0.42f), openT = juce::jmax (0.0f, (t - 0.42f) / 0.58f);
                curtain.progress = t < 0.42f ? closeT * closeT * (3.0f - 2.0f * closeT) : 1.0f - openT * openT * (3.0f - 2.0f * openT);
                if (t >= 0.42f && ! switched)
                {
                    setPageNow (animTarget);
                    curtain.toFront (false);
                    switched = true;
                }
                curtain.repaint();
                break;
            }
            case Anim::none: break;
        }
        if (animT >= 1.0)
        {
            if (anim != Anim::curtainTo) setPageNow (animTarget);
            curtain.setVisible (false);
            curtain.progress = 0.0f;
            anim = Anim::none;
            ui::fx::freezeCaches() = false;
        }
    }

    /** Strings on stage = the processor's string types (or the demo's) + pending "?" strings. */
    void syncSlots()
    {
        const std::uint32_t types = demo.isNotEmpty() ? demoTypes : proc.getStringTypes();
        std::vector<ui::StageModel::Slot> next;
        for (std::size_t t = 0; t < plugin_dsp::numDrumTypes; ++t)
        {
            if (((types >> t) & 1u) == 0) continue;
            const auto type = static_cast<ui::DrumType> (t);
            if (auto* old = model.slotFor (type)) next.push_back (*old);
            else next.push_back ({ false, type, 0.0f, 0.0f });   // new: drops in
        }
        int pendingKept = 0;
        for (auto& s : model.slots)
            if (s.pending && pendingKept < model.pendingCount) { next.push_back (s); ++pendingKept; }
        for (; pendingKept < model.pendingCount; ++pendingKept)
            next.push_back ({ true, ui::DrumType::misc, 0.0f, 0.0f });
        model.slots = std::move (next);
    }

    void onHit (ui::DrumType type, float level01, float confidence, float phase)
    {
        if (auto* s = model.slotFor (type)) s->jerk = juce::jmax (s->jerk, 0.35f + 0.65f * level01);
        if (type == ui::DrumType::kick) model.pulse = juce::jmax (model.pulse, level01);
        model.addHit ({ type, phase, level01, juce::jlimit (0.25f, 1.0f, confidence), model.now });
    }

    void pullHits()
    {
        const auto tr = proc.getTransport();
        model.beatsPerBar = tr.beatsPerBar;
        const double samplesPerBeat = tr.sampleRate * 60.0 / juce::jmax (1.0, tr.bpm);
        plugin_dsp::ShadowEvent ev;
        while (proc.shadowEvents().pop (ev))
        {
            const double beats = tr.playing ? tr.ppq + static_cast<double> (ev.sample - tr.blockStart) / samplesPerBeat
                                            : static_cast<double> (ev.sample) / samplesPerBeat;
            const double bar = beats / static_cast<double> (tr.beatsPerBar);
            onHit (ev.type, juce::jlimit (0.0f, 1.0f, (ev.levelDb + 40.0f) / 40.0f), ev.confidence, static_cast<float> (bar - std::floor (bar)));
        }
        // playhead: extrapolate from the last block's position
        if (tr.playing)
        {
            if (tr.blockStart != lastBlockStart) { lastBlockStart = tr.blockStart; blockSeenAt = model.now; }
            const double beats = tr.ppq + (model.now - blockSeenAt) * tr.bpm / 60.0;
            const double bar = beats / static_cast<double> (tr.beatsPerBar);
            model.playPhase = static_cast<float> (bar - std::floor (bar));
        }
    }

    // ---------------------------------------------------------------------------- demo mode
    void startDemo()
    {
        using ui::DrumType;
        demoTypes = plugin_dsp::bit (DrumType::kick) | plugin_dsp::bit (DrumType::snare) | plugin_dsp::bit (DrumType::closedHat)
                    | plugin_dsp::bit (DrumType::openHat) | plugin_dsp::bit (DrumType::clap) | plugin_dsp::bit (DrumType::ride);
        if (demo == "listen") demoTypes = 0;   // a fresh instance: an empty stage, listening
        syncSlots();
        for (auto& s : model.slots) s.drop = 1.0f;
        model.now = 100.0;
        for (int step = 0; step < 32; ++step)   // two bars of history already on the ring
            demoStep (step, model.now - (32 - step) * 0.125);
        demoClockStart = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (demo == "kick") { deep.show (false, DrumType::kick); setPageNow (Page::deep); }
        else if (demo == "snare") { deep.show (false, DrumType::snare); setPageNow (Page::deep); }
        else if (demo == "new")
        {
            model.pendingCount = 1;
            syncSlots();
            model.slots.back().drop = 1.0f;
            deep.show (true, DrumType::misc);
            setPageNow (Page::deep);
        }
        else if (demo == "show") { show.showDemoOn(); setPageNow (Page::show); }
        else if (demo == "advanced") setPageNow (Page::advanced);
        else if (demo == "about") setPageNow (Page::about);
        else if (demo == "shadow") shadow.setVisible (true);
        else if (demo == "curtain") { curtain.setVisible (true); curtain.progress = 0.72f; curtain.toFront (false); }
    }

    void demoStep (int step, double when)
    {
        using ui::DrumType;
        const int s = step % 16;
        const float phase = static_cast<float> (s) / 16.0f + 0.004f * std::sin (static_cast<float> (step) * 1.7f);
        const double saved = model.now;
        model.now = when;
        auto hit = [this, phase] (DrumType t, float lvl, float conf) { onHit (t, lvl, conf, phase); };
        if (s == 0 || s == 6 || s == 10) hit (DrumType::kick, 0.95f, 0.95f);
        if (s == 4 || s == 12) hit (DrumType::snare, 0.8f, 0.9f);
        if (s == 12) hit (DrumType::clap, 0.55f, 0.75f);
        if (s % 2 == 0 && s != 14) hit (DrumType::closedHat, 0.35f, 0.85f);
        if (s == 14) hit (DrumType::openHat, 0.45f, 0.7f);
        if (s == 2 || s == 10) hit (DrumType::ride, 0.4f, 0.6f);
        model.now = saved;
    }

    void timerCallback() override
    {
        const double wall = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const double dt = juce::jlimit (0.0, 0.1, wall - lastTime);
        lastTime = wall;
        model.now += dt;

        if (demo.isEmpty())
        {
            model.listening = proc.getDetectMode() == DrumMasterProcessor::DetectMode::autoArmed;
            pullHits();
        }
        else
        {
            const double clock = wall - demoClockStart;
            const int step = static_cast<int> (clock / 0.125);
            for (; demoLastStep < step; ++demoLastStep) demoStep (demoLastStep + 1, model.now);
            model.playPhase = static_cast<float> (std::fmod (clock / 2.0, 1.0));
            model.listening = demo == "listen";
        }
        stage.listen.setToggleState (model.listening, juce::dontSendNotification);
        if (model.listening)
        {
            stage.listen.getProperties().set ("pulse", 0.5 + 0.5 * std::sin (model.now * 4.0));
            stage.listen.repaint();
        }
        syncSlots();

        // animations: jerks settle, new strings drop in, the master sways and breathes, the moon flickers
        const auto fdt = static_cast<float> (dt);
        for (auto& s : model.slots)
        {
            s.jerk *= std::exp (-fdt / 0.11f);
            s.drop = juce::jmin (1.0f, s.drop + fdt / 0.7f);
        }
        model.pulse *= std::exp (-fdt / 0.22f);
        const bool bypassed = proc.apvts.getRawParameterValue ("global_bypass")->load() > 0.5f;
        model.slack += ((bypassed ? 1.0f : 0.0f) - model.slack) * juce::jmin (1.0f, fdt * 5.0f);
        model.sway = 3.0f * std::sin (static_cast<float> (model.now) * 0.55f);
        model.breathe = 1.6f * std::sin (static_cast<float> (model.now) * 0.9f);
        model.flicker = random.nextFloat() < 0.004f ? 0.3f : juce::jmin (1.0f, model.flicker + fdt * 2.0f);

        stepAnimation (dt);
        ++frame;
        if (stage.isVisible()) stage.tick();
        if (deep.isVisible()) deep.tick();
        if (show.isVisible() && (frame & 1) == 0) show.refresh();   // the live EQ graph runs at 30 fps
        if (advanced.isVisible())
        {
            advanced.sway = model.sway * 2.0f;
            advanced.breathe = model.breathe;
            advanced.time = static_cast<float> (model.now);
            advanced.flicker = model.flicker;
            if ((frame & 1) == 0) advanced.repaint();   // a slow sway: 30 fps is plenty
        }
        if (shadow.isVisible()) shadow.repaint();
    }

    void restorePage()
    {
        syncSlots();
        const auto p = proc.getUiPage();
        for (std::size_t t = 0; t < plugin_dsp::numDrumTypes; ++t)
            if (p == plugin_dsp::drumTypeName (static_cast<ui::DrumType> (t)) && model.slotFor (static_cast<ui::DrumType> (t)) != nullptr)
            {
                deep.show (false, static_cast<ui::DrumType> (t));
                setPageNow (Page::deep);
                return;
            }
        setPageNow (p == "show" ? Page::show : p == "advanced" ? Page::advanced : p == "about" ? Page::about : Page::stage);
    }

    DrumMasterProcessor& proc;
    ui::LookAndFeel lnf;
    ui::StageModel model;
    juce::Component content;
    ui::StagePage stage;
    ui::DeepEditPage deep;
    ui::ShowPage show;
    ui::AdvancedPage advanced;
    ui::AboutPage about;
    ui::ShadowOverlay shadow { model };
    ui::CurtainOverlay curtain;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Random random;

    Page page = Page::stage, animTarget = Page::stage;
    Anim anim = Anim::none;
    double animT = 0.0, animDuration = 0.3, lastTime = 0.0, blockSeenAt = 0.0, demoClockStart = 0.0;
    std::int64_t lastBlockStart = -1;
    int demoLastStep = 0, frame = 0;
    bool switched = false;
    juce::Point<float> zoomOrigin { 550.0f, 300.0f };
    juce::String demo;
    std::uint32_t demoTypes = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumMasterEditor)
};
