// File: Source/PluginEditor.h
// The Drum Master's editor (phase 4, docs/04-ui-concept.md): the puppet stage with its strings and
// the Shadow Play ring, the quick mix, the deep edit of one drum, The Puppet Show behind its
// curtain, the master's hands (advanced) and the about page.
//
// Everything is laid out in a fixed 1100 x 688 design space and scaled to the window, so the
// window can be resized freely at a fixed 16:10 shape. A 60 Hz timer runs the animations; it reads
// hits from the lock-free Shadow Play queue (this is its only reader), the band meters and the
// transport (atomics). UI actions reach the DSP only through the processor's thread-safe calls.
//
// DRUM_MASTER_DEMO=<stage|kick|snare|new|show|curtain|advanced|about|shadow> (environment variable)
// makes the editor play made-up hits and open that page, for screenshots without audio. It never
// changes the sound; without the variable it does nothing.
#pragma once

#include "PluginProcessor.h"
#include "ui/Art.h"
#include "ui/Theme.h"
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
        float swing = 0.0f;            // px, sideways swing of the string's middle (shivers, landings)
        float swingVel = 0.0f;
    };
    static constexpr float landAt = 0.32f;   // drop phase where a falling drum reaches its string's length
    struct Hit { DrumType type = DrumType::misc; float phase = 0.0f, level = 0.5f, confidence = 1.0f; double born = 0.0; };

    std::vector<Slot> slots;
    int pendingCount = 0;
    std::array<Hit, 384> hits {};
    std::size_t hitNext = 0, hitCount = 0;
    float playPhase = 0.0f;
    int beatsPerBar = 4;
    double now = 0.0;
    float sway = 0.0f, breathe = 0.0f, flicker = 1.0f;
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

inline juce::Colour shapeColour (DrumType t)
{
    switch (t)
    {
        case DrumType::kick: return colours::blood.brighter (0.2f);
        case DrumType::closedHat: case DrumType::openHat: return colours::gold;
        case DrumType::crash: case DrumType::ride: return colours::gold.brighter (0.3f);
        case DrumType::misc: return colours::boneShadow;
        case DrumType::snare: case DrumType::rimshot: case DrumType::clap: break;
    }
    return colours::bone;
}

/** The Shadow Play ring: a clock face in perspective; hits stack up and fade like smoke. */
inline void drawShadowRing (juce::Graphics& g, const StageModel& m, juce::Point<float> centre, float rx, float ry, float scale)
{
    for (float f : { 0.5f, 0.75f, 1.0f })
    {
        g.setColour (colours::bone.withAlpha (f > 0.9f ? 0.22f : 0.08f));
        g.drawEllipse (juce::Rectangle<float> (rx * 2.0f * f, ry * 2.0f * f).withCentre (centre), f > 0.9f ? 1.5f : 1.0f);
    }
    for (int b = 0; b < m.beatsPerBar; ++b)   // beat marks on the rim
    {
        const float a = static_cast<float> (b) / static_cast<float> (m.beatsPerBar) * 2.0f * art::pi - art::pi * 0.5f;
        const auto p = centre.translated (std::cos (a) * rx, std::sin (a) * ry);
        g.setColour (colours::gold.withAlpha (b == 0 ? 0.9f : 0.45f));
        g.fillEllipse (juce::Rectangle<float> (5.0f * scale, 5.0f * scale).withCentre (p));
    }
    auto radius = [] (DrumType t) {
        switch (t)
        {
            case DrumType::kick: return 0.42f;
            case DrumType::misc: return 0.55f;
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
        const float a = h.phase * 2.0f * art::pi - art::pi * 0.5f, f = radius (h.type);
        const auto p = centre.translated (std::cos (a) * rx * f, std::sin (a) * ry * f);
        const float size = (7.0f + 11.0f * h.level) * scale * (1.0f + 0.35f * std::exp (-age * 3.0f));   // fresh hits pop
        art::shadowShape (g, h.type, p, size, shapeColour (h.type).withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
    }
    // the playhead: a thin green hand from the centre
    const float a = m.playPhase * 2.0f * art::pi - art::pi * 0.5f;
    const auto tip = centre.translated (std::cos (a) * rx * 1.04f, std::sin (a) * ry * 1.04f);
    g.setColour (colours::glow.withAlpha (0.7f));
    g.drawLine (centre.x, centre.y, tip.x, tip.y, 1.5f * scale);
    g.fillEllipse (juce::Rectangle<float> (6.0f * scale, 6.0f * scale).withCentre (tip));
}

inline juce::TextButton& styleBack (juce::TextButton& b)
{
    b.setButtonText (juce::String::fromUTF8 ("\xe2\x86\x90 STAGE"));
    return b;
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
        for (auto* b : { &listen, &addString, &show, &about })
            addAndMakeVisible (*b);
        addAndMakeVisible (bypass);
        listen.setTooltip ("Auto detect: clears the strings and lets them drop in as the master hears each drum");
        listen.onClick = [this] { if (onListen) onListen(); };
        addString.onClick = [this] { if (onAddString) onAddString(); };
        show.onClick = [this] { if (onShow) onShow(); };
        about.onClick = [this] { if (onAbout) onAbout(); };

        const char* quick[3][3][2] = { { { "qm_kick_volume", "Volume" }, { "qm_kick_punch", "Punch" }, { "qm_kick_weight", "Weight" } },
                                       { { "qm_snare_volume", "Volume" }, { "qm_snare_snap", "Snap" }, { "qm_snare_fat", "Fat" } },
                                       { { "qm_hats_volume", "Volume" }, { "qm_hats_tone", "Tone" }, { "qm_hats_tame", "Tame" } } };
        for (int grp = 0; grp < 3; ++grp)
            for (int k = 0; k < 3; ++k)
                knobs.push_back (std::make_unique<Knob> (s, quick[grp][k][0], quick[grp][k][1]));
        const char* globals[5][2] = { { "global_mix", "Dry/Wet" }, { "global_output", "Output" }, { "global_sensitivity", "Sensitivity" },
                                      { "global_punch", "Punch" }, { "global_clip", "Clip" } };
        for (auto& gl : globals)
            knobs.push_back (std::make_unique<Knob> (s, gl[0], gl[1]));
        for (auto& k : knobs) addAndMakeVisible (*k);
        addAndMakeVisible (link);
        link.setTooltip ("Link: the three group volumes keep their dB offsets");
    }

    void resized() override
    {
        listen.setBounds (700, 12, 88, 28);
        addString.setBounds (794, 12, 96, 28);
        show.setBounds (896, 12, 118, 28);
        bypass.setBounds (1020, 12, 36, 28);
        about.setBounds (1062, 12, 28, 28);
        bypass.setButtonText (juce::String::fromUTF8 ("\xe2\x8f\xbb"));   // power symbol
        bypass.setTooltip ("Bypass (the 50 ms latency stays, so nothing jumps)");
        const int groupX[3] = { 48, 268, 488 };
        for (int grp = 0; grp < 3; ++grp)
            for (int k = 0; k < 3; ++k)
                knobs[static_cast<std::size_t> (grp * 3 + k)]->setBounds (groupX[grp] + k * 66, 562, 64, 84);
        link.setBounds (700, 594, 58, 26);
        for (int k = 0; k < 5; ++k)
            knobs[static_cast<std::size_t> (9 + k)]->setBounds (786 + k * 56, 566, 54, 78);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        layout();
        const bool rendered = background.isAvailable();
        if (rendered)
        {
            // the 3D room with the master in it (arms raised, holding the bar above the window);
            // three lamp levels crossfade for the flicker, the moonlight stays
            background.draw (g, r, model.flicker);
        }
        else
        {
            art::room (g, r, model.flicker, static_cast<float> (model.now));
            art::oldDrumSet (g, { 975.0f, 406.0f }, 0.68f, model.flicker * 0.8f);
            art::legs (g, 550.0f, 452.0f, model.sway * 1.5f, 1.0f);   // the master is very tall: his legs stand in the room
            // the master: mask above, arms reaching down, hands holding the bar
            art::hand (g, { barX1 + 18.0f, barY }, 74.0f, true, model.sway, 400.0f);
            art::hand (g, { barX2 - 18.0f, barY }, 74.0f, false, -model.sway, 700.0f);
            art::mask (g, 550.0f, 92.0f + model.breathe, 190.0f, model.breathe);   // the head in front of the arms
            art::controlBar (g, barX1, barX2, barY + model.sway * 0.3f, 13.0f);
        }

        g.setFont (serif (24.0f, true));
        g.setColour (colours::gold);
        g.drawText ("THE DRUM MASTER", 22, 10, 300, 32, juce::Justification::centredLeft);
        if (model.listening)
        {
            g.setFont (serif (14.0f));
            g.setColour (colours::glow.withAlpha (0.6f + 0.4f * std::sin (static_cast<float> (model.now) * 3.0f)));
            g.drawText ("the master is listening...", 24, 40, 300, 18, juce::Justification::centredLeft);
        }

        // strings and their drums
        for (std::size_t i = 0; i < model.slots.size(); ++i)
        {
            const auto& s = model.slots[i];
            const float x = xs[i];
            const float y = drumY (s);
            const float top = rendered ? -6.0f : barY + 6.0f;      // the bar is above the window in the 3D art
            juce::Path str;
            str.startNewSubPath (x, top);
            str.quadraticTo (x + model.sway * 0.6f + s.swing, (top + y) * 0.5f, x, y - 34.0f);
            g.setColour (colours::string.withAlpha (0.85f));
            g.strokePath (str, juce::PathStrokeType (1.3f));
            const float appear = juce::jmin (1.0f, s.drop * 2.5f);  // a new drum fades in as it falls
            if (appear < 1.0f) g.beginTransparencyLayer (appear);
            if (s.pending) art::questionTag (g, { x, y }, 64.0f);
            else art::drum (g, s.type, { x, y }, 64.0f);
            if (appear < 1.0f) g.endTransparencyLayer();
            g.setFont (serif (13.0f, true));
            g.setColour (colours::gold.withAlpha (s.drop));
            g.drawText (s.pending ? juce::String ("CHOOSE...") : typeTitle (s.type), juce::Rectangle<float> (xs[i] - 55.0f, y + 36.0f, 110.0f, 18.0f),
                        juce::Justification::centred);
        }
        if (model.slots.empty())
        {
            g.setFont (serif (18.0f));
            g.setColour (colours::bone.withAlpha (0.7f));
            g.drawText (model.listening ? juce::String::fromUTF8 ("Play your loop\xe2\x80\xa6 the master is listening")
                                        : juce::String ("No strings yet: press LISTEN, or + STRING to hang one"),
                        juce::Rectangle<float> (250.0f, 260.0f, 600.0f, 30.0f), juce::Justification::centred);
        }

        drawShadowRing (g, model, ringCentre, ringRx, ringRy, 1.0f);

        // quick mix panel
        const auto panel = juce::Rectangle<float> (30.0f, 532.0f, 1040.0f, 146.0f);
        if (! art3d::drawImageIn (g, art3d::panel(), panel))
        {
            g.setColour (colours::night.withAlpha (0.82f));
            g.fillRoundedRectangle (panel, 6.0f);
            g.setColour (colours::goldDim);
            g.drawRoundedRectangle (panel, 6.0f, 1.0f);
        }
        g.setFont (serif (15.0f, true));
        g.setColour (colours::gold);
        const char* titles[] = { "KICK", "SNARE", "HATS" };
        const int groupX[3] = { 48, 268, 488 };
        for (int grp = 0; grp < 3; ++grp)
            g.drawText (titles[grp], groupX[grp], 540, 196, 18, juce::Justification::centred);
        g.drawText ("GLOBAL", 786, 540, 276, 18, juce::Justification::centred);
        g.setColour (colours::goldDim.withAlpha (0.6f));
        for (float x : { 262.0f, 482.0f, 772.0f })
            g.drawVerticalLine (juce::roundToInt (x), 544.0f, 668.0f);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (target (e.position) != Target::none ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

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

    /** Where a slot's drum hangs (for the zoom origin). */
    juce::Point<float> slotPosition (int i)
    {
        layout();
        if (i < 0 || i >= static_cast<int> (xs.size())) return { 550.0f, 300.0f };
        return { xs[static_cast<std::size_t> (i)], drumY (model.slots[static_cast<std::size_t> (i)]) };
    }

    juce::TextButton listen { "LISTEN" }, addString { "+ STRING" }, show { "PUPPET SHOW" }, about { "?" };

private:
    enum class Target { none, slot, puppet, ring };

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

    /** Drop-in: falls from above the window (accelerating), then the drum bobs on its string like a
        weight on a spring and settles. drop runs 0 -> 1; returns px from the rest position. */
    static float dropOffset (float x)
    {
        constexpr float fall = StageModel::landAt, height = 360.0f;
        if (x < fall)
        {
            const float u = x / fall;
            return -(1.0f - u * u) * height;
        }
        const float t = (x - fall) / (1.0f - fall);
        return 0.11f * height * std::exp (-5.0f * t) * std::sin (2.0f * art::pi * 2.6f * t) * (1.0f - t);
    }

    float drumY (const StageModel::Slot& s) const
    {
        return 318.0f - s.jerk * 26.0f + dropOffset (s.drop);
    }

    Target target (juce::Point<float> p, int* slotOut = nullptr)
    {
        layout();
        for (std::size_t i = 0; i < xs.size(); ++i)
        {
            const float y = drumY (model.slots[i]);
            const bool onDrum = p.getDistanceFrom ({ xs[i], y }) < 40.0f;
            const bool onString = std::abs (p.x - xs[i]) < 12.0f && p.y > (background.isAvailable() ? 46.0f : barY + 8.0f) && p.y < y;
            if (onDrum || onString)
            {
                if (slotOut) *slotOut = static_cast<int> (i);
                return Target::slot;
            }
        }
        const bool onBar = p.y > barY - 90.0f && p.y < barY + 12.0f && p.x > barX1 - 40.0f && p.x < barX2 + 40.0f && p.y > 46.0f;
        const bool onMask = p.getDistanceFrom ({ 550.0f, 40.0f }) < 95.0f && p.y > 46.0f;
        const bool onMaster = juce::Rectangle<float> (480.0f, 46.0f, 140.0f, 230.0f).contains (p);   // 3D art: his torso
        if (background.isAvailable() ? onMaster : (onBar || onMask)) return Target::puppet;
        const auto d = (p - ringCentre);
        if ((d.x * d.x) / (ringRx * ringRx) + (d.y * d.y) / (ringRy * ringRy) < 1.1f) return Target::ring;
        return Target::none;
    }

    DrumMasterProcessor& proc;
    StageModel& model;
    std::vector<std::unique_ptr<Knob>> knobs;
    ParamToggle link { proc.apvts, "qm_link", "LINK" };
    ParamToggle bypass { proc.apvts, "global_bypass", "" };
    std::vector<float> xs;
    float barX1 = 430.0f, barX2 = 670.0f;
    const float barY = 168.0f;
    const juce::Point<float> ringCentre { 550.0f, 470.0f };
    art3d::StageBackground background;
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
        addAndMakeVisible (cut);
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
        knobs.clear();
        titles.clear();
        if (! pending)
        {
            const auto groups = deepEditGroups (t);
            const int rows = static_cast<int> (groups.size());
            const int rowH = juce::jmin (124, 610 / juce::jmax (1, rows));
            int y = 58;
            for (const auto& gr : groups)
            {
                titles.push_back ({ gr.title, y });
                int x = 440;
                for (const auto& k : gr.knobs)
                {
                    auto knob = std::make_unique<Knob> (proc.apvts, k.first, k.second);
                    const int size = rowH - 26;
                    knob->setBounds (x, y + 20, juce::jmax (70, size), size);
                    x += juce::jmax (70, size) + 26;
                    addAndMakeVisible (*knob);
                    knobs.push_back (std::move (knob));
                }
                y += rowH;
            }
        }
        repaint();
    }

    bool isPending() const noexcept { return pending; }
    DrumType currentType() const noexcept { return type; }

    void resized() override
    {
        back.setBounds (20, 14, 110, 30);
        typeBox.setBounds (60, 548, 300, 32);
        cut.setBounds (60, 596, 300, 30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::wallDark);
        // cold moonlight spot on the drum
        g.setGradientFill (juce::ColourGradient (colours::wall.brighter (0.35f), 210.0f, 300.0f, colours::wallDark, 210.0f, 620.0f, true));
        g.fillEllipse (juce::Rectangle<float> (420.0f, 640.0f).withCentre ({ 210.0f, 330.0f }));
        g.setColour (colours::goldDim.withAlpha (0.5f));
        g.drawVerticalLine (400, 30.0f, 660.0f);

        float jerk = 0.0f;
        for (auto& s : model.slots)
            if (s.pending == pending && (pending || s.type == type)) jerk = s.jerk;
        const float y = 300.0f - jerk * 30.0f;
        g.setColour (colours::string);
        g.drawLine (210.0f, 0.0f, 210.0f, y - 100.0f, 1.5f);
        if (pending) art::questionTag (g, { 210.0f, y }, 200.0f);
        else art::drum (g, type, { 210.0f, y }, 200.0f);

        g.setFont (serif (30.0f, true));
        g.setColour (colours::gold);
        g.drawText (pending ? juce::String ("A NEW STRING") : typeTitle (type), 20, 430, 380, 40, juce::Justification::centred);
        g.setFont (serif (14.0f, true));
        g.setColour (colours::goldDim);
        g.drawText ("THIS STRING IS A", 60, 526, 300, 20, juce::Justification::centredLeft);

        if (pending)
        {
            g.setFont (serif (22.0f));
            g.setColour (colours::bone.withAlpha (0.8f));
            g.drawFittedText (juce::String::fromUTF8 ("Choose what hangs on this string\xe2\x80\xa6\nIts knobs appear here."),
                              juce::Rectangle<int> (440, 260, 620, 80), juce::Justification::centred, 2);
        }
        g.setFont (serif (16.0f, true));
        for (const auto& t : titles)
        {
            g.setColour (colours::gold);
            g.drawText (t.first, 440, t.second, 400, 18, juce::Justification::centredLeft);
            g.setColour (colours::goldDim.withAlpha (0.4f));
            g.drawHorizontalLine (t.second + 18, 440.0f, 1070.0f);
        }
    }

private:
    DrumMasterProcessor& proc;
    StageModel& model;
    bool pending = false, updating = false;
    DrumType type = DrumType::kick;
    juce::TextButton back, cut { "CUT STRING" };
    juce::ComboBox typeBox;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::pair<juce::String, int>> titles;
};

// ------------------------------------------------------------------------- The Puppet Show
class ShowPage final : public juce::Component
{
public:
    std::function<void()> onBack;
    std::function<float (int)> meterValue;   // dB, measure index 0..4

    explicit ShowPage (DrumMasterProcessor& p) : proc (p)
    {
        auto& s = proc.apvts;
        addAndMakeVisible (styleBack (back));
        back.onClick = [this] { if (onBack) onBack(); };
        addAndMakeVisible (lever);
        genre = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter ("ps_genre"));
        const char* names[] = { "ROCK", "POP", "EDM", "METAL", "HIP-HOP" };
        for (int i = 0; i < 5; ++i)
        {
            auto& b = genres[static_cast<std::size_t> (i)];
            b.setButtonText (names[i]);
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
        depth = std::make_unique<Knob> (s, "ps_depth", "DEPTH");
        addAndMakeVisible (*depth);
        const char* ks[4][2] = { { "ps_bass", "Bass boost" }, { "ps_mud", "Mud control" }, { "ps_presence", "Mid presence" }, { "ps_high", "High control" } };
        for (auto& k : ks)
        {
            knobs.push_back (std::make_unique<Knob> (s, k[0], k[1]));
            addAndMakeVisible (*knobs.back());
        }
    }

    void refresh()
    {
        lever.setButtonText (lever.getToggleState() ? "THE SHOW IS ON" : "THE SHOW IS OFF");
        if (genre != nullptr)
        {
            const int idx = genre->getIndex();
            for (int i = 0; i < 5; ++i)
                genres[static_cast<std::size_t> (i)].setToggleState (i == idx, juce::dontSendNotification);
        }
        repaint (juce::Rectangle<int> (150, 430, 800, 250));
    }

    void resized() override
    {
        back.setBounds (20, 14, 110, 30);
        lever.setBounds (910, 14, 170, 30);
        for (int i = 0; i < 5; ++i)
            genres[static_cast<std::size_t> (i)].setBounds (180 + i * 150, 124, 136, 40);
        depth->setBounds (470, 186, 160, 190);
        const int kx[4] = { 150, 300, 690, 840 };
        for (int i = 0; i < 4; ++i)
            knobs[static_cast<std::size_t> (i)]->setBounds (kx[i], 222, 110, 134);
    }

    void paint (juce::Graphics& g) override
    {
        art::stage (g, getLocalBounds().toFloat(), 1.0f);
        // marquee with bulbs
        const auto marquee = juce::Rectangle<float> (330.0f, 26.0f, 440.0f, 76.0f);
        g.setColour (colours::velvetDark);
        g.fillRoundedRectangle (marquee, 8.0f);
        g.setColour (colours::gold);
        g.drawRoundedRectangle (marquee, 8.0f, 2.0f);
        for (int i = 0; i < 22; ++i)
        {
            const float t = static_cast<float> (i) / 22.0f;
            const auto p = t < 0.5f ? juce::Point<float> (marquee.getX() + 8.0f + (marquee.getWidth() - 16.0f) * t * 2.0f, marquee.getY() + 7.0f)
                                    : juce::Point<float> (marquee.getX() + 8.0f + (marquee.getWidth() - 16.0f) * (t - 0.5f) * 2.0f, marquee.getBottom() - 7.0f);
            const bool on = (i + static_cast<int> (juce::Time::getMillisecondCounter() / 250)) % 3 != 0;
            g.setColour (on ? colours::gold.brighter (0.4f) : colours::goldDim.darker (0.3f));
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (p));
        }
        g.setFont (serif (32.0f, true));
        g.setColour (colours::bone);
        g.drawText ("THE PUPPET SHOW", marquee.reduced (10.0f, 14.0f), juce::Justification::centred);
        g.setFont (serif (14.0f));
        g.setColour (colours::gold);
        g.drawText ("pick a genre, set the depth, press play", 300, 170, 500, 18, juce::Justification::centred);

        // live band meters: up = boost, down = cut (+-8 dB)
        const char* names[] = { "KICK SUB", "MUD", "PRESENCE", "KICK CLICK", "AIR" };
        const auto area = juce::Rectangle<float> (190.0f, 420.0f, 720.0f, 210.0f);
        g.setColour (colours::night.withAlpha (0.8f));
        g.fillRoundedRectangle (area.withTrimmedLeft (-44.0f).expanded (14.0f, 10.0f), 6.0f);
        const float mid = area.getCentreY() - 10.0f, half = 80.0f;
        g.setColour (colours::goldDim.withAlpha (0.5f));
        g.drawHorizontalLine (juce::roundToInt (mid), area.getX(), area.getRight());
        g.setFont (serif (11.0f));
        for (int db : { 8, 4, -4, -8 })
        {
            const float y = mid - static_cast<float> (db) / 8.0f * half;
            g.setColour (colours::goldDim.withAlpha (0.25f));
            g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
            g.setColour (colours::goldDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (area.getX() - 34.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
        }
        for (int i = 0; i < 5; ++i)
        {
            const float v = meterValue ? juce::jlimit (-8.0f, 8.0f, meterValue (i)) : 0.0f;
            const float x = area.getX() + 40.0f + static_cast<float> (i) * 144.0f;
            const float h = std::abs (v) / 8.0f * half;
            const auto bar = juce::Rectangle<float> (x, v >= 0.0f ? mid - h : mid, 64.0f, h);
            g.setGradientFill (juce::ColourGradient (colours::glow, x, mid, colours::glow.withAlpha (0.35f), x, v >= 0.0f ? mid - half : mid + half, false));
            g.fillRect (bar);
            g.setColour (colours::bone);
            g.setFont (serif (13.0f, true));
            g.drawText (names[i], juce::Rectangle<float> (x - 30.0f, area.getBottom() - 16.0f, 124.0f, 16.0f), juce::Justification::centred);
            g.setFont (serif (12.0f));
            g.drawText ((v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB", juce::Rectangle<float> (x - 20.0f, mid + (v >= 0.0f ? 6.0f : -20.0f), 104.0f, 14.0f), juce::Justification::centred);
        }
    }

private:
    DrumMasterProcessor& proc;
    juce::TextButton back;
    ParamToggle lever { proc.apvts, "ps_enabled", "THE SHOW IS OFF" };
    juce::AudioParameterChoice* genre = nullptr;
    std::array<juce::TextButton, 5> genres;
    std::unique_ptr<Knob> depth;
    std::vector<std::unique_ptr<Knob>> knobs;
};

// ---------------------------------------------------------------------- the master's hands
class AdvancedPage final : public juce::Component
{
public:
    std::function<void()> onBack, onForget;

    explicit AdvancedPage (DrumMasterProcessor& p) : proc (p)
    {
        addAndMakeVisible (styleBack (back));
        back.onClick = [this] { if (onBack) onBack(); };
        addAndMakeVisible (forget);
        forget.setTooltip ("Forget what was learned about this loop (which drums are present and how they sound)");
        forget.onClick = [this] {
            if (onForget) onForget();
            forget.setButtonText ("FORGOTTEN");
        };
        const char* ks[4][2] = { { "adv_drive", "Distortion" }, { "adv_sat", "Saturation" }, { "adv_hpf", "High-pass" }, { "adv_lpf", "Low-pass" } };
        for (auto& k : ks)
        {
            knobs.push_back (std::make_unique<Knob> (proc.apvts, k[0], k[1]));
            addAndMakeVisible (*knobs.back());
        }
    }

    void resized() override
    {
        back.setBounds (20, 14, 110, 30);
        for (int i = 0; i < 4; ++i)
            knobs[static_cast<std::size_t> (i)]->setBounds (230 + i * 170, 380, 130, 158);
        forget.setBounds (440, 590, 220, 34);
    }

    void paint (juce::Graphics& g) override
    {
        art::room (g, getLocalBounds().toFloat(), 1.0f, 0.0f);
        art::legs (g, 550.0f, 452.0f, 0.0f, 1.0f);
        art::hand (g, { 330.0f, 250.0f }, 150.0f, true, 0.0f, 440.0f);
        art::hand (g, { 770.0f, 250.0f }, 150.0f, false, 0.0f, 660.0f);
        art::mask (g, 550.0f, 110.0f, 230.0f, 0.0f);
        g.setFont (serif (30.0f, true));
        g.setColour (colours::gold);
        g.drawText ("THE MASTER'S HANDS", 250, 318, 600, 40, juce::Justification::centred);
        g.setFont (serif (14.0f));
        g.setColour (colours::boneShadow);
        g.drawText ("on the whole bus, after every drum", 250, 350, 600, 20, juce::Justification::centred);
    }

private:
    DrumMasterProcessor& proc;
    juce::TextButton back, forget { "FORGET THIS LOOP" };
    std::vector<std::unique_ptr<Knob>> knobs;
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
    void resized() override { back.setBounds (20, 14, 110, 30); }
    void paint (juce::Graphics& g) override
    {
        art::room (g, getLocalBounds().toFloat(), 1.0f, 0.0f);
        art::oldDrumSet (g, { 975.0f, 406.0f }, 0.68f, 0.8f);
        g.setFont (serif (34.0f, true));
        g.setColour (colours::gold);
        g.drawText ("THE DRUM MASTER", 200, 110, 700, 44, juce::Justification::centred);
        g.setFont (serif (16.0f));
        g.setColour (colours::bone);
        g.drawText (juce::String ("version ") + JucePlugin_VersionString, 200, 156, 700, 22, juce::Justification::centred);
        const juce::String credits =
            "The hit classifier was trained in part on the Groove MIDI Dataset\n"
            "(Gillick, Roberts, Engel, Tsao and Bamman, 2019) and the Expanded Groove MIDI Dataset\n"
            "(Callender, Hawthorne and Engel, 2020), Magenta, Google - both CC BY 4.0,\n"
            "and on our own synthetic drum loops.\n\n"
            "Every drum is detected 50 ms ahead and shaped only in its own frequency range.\n"
            "Misc hits stay neutral: a wrong guess should never sound bad.";
        g.setFont (serif (17.0f));
        g.drawFittedText (credits, juce::Rectangle<int> (150, 230, 800, 220), juce::Justification::centred, 8);
    }

private:
    juce::TextButton back;
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
        g.fillAll (colours::night);
        g.setGradientFill (juce::ColourGradient (colours::stageLight, 550.0f, 380.0f, colours::night, 550.0f, 700.0f, true));
        g.fillEllipse (juce::Rectangle<float> (1100.0f, 640.0f).withCentre ({ 550.0f, 380.0f }));
        g.setFont (serif (22.0f, true));
        g.setColour (colours::gold);
        g.drawText ("SHADOW PLAY", 0, 20, getWidth(), 30, juce::Justification::centred);
        g.setFont (serif (13.0f));
        g.setColour (colours::boneShadow);
        g.drawText ("what the master hears - one bar, hits fade like smoke - click to close", 0, 50, getWidth(), 18, juce::Justification::centred);
        drawShadowRing (g, model, { 550.0f, 380.0f }, 460.0f, 250.0f, 2.2f);
        const DrumType types[] = { DrumType::kick, DrumType::snare, DrumType::closedHat, DrumType::openHat, DrumType::crash,
                                   DrumType::ride, DrumType::rimshot, DrumType::clap, DrumType::misc };
        for (int i = 0; i < 9; ++i)   // legend
        {
            const float x = 90.0f + static_cast<float> (i) * 112.0f;
            art::shadowShape (g, types[i], { x, 656.0f }, 18.0f, shapeColour (types[i]));
            g.setColour (colours::bone);
            g.setFont (serif (12.0f));
            g.drawText (plugin_dsp::drumTypeName (types[i]), juce::Rectangle<float> (x + 14.0f, 648.0f, 90.0f, 16.0f), juce::Justification::centredLeft);
        }
    }
    void mouseUp (const juce::MouseEvent&) override { if (onClose) onClose(); }

private:
    StageModel& model;
};
} // namespace ui

// ============================================================================== the editor
class DrumMasterEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrumMasterEditor (DrumMasterProcessor& p)
        : juce::AudioProcessorEditor (p), proc (p), stage (p, model), deep (p, model), show (p), advanced (p)
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
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override { g.fillAll (ui::colours::night); }

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
        start (Anim::zoomIn, Page::deep, 0.32);
    }

    void zoomOut() { start (Anim::zoomOut, Page::stage, 0.28); }
    void fadeTo (Page p) { start (Anim::fadeTo, p, 0.2); }
    void curtainTo (Page p) { start (Anim::curtainTo, p, 0.95); }

    void start (Anim a, Page target, double seconds)
    {
        anim = a;
        animTarget = target;
        animT = 0.0;
        animDuration = seconds;
        switched = false;
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
        const float e = t * t * (3.0f - 2.0f * t);   // smoothstep
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
                // close in ~0.4 s, switch the page behind, open in ~0.55 s
                curtain.progress = t < 0.42f ? t / 0.42f : juce::jmax (0.0f, 1.0f - (t - 0.42f) / 0.58f);
                if (t >= 0.42f && ! switched)
                {
                    setPageNow (animTarget);
                    curtain.toFront (false);
                    switched = true;
                }
                curtain.repaint();
                break;
            case Anim::none: break;
        }
        if (animT >= 1.0)
        {
            if (anim != Anim::curtainTo) setPageNow (animTarget);
            curtain.setVisible (false);
            curtain.progress = 0.0f;
            anim = Anim::none;
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
        else if (demo == "show") setPageNow (Page::show);
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
            model.listening = false;
        }
        stage.listen.setToggleState (model.listening, juce::dontSendNotification);
        syncSlots();

        // animations: jerks settle, new strings drop in, the master sways and breathes, the light flickers
        const float fdt = static_cast<float> (dt);
        for (std::size_t i = 0; i < model.slots.size(); ++i)
        {
            auto& s = model.slots[i];
            s.jerk *= static_cast<float> (std::exp (-dt / 0.11));
            const float before = s.drop;
            s.drop = juce::jmin (1.0f, s.drop + fdt / 0.95f);
            if (before < ui::StageModel::landAt && s.drop >= ui::StageModel::landAt)
            {
                // it landed: the bar above jolts, the neighbouring strings shiver
                s.swingVel += (i % 2 == 0 ? 1.0f : -1.0f) * 160.0f;
                for (std::size_t j : { i - 1, i + 1 })                // i - 1 wraps for i == 0: skipped
                    if (j < model.slots.size())
                    {
                        model.slots[j].jerk = juce::jmax (model.slots[j].jerk, 0.28f);
                        model.slots[j].swingVel += (j < i ? -1.0f : 1.0f) * 90.0f;
                    }
            }
        }
        for (auto& s : model.slots)                                    // damped spring for the swings
        {
            s.swingVel += (-120.0f * s.swing - 7.0f * s.swingVel) * fdt;
            s.swing += s.swingVel * fdt;
        }
        model.sway = 3.0f * std::sin (static_cast<float> (model.now) * 0.55f);
        model.breathe = 1.6f * std::sin (static_cast<float> (model.now) * 0.9f);
        // now and then the warm lamp nearly goes out and comes back (the 3D art crossfades lamp levels)
        model.flicker = random.nextFloat() < 0.0015f ? 0.18f + 0.15f * random.nextFloat()
                                                     : juce::jmin (1.0f, model.flicker + static_cast<float> (dt * 2.0));

        stepAnimation (dt);
        if (stage.isVisible()) stage.repaint();
        if (deep.isVisible()) deep.repaint (0, 0, 400, 520);
        if (show.isVisible()) show.refresh();
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
    int demoLastStep = 0;
    bool switched = false;
    juce::Point<float> zoomOrigin { 550.0f, 300.0f };
    juce::String demo;
    std::uint32_t demoTypes = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumMasterEditor)
};
