// File: Source/PluginProcessor.h
// The Drum Master processor: the 97 parameters of docs/02-parameters.md (Source/Parameters.h),
// the whole DSP in plugin_dsp::DrumEngine, a fixed 50 ms latency, saved non-automatable state
// (expected drum types, detect mode, UI page), the quick-mix Link behaviour, and the generic
// editor until phase 4 (UI).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <plugin_dsp/DrumEngine.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

class DrumMasterProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    DrumMasterProcessor();
    ~DrumMasterProcessor() override;

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
    juce::AudioProcessorParameter* getBypassParameter() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Non-automatable state (docs/02-parameters.md §5), for the UI in phase 4.
    enum class DetectMode { manual = 0, autoArmed = 1 };
    void setExpectedTypes (std::uint32_t mask) noexcept;
    std::uint32_t getExpectedTypes() const noexcept { return expectedTypes.load(); }
    void armAutoDetect() noexcept;
    DetectMode getDetectMode() const noexcept { return static_cast<DetectMode> (detectMode.load()); }

    /** Hits for Shadow Play (read on the UI thread only). */
    plugin_dsp::SpscQueue<plugin_dsp::ShadowEvent, 256>& shadowEvents() noexcept { return engine->shadowEvents(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override;   // quick-mix Link, on the message thread
    void rebaseLink() noexcept;

    std::unique_ptr<plugin_dsp::DrumEngine> engine;
    plugin_dsp::DrumParams dspParams;
    std::vector<std::atomic<float>*> raw;   // same order as params::table()

    std::atomic<std::uint32_t> expectedTypes { plugin_dsp::allDrumTypes };
    std::atomic<int> detectMode { static_cast<int> (DetectMode::manual) };
    std::uint32_t appliedTypes = 0;   // audio thread only
    bool wasPlaying = false;          // audio thread only
    juce::String uiZoom { "main" };

    std::array<juce::RangedAudioParameter*, 3> groupVolumes {};
    std::array<float, 3> linkBase {};
    juce::RangedAudioParameter* linkParam = nullptr;
    juce::RangedAudioParameter* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumMasterProcessor)
};
