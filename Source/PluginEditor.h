#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "NoteEditor.h"

namespace hhbm::ui
{
namespace Colours
{
    const juce::Colour bg       { 0xff121214 };
    const juce::Colour panel    { 0xff1c1c20 };
    const juce::Colour panelHi  { 0xff26262c };
    const juce::Colour gold     { 0xfff2b134 };
    const juce::Colour text     { 0xffe8e6e1 };
    const juce::Colour dim      { 0xff8a8790 };

    inline juce::Colour track (int t)
    {
        static const juce::uint32 c[] = { 0xfff2b134, 0xffe0533d, 0xff4fb3bf, 0xff8e6fd8, 0xff7ac36a };
        return juce::Colour (c[t % 5]);
    }
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
};

/** Mini piano-roll of one track with playhead. */
class Lane : public juce::Component, public juce::SettableTooltipClient
{
public:
    Lane (HipHopBeatMakerProcessor& p, int t) : proc (p), track (t) {}
    void paint (juce::Graphics&) override;
    void setPattern (std::shared_ptr<const Pattern> p) { pattern = std::move (p); repaint(); }
    void setPlayhead (double beat) { if (beat != playhead) { playhead = beat; repaint(); } }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void (double beat)> onClick;

private:
    HipHopBeatMakerProcessor& proc;
    int track;
    std::shared_ptr<const Pattern> pattern;
    double playhead = -1;
};

/** Drag this into Cubase to drop the MIDI file. */
class DragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragTile (HipHopBeatMakerProcessor& p, int track, juce::String label);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    HipHopBeatMakerProcessor& proc;
    int track;
    juce::String label;
    bool dragStarted = false;
};
/** Settings popup for an instrument sample slot (Bass, Keys, Pad, Lead). */
class SamplePanel : public juce::Component, private juce::Timer
{
public:
    SamplePanel (HipHopBeatMakerProcessor& p, int track, std::function<void()> onLoad);
    ~SamplePanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildPeaks();
    juce::Rectangle<int> waveArea() const;

    using APVTS = juce::AudioProcessorValueTreeState;
    HipHopBeatMakerProcessor& proc;
    int track;
    std::function<void()> onLoad;

    std::shared_ptr<const SampleData> shown;
    bool initialised = false;
    std::vector<std::pair<float, float>> peaks;

    juce::TextButton loadBtn, clearBtn;
    juce::Label nameLbl, infoLbl;
    juce::Slider root, fine, attack, release, loopStart, loopEnd;
    juce::Label rootLbl, fineLbl, attackLbl, releaseLbl, lsLbl, leLbl;
    juce::ToggleButton loop { "Loop" };
    std::unique_ptr<APVTS::SliderAttachment> rootAtt, fineAtt, attAtt, relAtt, lsAtt, leAtt;
    std::unique_ptr<APVTS::ButtonAttachment> loopAtt;
};
/** Rotary knob bound to a parameter. */
class ParamKnob : public juce::Component
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& label);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

/** Full sound editor for one track: synth parameters, sample slot, presets. */
class SoundEditor : public juce::Component, private juce::Timer
{
public:
    SoundEditor (HipHopBeatMakerProcessor& p, std::function<void (int)> loadInstrumentSample,
                 std::function<void (int)> loadDrumSample);
    void setTrack (int track, bool showSampleTab);
    int getTrack() const { return track; }
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;
    /** asks for a name (used for presets) */
    std::function<void (const juce::String& title, const juce::String& def, std::function<void (juce::String)>)> askName;

private:
    void timerCallback() override;
    void showPage (bool sample);
    void showPresetMenu();
    void layoutSynth (juce::Rectangle<int> area);
    void layoutDrums (juce::Rectangle<int> area);

    HipHopBeatMakerProcessor& proc;
    std::function<void (int)> loadInstrumentSample, loadDrumSample;
    int track = -1;
    bool samplePage = false;

    juce::OwnedArray<juce::TextButton> trackTabs;
    juce::TextButton synthTab { "Synth" }, sampleTab { "Sample" }, presetBtn, defaultBtn, closeBtn { "X" };

    // synth page
    std::unique_ptr<juce::ComboBox> oscBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oscAtt;
    juce::OwnedArray<ParamKnob> knobs; // index = SynthParam (osc slot unused)
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> groups;

    // sample page
    std::unique_ptr<SamplePanel> samplePanel;

    // drum page
    juce::OwnedArray<juce::TextButton> drumSampleBtns;
    juce::OwnedArray<juce::Label> drumNames;
};
} // namespace hhbm::ui

class HipHopBeatMakerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HipHopBeatMakerEditor (HipHopBeatMakerProcessor&);
    ~HipHopBeatMakerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setChoice (const juce::String& id, int index);
    void loadSampleFor (int slot);
    void exportAndReveal (bool fullSong);
    void showSoundEditor (int track);
    void showNoteEditor (int track, double beat);
    void showProfileMenu();
    void askName (const juce::String& title, const juce::String& def, std::function<void (juce::String)> done);
    void showStyleDialog();
    void showProjectMenu();
    void showAudioMenu();
    void saveProjectAs();
    void loadInstrumentFor (int track);

    using APVTS = juce::AudioProcessorValueTreeState;

    HipHopBeatMakerProcessor& proc;
    hhbm::ui::Look look;
    juce::TooltipWindow tooltips { this, 500 };

    // top
    juce::ComboBox styleBox, keyBox, scaleBox, chordBox, tuningBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> styleAtt, keyAtt, scaleAtt, chordAtt, tuningAtt;
    juce::Label styleLbl, keyLbl, scaleLbl, chordLbl, tuningLbl;

    // sections
    juce::OwnedArray<juce::TextButton> sectionButtons;

    // groove knobs
    juce::Slider swing, variation, humanize, seed, bpm;
    std::unique_ptr<APVTS::SliderAttachment> swingAtt, variationAtt, humanizeAtt, seedAtt, bpmAtt;
    juce::Label swingLbl, variationLbl, humanizeLbl, seedLbl, bpmLbl;
    juce::TextButton rerollBtn { "Neu wuerfeln" }, playBtn { "Play" };
    juce::ToggleButton soundBtn { "Sounds" }, midiOutBtn { "MIDI Out" };
    std::unique_ptr<APVTS::ButtonAttachment> soundAtt, midiOutAtt;
    juce::Label bpmInfo;

    // lanes
    juce::OwnedArray<hhbm::ui::Lane> lanes;
    juce::OwnedArray<hhbm::ui::DragTile> dragTiles;
    juce::OwnedArray<juce::ToggleButton> muteBtns;
    juce::OwnedArray<juce::Slider> volSliders;
    juce::OwnedArray<APVTS::ButtonAttachment> muteAtts;
    juce::OwnedArray<APVTS::SliderAttachment> volAtts;
    std::array<std::unique_ptr<juce::TextButton>, hhbm::numTracks> instSampleBtns;

    // bottom
    std::unique_ptr<hhbm::ui::DragTile> allTile;
    juce::TextButton exportSectionBtn { "MIDI: Section" }, exportSongBtn { "MIDI: Full Song" }, openFolderBtn { "Export-Ordner" };

    // samples
    juce::OwnedArray<juce::TextButton> sampleBtns;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label statusLbl;
    std::unique_ptr<hhbm::ui::SoundEditor> soundEditor;
    std::unique_ptr<hhbm::ui::NoteEditor> noteEditor;
    juce::TextButton profileBtn, projectBtn, audioBtn;
    juce::ToggleButton loopBtn { "Loop" };
    std::unique_ptr<APVTS::ButtonAttachment> loopAtt;
    juce::Label posLbl, kbdLbl, kbdInfo;
    juce::ComboBox kbdBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> kbdAtt;
    int lastRenderCount = 0, kbdFlash = 0, lastNoteSeen = -1;
    std::unique_ptr<juce::AlertWindow> alert;
    bool styleDialogOpen = false;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HipHopBeatMakerEditor)
};
