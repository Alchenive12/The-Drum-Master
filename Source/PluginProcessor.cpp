// File: Source/PluginProcessor.cpp
#include "PluginProcessor.h"
#include <plugin_dsp/GainMath.h>

namespace ParamIDs
{
    // Parameter IDs are permanent once a session has been saved with them.
    static constexpr const char* gain = "gain";
}

AwayTemplateProcessor::AwayTemplateProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    gainDbParam = apvts.getRawParameterValue (ParamIDs::gain);
    jassert (gainDbParam != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout AwayTemplateProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::gain, 1 },
        "Gain",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void AwayTemplateProcessor::prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
{
    gainSmoothed.reset (sampleRate, 0.02);   // 20 ms ramp
    gainSmoothed.setCurrentAndTargetValue (plugin_dsp::dbToGain (gainDbParam->load()));
}

bool AwayTemplateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void AwayTemplateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    gainSmoothed.setTargetValue (plugin_dsp::dbToGain (gainDbParam->load (std::memory_order_relaxed)));

    if (! gainSmoothed.isSmoothing())
    {
        buffer.applyGain (gainSmoothed.getTargetValue());
        return;
    }

    auto* const* channels = buffer.getArrayOfWritePointers();
    const int numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= g;
    }
}

juce::AudioProcessorEditor* AwayTemplateProcessor::createEditor()
{
    // Phase 3: generic editor, so the plugin is fully testable before any custom UI exists.
    return new juce::GenericAudioProcessorEditor (*this);
}

void AwayTemplateProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AwayTemplateProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AwayTemplateProcessor();
}
