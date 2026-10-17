# Hip-Hop BeatMaker

📖 **Bedienungsanleitung:** [Docs/Bedienungsanleitung.pdf](Docs/Bedienungsanleitung.pdf) (liegt auch in jedem Download-ZIP)

VST3 / AU / Standalone-Instrument (Windows & macOS) von **JustCreateit.ch**.
Erzeugt komplette Hip-Hop-Beats – Drums, Bass/808, Keys, Pad und Lead – zum Vorhören und als **MIDI-Export pro Instrument**.

## Styles
| Style | Tempo | Charakter |
|---|---|---|
| Boom Bap | 90 | swingende 90s-Drums, Piano-Stabs, Flöten-/Horn-Lead |
| Trap | 140 | Half-Time, Hi-Hat-Rolls (32tel/Triolen), 808 mit Glides, Bell-Melodie |
| Drill | 142 | Triolen-Hats, gleitende 808-Slides, dunkle Melodie |
| Lo-Fi | 80 | starker Swing, Rhodes-Septakkorde/None, weiche Drums |
| G-Funk | 94 | funky Synth-Bass, Rhodes-Offbeats, Portamento-Whine-Lead |
| Drum & Bass | 174 | Two-Step, rollender Sub-Bass, Akkord-Stabs, Pluck-Arps, Snare-Rolls |
| Neurofunk | 174 | harte Two-Step-Drums, Reese-/Growl-Bass in 16teln, dunkle Stabs |
| Liquid DnB | 174 | weiche Drums, seidiger Sub, Rhodes-Akkorde, warme Pads |
| Jungle | 165 | zerhackte Amen-Breaks, Dub-Sub, Offbeat-Skank, Horn-Stabs |

## Bedienung
- **Style, Key, Scale, Chords** (Akkordfolge – bei „Auto (Style)" wählt der Seed eine style-typische Folge), **Tuning 440/432 Hz**
- **Sections:** Intro · Verse · Hook · Bridge · Outro · Full Song (Intro–Verse–Hook–Verse–Hook–Bridge–Hook–Outro)
- **Swing, Variation, Humanize, Seed** – „Neu würfeln" erzeugt eine neue Variante
- **Play** = Vorhören ohne Host. Läuft Cubase, folgt das Plugin dem Host-Transport und Tempo.
- **Sounds** aus = nur MIDI · **MIDI Out** = Noten zusätzlich an den Host senden
- **Drum-Samples:** Klick auf einen Slot lädt ein eigenes WAV/AIFF/FLAC, Shift+Klick = zurück zum Synth-Sound
- **Instrument-Samples (Bass, Keys, Pad, Lead):** Button „Synth/Sample" neben der Spur öffnet das Sample-Panel
  - Ein Sample wird über die Tastatur transponiert
  - **Grundton** wird automatisch erkannt: Notenname im Dateinamen (z. B. `808 Boom F1.wav`, C3 = 60) hat Vorrang, die Oktave und Feinstimmung kommen aus der Tonhöhen-Analyse
  - **Fine** (±100 Cent), **Attack/Release**, **Loop** mit Start/Ende und Crossfade (Pad standardmässig mit Loop)
  - 808-Glides funktionieren auch mit eigenem Sample (Trap/Drill)
  - Tuning 432 Hz wirkt auch auf Samples

## Standalone-App
`Hip-Hop BeatMaker.exe` (Windows) bzw. `Hip-Hop BeatMaker.app` (macOS) liegt im selben ZIP wie das Plugin – einfach starten.
- **Options** (oben links): Audio-Ausgang und **MIDI-Eingang** (Keyboard) aktivieren
- **Transport:** ▶ Play startet am Anfang, ■ Stop · **Loop** an = Schleife, aus = einmal abspielen · Positionsanzeige (Takt, Section)
- **Keyboard:** wählt, welches Instrument dein MIDI-Keyboard spielt (Auto: Kanal 10 = Drums, 1 = Bass, 2 = Keys, 3 = Pad, 4 = Lead)
- **WAV-Export ▾:** Section oder Full Song als Mix und/oder Stems pro Spur (48 kHz / 24 Bit) nach `Dokumente/Hip-Hop BeatMaker/Exports`
- **Projekt ▾:** Öffnen, Speichern, Speichern unter, Zuletzt – Dateien `.hhbeat` in `Dokumente/Hip-Hop BeatMaker/Projekte`
  (enthält alle Einstellungen, Sounds und Sample-Pfade; die Samples selbst bleiben an ihrem Ort)

Alle diese Funktionen gibt es auch im Plugin.

## Noten-Editor
**Klick auf eine Spur** (Mini-Notenansicht) öffnet den Editor – im Full Song wird die angeklickte Section geöffnet.
- Tabs oben: Instrument (Drums, Bass, Keys, Pad, Lead) und Section (Intro … Outro; • = bearbeitet)
- **Drums** als Raster mit Instrument-Namen, alle anderen als Piano-Roll (C3 = 60)
- Doppelklick = neue Note · ziehen = verschieben · rechter Rand = Länge · Rechtsklick = löschen · Rahmen = Auswahl
- Raster 1/4 … 1/32 inkl. Triolen · **Quantize** · **Velocity**-Leiste unten
- **Kopieren / Einfügen / Duplizieren / Löschen** (auch Strg/Cmd+C/V/D, Entf) · **Undo/Redo** (Strg/Cmd+Z, Shift+Z)
- Pfeile ↑/↓ transponieren (Shift = Oktave), ←/→ verschieben
- Bearbeitete Spuren sind **gesperrt** (Schloss in der Spur): Seed, Style, Key usw. ändern sie nicht mehr.
  **„Neu generieren"** gibt die Spur wieder an den Generator zurück.
- Änderungen gelten pro Section – eine bearbeitete Hook erscheint in jeder Hook des Full Songs
- Drag & Drop, MIDI- und WAV-Export sowie Projekte enthalten die bearbeiteten Noten

## Sound-Editor
Button **„Sound"** neben jeder Spur öffnet den Editor (Tabs oben wechseln das Instrument):
- **Bass, Keys, Pad, Lead – Synth:**
  - Oszillator: Wellenform (Sine/Saw/Square/Triangle/FM), Unison, Detune, Sub, FM Ratio/Index/Decay
  - Filter: Cutoff, Resonanz, Filter-Env, Keytrack, Drive
  - Hüllkurve: Attack, Decay, Sustain, Release, Level
  - Modulation: Glide, Vibrato, Vib-Rate, Pitch-Env, Pitch-Zeit, Tremolo
  - Doppelklick auf einen Regler = Standardwert
- **Sample-Tab:** eigenes Sample statt Synth (siehe oben)
- **Drums:** pro Drum Tune, Decay, Tone, Level + Sample-Slot
- **Presets ▾:** Instrument-Preset speichern/laden (`Dokumente/Hip-Hop BeatMaker/Presets/<Instrument>`)
- **Style-Standard:** Instrument auf den Sound des aktuellen Styles zurücksetzen
- **Sound-Profil ▾** (oben): kompletten Sound aller Instrumente speichern/laden (`Presets/Sound-Profile`)

**Style-Wechsel:** Jeder Style lädt seinen eigenen Sound. Wurde der Sound verändert, fragt das Plugin vorher:
*Speichern & laden* (eigenes Profil-File), *Nicht speichern* oder *Sound behalten*. Die Style-Standards selbst gehen nie verloren.
Ohne geöffnetes Fenster (z. B. Automation) wird automatisch ein „Auto-Backup"-Profil angelegt.

## MIDI-Export
- **„Drums MIDI ⇣" usw. in Cubase ziehen** → eine Spur pro Instrument
- **„Alle Spuren ⇣"** → eine Type-1-Datei mit allen Spuren (+ Section-Marker bei Full Song)
- **Section / Full Song exportieren** → Ordner mit allen Einzelspuren unter `Dokumente/Hip-Hop BeatMaker/Exports`

MIDI-Kanäle: Drums 10 (GM-Mapping: Kick 36, Snare 38, Clap 39, HH 42, OH 46, Rim 37, Shaker 70, Crash 49), Bass 1, Keys 2, Pad 3, Lead 4.
Hinweis 432 Hz: Der MIDI-Export enthält nur Noten – für 432 Hz das Ziel-Instrument in Cubase auf A = 432 Hz stimmen.

## Build
Automatisch über GitHub Actions. **Download:** Repo-Startseite → rechts **Releases → „Neueste Version"** (wird bei jedem Push aktualisiert).
Feste Versionen: Tag `v*` pushen.

Lokal:
```bash
cmake -S . -B build
cmake --build build --config Release
```
Tests (Generator, Synth, Samples, Prozessor, Export, Projekte):
```bash
cmake -S . -B build -DHHBM_BUILD_TESTS=ON
cmake --build build --target HHBMRenderTest HHBMProcessorTest
./build/HHBMRenderTest_artefacts/Release/HHBMRenderTest ./render
./build/HHBMProcessorTest_artefacts/Release/HHBMProcessorTest
```

Anleitung neu erzeugen (Screenshots in `Docs/img`): `python3 Docs/make_manual.py`

## Installation
- **Windows:** `Hip-Hop BeatMaker.vst3` nach `C:\Program Files\Common Files\VST3\`
- **macOS:** `.vst3` nach `~/Library/Audio/Plug-Ins/VST3/`, `.component` nach `~/Library/Audio/Plug-Ins/Components/`
  (unsigniert: beim ersten Mal ggf. `xattr -cr "<Pfad zum Plugin>"` ausführen)
