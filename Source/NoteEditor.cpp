#include "NoteEditor.h"
#include "PluginEditor.h"

using namespace hhbm;
using namespace hhbm::ui;

static juce::String U8 (const char* s) { return juce::String::fromUTF8 (s); }

std::vector<Note> NoteEditor::clipboard;

namespace
{
// drum rows, top to bottom
const int drumRowSlots[numDrumSlots] = { 7, 4, 3, 6, 5, 2, 1, 0 };
constexpr int keysW = 64, velH = 64, header1 = 32, header2 = 30;
constexpr int lowestPitch = 21, highestPitch = 108;

const double gridValues[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
const char* gridNames[] = { "1/4", "1/8", "1/16", "1/32", "1/8 Triole", "1/16 Triole" };
}

NoteEditor::NoteEditor (HipHopBeatMakerProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);

    for (int t = 0; t < numTracks; ++t)
    {
        auto* b = trackTabs.add (new juce::TextButton (trackName (t)));
        b->setColour (juce::TextButton::buttonOnColourId, Colours::track (t));
        b->onClick = [this, t] { open (t, section); };
        addAndMakeVisible (b);
    }
    for (int s = 0; s < 5; ++s)
    {
        auto* b = sectionTabs.add (new juce::TextButton (sectionName (s)));
        b->onClick = [this, s] { open (track, s); };
        addAndMakeVisible (b);
    }

    closeBtn.onClick = [this] { if (onClose) onClose(); };
    unlockBtn.setButtonText (U8 ("Neu generieren"));
    unlockBtn.setTooltip (U8 ("Deine Änderungen an dieser Spur verwerfen – der Generator übernimmt wieder (Undo möglich)"));
    unlockBtn.onClick = [this] { proc.unlockTrack (section, track); reload (false); };
    undoBtn.setButtonText ("Undo");
    redoBtn.setButtonText ("Redo");
    undoBtn.onClick = [this]
    {
        if (proc.undoEdit())
        {
            const auto [s, t] = proc.lastUndoTarget;
            if (s != section || t != track) open (t, s); else reload (false);
        }
    };
    redoBtn.onClick = [this]
    {
        if (proc.redoEdit())
        {
            const auto [s, t] = proc.lastUndoTarget;
            if (s != section || t != track) open (t, s); else reload (false);
        }
    };
    quantBtn.setButtonText ("Quantize");
    quantBtn.setTooltip ("Auswahl (oder alles) aufs Raster setzen  [Q]");
    quantBtn.onClick = [this] { quantize(); };
    copyBtn.setButtonText ("Kopieren");
    copyBtn.onClick = [this] { copySelected(); };
    pasteBtn.setButtonText (U8 ("Einfügen"));
    pasteBtn.setTooltip (U8 ("An der zuletzt angeklickten Stelle einfügen  [Strg/Cmd+V]"));
    pasteBtn.onClick = [this] { paste (pasteBeat); };
    dupBtn.setButtonText ("Duplizieren");
    dupBtn.setTooltip (U8 ("Auswahl dahinter wiederholen  [Strg/Cmd+D]"));
    dupBtn.onClick = [this] { duplicateSelected(); };
    delBtn.setButtonText (U8 ("Löschen"));
    delBtn.onClick = [this] { deleteSelected(); };

    for (auto* b : { &closeBtn, &unlockBtn, &undoBtn, &redoBtn, &quantBtn, &copyBtn, &pasteBtn, &dupBtn, &delBtn })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }

    for (int i = 0; i < 6; ++i) gridBox.addItem (gridNames[i], i + 1);
    gridBox.setSelectedId (3, juce::dontSendNotification);
    gridBox.onChange = [this] { repaint(); };
    gridBox.setWantsKeyboardFocus (false);
    addAndMakeVisible (gridBox);

    lockLbl.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (lockLbl);
    hintLbl.setFont (juce::FontOptions (11.0f));
    hintLbl.setColour (juce::Label::textColourId, Colours::dim);
    hintLbl.setText (U8 ("Tipps: Maus hier parken"), juce::dontSendNotification);
    const auto tip = U8 ("Doppelklick: neue Note · Note ziehen: verschieben · rechter Rand: Länge · Rechtsklick: löschen\n"
                         "Rahmen ziehen: Auswahl (Shift = hinzufügen) · ↑/↓: transponieren (Shift = Oktave) · ←/→: verschieben\n"
                         "Strg/Cmd: C kopieren · V einfügen · D duplizieren · A alles · Z Undo · Shift+Z Redo · Q quantisieren · Entf löschen\n"
                         "Velocity: unten in der Leiste ziehen (wirkt auf Auswahl, sonst auf Noten unter der Maus)");
    hintLbl.setTooltip (tip);
    setTooltip (tip);
    addAndMakeVisible (hintLbl);

    startTimerHz (30);
}

NoteEditor::~NoteEditor() { stopTimer(); }

void NoteEditor::open (int t, int s)
{
    track = juce::jlimit (0, numTracks - 1, t);
    section = juce::jlimit (0, 4, s);
    for (int i = 0; i < trackTabs.size(); ++i) trackTabs[i]->setToggleState (i == track, juce::dontSendNotification);
    for (int i = 0; i < sectionTabs.size(); ++i) sectionTabs[i]->setToggleState (i == section, juce::dontSendNotification);
    pasteBeat = -1;
    reload (false);

    // vertical position: centre on the notes
    if (! drumMode() && ! notes.empty())
    {
        int hi = 0, lo = 127;
        for (auto& n : notes) { hi = std::max (hi, n.pitch); lo = std::min (lo, n.pitch); }
        const int visible = (int) (rollArea().getHeight() / rowHeight());
        topPitch = juce::jlimit (lowestPitch + visible - 1, highestPitch, (hi + lo) / 2 + visible / 2);
    }
    grabKeyboardFocus();
    repaint();
}

void NoteEditor::reload (bool keepSelection)
{
    auto fresh = proc.sectionNotes (section, track);
    const bool same = keepSelection && fresh.size() == notes.size();
    notes = std::move (fresh);
    if (! same) sel.assign (notes.size(), false);
    lastVersion = proc.patternVersion.load();
    updateButtons();
    repaint();
}

void NoteEditor::commit()
{
    // keep selection across the round trip by sorting notes + flags together
    std::vector<std::pair<Note, bool>> tmp;
    for (size_t i = 0; i < notes.size(); ++i) tmp.push_back ({ notes[i], sel[i] });
    std::stable_sort (tmp.begin(), tmp.end(), [] (auto& a, auto& b) { return a.first.start < b.first.start; });
    const double len = lengthBeats();
    notes.clear(); sel.clear();
    for (auto& [n, s] : tmp)
        if (n.start >= 0 && n.start < len) { notes.push_back (n); sel.push_back (s); }

    proc.setEditedNotes (section, track, notes);
    lastVersion = proc.patternVersion.load();
    updateButtons();
    repaint();
}

void NoteEditor::updateButtons()
{
    const bool locked = proc.isLocked (section, track);
    lockLbl.setText (locked ? U8 ("● Bearbeitet – Generator gesperrt") : U8 ("generiert (Bearbeiten sperrt die Spur)"), juce::dontSendNotification);
    lockLbl.setColour (juce::Label::textColourId, locked ? Colours::gold : Colours::dim);
    unlockBtn.setEnabled (locked);
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
    for (int s = 0; s < sectionTabs.size(); ++s)
        sectionTabs[s]->setButtonText (juce::String (sectionName (s)) + (proc.isLocked (s, track) ? U8 (" •") : juce::String()));
}

void NoteEditor::timerCallback()
{
    if (drag == Drag::None && proc.patternVersion.load() != lastVersion)
        reload (true); // e.g. style/seed change of an unlocked track, or undo from elsewhere
    else
        repaint (rollArea().withTrimmedLeft (-2));
}

// ============================================================ geometry
juce::Rectangle<int> NoteEditor::keysArea() const
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (header1 + header2 + 8);
    r.removeFromBottom (velH + 4);
    return r.removeFromLeft (keysW);
}

juce::Rectangle<int> NoteEditor::rollArea() const
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (header1 + header2 + 8);
    r.removeFromBottom (velH + 4);
    r.removeFromLeft (keysW);
    return r;
}

juce::Rectangle<int> NoteEditor::velArea() const
{
    auto r = getLocalBounds().reduced (10, 8);
    auto v = r.removeFromBottom (velH);
    v.removeFromLeft (keysW);
    return v;
}

double NoteEditor::lengthBeats() const { return Generator::sectionBars (section) * 4.0; }
int NoteEditor::numRows() const { return drumMode() ? numDrumSlots : (int) (rollArea().getHeight() / rowHeight()) + 1; }
float NoteEditor::rowHeight() const { return drumMode() ? (float) rollArea().getHeight() / numDrumSlots : 12.0f; }

int NoteEditor::rowToPitch (int row) const
{
    if (drumMode()) return (row >= 0 && row < numDrumSlots) ? drumSlotNote (drumRowSlots[row]) : -1;
    return topPitch - row;
}

int NoteEditor::pitchToRow (int pitch) const
{
    if (drumMode())
    {
        const int slot = drumNoteToSlot (pitch);
        for (int r = 0; r < numDrumSlots; ++r) if (drumRowSlots[r] == slot) return r;
        return -1;
    }
    return topPitch - pitch;
}

float NoteEditor::beatToX (double beat) const
{
    auto r = rollArea();
    return (float) r.getX() + (float) (beat / lengthBeats()) * (float) r.getWidth();
}

double NoteEditor::xToBeat (float x) const
{
    auto r = rollArea();
    return juce::jlimit (0.0, lengthBeats(), (double) (x - (float) r.getX()) / r.getWidth() * lengthBeats());
}

int NoteEditor::yToRow (float y) const { return (int) std::floor ((y - (float) rollArea().getY()) / rowHeight()); }

juce::Rectangle<float> NoteEditor::noteRect (const Note& n) const
{
    const int row = pitchToRow (n.pitch);
    const float x1 = beatToX (n.start), x2 = beatToX (n.start + n.length);
    const float w = drumMode() ? std::max (6.0f, x2 - x1) : std::max (3.0f, x2 - x1);
    return { x1, (float) rollArea().getY() + row * rowHeight() + 1.0f, w - 1.0f, rowHeight() - 2.0f };
}

int NoteEditor::hitNote (juce::Point<float> p, bool& onEdge) const
{
    for (int i = (int) notes.size() - 1; i >= 0; --i)
    {
        auto r = noteRect (notes[(size_t) i]);
        if (r.contains (p))
        {
            onEdge = ! drumMode() && r.getWidth() > 10 && p.x > r.getRight() - 6;
            return i;
        }
    }
    onEdge = false;
    return -1;
}

double NoteEditor::grid() const { return gridValues[juce::jlimit (0, 5, gridBox.getSelectedId() - 1)]; }

// ============================================================ painting
void NoteEditor::paint (juce::Graphics& g)
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (Colours::track (track).withAlpha (0.6f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.2f);

    const auto roll = rollArea(), keys = keysArea(), vel = velArea();
    const float rh = rowHeight();
    const int rows = numRows();
    const double len = lengthBeats();

    g.saveState();
    g.reduceClipRegion (roll.getUnion (keys));

    // rows
    for (int row = 0; row < rows; ++row)
    {
        const int pitch = rowToPitch (row);
        if (pitch < 0) continue;
        const float y = (float) roll.getY() + row * rh;
        const bool black = ! drumMode() && juce::MidiMessage::isMidiNoteBlack (pitch);
        g.setColour (black ? Colours::bg : Colours::bg.brighter (0.06f));
        g.fillRect ((float) roll.getX(), y, (float) roll.getWidth(), rh);
        g.setColour (Colours::panelHi.withAlpha (0.5f));
        g.drawHorizontalLine ((int) y, (float) roll.getX(), (float) roll.getRight());

        // keyboard / drum names
        g.setColour (black ? juce::Colour (0xff2a2a30) : juce::Colour (0xffd8d6d0));
        if (drumMode()) g.setColour (Colours::panelHi);
        g.fillRect ((float) keys.getX(), y, (float) keys.getWidth() - 2.0f, rh - 1.0f);
        g.setFont (juce::FontOptions (drumMode() ? 12.0f : 10.0f, drumMode() ? juce::Font::bold : juce::Font::plain));
        if (drumMode())
        {
            g.setColour (Colours::text);
            g.drawText (drumSlotName (drumRowSlots[row]), juce::Rectangle<float> ((float) keys.getX() + 4, y, (float) keys.getWidth() - 6, rh), juce::Justification::centredLeft);
        }
        else if (pitch % 12 == 0)
        {
            g.setColour (juce::Colours::black);
            g.drawText (SampleTools::noteName (pitch), juce::Rectangle<float> ((float) keys.getX() + 2, y, (float) keys.getWidth() - 6, rh), juce::Justification::centredRight);
        }
    }

    // vertical grid
    const double gstep = grid();
    for (double b = 0; b <= len + 1e-6; b += gstep)
    {
        const float x = beatToX (b);
        const bool bar = std::fmod (b + 1e-6, 4.0) < 2e-6;
        const bool beat = std::fmod (b + 1e-6, 1.0) < 2e-6;
        g.setColour (bar ? Colours::dim.withAlpha (0.7f) : (beat ? Colours::panelHi.brighter (0.25f) : Colours::panelHi.withAlpha (0.6f)));
        g.drawVerticalLine ((int) x, (float) roll.getY(), (float) roll.getBottom());
    }
    // bar numbers
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (Colours::dim);
    for (int bar = 0; bar * 4.0 < len; ++bar)
        g.drawText (juce::String (bar + 1), juce::Rectangle<float> (beatToX (bar * 4.0) + 3, (float) roll.getY(), 30, 12), juce::Justification::left);

    // notes
    const auto col = Colours::track (track);
    for (size_t i = 0; i < notes.size(); ++i)
    {
        if (pitchToRow (notes[i].pitch) < 0 || pitchToRow (notes[i].pitch) >= rows) continue;
        auto r = noteRect (notes[i]);
        g.setColour (col.withAlpha (0.35f + 0.65f * notes[i].velocity));
        g.fillRoundedRectangle (r, 2.0f);
        if (sel[i])
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 2.0f, 1.5f);
        }
    }

    // paste cursor
    if (pasteBeat >= 0)
    {
        g.setColour (Colours::gold.withAlpha (0.5f));
        const float x = beatToX (pasteBeat);
        g.drawLine (x, (float) roll.getY(), x, (float) roll.getBottom(), 1.0f);
    }

    // playhead (only if this section is playing)
    const double ph = proc.uiBeat.load();
    if (ph >= 0)
    {
        auto pat = proc.getPattern();
        double local = -1;
        const int playing = (int) proc.apvts.getRawParameterValue ("section")->load();
        if (playing == section) local = ph;
        else if (pat && playing == (int) Section::FullSong)
            for (auto& m : pat->markers)
                if (m.section == section && ph >= m.start && ph < m.start + m.length) local = ph - m.start;
        if (local >= 0)
        {
            g.setColour (Colours::text.withAlpha (0.8f));
            g.drawVerticalLine ((int) beatToX (local), (float) roll.getY(), (float) roll.getBottom());
        }
    }

    if (drag == Drag::Rubber)
    {
        g.setColour (Colours::gold.withAlpha (0.15f));
        g.fillRect (rubber);
        g.setColour (Colours::gold);
        g.drawRect (rubber, 1.0f);
    }
    g.restoreState();

    // velocity lane
    g.setColour (Colours::bg);
    g.fillRect (vel);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("VELOCITY", juce::Rectangle<int> (vel.getX() - keysW, vel.getY(), keysW - 4, vel.getHeight()), juce::Justification::centredRight);
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const float x = beatToX (notes[i].start);
        const float h = notes[i].velocity * (vel.getHeight() - 4);
        g.setColour (sel[i] ? juce::Colours::white : col);
        g.fillRect (x, (float) vel.getBottom() - h, 3.0f, h);
    }
}

void NoteEditor::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto h1 = r.removeFromTop (header1);
    closeBtn.setBounds (h1.removeFromRight (30));
    h1.removeFromRight (8);
    for (auto* b : trackTabs) { b->setBounds (h1.removeFromLeft (72)); h1.removeFromLeft (3); }
    h1.removeFromLeft (14);
    for (auto* b : sectionTabs) { b->setBounds (h1.removeFromLeft (72)); h1.removeFromLeft (3); }
    h1.removeFromLeft (10);
    lockLbl.setBounds (h1);

    r.removeFromTop (6);
    auto h2 = r.removeFromTop (header2 - 4);
    gridBox.setBounds (h2.removeFromLeft (110));
    h2.removeFromLeft (8);
    for (auto* b : { &quantBtn, &undoBtn, &redoBtn, &copyBtn, &pasteBtn, &dupBtn, &delBtn })
    {
        b->setBounds (h2.removeFromLeft (b == &undoBtn || b == &redoBtn ? 58 : 84));
        h2.removeFromLeft (4);
    }
    h2.removeFromLeft (6);
    unlockBtn.setBounds (h2.removeFromLeft (120));
    h2.removeFromLeft (10);
    hintLbl.setBounds (h2);
}

// ============================================================ mouse
void NoteEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.position;
    changed = false;
    dragStart = p;

    if (velArea().contains (p.toInt()))
    {
        drag = Drag::Velocity;
        dragOrig = notes;
        mouseDrag (e);
        return;
    }
    if (! rollArea().contains (p.toInt())) return;

    bool edge = false;
    const int hit = hitNote (p, edge);

    if (hit >= 0)
    {
        if (e.mods.isPopupMenu())
        {
            notes.erase (notes.begin() + hit);
            sel.erase (sel.begin() + hit);
            commit();
            return;
        }
        if (! sel[(size_t) hit])
        {
            if (! e.mods.isShiftDown()) selectAll (false);
            sel[(size_t) hit] = true;
        }
        else if (e.mods.isShiftDown())
        {
            sel[(size_t) hit] = false;
            repaint();
            return;
        }
        drag = edge ? Drag::Resize : Drag::Move;
        dragOrig = notes;
    }
    else
    {
        if (! e.mods.isShiftDown()) selectAll (false);
        pasteBeat = snapDown (xToBeat (p.x));
        drag = Drag::Rubber;
        rubber = { p, p };
    }
    repaint();
}

void NoteEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    switch (drag)
    {
        case Drag::Move:
        {
            const double db = snap (xToBeat (p.x) - xToBeat (dragStart.x));
            const int dr = (int) std::round ((p.y - dragStart.y) / rowHeight());
            for (size_t i = 0; i < notes.size(); ++i)
            {
                if (! sel[i]) continue;
                auto n = dragOrig[i];
                n.start = juce::jlimit (0.0, lengthBeats() - grid(), n.start + db);
                const int row = pitchToRow (n.pitch) + dr;
                const int np = drumMode() ? rowToPitch (juce::jlimit (0, numDrumSlots - 1, row)) : juce::jlimit (0, 127, n.pitch - dr);
                n.pitch = np;
                if (! (notes[i].start == n.start && notes[i].pitch == n.pitch)) changed = true;
                notes[i] = n;
            }
            break;
        }
        case Drag::Resize:
        {
            const double db = xToBeat (p.x) - xToBeat (dragStart.x);
            for (size_t i = 0; i < notes.size(); ++i)
            {
                if (! sel[i]) continue;
                auto n = dragOrig[i];
                const double end = std::max (n.start + grid(), snap (n.start + n.length + db));
                n.length = std::min (end, lengthBeats()) - n.start;
                if (notes[i].length != n.length) changed = true;
                notes[i] = n;
            }
            break;
        }
        case Drag::Rubber:
        {
            rubber = juce::Rectangle<float> (dragStart, p);
            for (size_t i = 0; i < notes.size(); ++i)
                if (noteRect (notes[i]).intersects (rubber)) sel[i] = true;
                else if (! e.mods.isShiftDown()) sel[i] = false;
            break;
        }
        case Drag::Velocity:
        {
            const auto v = velArea();
            const float vel = juce::jlimit (0.02f, 1.0f, 1.0f - (p.y - (float) v.getY()) / (float) v.getHeight());
            const bool onlySelected = anySelected();
            for (size_t i = 0; i < notes.size(); ++i)
            {
                if (onlySelected && ! sel[i]) continue;
                if (std::abs (beatToX (notes[i].start) - p.x) <= 4.0f)
                {
                    notes[i].velocity = vel;
                    changed = true;
                }
            }
            break;
        }
        case Drag::None: default: break;
    }
    repaint();
}

void NoteEditor::mouseUp (const juce::MouseEvent&)
{
    const bool commitNow = changed && (drag == Drag::Move || drag == Drag::Resize || drag == Drag::Velocity);
    drag = Drag::None;
    if (commitNow) commit();
    repaint();
}

void NoteEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (! rollArea().contains (p.toInt())) return;
    bool edge;
    if (hitNote (p, edge) >= 0) return;
    const int pitch = rowToPitch (yToRow (p.y));
    if (pitch < 0) return;
    const double start = snapDown (xToBeat (p.x));
    if (start >= lengthBeats()) return;
    selectAll (false);
    notes.push_back ({ pitch, start, drumMode() ? 0.2 : grid(), 0.8f });
    sel.push_back (true);
    commit();
}

void NoteEditor::mouseMove (const juce::MouseEvent& e)
{
    bool edge = false;
    const int hit = rollArea().contains (e.position.toInt()) ? hitNote (e.position, edge) : -1;
    setMouseCursor (hit >= 0 ? (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor)
                             : juce::MouseCursor::NormalCursor);
}

void NoteEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (drumMode()) return;
    const int visible = (int) (rollArea().getHeight() / rowHeight());
    topPitch = juce::jlimit (lowestPitch + visible - 1, highestPitch, topPitch + (w.deltaY > 0 ? 2 : -2));
    repaint();
}

// ============================================================ editing
void NoteEditor::selectAll (bool on) { std::fill (sel.begin(), sel.end(), on); repaint(); }
bool NoteEditor::anySelected() const { return std::find (sel.begin(), sel.end(), true) != sel.end(); }

void NoteEditor::deleteSelected()
{
    if (! anySelected()) return;
    std::vector<Note> keep;
    for (size_t i = 0; i < notes.size(); ++i) if (! sel[i]) keep.push_back (notes[i]);
    notes = keep;
    sel.assign (notes.size(), false);
    commit();
}

void NoteEditor::copySelected()
{
    clipboard.clear();
    double first = 1e9;
    for (size_t i = 0; i < notes.size(); ++i) if (sel[i]) first = std::min (first, notes[i].start);
    for (size_t i = 0; i < notes.size(); ++i)
        if (sel[i]) { auto n = notes[i]; n.start -= first; clipboard.push_back (n); }
}

void NoteEditor::paste (double at)
{
    if (clipboard.empty()) return;
    if (at < 0) at = 0;
    selectAll (false);
    double span = 0;
    for (auto n : clipboard)
    {
        n.start += at;
        span = std::max (span, n.start + n.length - at);
        if (n.start >= lengthBeats()) continue;
        if (drumMode() && drumNoteToSlot (n.pitch) < 0) continue; // melodic notes don't fit the drum grid
        notes.push_back (n);
        sel.push_back (true);
    }
    pasteBeat = std::min (lengthBeats(), at + std::ceil (span / grid() - 1e-6) * grid());
    commit();
}

void NoteEditor::duplicateSelected()
{
    if (! anySelected()) return;
    double first = 1e9, last = 0;
    for (size_t i = 0; i < notes.size(); ++i)
        if (sel[i]) { first = std::min (first, notes[i].start); last = std::max (last, notes[i].start + notes[i].length); }
    // repeat after the selection, rounded up to whole beats (whole bars if > 2 beats)
    const double unit = (last - first) > 2.0 ? 4.0 : 1.0;
    const double span = std::max (unit, std::ceil ((last - first) / unit - 1e-6) * unit);
    copySelected();
    paste (first + span);
}

void NoteEditor::quantize()
{
    const bool all = ! anySelected();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        if (! all && ! sel[i]) continue;
        notes[i].start = std::min (snap (notes[i].start), lengthBeats() - grid());
        if (notes[i].length < grid() * 0.5 && ! drumMode()) notes[i].length = grid();
    }
    commit();
}

void NoteEditor::transpose (int semis)
{
    if (! anySelected()) return;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        if (! sel[i]) continue;
        if (drumMode())
        {
            const int row = juce::jlimit (0, numDrumSlots - 1, pitchToRow (notes[i].pitch) - juce::jlimit (-1, 1, semis));
            notes[i].pitch = rowToPitch (row);
        }
        else notes[i].pitch = juce::jlimit (0, 127, notes[i].pitch + semis);
    }
    commit();
}

void NoteEditor::nudge (double beats)
{
    if (! anySelected()) return;
    for (size_t i = 0; i < notes.size(); ++i)
        if (sel[i]) notes[i].start = juce::jlimit (0.0, lengthBeats() - grid(), notes[i].start + beats);
    commit();
}

bool NoteEditor::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const bool cmd = m.isCommandDown();
    const int code = k.getKeyCode();

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (cmd && (code == 'Z' || code == 'z'))
    {
        if (m.isShiftDown() ? proc.redoEdit() : proc.undoEdit())
        {
            const auto [s, t] = proc.lastUndoTarget;
            if (s != section || t != track) open (t, s); else reload (false);
        }
        return true;
    }
    if (cmd && (code == 'Y' || code == 'y')) { if (proc.redoEdit()) reload (false); return true; }
    if (cmd && (code == 'C' || code == 'c')) { copySelected(); return true; }
    if (cmd && (code == 'V' || code == 'v')) { paste (pasteBeat); return true; }
    if (cmd && (code == 'D' || code == 'd')) { duplicateSelected(); return true; }
    if (cmd && (code == 'A' || code == 'a')) { selectAll (true); return true; }
    if (! cmd && (code == 'Q' || code == 'q')) { quantize(); return true; }
    if (code == juce::KeyPress::upKey)    { transpose (m.isShiftDown() ? 12 : 1); return true; }
    if (code == juce::KeyPress::downKey)  { transpose (m.isShiftDown() ? -12 : -1); return true; }
    if (code == juce::KeyPress::leftKey)  { nudge (-grid()); return true; }
    if (code == juce::KeyPress::rightKey) { nudge (grid()); return true; }
    if (code == juce::KeyPress::escapeKey) { if (anySelected()) selectAll (false); else if (onClose) onClose(); return true; }
    return false;
}
