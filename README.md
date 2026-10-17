# Hip-Hop BeatMaker

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

## Bedienung
- **Style, Key, Scale, Chords** (Akkordfolge), **Tuning 440/432 Hz**
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

## MIDI-Export
- **„Drums MIDI ⇣" usw. in Cubase ziehen** → eine Spur pro Instrument
- **„Alle Spuren ⇣"** → eine Type-1-Datei mit allen Spuren (+ Section-Marker bei Full Song)
- **Section / Full Song exportieren** → Ordner mit allen Einzelspuren unter `Dokumente/Hip-Hop BeatMaker/Exports`

MIDI-Kanäle: Drums 10 (GM-Mapping: Kick 36, Snare 38, Clap 39, HH 42, OH 46, Rim 37, Shaker 70, Crash 49), Bass 1, Keys 2, Pad 3, Lead 4.
Hinweis 432 Hz: Der MIDI-Export enthält nur Noten – für 432 Hz das Ziel-Instrument in Cubase auf A = 432 Hz stimmen.

## Build
Automatisch über GitHub Actions (Tab *Actions* → Artefakte herunterladen). Release: Tag `v*` pushen.

Lokal:
```bash
cmake -S . -B build
cmake --build build --config Release
```
Offline-Test (generiert alle Styles, prüft Daten, rendert WAVs):
```bash
cmake -S . -B build -DHHBM_BUILD_TESTS=ON && cmake --build build --target HHBMRenderTest
./build/HHBMRenderTest_artefacts/Release/HHBMRenderTest ./render
```

## Installation
- **Windows:** `Hip-Hop BeatMaker.vst3` nach `C:\Program Files\Common Files\VST3\`
- **macOS:** `.vst3` nach `~/Library/Audio/Plug-Ins/VST3/`, `.component` nach `~/Library/Audio/Plug-Ins/Components/`
  (unsigniert: beim ersten Mal ggf. `xattr -cr "<Pfad zum Plugin>"` ausführen)
