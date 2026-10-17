#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace hhbm::ui
{
/** Piano roll / drum grid for one section + track, with velocity lane. */
class NoteEditor : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit NoteEditor (HipHopBeatMakerProcessor& p);
    ~NoteEditor() override;

    void open (int track, int section);
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void reload (bool keepSelection);
    void commit();
    void updateButtons();

    // geometry
    juce::Rectangle<int> rollArea() const;
    juce::Rectangle<int> keysArea() const;
    juce::Rectangle<int> velArea() const;
    double lengthBeats() const;
    bool drumMode() const { return track == 0; }
    int numRows() const;
    int rowToPitch (int row) const;   // row 0 = top
    int pitchToRow (int pitch) const; // -1 if not visible/mapped
    float rowHeight() const;
    float beatToX (double beat) const;
    double xToBeat (float x) const;
    int yToRow (float y) const;
    juce::Rectangle<float> noteRect (const Note& n) const;
    int hitNote (juce::Point<float> p, bool& onEdge) const;
    double grid() const;
    double snap (double beat) const { return std::round (beat / grid()) * grid(); }
    double snapDown (double beat) const { return std::floor (beat / grid() + 1.0e-6) * grid(); }

    // editing
    void selectAll (bool on);
    bool anySelected() const;
    void deleteSelected();
    void copySelected();
    void paste (double atBeat);
    void duplicateSelected();
    void quantize();
    void transpose (int semis);
    void nudge (double beats);

    HipHopBeatMakerProcessor& proc;
    int track = 1, section = 1;

    std::vector<Note> notes;
    std::vector<bool> sel;
    int lastVersion = -1;
    int topPitch = 84;

    enum class Drag { None, Move, Resize, Rubber, Velocity };
    Drag drag = Drag::None;
    std::vector<Note> dragOrig;
    juce::Point<float> dragStart;
    juce::Rectangle<float> rubber;
    bool changed = false;
    double pasteBeat = -1;
    static std::vector<Note> clipboard;

    juce::OwnedArray<juce::TextButton> trackTabs, sectionTabs;
    juce::TextButton closeBtn { "X" }, unlockBtn, undoBtn, redoBtn, quantBtn, copyBtn, pasteBtn, dupBtn, delBtn;
    juce::ComboBox gridBox;
    juce::Label lockLbl, hintLbl;
};
} // namespace hhbm::ui
