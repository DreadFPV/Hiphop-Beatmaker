#!/usr/bin/env python3
"""Erzeugt Docs/Bedienungsanleitung.pdf (Hip-Hop BeatMaker).  Aufruf: python3 Docs/make_manual.py"""
import os, datetime
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, KeepTogether, NextPageTemplate, CondPageBreak)

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, "img")
OUT = os.path.join(HERE, "Bedienungsanleitung.pdf")
VERSION = "0.4"

FONT_DIR = "/usr/share/fonts/truetype/dejavu"
pdfmetrics.registerFont(TTFont("DV", os.path.join(FONT_DIR, "DejaVuSans.ttf")))
pdfmetrics.registerFont(TTFont("DVB", os.path.join(FONT_DIR, "DejaVuSans-Bold.ttf")))
pdfmetrics.registerFont(TTFont("DVI", os.path.join(FONT_DIR, "DejaVuSans-Oblique.ttf")))
pdfmetrics.registerFont(TTFont("DVM", os.path.join(FONT_DIR, "DejaVuSansMono.ttf")))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily("DV", normal="DV", bold="DVB", italic="DVI", boldItalic="DVB")

GOLD = colors.HexColor("#E0A02A")
DARK = colors.HexColor("#17171A")
GREY = colors.HexColor("#5A5860")
LIGHT = colors.HexColor("#F4F1EA")
LINE = colors.HexColor("#D9D4C7")

# ------------------------------------------------------------------ styles
body = ParagraphStyle("body", fontName="DV", fontSize=9.6, leading=13.6, textColor=DARK, spaceAfter=5)
small = ParagraphStyle("small", parent=body, fontSize=8.4, leading=11.4, textColor=GREY)
h1 = ParagraphStyle("h1", fontName="DVB", fontSize=19, leading=24, textColor=DARK, spaceBefore=4, spaceAfter=10)
h2 = ParagraphStyle("h2", fontName="DVB", fontSize=12.5, leading=16, textColor=DARK, spaceBefore=10, spaceAfter=5)
h3 = ParagraphStyle("h3", fontName="DVB", fontSize=10.4, leading=14, textColor=GREY, spaceBefore=6, spaceAfter=3)
bullet = ParagraphStyle("bullet", parent=body, leftIndent=12, bulletIndent=2, spaceAfter=2.5)
cell = ParagraphStyle("cell", parent=body, fontSize=8.8, leading=11.8, spaceAfter=0)
cellb = ParagraphStyle("cellb", parent=cell, fontName="DVB")
caption = ParagraphStyle("caption", parent=small, alignment=TA_CENTER, spaceBefore=3, spaceAfter=10)
tipstyle = ParagraphStyle("tip", parent=body, fontSize=9.2, leading=13, spaceAfter=0)
mono = ParagraphStyle("mono", parent=body, fontName="DVM", fontSize=8.6, leading=12, textColor=DARK,
                      backColor=LIGHT, borderPadding=(5, 6, 5, 6), spaceBefore=4, spaceAfter=8)

CONTENT_W = A4[0] - 40 * mm


def P(text, st=body):
    return Paragraph(text, st)


def B(items):
    return [Paragraph(t, bullet, bulletText="•") for t in items]


def img(name, width=CONTENT_W, cap=None):
    path = os.path.join(IMG, name)
    w, h = Image.open(path).size
    out = [RLImage(path, width=width, height=width * h / w)]
    if cap:
        out.append(P(cap, caption))
    return KeepTogether(out)


def table(rows, widths, header=True):
    data = [[P(c, cellb if (header and r == 0) else cell) if isinstance(c, str) else c for c in row]
            for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    st = [("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
          ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
          ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5)]
    if header:
        st += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 1.0, GOLD)]
    t.setStyle(TableStyle(st))
    return t


def tip(text, title="Tipp"):
    t = Table([[P(f"<b>{title}:</b> {text}", tipstyle)]], colWidths=[CONTENT_W])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#FBF3E2")),
                           ("LINEBEFORE", (0, 0), (0, -1), 3, GOLD),
                           ("LEFTPADDING", (0, 0), (-1, -1), 9), ("RIGHTPADDING", (0, 0), (-1, -1), 9),
                           ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    return [Spacer(1, 3), t, Spacer(1, 8)]


# ------------------------------------------------------------------ annotated overview
def annotate():
    src = Image.open(os.path.join(IMG, "01_main.png")).convert("RGB")
    d = ImageDraw.Draw(src)
    f = ImageFont.truetype(os.path.join(FONT_DIR, "DejaVuSans-Bold.ttf"), 13)
    marks = [(1, 532, 52), (2, 8, 102), (3, 680, 101), (4, 8, 155), (5, 606, 155), (6, 8, 205),
             (7, 290, 228), (8, 600, 186), (9, 748, 186), (10, 8, 330), (11, 120, 318),
             (12, 196, 300), (13, 872, 300), (14, 8, 586), (15, 8, 640)]
    for n, x, y in marks:
        r = 11
        d.ellipse([x - r, y - r, x + r, y + r], fill=(242, 177, 52), outline=(0, 0, 0), width=2)
        d.text((x, y), str(n), fill=(0, 0, 0), font=f, anchor="mm")
    p = os.path.join(IMG, "01_main_annotated.png")
    src.save(p)
    return "01_main_annotated.png"


# ------------------------------------------------------------------ page decoration
def on_page(c, doc):
    c.saveState()
    c.setStrokeColor(GOLD)
    c.setLineWidth(1.2)
    c.line(20 * mm, A4[1] - 14 * mm, A4[0] - 20 * mm, A4[1] - 14 * mm)
    c.setFont("DVB", 8)
    c.setFillColor(DARK)
    c.drawString(20 * mm, A4[1] - 12 * mm, "HIP-HOP BEATMAKER")
    c.setFont("DV", 8)
    c.setFillColor(GREY)
    c.drawRightString(A4[0] - 20 * mm, A4[1] - 12 * mm, "Bedienungsanleitung")
    c.drawString(20 * mm, 12 * mm, "JustCreateit.ch")
    c.drawRightString(A4[0] - 20 * mm, 12 * mm, f"Seite {doc.page}")
    c.restoreState()


def on_cover(c, doc):
    c.saveState()
    c.setFillColor(DARK)
    c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    icon = os.path.join(IMG, "icon.png")
    s = 70 * mm
    c.drawImage(icon, (A4[0] - s) / 2, A4[1] - 40 * mm - s, s, s, mask="auto")
    c.setFillColor(GOLD)
    c.setFont("DVB", 34)
    c.drawCentredString(A4[0] / 2, A4[1] - 135 * mm, "HIP-HOP BEATMAKER")
    c.setFillColor(colors.white)
    c.setFont("DV", 16)
    c.drawCentredString(A4[0] / 2, A4[1] - 147 * mm, "Bedienungsanleitung")
    c.setFillColor(colors.HexColor("#B8B4AA"))
    c.setFont("DV", 10.5)
    lines = ["Beat-Generator · Synthesizer · Sampler · Noten-Editor",
             "Hip-Hop  ·  Trap  ·  Drill  ·  Lo-Fi  ·  G-Funk",
             "Drum & Bass  ·  Neurofunk  ·  Liquid DnB  ·  Jungle",
             "", "VST3 · AU · Standalone  –  Windows & macOS (Intel + Apple Silicon)"]
    y = A4[1] - 168 * mm
    for ln in lines:
        c.drawCentredString(A4[0] / 2, y, ln)
        y -= 6.2 * mm
    c.setFont("DV", 9)
    c.setFillColor(GOLD)
    c.drawCentredString(A4[0] / 2, 22 * mm, f"Version {VERSION}  ·  Stand {datetime.date.today().strftime('%d.%m.%Y')}  ·  JustCreateit.ch")
    c.restoreState()


# ------------------------------------------------------------------ content
def build():
    story = [NextPageTemplate("normal"), PageBreak()]

    # Inhalt
    story += [P("Inhalt", h1)]
    toc = [("1", "Überblick"), ("2", "Installation"), ("3", "Schnellstart in 5 Schritten"),
           ("4", "Die Oberfläche"), ("5", "Styles"), ("6", "Beat-Einstellungen"),
           ("7", "Abspielen & Transport"), ("8", "Spuren"), ("9", "Noten-Editor"),
           ("10", "Sound-Editor: Synth, Samples, Drums"), ("11", "Presets, Sound-Profile & Style-Wechsel"),
           ("12", "MIDI-Keyboard"), ("13", "Export: MIDI & WAV"), ("14", "Projekte"),
           ("15", "Arbeiten mit Cubase"), ("16", "Dateien & Ordner"), ("17", "Tastenkürzel"),
           ("18", "Fehlerbehebung")]
    story.append(table([[n, t] for n, t in toc], [12 * mm, CONTENT_W - 12 * mm], header=False))
    story.append(PageBreak())

    # 1
    story += [P("1  Überblick", h1),
              P("Der <b>Hip-Hop BeatMaker</b> erzeugt auf Knopfdruck komplette Beats in neun Styles – "
                "Drums, Bass bzw. 808, Keys, Pad und Lead – jeweils als eigene Spur. Alles lässt sich "
                "vorhören, mit dem eingebauten Synthesizer oder eigenen Samples klanglich formen, im "
                "Noten-Editor bearbeiten und anschliessend als MIDI (pro Instrument) oder WAV exportieren."),
              P("Formate", h3)]
    story += B(["<b>VST3</b> (Windows & macOS) – z. B. für Cubase",
                "<b>AU</b> (macOS) – z. B. für Logic, GarageBand",
                "<b>Standalone-App</b> – ohne DAW nutzbar, mit eigenem Transport, MIDI-Keyboard, WAV-Export und Projekten"])
    story += [P("Was der BeatMaker kann", h3)]
    story += B(["9 Styles mit je eigenem Groove, Harmonik und Sound",
                "Song-Sections: Intro, Verse, Hook, Bridge, Outro und kompletter Song",
                "Tonart, Skala, Akkordfolge, Tuning 440 / 432 Hz",
                "Swing, Variation, Humanize und Seed für unendlich viele Varianten",
                "Voller Synth-Editor pro Instrument, Drum-Synth mit Tune/Decay/Tone/Level",
                "Eigene Samples für jede Drum und jedes Instrument (mit automatischer Grundton-Erkennung)",
                "Noten-Editor (Piano-Roll und Drum-Raster) – bearbeitete Spuren werden gesperrt",
                "MIDI per Drag & Drop nach Cubase, MIDI-Ordner-Export, WAV-Mix und Stems",
                "Presets, Sound-Profile und Projekt-Dateien"])
    story.append(PageBreak())

    # 2
    story += [P("2  Installation", h1),
              P("Die aktuelle Version gibt es immer unter <b>github.com/DreadFPV/Hiphop-Beatmaker</b> → "
                "<b>Releases → „Neueste Version“</b>. Dort liegen zwei ZIP-Dateien:"),
              table([["Datei", "Inhalt"],
                     ["HipHopBeatMaker-Windows.zip", "Hip-Hop BeatMaker.vst3, Hip-Hop BeatMaker.exe (Standalone), README, diese Anleitung"],
                     ["HipHopBeatMaker-macOS.zip", "Hip-Hop BeatMaker.vst3, .component (AU), .app (Standalone), README, diese Anleitung – läuft auf Intel und Apple Silicon ab macOS 11"]],
                    [55 * mm, CONTENT_W - 55 * mm]),
              P("Windows", h2)]
    story += B(["ZIP entpacken.",
                "<b>Hip-Hop BeatMaker.vst3</b> nach <font name='DVM'>C:\\Program Files\\Common Files\\VST3\\</font> kopieren.",
                "Standalone: <b>Hip-Hop BeatMaker.exe</b> an einen beliebigen Ort legen und starten. "
                "Falls Windows SmartScreen warnt: „Weitere Informationen“ → „Trotzdem ausführen“."])
    story += [P("macOS", h2)]
    story += B(["ZIP entpacken (Doppelklick).",
                "<b>Hip-Hop BeatMaker.app</b> in den Ordner <b>Programme</b> ziehen.",
                "Plugins: <b>.vst3</b> nach <font name='DVM'>~/Library/Audio/Plug-Ins/VST3/</font>, "
                "<b>.component</b> nach <font name='DVM'>~/Library/Audio/Plug-Ins/Components/</font>.",
                "Weil die App nicht von Apple signiert ist, einmalig im Terminal die Sperre entfernen "
                "(Pfad per Drag & Drop ins Terminal ziehen):"])
    story += [P("xattr -cr \"/Applications/Hip-Hop BeatMaker.app\"<br/>"
                "xattr -cr ~/Library/Audio/Plug-Ins/VST3/\"Hip-Hop BeatMaker.vst3\"", mono)]
    story += B(["Danach die App per <b>Rechtsklick → Öffnen</b> starten."])
    story += [P("Erster Start der Standalone-App", h2),
              P("Oben links auf <b>Options</b> klicken → <b>Audio/MIDI Settings</b>: Ausgabegerät wählen und – falls "
                "vorhanden – das MIDI-Keyboard unter <b>Active MIDI inputs</b> aktivieren.")]
    story += tip("In Cubase nach dem Kopieren des Plugins einmal <b>Studio → VST-PlugIn-Manager → Aktualisieren</b> "
                 "ausführen. Das Plugin erscheint als Instrument unter <b>JustCreateit</b>.")
    story.append(PageBreak())

    # 3
    story += [P("3  Schnellstart in 5 Schritten", h1)]
    steps = [("1", "<b>Style wählen</b> – z. B. Boom Bap, Trap oder Neurofunk. Tempo und Sound passen sich automatisch an."),
             ("2", "<b>Tonart & Skala</b> einstellen (Key, Scale). Akkordfolge auf „Auto (Style)“ lassen oder selbst wählen."),
             ("3", "<b>▶ Play</b> drücken und mit den Section-Buttons (Intro … Full Song) durchhören."),
             ("4", "Gefällt die Variante nicht? <b>Neu würfeln</b> – oder Swing / Variation / Humanize verändern."),
             ("5", "Fertig? Die farbigen Kacheln <b>„Drums MIDI ⇣“, „Bass MIDI ⇣“ …</b> direkt in Cubase ziehen – "
                   "oder über <b>WAV-Export</b> als Audio rendern.")]
    story.append(table([[P(f"<b>{n}</b>", cell), P(t, cell)] for n, t in steps], [10 * mm, CONTENT_W - 10 * mm], header=False))
    story += tip("Eine Note klingt falsch oder ein Fill gefällt nicht? Einfach auf die Spur klicken und im "
                 "Noten-Editor korrigieren (Kapitel 9).")
    story.append(PageBreak())

    # 4
    story += [P("4  Die Oberfläche", h1),
              img(annotate(), width=CONTENT_W * 0.78, cap="Hauptfenster (Standalone). Die Nummern entsprechen der Tabelle.")]
    story.append(table([["Nr.", "Bereich", "Funktion"],
                        ["1", "Projekt ▾ / Sound-Profil ▾", "Projekte öffnen/speichern, komplette Sounds speichern/laden (Kap. 11, 14)"],
                        ["2", "Style, Key, Scale, Chords, Tuning", "Grundcharakter des Beats (Kap. 5, 6)"],
                        ["3", "Preview BPM", "Tempo für das Vorhören ohne DAW. In der DAW gilt das Host-Tempo."],
                        ["4", "Sections", "Intro, Verse, Hook, Bridge, Outro oder Full Song wählen"],
                        ["5", "Position · Loop · Neu würfeln · Play", "Transport (Kap. 7) und neue Variante per Zufalls-Seed"],
                        ["6", "Swing · Variation · Humanize", "Groove, Abwechslung und menschliches Timing"],
                        ["7", "Seed", "Nummer der Variante – gleicher Seed = gleicher Beat"],
                        ["8", "Sounds · MIDI Out", "Klang an/aus (nur MIDI) und Noten zusätzlich an die DAW senden"],
                        ["9", "Keyboard", "Welches Instrument dein MIDI-Keyboard spielt (Kap. 12)"],
                        ["10", "Volume · Mute", "Lautstärke und Stummschaltung pro Spur"],
                        ["11", "Sound", "Öffnet den Sound-Editor der Spur (Kap. 10)"],
                        ["12", "Notenansicht", "Klick öffnet den Noten-Editor (Kap. 9); Schloss = bearbeitet"],
                        ["13", "„… MIDI ⇣“", "Ziehen: MIDI-Datei der Spur per Drag & Drop in die DAW"],
                        ["14", "Export-Zeile", "Alle Spuren ziehen, MIDI-Ordner, Export-Ordner, WAV-Export (Kap. 13)"],
                        ["15", "Drum-Samples", "Eigenes Sample pro Drum laden (Klick) / zurück zum Synth (Shift+Klick)"]],
                       [10 * mm, 52 * mm, CONTENT_W - 62 * mm]))
    story.append(PageBreak())

    # 5
    story += [P("5  Styles", h1),
              P("Jeder Style bringt eigenes Tempo, eigene Drum-Patterns, Basslinien, Akkord-Rhythmen, Melodie-Typen "
                "und einen passenden Sound mit. Das Tempo wird beim Wechsel automatisch gesetzt (in der DAW gilt das Projekttempo).")]
    story.append(table([["Style", "BPM", "Charakter"],
                        ["Boom Bap", "90", "Swingende 90s-Drums, Ghost-Snares, Piano-Stabs, Flöten-/Horn-Melodie"],
                        ["Trap", "140", "Half-Time, Hi-Hat-Rolls (32tel/Triolen), 808 mit Glides, Bell-Melodien"],
                        ["Drill", "142", "Triolische Hats, verschobene Snares, gleitende 808-Slides, dunkle Piano-Figuren"],
                        ["Lo-Fi", "80", "Viel Swing, weiche Drums, Rhodes-Akkorde mit 7/9, ruhige Melodie"],
                        ["G-Funk", "94", "Funky 16tel-Synth-Bass, Rhodes-Offbeats, Portamento-„Whine“-Lead"],
                        ["Drum & Bass", "174", "Two-Step-Breakbeat, rollender Sub-Bass, Akkord-Stabs, Pluck-Arps, Snare-Rolls"],
                        ["Neurofunk", "174", "Harte Two-Step-Drums, Reese-/Growl-Bass in 16teln, dunkle Stabs"],
                        ["Liquid DnB", "174", "Weiche Drums, seidiger Sub, lange Rhodes-Akkorde, warme Pads, Legato-Melodie"],
                        ["Jungle", "165", "Zerhackte Amen-artige Breaks, Dub-Sub-Bass, Offbeat-Skank, Horn-Stabs"]],
                       [32 * mm, 14 * mm, CONTENT_W - 46 * mm]))
    story += [Spacer(1, 8), img("02_styles.png", width=CONTENT_W * 0.8, cap="Style-Auswahl")]
    story += tip("Beim Style-Wechsel lädt der BeatMaker den passenden Sound. Hast du den Sound vorher verändert, "
                 "fragt er, ob er ihn speichern soll (Kapitel 11).", "Hinweis")
    story.append(PageBreak())

    # 6
    story += [P("6  Beat-Einstellungen", h1), P("Harmonie", h2),
              table([["Regler", "Wirkung"],
                     ["Key", "Grundton der Tonart (C … B)"],
                     ["Scale", "Minor, Harmonic Minor, Dorian, Phrygian, Major, Mixolydian"],
                     ["Chords", "Akkordfolge. <b>Auto (Style)</b>: der Seed wählt eine style-typische Folge. "
                                "Feste Folgen: i–VI–III–VII, i–iv–v–i, i–VI–iv–v, ii–V–I–vi, i–VII–VI–VII, i–i–VI–V"],
                     ["Tuning", "440 Hz oder 432 Hz – wirkt auf Synth und Samples. MIDI-Export enthält nur Noten: "
                                "für 432 Hz das Instrument in der DAW auf A = 432 Hz stimmen."]],
                    [28 * mm, CONTENT_W - 28 * mm]),
              P("Sections", h2),
              table([["Section", "Länge", "Typischer Inhalt"],
                     ["Intro", "4 Takte", "Hats/Percussion, Akkorde; Drums setzen in der zweiten Hälfte ein, kurzer Drop vor dem Verse"],
                     ["Verse", "8 Takte", "Volle Drums, Bass, Keys – Platz für Rap/Vocals, wenig Melodie"],
                     ["Hook", "8 Takte", "Alles an: Melodie, Pad, mehr Variation, Crash zum Start"],
                     ["Bridge", "4 Takte", "Ausgedünnt: ohne Kick, halbierte Snares, andere Akkord-Reihenfolge"],
                     ["Outro", "4 Takte", "Drums laufen aus, Schlussschlag mit Crash"],
                     ["Full Song", "52 Takte", "Intro – Verse – Hook – Verse – Hook – Bridge – Hook – Outro"]],
                    [24 * mm, 20 * mm, CONTENT_W - 44 * mm]),
              P("Groove & Varianten", h2),
              table([["Regler", "Wirkung"],
                     ["Swing", "Verschiebt die Off-16tel (MPC-Swing). Wie stark, hängt vom Style ab (Lo-Fi stark, Neurofunk kaum)."],
                     ["Variation", "Mehr Ghost-Notes, Fills, Rolls, Basssprünge und Melodie-Abwandlungen"],
                     ["Humanize", "Kleine Timing- und Lautstärke-Schwankungen wie gespielt"],
                     ["Seed", "Nummer der Variante (1–9999). <b>Neu würfeln</b> setzt eine zufällige. "
                              "Der Seed bestimmt Drums, Bass, Akkord-Rhythmen, Pad-Voicing und Melodie."]],
                    [28 * mm, CONTENT_W - 28 * mm])]
    story += tip("Der Generator ist deterministisch: Gleiche Einstellungen + gleicher Seed ergeben auf jedem "
                 "Rechner exakt denselben Beat. Notiere dir gute Seeds!")
    story.append(PageBreak())

    # 7
    story += [P("7  Abspielen & Transport", h1)]
    story += B(["<b>▶ Play</b> startet die gewählte Section von vorne, <b>■ Stop</b> hält an.",
                "<b>Loop</b> an: die Section (bzw. der Song) läuft in Schleife. Aus: einmal abspielen und stoppen.",
                "Die <b>Positionsanzeige</b> zeigt Takt.Schlag und im Full Song die aktuelle Section.",
                "<b>In der DAW</b> folgt das Plugin automatisch dem Transport und Tempo des Hosts – der Beat läuft "
                "synchron zum Projekt, die gewählte Section wird geloopt.",
                "Section-, Style- oder Seed-Wechsel greifen sofort, auch während der Wiedergabe."])
    story += [P("Klang an / aus, MIDI Out", h2)]
    story += B(["<b>Sounds</b> aus: das Plugin bleibt stumm – praktisch, wenn du nur MIDI für andere Instrumente erzeugst.",
                "<b>MIDI Out</b> an: die Noten werden zusätzlich an die DAW gesendet (Kanäle siehe Kapitel 13)."])
    story += [P("8  Spuren", h1)]
    story += B(["Jede Spur (Drums, Bass, Keys, Pad, Lead) hat <b>Volume</b> und <b>Mute</b>. Stummgeschaltete Spuren "
                "fehlen auch beim WAV-Mix und bei den Stems.",
                "Die <b>Notenansicht</b> zeigt die Noten der Section, mit Abspiel-Linie. Ein <b>Schloss</b> oben rechts "
                "bedeutet: diese Spur wurde im Noten-Editor bearbeitet.",
                "<b>Klick auf die Notenansicht</b> öffnet den Noten-Editor, <b>Sound</b> den Sound-Editor."])
    story.append(PageBreak())

    # 9
    story += [P("9  Noten-Editor", h1),
              P("Ein Klick auf eine Spur öffnet den Editor für diese Spur und Section. Im Full Song öffnet sich die "
                "Section, auf die du geklickt hast. Oben wechselst du Instrument und Section; ein Punkt (•) hinter "
                "der Section zeigt, dass sie bearbeitet ist."),
              img("03_noteeditor.png", cap="Piano-Roll (Bass): Tastatur links (C3 = 60), Velocity-Leiste unten")]
    story += [P("Bearbeiten mit der Maus", h2),
              table([["Aktion", "So geht's"],
                     ["Note setzen", "Doppelklick auf eine leere Stelle (Länge = Raster)"],
                     ["Verschieben", "Note ziehen – waagerecht in der Zeit, senkrecht in der Tonhöhe"],
                     ["Länge ändern", "Am rechten Rand der Note ziehen (Mauszeiger ↔)"],
                     ["Löschen", "Rechtsklick auf die Note, oder Auswahl + Entf / „Löschen“"],
                     ["Auswählen", "Klick auf Note; Rahmen auf leerer Fläche aufziehen; Shift = hinzufügen/entfernen"],
                     ["Velocity", "In der Leiste unten ziehen – wirkt auf die Auswahl, sonst auf Noten unter der Maus"],
                     ["Einfügepunkt", "Klick auf leere Fläche setzt die goldene Linie – dort landet „Einfügen“"],
                     ["Hoch/Runter scrollen", "Mausrad (Piano-Roll)"]],
                    [34 * mm, CONTENT_W - 34 * mm])]
    story += [CondPageBreak(60 * mm), P("Werkzeugleiste", h2),
              table([["Element", "Funktion"],
                     ["Raster", "1/4, 1/8, 1/16, 1/32, 1/8-Triole, 1/16-Triole – für Setzen, Verschieben, Länge, Quantize"],
                     ["Quantize", "Auswahl (oder alle Noten) aufs Raster setzen"],
                     ["Undo / Redo", "Schritte rückgängig / wiederherstellen – auch über Spuren und Sections hinweg"],
                     ["Kopieren / Einfügen", "Auswahl kopieren, am Einfügepunkt einsetzen (auch in andere Sections/Spuren)"],
                     ["Duplizieren", "Auswahl direkt dahinter wiederholen (auf Schläge/Takte gerundet)"],
                     ["Neu generieren", "Deine Änderungen verwerfen – der Generator übernimmt die Spur wieder"]],
                    [38 * mm, CONTENT_W - 38 * mm])]
    story += [PageBreak(), P("Drum-Raster", h2),
              P("Für die Drums zeigt der Editor ein Raster mit Instrument-Namen statt Tonhöhen. Senkrechtes "
                "Ziehen verschiebt einen Schlag auf ein anderes Instrument, ↑/↓ ebenso."),
              img("04_drumgrid.png", cap="Drum-Raster mit Velocity-Leiste")]
    story += [P("Sperre: bearbeitete Spuren", h2)]
    story += B(["Sobald du eine Spur in einer Section änderst, ist sie <b>gesperrt</b>: Seed, Style, Key, Variation usw. "
                "verändern sie nicht mehr. Alle anderen Spuren werden weiterhin generiert.",
                "Die Sperre gilt <b>pro Section</b>: eine bearbeitete Hook erscheint in <b>jeder</b> Hook des Full Songs.",
                "<b>Neu generieren</b> hebt die Sperre auf (mit Undo rückgängig zu machen).",
                "Bearbeitete Noten werden exakt so übernommen – Swing und Humanize wirken auf sie nicht mehr.",
                "Drag & Drop, MIDI-/WAV-Export und Projekte enthalten immer deine bearbeiteten Noten."])
    story += tip("Erst Style, Tonart und Seed festlegen, dann im Editor feilen. Willst du später trotzdem die Tonart "
                 "wechseln: bearbeitete Spuren mit ↑/↓ (Auswahl: Strg/Cmd+A) transponieren.")
    story.append(PageBreak())

    # 10
    story += [P("10  Sound-Editor", h1),
              P("Der Button <b>Sound</b> neben einer Spur öffnet den Sound-Editor. Oben wählst du das Instrument, "
                "bei Bass, Keys, Pad und Lead zusätzlich <b>Synth</b> oder <b>Sample</b>. Alle Regler sind in der DAW "
                "automatisierbar; <b>Doppelklick</b> setzt einen Regler auf den Standardwert."),
              img("05_synth.png", cap="Synth-Seite (Bass)"),
              table([["Gruppe", "Regler"],
                     ["Oszillator", "<b>Wellenform</b> (Sine, Saw, Square, Triangle, FM) · <b>Unison</b> 1–4 Stimmen · <b>Detune</b> "
                                    "(Verstimmung der Stimmen) · <b>Sub</b> (Oktave darunter) · <b>FM Ratio / Index / Decay</b> "
                                    "(nur bei FM: Obertonfarbe, Stärke, Abklingen – E-Piano, Glocken)"],
                     ["Filter", "<b>Cutoff</b> · <b>Resonanz</b> · <b>Filter Env</b> (Hüllkurve öffnet das Filter, in Oktaven) · "
                                "<b>Keytrack</b> (hohe Noten heller) · <b>Drive</b> (Sättigung)"],
                     ["Hüllkurve", "<b>Attack</b> · <b>Decay</b> · <b>Sustain</b> · <b>Release</b> · <b>Level</b> (Lautstärke des Instruments)"],
                     ["Modulation", "<b>Glide</b> (Portamento – bei überlappenden Noten, z. B. 808-Slides) · <b>Vibrato</b> + <b>Vib Rate</b> · "
                                    "<b>Pitch Env</b> + <b>Pitch Zeit</b> (Tonhöhen-Sprung am Notenanfang, z. B. 808-Punch) · <b>Tremolo</b>"]],
                    [26 * mm, CONTENT_W - 26 * mm])]
    story += [PageBreak(), P("Eigene Instrument-Samples", h2),
              img("06_sample.png", width=CONTENT_W * 0.9, cap="Sample-Seite: Wellenform mit Loop-Bereich")]
    story += B(["<b>Sample laden…</b>: WAV, AIFF, FLAC, OGG oder MP3 (bis 30 s). Das Sample wird über die Tastatur transponiert.",
                "<b>Grundton</b> wird automatisch erkannt: Ein Notenname im Dateinamen (z. B. „808 Boom F1.wav“, C3 = 60) "
                "hat Vorrang, Oktave und Feinstimmung liefert die Tonhöhen-Analyse. Manuell korrigierbar mit −/+.",
                "<b>Fine</b> ±100 Cent, <b>Attack</b>, <b>Release</b>.",
                "<b>Loop</b> mit Start und Ende (mit Überblendung) – damit klingen kurze Samples bei langen Noten weiter. "
                "Beim Pad ist Loop standardmässig an.",
                "Glide, Vibrato und Pitch Env der Synth-Seite wirken auch auf das Sample – 808-Slides funktionieren also auch mit eigenem 808.",
                "<b>Synth verwenden</b> schaltet zurück auf den eingebauten Synth."])
    story += [KeepTogether([P("Drums", h2),
              img("07_drumsound.png", cap="Drum-Seite: pro Drum Sample-Slot und vier Regler")])]
    story += B(["<b>Tune</b> ±24 Halbtöne · <b>Decay</b> 0.25×–4× (kürzer/länger) · <b>Tone</b> (dunkel ↔ hell) · <b>Level</b>.",
                "Klick auf den Sample-Button lädt ein eigenes Sample, <b>Shift+Klick</b> schaltet zurück zum Synth-Sound. "
                "Tune und Level wirken auch auf Samples, Decay kürzt Samples bei Werten unter 1."])
    story.append(PageBreak())

    # 11
    story += [P("11  Presets, Sound-Profile & Style-Wechsel", h1), P("Instrument-Presets", h2)]
    story += B(["Im Sound-Editor: <b>Presets ▾ → Preset speichern…</b> sichert den Sound des gewählten Instruments.",
                "Gespeicherte Presets erscheinen im selben Menü und lassen sich auch auf andere Spuren laden.",
                "<b>Style-Standard</b> setzt das Instrument auf den Sound des aktuellen Styles zurück."])
    story += [P("Sound-Profile", h2),
              P("Ein Sound-Profil enthält den kompletten Sound: alle vier Synths und alle Drum-Einstellungen."),
              img("11_menu_profile.png", width=CONTENT_W * 0.85, cap="Menü Sound-Profil")]
    story += B(["<b>Sound-Profil speichern…</b> – unter eigenem Namen sichern.",
                "<b>Style-Standard laden</b> – alle Instrumente auf den Sound des aktuellen Styles.",
                "Gespeicherte Profile und <b>Auto-Backups</b> (Untermenü) direkt laden."])
    story += [PageBreak(), P("Was beim Style-Wechsel passiert", h2),
              P("Jeder Style hat seinen eigenen Standard-Sound. Hast du den aktuellen Sound verändert, fragt der "
                "BeatMaker vor dem Wechsel nach – die Style-Standards selbst gehen dabei nie verloren:"),
              img("08_dialog.png", cap="Nachfrage beim Style-Wechsel"),
              table([["Button", "Ergebnis"],
                     ["Speichern & laden", "Dein Sound wird als Sound-Profil (Name frei wählbar) gespeichert, danach lädt der neue Style seinen Sound."],
                     ["Nicht speichern", "Der neue Style lädt seinen Sound, deine Änderungen werden verworfen."],
                     ["Sound behalten", "Der Style wechselt (Patterns, Tempo), dein Sound bleibt."]],
                    [38 * mm, CONTENT_W - 38 * mm])]
    story += tip("Ist das Plugin-Fenster geschlossen (z. B. Style-Automation in der DAW), legt der BeatMaker automatisch "
                 "ein Auto-Backup an – höchstens eines pro Minute, die letzten 10 bleiben erhalten.", "Hinweis")
    story.append(PageBreak())

    # 12
    story += [P("12  MIDI-Keyboard", h1)]
    story += B(["Standalone: unter <b>Options → Audio/MIDI Settings</b> das Keyboard aktivieren. In der DAW einfach die "
                "Instrumentenspur mit dem Plugin aufnahmebereit schalten.",
                "<b>Keyboard</b> (Hauptfenster) bestimmt, welches Instrument gespielt wird: Drums, Bass, Keys, Pad oder Lead.",
                "<b>Auto (MIDI-Kanal)</b>: Kanal 10 = Drums, 1 = Bass, 2 = Keys, 3 = Pad, 4 = Lead.",
                "Du kannst live zum laufenden Beat spielen; die gespielte Note wird neben der Auswahl kurz angezeigt.",
                "Drums: Kick 36 (C1), Snare 38, Clap 39, Closed Hat 42, Open Hat 46, Rim 37, Shaker 70, Crash 49."])

    # 13
    story += [P("13  Export: MIDI & WAV", h1), P("MIDI per Drag & Drop", h2)]
    story += B(["Kachel <b>„Drums MIDI ⇣“</b>, <b>„Bass MIDI ⇣“</b> … mit der Maus in die DAW ziehen – es entsteht eine "
                "Spur pro Instrument mit dem Inhalt der aktuellen Section (bzw. des Songs).",
                "<b>„Alle Spuren ⇣ (1 Datei)“</b> erzeugt eine Mehrspur-MIDI-Datei; im Full Song inklusive Section-Markern.",
                "Tempo und Taktart sind in den Dateien enthalten."])
    story += [P("MIDI-Ordner", h2)]
    story += B(["<b>MIDI: Section</b> / <b>MIDI: Full Song</b> speichert jede Spur als eigene Datei plus eine Datei mit allen Spuren "
                "in einen neuen Ordner unter <font name='DVM'>Dokumente/Hip-Hop BeatMaker/Exports</font>.",
                "<b>Export-Ordner</b> öffnet diesen Ordner."])
    story += [CondPageBreak(90 * mm), P("WAV-Export", h2),
              img("12_menu_wav.png", width=CONTENT_W * 0.75, cap="Menü WAV-Export")]
    story += B(["Section oder Full Song als <b>Mix</b>, als <b>Stems</b> (jede Spur einzeln) oder beides.",
                "Format: WAV, 48 kHz, 24 Bit, inklusive 2.5 s Ausklang. Das Rendern läuft im Hintergrund; danach öffnet sich der Ordner.",
                "Es zählen deine aktuellen Sounds, Samples, Lautstärken und Mutes."])
    story.append(table([["Spur", "MIDI-Kanal", "Hinweis"],
                        ["Drums", "10", "General-MIDI-Belegung (siehe Kapitel 12)"],
                        ["Bass", "1", "808-Glides = überlappende Noten"],
                        ["Keys", "2", ""], ["Pad", "3", ""], ["Lead", "4", ""]],
                       [30 * mm, 26 * mm, CONTENT_W - 56 * mm]))
    story.append(PageBreak())

    # 14
    story += [P("14  Projekte", h1),
              P("Ein Projekt (<b>.hhbeat</b>) speichert alles: Style, alle Regler, Sounds, bearbeitete Noten und die "
                "Pfade deiner Samples. Die Sample-Dateien selbst bleiben an ihrem Ort – nicht verschieben oder löschen."),
              img("10_menu_project.png", width=CONTENT_W * 0.8, cap="Menü Projekt")]
    story += B(["<b>Öffnen…</b>, <b>Speichern</b>, <b>Speichern unter…</b>, <b>Zuletzt</b> (die letzten 12 Projekte), <b>Projekt-Ordner öffnen</b>.",
                "Standard-Ordner: <font name='DVM'>Dokumente/Hip-Hop BeatMaker/Projekte</font>.",
                "Der Name des geöffneten Projekts steht oben neben dem Logo.",
                "Im Plugin speichert zusätzlich die DAW den kompletten Zustand mit dem DAW-Projekt."])

    # 15
    story += [P("15  Arbeiten mit Cubase", h1)]
    story += B(["<b>Projekt → Spur hinzufügen → Instrument → Hip-Hop BeatMaker</b>.",
                "Projekttempo einstellen – das Plugin folgt Tempo und Transport.",
                "Beat im Plugin bauen, Sections durchhören (die gewählte Section läuft synchron zum Cubase-Transport).",
                "MIDI-Kacheln auf neue Instrumentenspuren ziehen – mit Cubase-Instrumenten (z. B. Groove Agent für die Drums) "
                "oder wieder mit dem BeatMaker als Klangerzeuger.",
                "Alternativ: WAV-Stems exportieren und als Audio importieren.",
                "Für einen ganzen Track: <b>Full Song</b> wählen und „Alle Spuren ⇣“ ziehen – die Section-Marker kommen mit."])
    story += tip("Möchtest du nur MIDI für andere Instrumente erzeugen, schalte <b>Sounds</b> aus. Mit <b>MIDI Out</b> kannst "
                 "du die Noten des laufenden Beats auch live in andere Spuren aufnehmen.")
    story.append(PageBreak())

    # 16
    story += [P("16  Dateien & Ordner", h1),
              P("Alles liegt unter <font name='DVM'>Dokumente/Hip-Hop BeatMaker/</font>:"),
              table([["Ordner", "Inhalt"],
                     ["Exports/", "MIDI-Ordner, WAV-Exporte; <font name='DVM'>_Drag/</font> = Dateien für Drag & Drop (dürfen gelöscht werden)"],
                     ["Projekte/", "Projekt-Dateien (.hhbeat)"],
                     ["Presets/Bass, Keys, Pad, Lead, Drums", "Instrument-Presets (.hhpreset)"],
                     ["Presets/Sound-Profile/", "Sound-Profile (.hhsound)"],
                     ["Presets/Sound-Profile/Auto-Backups/", "Automatische Sicherungen (max. 10)"]],
                    [62 * mm, CONTENT_W - 62 * mm])]

    # 17
    story += [P("17  Tastenkürzel (Noten-Editor)", h1),
              P("Auf macOS steht <b>Cmd</b> für Strg. Im Plugin muss das Editor-Fenster den Fokus haben (einmal hineinklicken)."),
              table([["Taste", "Funktion"],
                     ["Entf / Backspace", "Auswahl löschen"],
                     ["Strg/Cmd + C · V", "Kopieren · Einfügen (am Einfügepunkt)"],
                     ["Strg/Cmd + D", "Auswahl duplizieren"],
                     ["Strg/Cmd + A", "Alles auswählen"],
                     ["Strg/Cmd + Z · Shift + Strg/Cmd + Z (oder Strg + Y)", "Undo · Redo"],
                     ["Q", "Quantisieren"],
                     ["↑ / ↓  (Shift: Oktave)", "Auswahl transponieren (Drums: anderes Instrument)"],
                     ["← / →", "Auswahl um ein Raster verschieben"],
                     ["Esc", "Auswahl aufheben, sonst Editor schliessen"]],
                    [70 * mm, CONTENT_W - 70 * mm])]
    story.append(PageBreak())

    # 18
    story += [P("18  Fehlerbehebung", h1),
              table([["Problem", "Lösung"],
                     ["macOS: „App ist beschädigt“ / „kann nicht geöffnet werden“",
                      "Einmal <font name='DVM'>xattr -cr</font> auf die App bzw. das Plugin ausführen (Kapitel 2), dann Rechtsklick → Öffnen."],
                     ["macOS: „Diese Version kann nicht mit dieser macOS-Version verwendet werden“",
                      "Neueste Version laden – sie läuft ab macOS 11 auf Intel und Apple Silicon."],
                     ["Kein Ton in der Standalone-App", "Options → Audio/MIDI Settings: Ausgabegerät wählen. „Sounds“ eingeschaltet? Spur gemutet?"],
                     ["Plugin erscheint nicht in Cubase", "Datei im richtigen VST3-Ordner? VST-PlugIn-Manager → Aktualisieren."],
                     ["MIDI-Keyboard spielt nichts", "In Options als MIDI-Input aktivieren; Keyboard-Ziel prüfen (bei Auto auf den MIDI-Kanal achten)."],
                     ["Spur ändert sich beim Würfeln nicht", "Ist ein Schloss zu sehen? Die Spur wurde bearbeitet → im Noten-Editor „Neu generieren“."],
                     ["Tastenkürzel im Plugin funktionieren nicht", "Einmal in den Noten-Editor klicken; alternativ die Buttons der Werkzeugleiste nutzen."],
                     ["Sample fehlt nach dem Öffnen eines Projekts", "Die Sample-Datei wurde verschoben oder gelöscht – im Sound-Editor neu laden."],
                     ["Grundton eines Samples falsch erkannt", "Im Sample-Tab Grundton mit −/+ und Fine korrigieren, oder den Notennamen in den Dateinamen schreiben."]],
                    [62 * mm, CONTENT_W - 62 * mm]),
              Spacer(1, 14),
              P("Fragen, Ideen, Fehler? <b>JustCreateit.ch</b>", small)]

    doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=20 * mm, bottomMargin=20 * mm,
                          title="Hip-Hop BeatMaker – Bedienungsanleitung", author="JustCreateit.ch", subject="Bedienungsanleitung")
    frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
    doc.addPageTemplates([PageTemplate(id="cover", frames=[frame], onPage=on_cover),
                          PageTemplate(id="normal", frames=[frame], onPage=on_page)])
    doc.build(story)
    print("geschrieben:", OUT)


if __name__ == "__main__":
    build()
