// File: Source/PluginProcessor.cpp
#include "PluginProcessor.h"
#include "Parameters.h"

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
    appliedTypes = 0;
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

    // Auto detect ends when playback stops: the types heard become the expected types.
    bool playing = false;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
            playing = position->getIsPlaying();
    if (detectMode.load() == static_cast<int> (DetectMode::autoArmed) && wasPlaying && ! playing)
    {
        const auto heard = engine->presentTypes();
        if (heard != 0)
        {
            expectedTypes.store (heard | plugin_dsp::bit (plugin_dsp::DrumType::misc));
            detectMode.store (static_cast<int> (DetectMode::manual));
        }
    }
    wasPlaying = playing;

    const auto types = expectedTypes.load();
    if (types != appliedTypes)
    {
        engine->setAllowedTypes (types);
        appliedTypes = types;
    }

    if (numOut <= 0 || numSamples <= 0)
        return;
    float* left = buffer.getWritePointer (0);
    float* right = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    engine->process (left, right, static_cast<std::size_t> (numSamples));
}

void DrumMasterProcessor::setExpectedTypes (std::uint32_t mask) noexcept
{
    expectedTypes.store ((mask & plugin_dsp::allDrumTypes) | plugin_dsp::bit (plugin_dsp::DrumType::misc));
    detectMode.store (static_cast<int> (DetectMode::manual));
}

void DrumMasterProcessor::armAutoDetect() noexcept
{
    expectedTypes.store (plugin_dsp::allDrumTypes);
    detectMode.store (static_cast<int> (DetectMode::autoArmed));
}

void DrumMasterProcessor::rebaseLink() noexcept
{
    for (std::size_t g = 0; g < groupVolumes.size(); ++g)
        linkBase[g] = groupVolumes[g]->convertFrom0to1 (groupVolumes[g]->getValue());
}

void DrumMasterProcessor::timerCallback()
{
    // Link keeps the dB offsets between the three group volumes: when exactly one of them moved
    // since the last tick, the other two move by the same number of dB (clamped to their range).
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
            const float target = p->getNormalisableRange().snapToLegalValue (now[g] + delta);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (target));
            p->endChangeGesture();
        }
    }
    if (movedCount > 0) rebaseLink();
}

juce::AudioProcessorEditor* DrumMasterProcessor::createEditor()
{
    // Phase 3: generic editor, so the plugin is fully testable before any custom UI exists.
    return new juce::GenericAudioProcessorEditor (*this);
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
            const int types = state.getProperty (expectedTypesKey, static_cast<int> (plugin_dsp::allDrumTypes));
            expectedTypes.store ((static_cast<std::uint32_t> (types) & plugin_dsp::allDrumTypes) | plugin_dsp::bit (plugin_dsp::DrumType::misc));
            detectMode.store (state.getProperty (detectModeKey, "manual").toString() == "auto" ? static_cast<int> (DetectMode::autoArmed)
                                                                                                  : static_cast<int> (DetectMode::manual));
            uiZoom = state.getProperty (uiZoomKey, "main").toString();
            rebaseLink();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumMasterProcessor();
}
