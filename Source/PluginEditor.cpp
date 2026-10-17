#include "PluginEditor.h"

using namespace hhbm;
using namespace hhbm::ui;

static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

// ============================================================ Look
Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelHi);
    setColour (juce::ComboBox::outlineColourId, Colours::panelHi.brighter (0.1f));
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::gold);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::gold.withAlpha (0.25f));
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::Label::textColourId, Colours::dim);
    setColour (juce::TextButton::buttonColourId, Colours::panelHi);
    setColour (juce::TextButton::buttonOnColourId, Colours::gold);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::bg);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::ToggleButton::tickColourId, Colours::gold);
    setColour (juce::ToggleButton::tickDisabledColourId, Colours::dim);
    setColour (juce::Slider::thumbColourId, Colours::gold);
    setColour (juce::Slider::trackColourId, Colours::gold.withAlpha (0.6f));
    setColour (juce::Slider::backgroundColourId, Colours::panelHi);
    setColour (juce::Slider::rotarySliderFillColourId, Colours::gold);
    setColour (juce::Slider::rotarySliderOutlineColourId, Colours::panelHi);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::panel);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = start + pos * (end - start);

    juce::Path track, fill;
    track.addCentredArc (c.x, c.y, r, r, 0, start, end, true);
    fill.addCentredArc (c.x, c.y, r, r, 0, start, ang, true);
    g.setColour (Colours::panelHi);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Colours::gold);
    g.strokePath (fill, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (Colours::panel.brighter (0.15f));
    g.fillEllipse (b.withSizeKeepingCentre (r * 1.3f, r * 1.3f));
    juce::Point<float> tip (c.x + std::sin (ang) * r * 0.6f, c.y - std::cos (ang) * r * 0.6f);
    g.setColour (Colours::text);
    g.drawLine ({ c, tip }, 2.0f);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = b.getToggleState() ? Colours::gold : bgc;
    if (down) c = c.darker (0.2f);
    else if (over) c = c.brighter (0.08f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.0f);
}

juce::Font Look::getTextButtonFont (juce::TextButton&, int h)
{
    return juce::Font (juce::FontOptions (juce::jmin (14.0f, h * 0.5f), juce::Font::bold));
}

// ============================================================ Lane
void Lane::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (b, 4.0f);

    if (pattern == nullptr || pattern->lengthBeats <= 0) return;
    const double len = pattern->lengthBeats;
    const float wPerBeat = (float) (b.getWidth() / len);

    // bar lines
    for (int bar = 0; bar * 4.0 < len; ++bar)
    {
        const float x = b.getX() + (float) (bar * 4.0) * wPerBeat;
        g.setColour (bar % 4 == 0 ? Colours::panelHi.brighter (0.15f) : Colours::panelHi);
        g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
    }

    // section markers (full song)
    if (pattern->markers.size() > 1)
    {
        g.setFont (juce::FontOptions (10.0f));
        for (auto& m : pattern->markers)
        {
            const float x = b.getX() + (float) m.start * wPerBeat;
            g.setColour (Colours::gold.withAlpha (0.5f));
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            if (track == 0)
                g.drawText (sectionName (m.section), juce::Rectangle<float> (x + 3, b.getY() + 1, 80, 11), juce::Justification::left);
        }
    }

    const auto& notes = pattern->tracks[(size_t) track];
    if (! notes.empty())
    {
        int lo = 127, hi = 0;
        for (auto& n : notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
        const int range = std::max (12, hi - lo + 1);
        const float rowH = (b.getHeight() - 6.0f) / (float) range;
        const auto col = Colours::track (track);

        for (auto& n : notes)
        {
            const float x = b.getX() + (float) n.start * wPerBeat;
            const float w = std::max (2.0f, (float) n.length * wPerBeat - 1.0f);
            const float y = b.getBottom() - 3.0f - (float) (n.pitch - lo + 1) * rowH;
            g.setColour (col.withAlpha (0.35f + 0.65f * n.velocity));
            g.fillRoundedRectangle (x, y, w, std::max (2.0f, rowH - 1.0f), 1.5f);
        }
    }

    if (playhead >= 0)
    {
        const float x = b.getX() + (float) playhead * wPerBeat;
        g.setColour (Colours::text.withAlpha (0.8f));
        g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
    }
}

// ============================================================ DragTile
DragTile::DragTile (HipHopBeatMakerProcessor& p, int t, juce::String l) : proc (p), track (t), label (std::move (l))
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip (U ("Ziehen und in Cubase fallen lassen"));
}

void DragTile::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = track >= 0 ? Colours::track (track) : Colours::gold;
    g.setColour (isMouseOver() ? col.withAlpha (0.35f) : col.withAlpha (0.18f));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (col);
    g.drawRoundedRectangle (b, 5.0f, 1.2f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (label, b, juce::Justification::centred);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || e.getDistanceFromDragStart() < 6) return;
    dragStarted = true;
    const auto file = proc.writeDragFile (track);
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

// ============================================================ Editor
HipHopBeatMakerEditor::HipHopBeatMakerEditor (HipHopBeatMakerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;

    auto setupCombo = [&] (juce::ComboBox& box, juce::Label& lbl, const juce::String& text, const juce::String& id,
                           std::unique_ptr<APVTS::ComboBoxAttachment>& att)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            box.addItemList (choice->choices, 1);
        att = std::make_unique<APVTS::ComboBoxAttachment> (s, id, box);
        lbl.setText (text, juce::dontSendNotification);
        lbl.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (box);
        addAndMakeVisible (lbl);
    };
    setupCombo (styleBox,  styleLbl,  "STYLE",  "style",       styleAtt);
    setupCombo (keyBox,    keyLbl,    "KEY",    "key",         keyAtt);
    setupCombo (scaleBox,  scaleLbl,  "SCALE",  "scale",       scaleAtt);
    setupCombo (chordBox,  chordLbl,  "CHORDS", "progression", chordAtt);
    setupCombo (tuningBox, tuningLbl, "TUNING", "tuning",      tuningAtt);

    for (int i = 0; i < numSections; ++i)
    {
        auto* b = sectionButtons.add (new juce::TextButton (sectionName (i)));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { setChoice ("section", i); };
        addAndMakeVisible (b);
    }

    auto setupKnob = [&] (juce::Slider& sl, juce::Label& lbl, const juce::String& text, const juce::String& id,
                          std::unique_ptr<APVTS::SliderAttachment>& att)
    {
        sl.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sl.setPopupDisplayEnabled (true, true, this);
        att = std::make_unique<APVTS::SliderAttachment> (s, id, sl);
        lbl.setText (text, juce::dontSendNotification);
        lbl.setJustificationType (juce::Justification::centred);
        lbl.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (sl);
        addAndMakeVisible (lbl);
    };
    setupKnob (swing,     swingLbl,     "SWING",     "swing",     swingAtt);
    setupKnob (variation, variationLbl, "VARIATION", "variation", variationAtt);
    setupKnob (humanize,  humanizeLbl,  "HUMANIZE",  "humanize",  humanizeAtt);

    seed.setSliderStyle (juce::Slider::LinearHorizontal);
    seed.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
    seedAtt = std::make_unique<APVTS::SliderAttachment> (s, "seed", seed);
    seedLbl.setText ("SEED", juce::dontSendNotification);
    seedLbl.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (seed);
    addAndMakeVisible (seedLbl);

    bpm.setSliderStyle (juce::Slider::LinearHorizontal);
    bpm.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 22);
    bpmAtt = std::make_unique<APVTS::SliderAttachment> (s, "bpm", bpm);
    bpmLbl.setText ("PREVIEW BPM", juce::dontSendNotification);
    bpmLbl.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (bpm);
    addAndMakeVisible (bpmLbl);

    rerollBtn.setButtonText (U ("Neu würfeln"));
    rerollBtn.setTooltip (U ("Neue Variante mit zufälligem Seed"));
    rerollBtn.onClick = [this] { proc.reroll(); };
    addAndMakeVisible (rerollBtn);

    playBtn.setTooltip (U ("Vorhören ohne laufenden Host – läuft der Host, folgt das Plugin dem Host"));
    playBtn.onClick = [this] { proc.setPreviewPlaying (! proc.isPreviewPlaying()); };
    addAndMakeVisible (playBtn);

    soundAtt = std::make_unique<APVTS::ButtonAttachment> (s, "sound", soundBtn);
    midiOutAtt = std::make_unique<APVTS::ButtonAttachment> (s, "midiout", midiOutBtn);
    soundBtn.setTooltip (U ("Aus = nur MIDI (Plugin bleibt stumm)"));
    midiOutBtn.setTooltip (U ("Sendet die Noten zusätzlich als MIDI an den Host (Kanäle: Drums 10, Bass 1, Keys 2, Pad 3, Lead 4)"));
    addAndMakeVisible (soundBtn);
    addAndMakeVisible (midiOutBtn);

    bpmInfo.setJustificationType (juce::Justification::centredRight);
    bpmInfo.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (bpmInfo);

    for (int t = 0; t < numTracks; ++t)
    {
        auto* lane = lanes.add (new Lane (proc, t));
        addAndMakeVisible (lane);

        auto* tile = dragTiles.add (new DragTile (proc, t, juce::String (trackName (t)) + U (" MIDI ⇣")));
        addAndMakeVisible (tile);

        auto* mute = muteBtns.add (new juce::ToggleButton ("Mute"));
        muteAtts.add (new APVTS::ButtonAttachment (s, "mute" + juce::String (t), *mute));
        addAndMakeVisible (mute);

        auto* vol = volSliders.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
        volAtts.add (new APVTS::SliderAttachment (s, "vol" + juce::String (t), *vol));
        addAndMakeVisible (vol);
    }

    allTile = std::make_unique<DragTile> (proc, -1, U ("Alle Spuren ⇣ (1 Datei)"));
    addAndMakeVisible (*allTile);

    exportSectionBtn.onClick = [this] { exportAndReveal (false); };
    exportSongBtn.onClick    = [this] { exportAndReveal (true); };
    openFolderBtn.onClick    = [] { MidiExport::exportRoot().startAsProcess(); };
    exportSectionBtn.setTooltip (U ("Speichert jede Spur als eigene .mid + eine Datei mit allen Spuren"));
    exportSongBtn.setTooltip (U ("Intro – Verse – Hook – Verse – Hook – Bridge – Hook – Outro, mit Section-Markern"));
    addAndMakeVisible (exportSectionBtn);
    addAndMakeVisible (exportSongBtn);
    addAndMakeVisible (openFolderBtn);

    for (int i = 0; i < numDrumSlots; ++i)
    {
        auto* b = sampleBtns.add (new juce::TextButton());
        b->setTooltip (U ("Klick: eigenes Sample laden (WAV/AIFF/FLAC). Shift+Klick: zurück zum Synth-Sound"));
        b->onClick = [this, i]
        {
            if (juce::ModifierKeys::currentModifiers.isShiftDown()) proc.clearSample (i);
            else loadSampleFor (i);
        };
        addAndMakeVisible (b);
    }

    statusLbl.setFont (juce::FontOptions (12.0f));
    statusLbl.setColour (juce::Label::textColourId, Colours::gold);
    addAndMakeVisible (statusLbl);

    setSize (1000, 656);
    startTimerHz (30);
    timerCallback();
}

HipHopBeatMakerEditor::~HipHopBeatMakerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HipHopBeatMakerEditor::setChoice (const juce::String& id, int index)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

void HipHopBeatMakerEditor::loadSampleFor (int slot)
{
    chooser = std::make_unique<juce::FileChooser> (U ("Sample für ") + drumSlotName (slot),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, slot] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile() && ! proc.loadSample (slot, f))
                                  statusLbl.setText (U ("Sample konnte nicht geladen werden: ") + f.getFileName(), juce::dontSendNotification);
                          });
}

void HipHopBeatMakerEditor::exportAndReveal (bool fullSong)
{
    const auto folder = proc.exportSet (fullSong);
    statusLbl.setText (U ("Exportiert: ") + folder.getFullPathName(), juce::dontSendNotification);
    folder.revealToUser();
}

void HipHopBeatMakerEditor::timerCallback()
{
    const int section = (int) proc.apvts.getRawParameterValue ("section")->load();
    for (int i = 0; i < sectionButtons.size(); ++i)
        sectionButtons[i]->setToggleState (i == section, juce::dontSendNotification);

    playBtn.setButtonText (proc.isPreviewPlaying() ? "Stop" : "Play");
    playBtn.setToggleState (proc.isPreviewPlaying(), juce::dontSendNotification);

    const double h = proc.hostBpm.load();
    bpmInfo.setText (h > 0 ? "Host: " + juce::String (h, 1) + " BPM" : juce::String ("Kein Host-Tempo"), juce::dontSendNotification);

    if (proc.patternVersion.load() != lastVersion)
    {
        lastVersion = proc.patternVersion.load();
        auto pat = proc.getPattern();
        for (auto* l : lanes) l->setPattern (pat);
    }

    const double beat = proc.uiBeat.load();
    for (auto* l : lanes) l->setPlayhead (beat);

    for (int i = 0; i < sampleBtns.size(); ++i)
    {
        const auto n = proc.sampleName (i);
        const auto text = juce::String (drumSlotName (i)) + "\n" + (n.isEmpty() ? juce::String ("Synth") : n);
        if (sampleBtns[i]->getButtonText() != text)
        {
            sampleBtns[i]->setButtonText (text);
            sampleBtns[i]->setColour (juce::TextButton::buttonColourId, n.isEmpty() ? Colours::panelHi : Colours::gold.withAlpha (0.35f));
        }
    }
}

void HipHopBeatMakerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    auto header = getLocalBounds().removeFromTop (50).reduced (16, 0);
    g.setColour (Colours::gold);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("HIP-HOP BEATMAKER", header, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("by JustCreateit.ch", header.withTrimmedLeft (265), juce::Justification::centredLeft);

    g.setColour (Colours::panelHi);
    g.drawHorizontalLine (50, 16.0f, (float) getWidth() - 16.0f);

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (U ("DRUM-SAMPLES  (Klick = laden · Shift+Klick = Synth)"), 16, 582, 500, 16, juce::Justification::left);
}

void HipHopBeatMakerEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    auto header = area.removeFromTop (50);
    bpmInfo.setBounds (header.removeFromRight (220));

    // combos
    area.removeFromTop (8);
    auto row = area.removeFromTop (44);
    auto placeCombo = [&] (juce::ComboBox& box, juce::Label& lbl, int w)
    {
        auto col = row.removeFromLeft (w);
        lbl.setBounds (col.removeFromTop (16));
        box.setBounds (col.removeFromTop (26));
        row.removeFromLeft (10);
    };
    placeCombo (styleBox, styleLbl, 140);
    placeCombo (keyBox, keyLbl, 70);
    placeCombo (scaleBox, scaleLbl, 140);
    placeCombo (chordBox, chordLbl, 170);
    placeCombo (tuningBox, tuningLbl, 90);
    {
        auto col = row;
        bpmLbl.setBounds (col.removeFromTop (16));
        bpm.setBounds (col.removeFromTop (26));
    }

    // sections + play
    area.removeFromTop (10);
    row = area.removeFromTop (34);
    for (auto* b : sectionButtons)
    {
        b->setBounds (row.removeFromLeft (92));
        row.removeFromLeft (6);
    }
    playBtn.setBounds (row.removeFromRight (90));
    row.removeFromRight (8);
    rerollBtn.setBounds (row.removeFromRight (120));

    // knobs
    area.removeFromTop (8);
    row = area.removeFromTop (86);
    auto knob = [&] (juce::Slider& sl, juce::Label& lbl)
    {
        auto col = row.removeFromLeft (84);
        lbl.setBounds (col.removeFromBottom (16));
        sl.setBounds (col);
    };
    knob (swing, swingLbl);
    knob (variation, variationLbl);
    knob (humanize, humanizeLbl);
    row.removeFromLeft (20);
    {
        auto col = row.removeFromLeft (300);
        col.removeFromTop (18);
        seedLbl.setBounds (col.removeFromTop (16));
        seed.setBounds (col.removeFromTop (26));
    }
    row.removeFromLeft (20);
    {
        auto col = row;
        col.removeFromTop (20);
        soundBtn.setBounds (col.removeFromTop (24).removeFromLeft (130));
        midiOutBtn.setBounds (col.removeFromTop (24).removeFromLeft (130));
    }

    // lanes
    area.removeFromTop (8);
    for (int t = 0; t < numTracks; ++t)
    {
        auto r = area.removeFromTop (54);
        auto left = r.removeFromLeft (90);
        muteBtns[t]->setBounds (left.removeFromBottom (22));
        volSliders[t]->setBounds (left.removeFromBottom (22));
        dragTiles[t]->setBounds (r.removeFromRight (120).reduced (0, 6));
        r.removeFromRight (8);
        lanes[t]->setBounds (r.reduced (0, 3));
        area.removeFromTop (4);
    }

    // export row
    area.removeFromTop (4);
    row = area.removeFromTop (34);
    allTile->setBounds (row.removeFromLeft (200));
    row.removeFromLeft (10);
    exportSectionBtn.setBounds (row.removeFromLeft (170));
    row.removeFromLeft (8);
    exportSongBtn.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (8);
    openFolderBtn.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (10);
    statusLbl.setBounds (row);

    // samples
    area.removeFromTop (24);
    row = area.removeFromTop (40);
    const int w = (row.getWidth() - 7 * 6) / numDrumSlots;
    for (auto* b : sampleBtns)
    {
        b->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (6);
    }
}
