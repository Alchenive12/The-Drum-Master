// File: Source/PluginProcessor.h
// The Drum Master processor: the 97 parameters of docs/02-parameters.md (Source/Parameters.h),
// the whole DSP in plugin_dsp::DrumEngine, a fixed 50 ms latency, saved non-automatable state
// (expected drum types, detect mode, UI page, MIDI mode and note map), the quick-mix Link behaviour,
// MIDI map presets, and the puppet-stage editor (PluginEditor.h, phase 4).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <plugin_dsp/DrumEngine.h>
#include <plugin_dsp/MidiMap.h>

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
    bool acceptsMidi() const override { return true; }   // MIDI mode
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

    // Non-automatable state (docs/02-parameters.md §5) and what the UI needs (docs/04-ui-concept.md).
    // Everything here is called on the message thread; changes reach the DSP through atomics that
    // processBlock applies at the start of the next block.
    enum class DetectMode { manual = 0, autoArmed = 1 };
    /** The strings on stage (drum types), without the always-allowed misc bit unless a misc string exists. */
    std::uint32_t getStringTypes() const noexcept;
    void addString (plugin_dsp::DrumType t) noexcept;
    void removeString (plugin_dsp::DrumType t) noexcept;
    void setExpectedTypes (std::uint32_t mask) noexcept;
    std::uint32_t getExpectedTypes() const noexcept { return expectedTypes.load(); }
    /** LISTEN: clear the strings, forget the loop, and let strings drop in as types are heard. */
    void armAutoDetect() noexcept;
    DetectMode getDetectMode() const noexcept { return static_cast<DetectMode> (detectMode.load()); }
    /** FORGET THIS LOOP (advanced page). */
    void forgetLoop() noexcept { forgetRequested.store (true); }

    /** Hits for Shadow Play and the string jerks (read on the UI thread only). */
    plugin_dsp::SpscQueue<plugin_dsp::ShadowEvent, 256>& shadowEvents() noexcept { return engine->shadowEvents(); }
    /** The Puppet Show's live band meters (meterDb() is safe on the UI thread). */
    const plugin_dsp::PuppetShow& puppetShow() const noexcept { return engine->puppet(); }

    /** Where the host playhead was at the start of the last block (for the Shadow Play ring). */
    struct Transport { double ppq = 0.0, bpm = 120.0, sampleRate = 48000.0; std::int64_t blockStart = 0; int beatsPerBar = 4; bool playing = false; };
    Transport getTransport() const noexcept;

    /** The Puppet Show's spectrum: a mono copy of the bus as it comes in and as it leaves (after the
        50 ms lookahead). The audio thread writes relaxed atomics into two rings, the UI copies the
        latest samples; a frame torn by a block arriving mid-copy only blurs one picture. */
    static constexpr std::size_t scopeSize = 4096;
    void copyScope (float* in, float* out, std::size_t n) const noexcept;

    // MIDI mode (docs/05-features.md F3): drum MIDI notes say which drum hits, instead of detection.
    bool getMidiMode() const noexcept { return midiMode.load(); }
    void setMidiMode (bool on) noexcept { midiMode.store (on); }
    plugin_dsp::MidiNoteMap getMidiMap() const noexcept;
    /** An edit by hand: the map no longer matches a preset. */
    void setMidiMap (const plugin_dsp::MidiNoteMap& m) noexcept;
    /** The next note-on received is assigned to this drum (-1 cancels). Works with MIDI mode off too. */
    void learnMidiNote (int drumIndex) noexcept { learnTarget.store (drumIndex); }
    int getLearnTarget() const noexcept { return learnTarget.load(); }
    /** The last note-on received (-1: none yet) and how many have arrived (the UI's activity light). */
    int getLastMidiNote() const noexcept { return lastMidiNote.load(); }
    std::uint32_t getMidiNoteCount() const noexcept { return midiNoteCount.load(); }
    /** True while MIDI notes, not audio detection, decide the hits. */
    bool isMidiDriving() const noexcept { return midiDriving.load(); }

    /** Presets: "General MIDI" is built in; others are files in midiPresetFolder(). "" = edited by hand. */
    static constexpr const char* generalMidiPreset = "General MIDI";
    static juce::File midiPresetFolder();
    juce::StringArray getMidiPresetNames() const;
    juce::String getMidiPresetName() const { return midiPresetName; }
    bool loadMidiPreset (const juce::String& name);
    bool saveMidiPreset (const juce::String& name);
    bool deleteMidiPreset (const juce::String& name);

    juce::String getUiPage() const { return uiZoom; }
    void setUiPage (const juce::String& page) { uiZoom = page; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override;   // quick-mix Link, on the message thread
    void rebaseLink() noexcept;

    std::unique_ptr<plugin_dsp::DrumEngine> engine;
    plugin_dsp::DrumParams dspParams;
    std::vector<std::atomic<float>*> raw;   // same order as params::table()

    std::atomic<std::uint32_t> expectedTypes { 0 };   // the strings (manual mode)
    std::atomic<std::uint32_t> heardTypes { 0 };      // present in the loop (auto detect), from the audio thread
    std::atomic<int> detectMode { static_cast<int> (DetectMode::autoArmed) };   // a fresh instance listens
    std::atomic<bool> forgetRequested { false };
    std::atomic<double> transportPpq { 0.0 }, transportBpm { 120.0 }, transportRate { 48000.0 };
    std::atomic<std::int64_t> transportBlockStart { 0 };
    std::atomic<int> transportBeatsPerBar { 4 };
    std::atomic<bool> transportPlaying { false };
    std::array<std::atomic<float>, scopeSize> scopeIn {}, scopeOut {};
    std::atomic<std::size_t> scopeWrite { 0 };   // total samples written (the ring index is this mod scopeSize)
    std::int64_t samplesProcessed = 0;   // audio thread only: the engine's input index at block start
    std::uint32_t appliedTypes = 0xffffffffu;   // audio thread only

    std::atomic<bool> midiMode { false }, midiDriving { false };
    std::array<std::atomic<std::int8_t>, 128> midiMap {};   // drum index per note, MidiNoteMap::none = ignored
    std::atomic<int> learnTarget { -1 }, lastMidiNote { -1 };
    std::atomic<std::uint32_t> midiNoteCount { 0 };
    std::array<plugin_dsp::MidiHit, 256> midiHits {};   // audio thread only: this block's mapped notes
    juce::String midiPresetName { generalMidiPreset };  // message thread
    bool wasPlaying = false;          // audio thread only
    juce::String uiZoom { "main" };

    std::array<juce::RangedAudioParameter*, 3> groupVolumes {};
    std::array<float, 3> linkBase {}, linkVirtual {};   // actual dB at the last tick; unclamped, so offsets survive a rail
    juce::RangedAudioParameter* linkParam = nullptr;
    juce::RangedAudioParameter* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumMasterProcessor)
};
