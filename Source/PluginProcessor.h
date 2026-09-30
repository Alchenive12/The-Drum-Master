// File: Source/PluginProcessor.h
// Template processor: one smoothed gain parameter, DSP math from dsp/ (plugin_dsp),
// generic editor until phase 4 (UI). Replace per plugin.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class AwayTemplateProcessor final : public juce::AudioProcessor
{
public:
    AwayTemplateProcessor();
    ~AwayTemplateProcessor() override = default;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    std::atomic<float>* gainDbParam = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AwayTemplateProcessor)
};
