#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

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
class Lane : public juce::Component
{
public:
    Lane (HipHopBeatMakerProcessor& p, int t) : proc (p), track (t) {}
    void paint (juce::Graphics&) override;
    void setPattern (std::shared_ptr<const Pattern> p) { pattern = std::move (p); repaint(); }
    void setPlayhead (double beat) { if (beat != playhead) { playhead = beat; repaint(); } }

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

    // bottom
    std::unique_ptr<hhbm::ui::DragTile> allTile;
    juce::TextButton exportSectionBtn { "Section exportieren" }, exportSongBtn { "Full Song exportieren" }, openFolderBtn { "Export-Ordner" };

    // samples
    juce::OwnedArray<juce::TextButton> sampleBtns;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label statusLbl;
    int lastVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HipHopBeatMakerEditor)
};
