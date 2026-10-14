// File: Source/PluginProcessor.cpp
#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"

namespace
{
constexpr std::array<const char*, 3> groupVolumeIds { "qm_kick_volume", "qm_snare_volume", "qm_hats_volume" };
const juce::Identifier expectedTypesKey { "expected_types" }, detectModeKey { "detect_mode" }, uiZoomKey { "ui_zoom" },
    stateVersionKey { "state_version" }, midiModeKey { "midi_mode" }, midiMapKey { "midi_map" }, midiPresetKey { "midi_preset" }, writeMiscKey { "midi_write_misc" };
const juce::Identifier presetTag { "DrumMasterMidiMap" }, presetMapKey { "map" };
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
    setMidiMap (plugin_dsp::MidiNoteMap::generalMidi());
    midiPresetName = generalMidiPreset;

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

void DrumMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    // MIDI mode: note-ons -> drums (and MIDI learn, which works with the mode off too).
    const bool midiOn = midiMode.load();
    engine->setMidiMode (midiOn);
    std::size_t numMidiHits = 0;
    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;   // velocity 0 counts as a note-off
        const int note = msg.getNoteNumber();
        if (int target = learnTarget.load(); target >= 0 && learnTarget.compare_exchange_strong (target, -1))
            midiMap[static_cast<std::size_t> (note)].store (static_cast<std::int8_t> (target));
        lastMidiNote.store (note);
        midiNoteCount.fetch_add (1);
        const int drum = midiMap[static_cast<std::size_t> (note)].load();
        if (midiOn && drum != plugin_dsp::MidiNoteMap::none && numMidiHits < midiHits.size() && numSamples > 0)
            midiHits[numMidiHits++] = { static_cast<std::size_t> (juce::jlimit (0, numSamples - 1, meta.samplePosition)),
                                        static_cast<plugin_dsp::DrumType> (drum) };
    }

    if (forgetRequested.exchange (false))
        engine->resetLearning();

    // Transport for the UI (Shadow Play ring), auto detect and MIDI write.
    bool playing = false;
    juce::Optional<double> blockPpq;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            playing = position->getIsPlaying();
            blockPpq = position->getPpqPosition();
            if (blockPpq) transportPpq.store (*blockPpq);
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
    // MIDI write: hits are timed in quarter notes from the host position (the host compensates the
    // 50 ms latency, so an onset at input sample n sits at the ppq of input sample n). The anchor
    // follows the playhead block by block; a jump (FL looping the pattern, a click elsewhere) ends
    // the take there, so loop passes never stack. Hits are decided ~40 ms late, so ones from before
    // the jump still use the old anchor.
    const std::int64_t blockStart = samplesProcessed;
    const bool writing = midiWriting.load();
    const double quartersPerSample = transportBpm.load() / (60.0 * transportRate.load());
    const double hostPpq = blockPpq ? *blockPpq : static_cast<double> (blockStart) * quartersPerSample;
    engine->setRecording (writing);
    const bool clockRuns = playing || ! blockPpq;   // a stopped host's position stands still
    if (writing && ! writeOriginSet.load())
    {
        writeAnchorPpq = hostPpq;   // the take's clock starts with playback
        writeAnchorSample = blockStart;
        writeEndSample = -1;
        if (clockRuns)
        {
            writeOrigin.store (hostPpq);
            writeOriginSet.store (true);
        }
    }
    else if (writing && writeEndSample < 0)
    {
        const double expected = writeAnchorPpq + static_cast<double> (blockStart - writeAnchorSample) * quartersPerSample;
        if (clockRuns && std::abs (hostPpq - expected) > 0.25) writeEndSample = blockStart;   // the playhead jumped
        else { writeAnchorPpq = hostPpq; writeAnchorSample = blockStart; }
    }
    if (writing && writeEndSample >= 0 && blockStart - writeEndSample > static_cast<std::int64_t> (0.1 * transportRate.load()))
        writeDone.store (true);   // every hit from before the jump has been decided
    auto ppqAt = [&] (std::int64_t sample) { return writeAnchorPpq + static_cast<double> (sample - writeAnchorSample) * quartersPerSample; };
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
    engine->process (left, right, n, midiHits.data(), numMidiHits);
    midiDriving.store (engine->midiDriving());
    plugin_dsp::ShadowEvent rec;
    while (engine->recordedHits().pop (rec))
        if (writing && writeOriginSet.load() && (writeEndSample < 0 || rec.sample < writeEndSample)) writeQueue.push ({ ppqAt (rec.sample), rec.type, rec.levelDb });
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

juce::File DrumMasterProcessor::writtenMidiFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("The Drum Master").getChildFile ("Written MIDI");
}

void DrumMasterProcessor::startMidiWrite()
{
    plugin_dsp::WrittenHit h;
    while (writeQueue.pop (h)) {}
    writtenHits.clear();
    writtenFile = juce::File();
    writeSawPlay = false;
    writeDone.store (false);
    writeOriginSet.store (false);
    midiWriting.store (true);
}

void DrumMasterProcessor::stopMidiWrite()
{
    if (! midiWriting.exchange (false)) return;
    plugin_dsp::WrittenHit h;
    while (writeQueue.pop (h)) writtenHits.push_back (h);

    // Notes from the start of the bar the take began in, so the file keeps its place in the bar.
    const int beatsPerBar = transportBeatsPerBar.load();
    const double origin = std::floor (writeOrigin.load() / beatsPerBar) * beatsPerBar;
    const auto notes = plugin_dsp::toNotes (writtenHits, getMidiMap(), writeMisc, origin);
    if (notes.empty() || ! writtenMidiFolder().createDirectory()) return;

    constexpr int ticksPerQuarter = 960, noteTicks = ticksPerQuarter / 4;
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60.0e6 / transportBpm.load())), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (beatsPerBar, 4), 0.0);
    for (const auto& n : notes)
    {
        if (n.ppq < 0.0) continue;   // decided just before the take's first bar line
        const double t = std::round (n.ppq * ticksPerQuarter);
        seq.addEvent (juce::MidiMessage::noteOn (10, n.note, static_cast<juce::uint8> (n.velocity)), t);
        seq.addEvent (juce::MidiMessage::noteOff (10, n.note), t + noteTicks);
    }
    seq.sort();
    seq.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);
    file.addTrack (seq);
    const auto out = writtenMidiFolder().getChildFile ("The Drum Master " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".mid");
    juce::FileOutputStream stream (out);
    if (stream.openedOk() && file.writeTo (stream))
    {
        writtenFile = out;
        writtenNoteCount = static_cast<int> (notes.size());
    }
}

void DrumMasterProcessor::timerCallback()
{
    // MIDI write: collect the timed hits; the take ends when playback stops.
    if (midiWriting.load())
    {
        plugin_dsp::WrittenHit h;
        while (writeQueue.pop (h)) writtenHits.push_back (h);
        const bool playing = transportPlaying.load();
        if (playing) writeSawPlay = true;
        if ((writeSawPlay && ! playing) || writeDone.load()) stopMidiWrite();
    }

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

plugin_dsp::MidiNoteMap DrumMasterProcessor::getMidiMap() const noexcept
{
    plugin_dsp::MidiNoteMap m;
    for (std::size_t n = 0; n < midiMap.size(); ++n) m.drum[n] = midiMap[n].load();
    return m;
}

void DrumMasterProcessor::setMidiMap (const plugin_dsp::MidiNoteMap& m) noexcept
{
    for (std::size_t n = 0; n < midiMap.size(); ++n) midiMap[n].store (m.drum[n]);
    midiPresetName = {};
}

juce::File DrumMasterProcessor::midiPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("The Drum Master").getChildFile ("MIDI maps");
}

juce::StringArray DrumMasterProcessor::getMidiPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : midiPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    names.removeString (generalMidiPreset);
    names.insert (0, generalMidiPreset);
    return names;
}

bool DrumMasterProcessor::loadMidiPreset (const juce::String& name)
{
    auto map = plugin_dsp::MidiNoteMap::generalMidi();
    if (name != generalMidiPreset)
    {
        const auto xml = juce::XmlDocument::parse (midiPresetFolder().getChildFile (juce::File::createLegalFileName (name) + ".xml"));
        if (xml == nullptr || ! xml->hasTagName (presetTag.toString())
            || ! plugin_dsp::MidiNoteMap::fromString (xml->getStringAttribute (presetMapKey.toString()).toStdString(), map))
            return false;
    }
    setMidiMap (map);
    midiPresetName = name;
    return true;
}

bool DrumMasterProcessor::saveMidiPreset (const juce::String& name)
{
    const auto legal = juce::File::createLegalFileName (name.trim());
    if (legal.isEmpty() || legal == generalMidiPreset || ! midiPresetFolder().createDirectory()) return false;
    juce::XmlElement xml (presetTag);
    xml.setAttribute ("version", 1);
    xml.setAttribute (presetMapKey, juce::String (getMidiMap().toString()));
    if (! xml.writeTo (midiPresetFolder().getChildFile (legal + ".xml"))) return false;
    midiPresetName = legal;
    return true;
}

bool DrumMasterProcessor::deleteMidiPreset (const juce::String& name)
{
    if (name == generalMidiPreset) return false;
    const bool ok = midiPresetFolder().getChildFile (juce::File::createLegalFileName (name) + ".xml").deleteFile();
    if (ok && midiPresetName == name) midiPresetName = {};
    return ok;
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
    state.setProperty (midiModeKey, midiMode.load(), nullptr);
    state.setProperty (midiMapKey, juce::String (getMidiMap().toString()), nullptr);
    state.setProperty (midiPresetKey, midiPresetName, nullptr);
    state.setProperty (writeMiscKey, writeMisc, nullptr);
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
            midiMode.store (static_cast<bool> (state.getProperty (midiModeKey, false)));
            auto map = plugin_dsp::MidiNoteMap::generalMidi();   // sessions from before MIDI mode: GM
            if (state.hasProperty (midiMapKey))
                plugin_dsp::MidiNoteMap::fromString (state.getProperty (midiMapKey).toString().toStdString(), map);
            setMidiMap (map);
            midiPresetName = state.getProperty (midiPresetKey, generalMidiPreset).toString();
            writeMisc = static_cast<bool> (state.getProperty (writeMiscKey, false));
            rebaseLink();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumMasterProcessor();
}
