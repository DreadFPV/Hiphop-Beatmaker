#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "MusicTypes.h"
#include "Generator.h"
#include "SynthEngine.h"
#include "MidiExport.h"

#include <bitset>

class HipHopBeatMakerProcessor : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::AsyncUpdater
{
public:
    HipHopBeatMakerProcessor();
    ~HipHopBeatMakerProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Hip-Hop BeatMaker"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ------------------------------------------------------------ app API (message thread)
    juce::AudioProcessorValueTreeState apvts;

    hhbm::Settings currentSettings() const;
    std::shared_ptr<const hhbm::Pattern> getPattern() const;
    void regenerateNow();

    double exportBpm() const;
    /** Writes one MIDI file per track + an "All Tracks" file into a new folder. Returns the folder. */
    juce::File exportSet (bool fullSong);
    /** Writes a single file for drag & drop. track = -1 -> all tracks in one file. */
    juce::File writeDragFile (int track);

    bool loadSample (int slot, const juce::File& file);
    void clearSample (int slot);
    juce::String sampleName (int slot) const;

    void reroll();
    void setPreviewPlaying (bool shouldPlay) { previewPlaying = shouldPlay; }
    bool isPreviewPlaying() const { return previewPlaying; }

    /** current playback position in beats inside the pattern, < 0 when stopped */
    std::atomic<double> uiBeat { -1.0 };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> patternVersion { 0 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void parameterChanged (const juce::String& id, float value) override;
    void handleAsyncUpdate() override;
    void releaseHeld (juce::MidiBuffer& midi, int offset);
    void reloadSamplesFromState();

    struct Event
    {
        double beat;
        int track, pitch;
        float vel;
        bool on;
    };

    struct Compiled
    {
        std::shared_ptr<const hhbm::Pattern> pattern;
        std::vector<Event> events;
    };

    std::vector<std::pair<int, const Event*>> pending;
    hhbm::SynthEngine engine;
    juce::AudioFormatManager formats;

    mutable juce::SpinLock patternLock;
    std::shared_ptr<const Compiled> compiled;     // shared with audio thread
    std::vector<std::shared_ptr<const Compiled>> graveyard; // freed on message thread

    // audio-thread state
    const Compiled* lastCompiled = nullptr;
    std::shared_ptr<const Compiled> audioCompiled;
    std::array<std::bitset<128>, hhbm::numTracks> held;
    double internalBeat = 0;
    double expectedNextBeat = -1;
    bool wasRunning = false;
    double sr = 44100;
    std::atomic<bool> previewPlaying { false };

    int lastStyle = -1;

    // raw params
    std::atomic<float>* pStyle = nullptr;
    std::atomic<float>* pTuning = nullptr;
    std::atomic<float>* pBpm = nullptr;
    std::atomic<float>* pSound = nullptr;
    std::atomic<float>* pMidiOut = nullptr;
    std::array<std::atomic<float>*, hhbm::numTracks> pVol {}, pMute {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HipHopBeatMakerProcessor)
};
