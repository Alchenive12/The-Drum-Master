// File: Source/PluginProcessor.cpp
#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"

namespace
{
constexpr std::array<const char*, 3> groupVolumeIds { "qm_kick_volume", "qm_snare_volume", "qm_hats_volume" };
const juce::Identifier expectedTypesKey { "expected_types" }, detectModeKey { "detect_mode" }, uiZoomKey { "ui_zoom" },
    stateVersionKey { "state_version" };
}

DrumMasterProcessor::DrumMasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      engine (std::make_unique<plugin_dsp::DrumEngine>())
{
    for (const auto& s : params::table())
    {
        raw.push_back (apvts.getRawParameterValue (s.id));
        jassert (raw.back() != nullptr);
    }
    for (std::size_t g = 0; g < groupVolumeIds.size(); ++g)
        groupVolumes[g] = apvts.getParameter (groupVolumeIds[g]);
    linkParam = apvts.getParameter ("qm_link");
    bypassParam = apvts.getParameter ("global_bypass");

    engine->prepare (48000.0);   // real rate comes in prepareToPlay; latency is 50 ms at every rate
    setLatencySamples (engine->latencySamples());
    rebaseLink();
    startTimerHz (30);
}

DrumMasterProcessor::~DrumMasterProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumMasterProcessor::createParameterLayout()
{
    return params::createLayout();
}

void DrumMasterProcessor::prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
{
    engine->prepare (sampleRate);   // allocates: only here, never in processBlock
    appliedTypes = 0xffffffffu;
    samplesProcessed = 0;
    transportRate.store (sampleRate);
    wasPlaying = false;
    setLatencySamples (engine->latencySamples());
}

bool DrumMasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

juce::AudioProcessorParameter* DrumMasterProcessor::getBypassParameter() const
{
    return bypassParam;   // host bypass = our bypass: the 50 ms latency stays, so nothing jumps
}

void DrumMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto& specs = params::table();
    for (std::size_t i = 0; i < specs.size(); ++i)
        specs[i].apply (dspParams, raw[i]->load (std::memory_order_relaxed));
    engine->setParams (dspParams);

    if (forgetRequested.exchange (false))
        engine->resetLearning();

    // Transport for the UI (Shadow Play ring) and for auto detect.
    bool playing = false;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            playing = position->getIsPlaying();
            if (const auto ppq = position->getPpqPosition()) transportPpq.store (*ppq);
            if (const auto bpm = position->getBpm()) transportBpm.store (*bpm);
            if (const auto sig = position->getTimeSignature())
                transportBeatsPerBar.store (juce::jmax (1, juce::roundToInt (4.0 * sig->numerator / juce::jmax (1, sig->denominator))));
        }
    transportPlaying.store (playing);
    transportBlockStart.store (samplesProcessed);

    // Auto detect: strings = the types heard so far; it ends when playback stops (they stay).
    const auto heard = engine->presentTypes();
    heardTypes.store (heard);
    const bool armed = detectMode.load() == static_cast<int> (DetectMode::autoArmed);
    if (armed && wasPlaying && ! playing && heard != 0)
    {
        expectedTypes.store (heard);
        detectMode.store (static_cast<int> (DetectMode::manual));
    }
    wasPlaying = playing;

    // The classifier may say every type while listening; afterwards only the strings (+ misc).
    const auto types = detectMode.load() == static_cast<int> (DetectMode::autoArmed)
                           ? plugin_dsp::allDrumTypes
                           : (expectedTypes.load() | plugin_dsp::bit (plugin_dsp::DrumType::misc));
    if (types != appliedTypes)
    {
        engine->setAllowedTypes (types);
        appliedTypes = types;
    }
    samplesProcessed += numSamples;

    if (numOut <= 0 || numSamples <= 0)
        return;
    float* left = buffer.getWritePointer (0);
    float* right = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const auto n = static_cast<std::size_t> (numSamples);
    const std::size_t w = scopeWrite.load (std::memory_order_relaxed);
    auto mono = [left, right] (std::size_t i) { return right != nullptr ? 0.5f * (left[i] + right[i]) : left[i]; };
    for (std::size_t i = 0; i < n; ++i)   // the spectrum's "before"
        scopeIn[(w + i) % scopeSize].store (mono (i), std::memory_order_relaxed);
    engine->process (left, right, n);
    for (std::size_t i = 0; i < n; ++i)   // ...and "after"
        scopeOut[(w + i) % scopeSize].store (mono (i), std::memory_order_relaxed);
    scopeWrite.store (w + n, std::memory_order_release);
}

void DrumMasterProcessor::copyScope (float* in, float* out, std::size_t n) const noexcept
{
    n = juce::jmin (n, scopeSize);
    const std::size_t end = scopeWrite.load (std::memory_order_acquire);
    for (std::size_t i = 0; i < n; ++i)   // oldest first
    {
        const std::size_t k = (end + scopeSize - n + i) % scopeSize;
        in[i] = scopeIn[k].load (std::memory_order_relaxed);
        out[i] = scopeOut[k].load (std::memory_order_relaxed);
    }
}

std::uint32_t DrumMasterProcessor::getStringTypes() const noexcept
{
    return detectMode.load() == static_cast<int> (DetectMode::autoArmed) ? heardTypes.load() : expectedTypes.load();
}

void DrumMasterProcessor::addString (plugin_dsp::DrumType t) noexcept
{
    setExpectedTypes (getStringTypes() | plugin_dsp::bit (t));   // adding by hand = manual mode
}

void DrumMasterProcessor::removeString (plugin_dsp::DrumType t) noexcept
{
    setExpectedTypes (getStringTypes() & ~plugin_dsp::bit (t));
}

void DrumMasterProcessor::setExpectedTypes (std::uint32_t mask) noexcept
{
    expectedTypes.store (mask & plugin_dsp::allDrumTypes);
    detectMode.store (static_cast<int> (DetectMode::manual));
}

void DrumMasterProcessor::armAutoDetect() noexcept
{
    expectedTypes.store (0);
    heardTypes.store (0);
    forgetRequested.store (true);
    detectMode.store (static_cast<int> (DetectMode::autoArmed));
}

DrumMasterProcessor::Transport DrumMasterProcessor::getTransport() const noexcept
{
    Transport t;
    t.ppq = transportPpq.load();
    t.bpm = transportBpm.load();
    t.sampleRate = transportRate.load();
    t.blockStart = transportBlockStart.load();
    t.beatsPerBar = transportBeatsPerBar.load();
    t.playing = transportPlaying.load();
    return t;
}

void DrumMasterProcessor::rebaseLink() noexcept
{
    for (std::size_t g = 0; g < groupVolumes.size(); ++g)
        linkBase[g] = groupVolumes[g]->convertFrom0to1 (groupVolumes[g]->getValue());
    linkVirtual = linkBase;
}

void DrumMasterProcessor::timerCallback()
{
    // Link keeps the dB offsets between the three group volumes: when exactly one of them moved
    // since the last tick, the other two move by the same number of dB (clamped to their range; the
    // unclamped position is kept, so a knob pushed against its rail comes back to its offset).
    std::array<float, 3> now {};
    int moved = -1, movedCount = 0;
    for (std::size_t g = 0; g < groupVolumes.size(); ++g)
    {
        now[g] = groupVolumes[g]->convertFrom0to1 (groupVolumes[g]->getValue());
        if (std::abs (now[g] - linkBase[g]) > 1.0e-4f)
        {
            moved = static_cast<int> (g);
            ++movedCount;
        }
    }
    if (movedCount == 1 && linkParam->getValue() > 0.5f)
    {
        const float delta = now[static_cast<std::size_t> (moved)] - linkBase[static_cast<std::size_t> (moved)];
        for (std::size_t g = 0; g < groupVolumes.size(); ++g)
        {
            if (static_cast<int> (g) == moved) continue;
            auto* p = groupVolumes[g];
            linkVirtual[g] += delta;
            const float target = p->getNormalisableRange().snapToLegalValue (linkVirtual[g]);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (target));
            p->endChangeGesture();
            linkBase[g] = p->convertFrom0to1 (p->getValue());
        }
        linkVirtual[static_cast<std::size_t> (moved)] = linkBase[static_cast<std::size_t> (moved)] = now[static_cast<std::size_t> (moved)];
    }
    else if (movedCount > 0)
        rebaseLink();
}

juce::AudioProcessorEditor* DrumMasterProcessor::createEditor()
{
    // Phase 4: the puppet stage (docs/04-ui-concept.md).
    return new DrumMasterEditor (*this);
}

void DrumMasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (stateVersionKey, 1, nullptr);
    state.setProperty (expectedTypesKey, static_cast<int> (expectedTypes.load()), nullptr);
    state.setProperty (detectModeKey, detectMode.load() == static_cast<int> (DetectMode::autoArmed) ? "auto" : "manual", nullptr);
    state.setProperty (uiZoomKey, uiZoom, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrumMasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto state = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (state);
            const int types = state.getProperty (expectedTypesKey, 0);
            expectedTypes.store (static_cast<std::uint32_t> (types) & plugin_dsp::allDrumTypes);
            detectMode.store (state.getProperty (detectModeKey, "auto").toString() == "auto" ? static_cast<int> (DetectMode::autoArmed)
                                                                                                : static_cast<int> (DetectMode::manual));
            uiZoom = state.getProperty (uiZoomKey, "main").toString();
            rebaseLink();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumMasterProcessor();
}
