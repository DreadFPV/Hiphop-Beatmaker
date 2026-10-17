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

// ============================================================ SamplePanel
SamplePanel::SamplePanel (HipHopBeatMakerProcessor& p, int t, std::function<void()> load)
    : proc (p), track (t), onLoad (std::move (load))
{
    auto& s = proc.apvts;
    auto id = [t] (const char* w) { return HipHopBeatMakerProcessor::sampleParamId (t, w); };

    loadBtn.setButtonText (U ("Sample laden…"));
    loadBtn.onClick = [this] { if (onLoad) onLoad(); };
    clearBtn.setButtonText (U ("Synth verwenden"));
    clearBtn.onClick = [this] { proc.clearInstrumentSample (track); };
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (clearBtn);

    nameLbl.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    nameLbl.setColour (juce::Label::textColourId, Colours::text);
    infoLbl.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (nameLbl);
    addAndMakeVisible (infoLbl);

    auto setup = [&] (juce::Slider& sl, juce::Label& lbl, const juce::String& text, juce::Slider::SliderStyle style,
                      const juce::String& pid, std::unique_ptr<APVTS::SliderAttachment>& att)
    {
        sl.setSliderStyle (style);
        if (style == juce::Slider::IncDecButtons)
            sl.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 22);
        else if (style == juce::Slider::LinearHorizontal)
            sl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        else
            sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        sl.setPopupDisplayEnabled (style == juce::Slider::LinearHorizontal, true, this);
        att = std::make_unique<APVTS::SliderAttachment> (s, pid, sl);
        lbl.setText (text, juce::dontSendNotification);
        lbl.setFont (juce::FontOptions (11.0f));
        lbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (sl);
        addAndMakeVisible (lbl);
    };
    setup (root,      rootLbl,    "GRUNDTON",   juce::Slider::IncDecButtons,               id ("root"), rootAtt);
    setup (fine,      fineLbl,    "FINE (ct)",  juce::Slider::RotaryHorizontalVerticalDrag, id ("fine"), fineAtt);
    setup (attack,    attackLbl,  "ATTACK",     juce::Slider::RotaryHorizontalVerticalDrag, id ("att"),  attAtt);
    setup (release,   releaseLbl, "RELEASE",    juce::Slider::RotaryHorizontalVerticalDrag, id ("rel"),  relAtt);
    setup (loopStart, lsLbl,      "LOOP START", juce::Slider::LinearHorizontal,            id ("ls"),   lsAtt);
    setup (loopEnd,   leLbl,      "LOOP ENDE",  juce::Slider::LinearHorizontal,            id ("le"),   leAtt);
    root.setTooltip (U ("Tonhöhe, auf der das Sample aufgenommen ist (C3 = 60). Wird beim Laden automatisch erkannt."));

    loopAtt = std::make_unique<APVTS::ButtonAttachment> (s, id ("loop"), loop);
    addAndMakeVisible (loop);

    setSize (460, 300);
    startTimerHz (20);
    timerCallback();
}

SamplePanel::~SamplePanel() { stopTimer(); }

juce::Rectangle<int> SamplePanel::waveArea() const
{
    return { 12, 70, getWidth() - 24, 80 };
}

void SamplePanel::rebuildPeaks()
{
    peaks.clear();
    if (shown == nullptr) return;
    const auto& d = shown->data;
    const int w = juce::jmax (1, waveArea().getWidth());
    const int n = d.getNumSamples();
    for (int x = 0; x < w; ++x)
    {
        const int a = (int) ((juce::int64) n * x / w);
        const int b = juce::jmax (a + 1, (int) ((juce::int64) n * (x + 1) / w));
        auto r = d.findMinMax (0, a, juce::jmin (b, n) - a);
        peaks.push_back ({ r.getStart(), r.getEnd() });
    }
}

void SamplePanel::timerCallback()
{
    auto cur = proc.instrumentSample (track);
    if (cur != shown || ! initialised)
    {
        initialised = true;
        shown = cur;
        rebuildPeaks();
        nameLbl.setText (shown ? shown->name : U ("Kein Sample – interner Synth"), juce::dontSendNotification);
        infoLbl.setText (shown ? juce::String (shown->data.getNumSamples() / shown->sampleRate, 2) + " s · "
                                     + juce::String ((int) shown->sampleRate) + " Hz"
                                     + (proc.lastDetection.isNotEmpty() ? U (" · Grundton: ") + proc.lastDetection : juce::String())
                               : U ("Lade eine WAV/AIFF/FLAC – sie wird über die Tastatur transponiert."),
                         juce::dontSendNotification);
        clearBtn.setEnabled (shown != nullptr);
    }
    repaint (waveArea());
}

void SamplePanel::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
    auto wa = waveArea().toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (wa, 4.0f);

    const auto col = Colours::track (track);
    if (! peaks.empty())
    {
        const float mid = wa.getCentreY(), h = wa.getHeight() * 0.45f;
        g.setColour (col.withAlpha (0.8f));
        for (size_t x = 0; x < peaks.size(); ++x)
            g.drawVerticalLine ((int) wa.getX() + (int) x, mid - peaks[x].second * h, mid - peaks[x].first * h + 1.0f);

        if (loop.getToggleState())
        {
            const float ls = (float) loopStart.getValue(), le = juce::jmax ((float) loopEnd.getValue(), ls + 0.01f);
            auto region = wa.withX (wa.getX() + ls * wa.getWidth()).withWidth ((le - ls) * wa.getWidth());
            g.setColour (Colours::gold.withAlpha (0.15f));
            g.fillRect (region);
            g.setColour (Colours::gold);
            g.drawVerticalLine ((int) region.getX(), wa.getY(), wa.getBottom());
            g.drawVerticalLine ((int) region.getRight(), wa.getY(), wa.getBottom());
        }
    }
    else
    {
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Synth", wa, juce::Justification::centred);
    }
}

void SamplePanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (26);
    clearBtn.setBounds (top.removeFromRight (130));
    top.removeFromRight (6);
    loadBtn.setBounds (top.removeFromRight (130));
    nameLbl.setBounds (top);
    infoLbl.setBounds (r.removeFromTop (22));
    r.removeFromTop (8 + 80 + 8); // waveform

    auto row = r.removeFromTop (78);
    auto col = row.removeFromLeft (120);
    rootLbl.setBounds (col.removeFromTop (14));
    root.setBounds (col.removeFromTop (26));
    loop.setBounds (col.removeFromTop (28));
    for (auto* pair : { &fine, &attack, &release })
    {
        auto c = row.removeFromLeft (100);
        auto* lbl = pair == &fine ? &fineLbl : (pair == &attack ? &attackLbl : &releaseLbl);
        lbl->setBounds (c.removeFromTop (14));
        pair->setBounds (c);
    }

    r.removeFromTop (4);
    auto loopRow = r.removeFromTop (34);
    auto half = loopRow.getWidth() / 2;
    auto a = loopRow.removeFromLeft (half - 6);
    lsLbl.setBounds (a.removeFromTop (14));
    loopStart.setBounds (a);
    loopRow.removeFromLeft (12);
    leLbl.setBounds (loopRow.removeFromTop (14));
    loopEnd.setBounds (loopRow);
}

// ============================================================ ParamKnob
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::dim);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    slider.setDoubleClickReturnValue (true, s.getParameter (paramId)->convertFrom0to1 (s.getParameter (paramId)->getDefaultValue()));
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::FontOptions (11.0f));
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

// ============================================================ SoundEditor
SoundEditor::SoundEditor (HipHopBeatMakerProcessor& p, std::function<void (int)> loadInst, std::function<void (int)> loadDrum)
    : proc (p), loadInstrumentSample (std::move (loadInst)), loadDrumSample (std::move (loadDrum))
{
    for (int t = 0; t < numTracks; ++t)
    {
        auto* b = trackTabs.add (new juce::TextButton (trackName (t)));
        b->onClick = [this, t] { setTrack (t, t > 0 && proc.instrumentSample (t) != nullptr); };
        addAndMakeVisible (b);
    }

    synthTab.onClick = [this] { showPage (false); };
    sampleTab.onClick = [this] { showPage (true); };
    addAndMakeVisible (synthTab);
    addAndMakeVisible (sampleTab);

    presetBtn.setButtonText (U ("Presets ▾"));
    presetBtn.onClick = [this] { showPresetMenu(); };
    defaultBtn.setButtonText (U ("Style-Standard"));
    defaultBtn.setTooltip (U ("Setzt dieses Instrument auf den Sound des aktuellen Styles zurück"));
    defaultBtn.onClick = [this] { proc.loadInstrumentStyleDefault (track); };
    closeBtn.setTooltip (U ("Schliessen"));
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (presetBtn);
    addAndMakeVisible (defaultBtn);
    addAndMakeVisible (closeBtn);

    startTimerHz (10);
}

void SoundEditor::setTrack (int t, bool showSampleTab)
{
    track = t;
    knobs.clear();
    oscAtt.reset();
    oscBox.reset();
    samplePanel.reset();
    drumSampleBtns.clear();
    drumNames.clear();

    for (int i = 0; i < trackTabs.size(); ++i)
    {
        trackTabs[i]->setToggleState (i == t, juce::dontSendNotification);
        trackTabs[i]->setColour (juce::TextButton::buttonOnColourId, Colours::track (i));
    }
    synthTab.setVisible (t > 0);
    sampleTab.setVisible (t > 0);

    auto& s = proc.apvts;
    if (t == 0)
    {
        for (int slot = 0; slot < numDrumSlots; ++slot)
        {
            auto* n = drumNames.add (new juce::Label ({}, drumSlotName (slot)));
            n->setFont (juce::FontOptions (13.0f, juce::Font::bold));
            n->setColour (juce::Label::textColourId, Colours::text);
            n->setJustificationType (juce::Justification::centred);
            addAndMakeVisible (n);

            auto* b = drumSampleBtns.add (new juce::TextButton());
            b->setTooltip (U ("Klick: Sample laden · Shift+Klick: zurück zum Synth"));
            b->onClick = [this, slot]
            {
                if (juce::ModifierKeys::currentModifiers.isShiftDown()) proc.clearSample (slot);
                else if (loadDrumSample) loadDrumSample (slot);
            };
            addAndMakeVisible (b);

            for (int i = 0; i < numDrumParams; ++i)
                addAndMakeVisible (knobs.add (new ParamKnob (s, drumParamId (slot, i), drumParamDef (i).name)));
        }
    }
    else
    {
        oscBox = std::make_unique<juce::ComboBox>();
        for (int w = 0; w < 5; ++w) oscBox->addItem (oscName (w), w + 1);
        oscAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, synthParamId (t, spOsc), *oscBox);
        addAndMakeVisible (*oscBox);

        for (int i = 0; i < numSynthParams; ++i)
        {
            if (i == spOsc) { knobs.add (nullptr); continue; }
            addAndMakeVisible (knobs.add (new ParamKnob (s, synthParamId (t, i), synthParamDef (i).name)));
        }

        samplePanel = std::make_unique<SamplePanel> (proc, t, [this] { if (loadInstrumentSample) loadInstrumentSample (track); });
        addChildComponent (*samplePanel);
    }

    showPage (t > 0 && showSampleTab);
    timerCallback();
}

void SoundEditor::showPage (bool sample)
{
    samplePage = sample && track > 0;
    synthTab.setToggleState (! samplePage, juce::dontSendNotification);
    sampleTab.setToggleState (samplePage, juce::dontSendNotification);
    if (oscBox) oscBox->setVisible (! samplePage);
    for (auto* k : knobs) if (k) k->setVisible (! samplePage);
    if (samplePanel) samplePanel->setVisible (samplePage);
    resized();
    repaint();
}

void SoundEditor::timerCallback()
{
    for (int i = 0; i < drumSampleBtns.size(); ++i)
    {
        const auto n = proc.sampleName (i);
        const auto text = n.isEmpty() ? juce::String ("Synth") : n;
        if (drumSampleBtns[i]->getButtonText() != text)
        {
            drumSampleBtns[i]->setButtonText (text);
            drumSampleBtns[i]->setColour (juce::TextButton::buttonColourId, n.isEmpty() ? Colours::panelHi : Colours::gold.withAlpha (0.35f));
        }
    }
    if (track > 0)
        sampleTab.setButtonText (proc.instrumentSample (track) ? U ("Sample ●") : U ("Sample"));
}

void SoundEditor::showPresetMenu()
{
    juce::PopupMenu m;
    m.addItem (1, U ("Preset speichern…"));
    m.addItem (2, U ("Ordner öffnen"));
    m.addSeparator();

    auto files = HipHopBeatMakerProcessor::instrumentFolder (track).findChildFiles (juce::File::findFiles, false, "*.hhpreset");
    files.sort();
    if (files.isEmpty()) m.addItem (-1, U ("(noch keine Presets)"), false);
    for (int i = 0; i < files.size(); ++i)
        m.addItem (100 + i, files[i].getFileNameWithoutExtension());

    juce::Component::SafePointer<SoundEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe, files] (int r)
    {
        if (safe == nullptr || r == 0) return;
        const int t = safe->track;
        if (r == 1 && safe->askName)
            safe->askName (U ("Preset speichern"), juce::String (trackName (t)) + " " + styleName ((int) safe->proc.apvts.getRawParameterValue ("style")->load()),
                           [safe, t] (juce::String name) { if (safe) safe->proc.saveInstrumentPreset (t, name); });
        else if (r == 2)
            HipHopBeatMakerProcessor::instrumentFolder (t).startAsProcess();
        else if (r >= 100)
            safe->proc.loadInstrumentPreset (t, files[r - 100]);
    });
}

void SoundEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (Colours::track (juce::jmax (0, track)).withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.2f);

    if (! samplePage)
    {
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        for (auto& [name, r] : groups)
        {
            g.setColour (Colours::panelHi.withAlpha (0.6f));
            g.fillRoundedRectangle (r.toFloat(), 6.0f);
            g.setColour (Colours::gold.withAlpha (0.8f));
            g.drawText (name, r.withHeight (18).reduced (8, 0), juce::Justification::centredLeft);
        }
    }
}

void SoundEditor::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    auto header = r.removeFromTop (30);
    closeBtn.setBounds (header.removeFromRight (30));
    header.removeFromRight (8);
    defaultBtn.setBounds (header.removeFromRight (130));
    header.removeFromRight (6);
    presetBtn.setBounds (header.removeFromRight (110));
    header.removeFromRight (16);
    if (track > 0)
    {
        sampleTab.setBounds (header.removeFromRight (90));
        header.removeFromRight (4);
        synthTab.setBounds (header.removeFromRight (90));
    }
    for (auto* b : trackTabs)
    {
        b->setBounds (header.removeFromLeft (78));
        header.removeFromLeft (4);
    }
    r.removeFromTop (8);

    groups.clear();
    if (track == 0) layoutDrums (r);
    else if (samplePage && samplePanel) samplePanel->setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 560), juce::jmin (r.getHeight(), 320)));
    else layoutSynth (r);
}

void SoundEditor::layoutSynth (juce::Rectangle<int> area)
{
    if (knobs.isEmpty()) return;
    const int rowH = (area.getHeight() - 8) / 2;
    auto row1 = area.removeFromTop (rowH);
    area.removeFromTop (8);
    auto row2 = area.removeFromTop (rowH);

    auto placeGroup = [this] (juce::Rectangle<int>& row, const juce::String& name, std::initializer_list<int> params, int knobW)
    {
        auto g = row.removeFromLeft (knobW * (int) params.size() + 12);
        row.removeFromLeft (8);
        groups.push_back ({ name, g });
        auto inner = g.reduced (6, 0).withTrimmedTop (20).withTrimmedBottom (4);
        for (int p : params)
        {
            auto cell = inner.removeFromLeft (knobW);
            if (p == spOsc)
            {
                auto c = cell.reduced (4, 0);
                auto lblArea = c.removeFromTop (14);
                juce::ignoreUnused (lblArea);
                oscBox->setBounds (c.withSizeKeepingCentre (c.getWidth(), 26));
            }
            else if (auto* k = knobs[p])
                k->setBounds (cell);
        }
    };

    const int w = (row1.getWidth() - 12 * 2 - 8) / 12;
    placeGroup (row1, "OSZILLATOR", { spOsc, spUnison, spDetune, spSub, spFmRatio, spFmIndex, spFmDecay }, w);
    placeGroup (row1, "FILTER", { spCutoff, spRes, spFilterEnv, spKeyTrack, spDrive }, w);
    placeGroup (row2, U ("HÜLLKURVE"), { spAttack, spDecay, spSustain, spRelease, spLevel }, w);
    placeGroup (row2, "MODULATION", { spGlide, spVibDepth, spVibRate, spPitchEnv, spPitchTime, spTremolo }, w);
}

void SoundEditor::layoutDrums (juce::Rectangle<int> area)
{
    const int colW = area.getWidth() / numDrumSlots;
    for (int slot = 0; slot < numDrumSlots; ++slot)
    {
        auto col = area.removeFromLeft (colW).reduced (3, 0);
        groups.push_back ({ {}, col });
        auto inner = col.reduced (4, 6);
        drumNames[slot]->setBounds (inner.removeFromTop (20));
        drumSampleBtns[slot]->setBounds (inner.removeFromTop (24));
        inner.removeFromTop (6);
        const int kh = juce::jmin (140, inner.getHeight() / 2 - 4);
        for (int row = 0; row < 2; ++row)
        {
            auto line = inner.removeFromTop (kh);
            for (int c = 0; c < 2; ++c)
            {
                const int i = row * 2 + c;
                knobs[slot * numDrumParams + i]->setBounds (line.removeFromLeft (line.getWidth() / (2 - c)));
            }
        }
    }
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

        {
            auto& b = instSampleBtns[(size_t) t];
            b = std::make_unique<juce::TextButton> ("Sound");
            b->setTooltip (U ("Synth-Editor, eigenes Sample und Presets für ") + trackName (t));
            b->onClick = [this, t] { showSoundEditor (t); };
            addAndMakeVisible (*b);
        }
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

    profileBtn.setButtonText (U ("Sound-Profil ▾"));
    profileBtn.setTooltip (U ("Kompletten Sound (alle Instrumente + Drums) speichern oder laden"));
    profileBtn.onClick = [this] { showProfileMenu(); };
    addAndMakeVisible (profileBtn);

    soundEditor = std::make_unique<SoundEditor> (proc,
        [this] (int t) { loadInstrumentFor (t); },
        [this] (int slot) { loadSampleFor (slot); });
    soundEditor->onClose = [this] { soundEditor->setVisible (false); };
    soundEditor->askName = [this] (const juce::String& t, const juce::String& d, std::function<void (juce::String)> done) { askName (t, d, std::move (done)); };
    addChildComponent (*soundEditor);
    proc.editorOpen = true;

    statusLbl.setFont (juce::FontOptions (12.0f));
    statusLbl.setColour (juce::Label::textColourId, Colours::gold);
    addAndMakeVisible (statusLbl);

    setSize (1000, 656);
    startTimerHz (30);
    timerCallback();
}

HipHopBeatMakerEditor::~HipHopBeatMakerEditor()
{
    proc.editorOpen = false;
    alert.reset();
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

void HipHopBeatMakerEditor::showSoundEditor (int track)
{
    if (soundEditor->isVisible() && soundEditor->getTrack() == track)
    {
        soundEditor->setVisible (false);
        return;
    }
    soundEditor->setTrack (track, track > 0 && proc.instrumentSample (track) != nullptr);
    soundEditor->setVisible (true);
    soundEditor->toFront (false);
}

void HipHopBeatMakerEditor::askName (const juce::String& title, const juce::String& def, std::function<void (juce::String)> done)
{
    alert = std::make_unique<juce::AlertWindow> (title, U ("Name:"), juce::MessageBoxIconType::NoIcon, this);
    alert->addTextEditor ("name", def);
    alert->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<HipHopBeatMakerEditor> safe (this);
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([safe, done] (int r)
    {
        if (safe == nullptr || safe->alert == nullptr) return;
        const auto name = safe->alert->getTextEditorContents ("name");
        safe->alert.reset();
        if (r == 1 && name.trim().isNotEmpty()) done (name);
    }), false);
}

void HipHopBeatMakerEditor::showProfileMenu()
{
    juce::PopupMenu m;
    m.addItem (1, U ("Sound-Profil speichern…"));
    m.addItem (2, U ("Style-Standard laden (alle Instrumente)"));
    m.addItem (3, U ("Ordner öffnen"));
    m.addSeparator();

    auto files = HipHopBeatMakerProcessor::profileFolder().findChildFiles (juce::File::findFiles, false, "*.hhsound");
    files.sort();
    if (files.isEmpty()) m.addItem (-1, U ("(noch keine Profile)"), false);
    for (int i = 0; i < files.size(); ++i)
        m.addItem (100 + i, files[i].getFileNameWithoutExtension());

    juce::Component::SafePointer<HipHopBeatMakerEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&profileBtn), [safe, files] (int r)
    {
        if (safe == nullptr || r == 0) return;
        const int style = (int) safe->proc.apvts.getRawParameterValue ("style")->load();
        if (r == 1)
            safe->askName (U ("Sound-Profil speichern"), juce::String (styleName (style)) + " eigener Sound",
                           [safe] (juce::String name)
                           {
                               if (safe == nullptr) return;
                               const auto f = safe->proc.saveProfile (name);
                               safe->statusLbl.setText (U ("Profil gespeichert: ") + f.getFileName(), juce::dontSendNotification);
                           });
        else if (r == 2) safe->proc.loadStyleSound (style);
        else if (r == 3) HipHopBeatMakerProcessor::profileFolder().startAsProcess();
        else if (r >= 100 && safe->proc.loadProfile (files[r - 100]))
            safe->statusLbl.setText (U ("Profil geladen: ") + files[r - 100].getFileNameWithoutExtension(), juce::dontSendNotification);
    });
}

void HipHopBeatMakerEditor::showStyleDialog()
{
    const auto pending = proc.pendingStyleChange;
    if (pending.to < 0 || styleDialogOpen) return;
    styleDialogOpen = true;

    const juce::String from (styleName (pending.from)), to (styleName (pending.to));
    alert = std::make_unique<juce::AlertWindow> (U ("Sound vor dem Style-Wechsel speichern?"),
        U ("Du hast den ") + from + U ("-Sound verändert.\nSoll er als eigenes Sound-Profil gespeichert werden, bevor der ")
            + to + U ("-Standard geladen wird?\n(Die Style-Standards bleiben immer erhalten.)"),
        juce::MessageBoxIconType::NoIcon, this);
    alert->addTextEditor ("name", from + " eigener Sound " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M"), U ("Profil-Name:"));
    alert->addButton (U ("Speichern & laden"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton (U ("Nicht speichern"), 2);
    alert->addButton (U ("Sound behalten"), 3, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<HipHopBeatMakerEditor> safe (this);
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int r)
    {
        if (safe == nullptr) return;
        const auto name = safe->alert ? safe->alert->getTextEditorContents ("name") : juce::String();
        safe->alert.reset();
        safe->styleDialogOpen = false;
        using C = HipHopBeatMakerProcessor::StyleChoice;
        safe->proc.resolveStyleChange (r == 1 ? C::SaveAndLoad : (r == 2 ? C::DiscardAndLoad : C::KeepSound), name);
        if (r == 1) safe->statusLbl.setText (U ("Profil gespeichert: ") + name, juce::dontSendNotification);
    }), false);
}

void HipHopBeatMakerEditor::loadInstrumentFor (int track)
{
    chooser = std::make_unique<juce::FileChooser> (U ("Sample für ") + trackName (track),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<HipHopBeatMakerEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, track] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              const auto f = fc.getResult();
                              if (f.existsAsFile() && ! safe->proc.loadInstrumentSample (track, f, true))
                                  safe->statusLbl.setText (U ("Sample konnte nicht geladen werden: ") + f.getFileName(), juce::dontSendNotification);
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
    if (proc.pendingStyleChange.to >= 0) showStyleDialog();

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

    for (int t = 0; t < numTracks; ++t)
    {
        if (auto* b = instSampleBtns[(size_t) t].get())
        {
            const bool has = t > 0 && proc.instrumentSample (t) != nullptr;
            const juce::String text = has ? "Sample" : "Sound";
            if (b->getButtonText() != text)
            {
                b->setButtonText (text);
                b->setColour (juce::TextButton::buttonColourId, has ? Colours::track (t).withAlpha (0.45f) : Colours::panelHi);
            }
        }
    }

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
    bpmInfo.setBounds (header.removeFromRight (180));
    profileBtn.setBounds (header.removeFromRight (150).withSizeKeepingCentre (150, 30));

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
    if (soundEditor) soundEditor->setBounds (area.getX(), area.getY(), area.getWidth(), getHeight() - area.getY() - 10);
    for (int t = 0; t < numTracks; ++t)
    {
        auto r = area.removeFromTop (54);
        auto left = r.removeFromLeft (90);
        muteBtns[t]->setBounds (left.removeFromBottom (22));
        volSliders[t]->setBounds (left.removeFromBottom (22));
        r.removeFromLeft (6);
        auto sampleCol = r.removeFromLeft (64);
        if (auto* b = instSampleBtns[(size_t) t].get())
            b->setBounds (sampleCol.reduced (0, 9));
        r.removeFromLeft (6);
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
