#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "MusicTypes.h"
#include "Generator.h"
#include "SynthEngine.h"
#include "MidiExport.h"
#include "SampleTools.h"
#include "SoundParams.h"

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

    /** Instrument samples (Bass, Keys, Pad, Lead). detectRoot: set root/fine from name + pitch analysis. */
    bool loadInstrumentSample (int track, const juce::File& file, bool detectRoot);
    void clearInstrumentSample (int track);
    std::shared_ptr<const hhbm::SampleData> instrumentSample (int track) const { return engine.getInstrumentSample (track); }
    static bool trackTakesSample (int track) { return track > 0 && track < hhbm::numTracks; }
    // ------------------------------------------------------------ sound design
    /** Sets all synth + drum sound parameters to the built-in sound of a style. */
    void loadStyleSound (int style);
    /** true when the current sound differs from the built-in sound of 'style'. */
    bool isSoundModified (int style) const;

    static juce::File presetsRoot();
    static juce::File profileFolder();
    static juce::File instrumentFolder (int track);

    /** Whole sound (all instruments + drums) as a profile file. */
    juce::File saveProfile (const juce::String& name, int sourceStyle = -1);
    bool loadProfile (const juce::File& f);
    /** One instrument (track 0 = drum kit). */
    juce::File saveInstrumentPreset (int track, const juce::String& name);
    bool loadInstrumentPreset (int track, const juce::File& f);
    void loadInstrumentStyleDefault (int track);

    /** Style changed while the sound was modified: the editor asks the user what to do. */
    struct PendingStyleChange { int from = -1, to = -1; };
    PendingStyleChange pendingStyleChange; // message thread only
    enum class StyleChoice { SaveAndLoad, DiscardAndLoad, KeepSound };
    void resolveStyleChange (StyleChoice choice, const juce::String& profileName);
    std::atomic<bool> editorOpen { false };

    juce::String lastDetection; // how the root note was found ("Dateiname", "Analyse", ...)
    static juce::String sampleParamId (int track, const char* what) { return "s" + juce::String (track) + what; }

    void reroll();
    void setPreviewPlaying (bool shouldPlay) { previewPlaying = shouldPlay; }
    bool isPreviewPlaying() const { return previewPlaying; }

    /** current playback position in beats inside the pattern, < 0 when stopped */
    std::atomic<double> uiBeat { -1.0 };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> patternVersion { 0 };
    std::atomic<int> lastLiveNote { -1 };  // for the UI (keyboard activity)

    /** Restarts the internal transport at the first beat. */
    void restartPreview() { restartRequested = true; previewPlaying = true; }

    // ------------------------------------------------------------ audio export
    /** Renders mix and/or stems to WAV (48 kHz / 24 bit) on a background thread. */
    bool startAudioRender (bool fullSong, bool mix, bool stems);
    std::atomic<bool> renderBusy { false };
    std::atomic<float> renderProgress { 0.0f };
    std::atomic<int> renderCounter { 0 };
    juce::File lastRenderFolder() const { const juce::SpinLock::ScopedLockType l (renderLock); return renderFolder; }

    // ------------------------------------------------------------ projects
    bool saveProject (const juce::File& f);
    bool loadProject (const juce::File& f);
    static juce::File projectFolder();
    juce::File currentProject;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void parameterChanged (const juce::String& id, float value) override;
    void handleAsyncUpdate() override;
    void releaseHeld (juce::MidiBuffer& midi, int offset);
    void reloadSamplesFromState();

    struct Event
    {
        double beat;        // live events: sample offset in the block
        int track, pitch;
        float vel;
        bool on;
        bool live = false;  // from the MIDI keyboard
    };
    std::vector<Event> live;
    std::atomic<bool> restartRequested { false };
    juce::File renderAudioNow (bool fullSong, bool mix, bool stems);
    juce::ThreadPool renderPool { 1 };
    mutable juce::SpinLock renderLock;
    juce::File renderFolder;
    void applyParams (hhbm::SynthEngine& e) const;
    int keyboardTrack (int midiChannel) const;
    bool soundOnParam() const { return pSound->load() > 0.5f; }

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
    std::atomic<float>* pLoop = nullptr;
    std::atomic<float>* pKbd = nullptr;
    std::array<std::atomic<float>*, hhbm::numTracks> pVol {}, pMute {};
    struct SampleParamPtrs { std::atomic<float>* root = nullptr, *fine = nullptr, *att = nullptr, *rel = nullptr, *loop = nullptr, *ls = nullptr, *le = nullptr; };
    std::array<SampleParamPtrs, hhbm::numTracks> pSmp {};
    std::array<std::array<std::atomic<float>*, hhbm::numSynthParams>, hhbm::numTracks> pSyn {};
    std::array<std::array<std::atomic<float>*, hhbm::numDrumParams>, hhbm::numDrumSlots> pDrum {};

    void setParam (const juce::String& id, float value);
    std::unique_ptr<juce::XmlElement> soundToXml (int scope, int sourceStyle = -1) const; // scope -1 = everything
    bool soundFromXml (const juce::XmlElement& xml, int scope);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HipHopBeatMakerProcessor)
};
