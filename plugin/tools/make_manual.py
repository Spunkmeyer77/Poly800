#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds plugin/docs/P-800 Manual.pdf (reportlab). Run from the repo root."""
import csv
import pathlib
import re

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import (Image, KeepTogether, PageBreak, Paragraph,
                                SimpleDocTemplate, Spacer, Table, TableStyle)

ROOT = pathlib.Path(__file__).resolve().parents[2]
DOCS = ROOT / "plugin" / "docs"
OUT = DOCS / "P-800 Manual.pdf"
# Version comes from plugin/CMakeLists.txt (project VERSION), the single source of truth.
VERSION = re.search(r"project\(P800 VERSION ([0-9.]+)", (ROOT / "plugin" / "CMakeLists.txt").read_text()).group(1)

ACCENT = colors.HexColor("#c8741c")
INK = colors.HexColor("#1d1d1f")
GREY = colors.HexColor("#5a5650")
RULE = colors.HexColor("#d9d2c2")

ss = getSampleStyleSheet()
body = ParagraphStyle("body", parent=ss["Normal"], fontName="Helvetica", fontSize=10, leading=14.5, textColor=INK, spaceAfter=6)
small = ParagraphStyle("small", parent=body, fontSize=8.5, leading=11.5, textColor=GREY)
h1 = ParagraphStyle("h1", parent=body, fontName="Helvetica-Bold", fontSize=17, leading=21, spaceBefore=4, spaceAfter=10, textColor=INK)
h2 = ParagraphStyle("h2", parent=body, fontName="Helvetica-Bold", fontSize=12, leading=15, spaceBefore=10, spaceAfter=4, textColor=ACCENT)
cell = ParagraphStyle("cell", parent=body, fontSize=8.5, leading=11, spaceAfter=0)
cellb = ParagraphStyle("cellb", parent=cell, fontName="Helvetica-Bold")
title = ParagraphStyle("title", parent=body, fontName="Helvetica-Bold", fontSize=46, leading=50, alignment=TA_CENTER)
subtitle = ParagraphStyle("subtitle", parent=body, fontSize=13, leading=18, alignment=TA_CENTER, textColor=GREY)
brand = ParagraphStyle("brand", parent=body, fontName="Helvetica-Bold", fontSize=12, leading=16, alignment=TA_CENTER, textColor=ACCENT)


def bullets(items):
    return [Paragraph("•&nbsp;&nbsp;" + t, ParagraphStyle("b", parent=body, leftIndent=12, firstLineIndent=-10)) for t in items]


def table(rows, widths, header=True):
    data = [[Paragraph(str(c), cellb if (header and i == 0) else cell) for c in r] for i, r in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, -1), 0.4, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]
    if header:
        style += [("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#efe9dc")), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACCENT)]
    t.setStyle(TableStyle(style))
    return t


# Parameter reference: number, name, range, what it does.
PARAMS = [
    ("DCO 1", [
        ("11", "Octave", "1-3", "Pitch register of DCO 1: 1 low (16'), 2 middle, 3 high."),
        ("12", "Waveform", "1-2", "1 square. 2 the stepped saw-like mix (footages weighted 1, 1/2, 1/4, 1/8)."),
        ("13-16", "16' / 8' / 4' / 2'", "0-1", "Switch each square-wave footage on or off. Combine them to build the tone."),
        ("17", "Level", "0-31", "DCO 1 volume."),
        ("18", "Mode", "1-2", "1 WHOLE: 8 voices, DCO 1 only. 2 DOUBLE: 4 voices, DCO 1 + DCO 2."),
    ]),
    ("DCO 2 (heard in DOUBLE mode)", [
        ("21", "Octave", "1-3", "Pitch register of DCO 2."),
        ("22", "Waveform", "1-2", "As P12."),
        ("23-26", "16' / 8' / 4' / 2'", "0-1", "DCO 2 footages."),
        ("27", "Level", "0-31", "DCO 2 volume."),
        ("31", "Interval", "0-12", "DCO 2 transposition in semitones above DCO 1."),
        ("32", "Detune", "0-3", "Detunes DCO 2 downward, up to about -20 cents."),
    ]),
    ("Noise", [
        ("33", "Noise level", "0-15", "White noise into the filter, shaped by DEG 3. At 15 it is about as loud as one full voice."),
    ]),
    ("VCF (one filter shared by all voices)", [
        ("41", "Cutoff", "0-99", "Filter cutoff frequency."),
        ("42", "Resonance", "0-15", "Emphasis at the cutoff. Very high settings ring and whistle."),
        ("43", "Keyboard track", "0-2", "0 off, 1 half, 2 full: the cutoff rises with the highest note played (full: one octave per octave, from middle C)."),
        ("44", "EG polarity", "1-2", "1 inverted, 2 normal direction of the DEG 3 sweep."),
        ("45", "EG intensity", "0-15", "How far DEG 3 sweeps the cutoff."),
        ("46", "Trigger", "1-2", "1 single: DEG 3 restarts only when all keys are released. 2 multi: every note restarts it."),
        ("48", "Effect", "0-1", "Turns the stereo effect on or off (Chorus, Phaser or Ensemble, chosen with TYPE)."),
    ]),
    ("DEG 1 / DEG 2 / DEG 3 (six-stage envelopes)", [
        ("51-56", "DEG 1", "0-31", "Volume envelope of DCO 1: Attack, Decay, Break point, Slope, Sustain, Release."),
        ("61-66", "DEG 2", "0-31", "Volume envelope of DCO 2, same stages."),
        ("71-76", "DEG 3", "0-31", "Filter and noise envelope, same stages."),
    ]),
    ("MG (modulation generator)", [
        ("81", "Frequency", "0-15", "MG speed (16 steps from the original ROM table)."),
        ("82", "Delay", "0-15", "Time before the MG fades in after a note starts."),
        ("83", "DCO depth", "0-15", "Vibrato depth, up to about +/-160 cents."),
        ("84", "VCF depth", "0-15", "Filter modulation depth."),
    ]),
]


def build():
    DOCS.mkdir(parents=True, exist_ok=True)
    names = {r["program"]: r["name"] for r in csv.DictReader((ROOT / "factory" / "poly800-mk1-catalog.csv").open())}

    def on_page(c, doc):
        if doc.page == 1:
            return
        c.saveState()
        c.setStrokeColor(RULE)
        c.line(18 * mm, 14 * mm, A4[0] - 18 * mm, 14 * mm)
        c.setFont("Helvetica", 8)
        c.setFillColor(GREY)
        c.drawString(18 * mm, 9 * mm, f"P-800 v{VERSION}  ·  Ticco Ross")
        c.drawRightString(A4[0] - 18 * mm, 9 * mm, str(doc.page))
        c.restoreState()

    doc = SimpleDocTemplate(str(OUT), pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm,
                            topMargin=18 * mm, bottomMargin=20 * mm,
                            title="P-800 Manual", author="Ticco Ross", subject="P-800 AU/VST3 synthesizer")
    W = A4[0] - 36 * mm
    s = []

    # Cover.
    s += [Spacer(1, 40 * mm), Paragraph("TICCO ROSS", brand), Spacer(1, 4 * mm),
          Paragraph("P-800", title), Spacer(1, 3 * mm),
          Paragraph("8-voice programmable synthesizer · AU / VST3", subtitle),
          Paragraph(f"User manual · version {VERSION}", subtitle), Spacer(1, 14 * mm)]
    panel = DOCS / "panel.png"
    if panel.exists():
        s.append(Image(str(panel), width=W, height=W * 800 / 1440))
    s += [Spacer(1, 12 * mm), Paragraph(
        "A reconstruction of a classic 1980s digital-analog hybrid: eight voices of stacked square-wave "
        "footages, three six-stage envelopes, one shared resonant filter, and stereo chorus, phaser and ensemble.", subtitle), PageBreak()]

    # 1. Getting started.
    s += [Paragraph("1. Getting started", h1), Paragraph("Installing", h2)]
    s += bullets([
        "Run the <b>P-800 .pkg</b> installer. It installs the Audio Unit to <i>/Library/Audio/Plug-Ins/Components</i> "
        "and the VST3 to <i>/Library/Audio/Plug-Ins/VST3</i>.",
        "The installer is not yet signed by Apple. If macOS blocks it, right-click the .pkg and choose <b>Open</b>, "
        "or allow it under <b>System Settings › Privacy &amp; Security</b>.",
        "In Ableton Live, P-800 appears under <b>Plug-ins › Audio Units › Ticco Ross</b> (or VST3). "
        "Rescan plug-ins if it does not show up.",
        "P-800 runs at any sample rate. Below 88 kHz it runs its engine at twice the host rate internally, so it "
        "sounds the same at 44.1, 48 and 96 kHz.",
    ])
    s += [Paragraph("First sounds", h2), Paragraph(
        "P-800 opens on factory program <b>11 Strings</b>. Step through the 64 factory programs with the "
        "<b>&lt; PROG</b> and <b>PROG &gt;</b> buttons under the display, from your host's preset menu, or by "
        "typing a program number on the keypad (section 3).", body)]

    # 2. The panel.
    s += [Paragraph("2. The panel", h1)]
    if panel.exists():
        s.append(Image(str(panel), width=W, height=W * 800 / 1440))
        s.append(Spacer(1, 4 * mm))
    s.append(table([
        ["Section", "Controls"],
        ["Joystick / Tune", "<b>Tune</b> slider (±100 cents, double-click to reset). <b>Joystick</b>: left/right bends pitch, "
                            "up adds vibrato (MG to DCO), down adds filter wobble (MG to VCF). It springs back when released."],
        ["Volume", "Master output level."],
        ["Key assign", "<b>POLY</b>: normal playing. <b>CHORD</b>: chord memory, each key plays a stored chord (section 4). "
                       "<b>HOLD</b>: notes keep sounding after you let go (the sustain pedal does the same). "
                       "<b>CHORD MEMORY</b> stores the keys you are holding as the chord."],
        ["Programmer", "LED display (program, parameter, value), keypad, PROG/PARA key, <b>DOWN</b> / <b>UP</b>."],
        ["Performance / Effects", "<b>VOICES</b> switch (8 voices WHOLE / 4 voices DOUBLE, same as P18), <b>EFFECT</b> on/off (P48), "
                                  "effect <b>TYPE</b> (Chorus, Phaser or Ensemble) and the <b>SPEED</b> slider (chorus or phaser rate, or ensemble intensity)."],
        ["Presets", "<b>BANK</b> (click to choose a bank), the preset name (click for the bank's list), "
                    "<b>&lt;</b> / <b>&gt;</b> to step through it, <b>SAVE</b>, <b>NEW BANK</b>, <b>DELETE</b> and the "
                    "favourite <b>star</b> (section 4)."],
        ["Sequencer", "<b>SEQUENCER</b> swaps the parameter chart for the step sequencer screen (section 5); press it again "
                      "for the chart. <b>PLAY</b> starts the sequencer and reads <b>STOP</b> while it runs."],
        ["Parameter chart", "Every sound parameter, laid out like the printed chart on the original. Each cell shows its name, "
                            "number, range and current value."],
    ], [32 * mm, W - 32 * mm]))

    # 3. Editing.
    s += [PageBreak(), Paragraph("3. Editing sounds", h1), Paragraph(
        "The original instrument has no knobs: you pick a parameter by number and change its value. P-800 keeps that "
        "workflow and adds direct mouse editing on the chart. All methods change the same parameters.", body),
        Paragraph("On the chart", h2)]
    s += bullets([
        "<b>Click</b> a cell to select it. Its number and value appear on the LED display.",
        "<b>Drag</b> a cell up or down to change its value, or use the <b>scroll wheel</b> over it.",
        "Press <b>UP</b> / <b>DOWN</b> to step the selected parameter one value at a time.",
    ])
    s += [Paragraph("With the keypad", h2)]
    s += bullets([
        "The keypad has the digits 1-8, as on the original, plus the <b>PROG / PARA</b> key.",
        "In <b>PARA</b> mode, type two digits to select a parameter: <b>4</b> then <b>1</b> selects P41 Cutoff.",
        "Press <b>PROG/PARA</b> to switch to <b>PROG</b> mode. Two digits now load a factory program: <b>2</b> then <b>4</b> loads 24 “1999”.",
        "After the first digit the display shows it with a dash, waiting for the second.",
    ])
    s += [Paragraph("Saving your sounds", h2), Paragraph(
        "Your edits are stored with your Live set like any plug-in. To keep a sound for other projects, save it as a "
        "P-800 preset (section 4, Presets and banks).", body)]

    # 4. Playing.
    s += [Paragraph("4. Playing", h1), Paragraph("Joystick and controllers", h2)]
    s += bullets([
        "<b>Joystick X</b> bends pitch by up to ±<b>Bend Range</b> semitones (1-12, default 2; set it from your host's parameter list).",
        "<b>Joystick up</b> adds MG vibrato to the DCOs; <b>down</b> adds MG movement to the filter. Both add to the "
        "program's own MG depths (P83/P84) and follow the MG speed (P81).",
        "Your keyboard's <b>pitch wheel</b> adds to joystick X, and its <b>mod wheel</b> (CC1) works like pushing the joystick up.",
        "Joystick X and Y are host parameters, so you can automate them.",
    ])
    s += [Paragraph("Presets and banks", h2), Paragraph(
        "Presets are organised in banks, like the programs of the original. The preset section is under the effect "
        "speed slider:", body)]
    s += bullets([
        "<b>Factory</b> is the bank of the 64 original programs, 11 to 88. It is built in and cannot be changed.",
        "<b>User</b> is the first bank for your own sounds. Make more with <b>NEW BANK</b>, one per project or style.",
        "<b>Favourites</b> lists every preset you have starred, from any bank.",
        "Click <b>BANK</b> to choose a bank (its first preset loads). Click the preset name for the full list of the "
        "bank, or step through it with <b>&lt;</b> and <b>&gt;</b> (the &lt; PROG and PROG &gt; buttons do the same).",
        "Presets in every bank are numbered like the hardware: 11-18, 21-28 up to 88. The PROGRAM NO. display shows "
        "the number, and in PROG mode the keypad loads a preset by number from the bank shown.",
        "<b>SAVE</b> stores the current sound: give it a name and choose the bank. Saving under an existing name asks "
        "before replacing it. A preset stores all the sound parameters, the effect type and speed, the bend range, "
        "the key assign mode, the chord memory and the sequencer's eight patterns (not volume, tune or the joystick). "
        "Loading a factory program keeps the patterns you have.",
        "<b>DELETE</b> removes the current preset (user banks only). The bank menu can delete a whole bank, open a "
        "preset file from anywhere, or show the preset folder.",
        "The <b>star</b> adds or removes the current preset from Favourites.",
    ])
    s += [Paragraph(
        "Preset files (.p800preset) are kept in one folder per bank. macOS: <i>~/Library/Audio/Presets/Ticco Ross/P-800</i>. "
        "Windows: <i>Documents\\Ticco Ross\\P-800\\Presets</i>. Linux: <i>~/.local/share/Ticco Ross/P-800/Presets</i>. "
        "Copy a bank folder there to share it.", body)]
    s += [Paragraph("Chord memory", h2), Paragraph(
        "Like the original's chord memory, CHORD mode plays a whole stored chord from a single key. It works the same "
        "way as in Polytwelve:", body)]
    s += bullets([
        "Hold the chord you want (two or more keys) and press <b>CHORD</b>. The keys are stored, lowest note first.",
        "Or, already in CHORD mode, hold the keys and press <b>CHORD MEMORY</b>. With HOLD on you can play the notes "
        "one at a time first, then press CHORD MEMORY.",
        "Now each key plays the stored chord transposed to it. The most recent key wins (last-note priority); release "
        "it and the chord returns to the key still held.",
        "The stored chord and its name appear in the top strip, and it is saved with your session. Pressing CHORD "
        "with fewer than two keys held keeps the previous chord.",
        "A chord can have up to 8 notes in WHOLE mode; in DOUBLE mode (4 voices) its lowest 4 notes play.",
        "Press <b>POLY</b> to go back to normal playing (POLY also switches HOLD off).",
    ])
    s += [Paragraph("Effects", h2), Paragraph(
        "P-800 has the same three stereo effects as Polytwelve. P48 or the EFFECT switch turns the effect on and off; "
        "<b>TYPE</b> chooses it:", body)]
    s += bullets([
        "<b>Chorus</b> (default): a BBD-style stereo chorus. SPEED sets its rate, about 0.1 to 4 Hz.",
        "<b>Phaser</b>: a six-stage stereo phaser with feedback. SPEED sets its rate, about 0.05 to 6 Hz.",
        "<b>Ensemble</b>: a lush three-voice string ensemble. The slider sets its intensity.",
    ])
    s += [Paragraph("Voices and the shared filter", h2), Paragraph(
        "WHOLE mode gives 8 voices with DCO 1 only; DOUBLE mode gives 4 voices, each with DCO 1 and DCO 2 for fatter, "
        "detuned or interval sounds. All voices go through <b>one shared filter</b>, as on the original, so a new note "
        "can retrigger the filter envelope for chords already sounding (see P46 Trigger). Keyboard tracking follows the "
        "highest note held.", body)]

    # 5. Parameter reference.
    # 5. Sequencer.
    s += [PageBreak(), Paragraph("5. Step sequencer", h1), Paragraph(
        "A polyphonic step sequencer of 16 or 32 steps, with eight patterns (A to H) and parameter locks. Press "
        "<b>SEQUENCER</b> to show it in place of the parameter chart, and <b>PLAY</b> to run it. It keeps running "
        "when you go back to the chart.", body)]
    seq = DOCS / "sequencer.png"
    if seq.exists():
        s.append(Image(str(seq), width=W, height=W * 800 / 1440))
        s.append(Spacer(1, 3 * mm))
    s += [Paragraph("Top bar", h2)]
    s += bullets([
        "<b>LENGTH</b> 16 or 32 steps. Going from 16 to 32 with an empty second half copies the first half into it.",
        "<b>PATTERN</b> A to H. Patterns that hold something read brighter. Switching pattern while playing carries on from the same step.",
        "<b>RATE</b> (click): step length, 1/8, 1/16, 1/32, 1/8T or 1/16T. <b>SWING</b> 50% (straight) to 75% delays every "
        "second step. <b>GATE</b> is how much of its last step a note sounds. Drag up/down or scroll to change them.",
        "<b>SYNC</b>: <b>HOST</b> follows your DAW's tempo and transport (the sequencer plays while the DAW plays, if PLAY is on); "
        "<b>INT</b> runs on its own clock at <b>BPM</b>, from step 1 when you press PLAY.",
        "<b>OCT</b> scrolls the grid by an octave. <b>REC</b> records from your keyboard (below). <b>COPY</b> copies the "
        "pattern and becomes <b>PASTE</b>: choose another pattern and press it. <b>CLEAR</b> empties the pattern.",
    ])
    s += [Paragraph("Notes", h2)]
    s += bullets([
        "The grid shows two octaves; each column is a step. <b>Click</b> an empty cell to add a note, <b>drag</b> to the "
        "right to make it longer, and click a note to remove it.",
        "Steps are polyphonic: up to 8 notes each, the synth's voice count (in DOUBLE mode the synth has 4 voices, so "
        "bigger chords lose notes).",
        "Clicking a step (or its number) selects it, outlined in the numbers row.",
        "<b>Recording</b>: with REC on and the sequencer stopped, play a note or chord: it goes on the selected step, and "
        "when you let go of all keys the selection moves to the next step. While playing, notes you play are added to "
        "the nearest step.",
    ])
    s += [Paragraph("Lanes: probability, gate and parameter locks", h2)]
    s += bullets([
        "<b>PROB</b>: the chance (0-100%) that a step plays. Drag a bar; right-click or double-click resets it to 100%.",
        "<b>GATE</b>: how long the step's notes sound, as a percentage of the global GATE.",
        "<b>P-LOCK</b>: choose a parameter at the top of the lane (any chart parameter; the menu marks those with locks), "
        "then drag a bar to lock that parameter to a value on that step. Steps without a lock show a dash and use the "
        "panel value. Right-click or double-click removes a lock.",
        "A step can lock several parameters: choose another parameter and draw its values. A red dot on a step number "
        "means the step has locks. Locks hold for the length of their step.",
    ])
    s += [Paragraph(
        "The Poly-800 has no velocity, so notes always play at the same level; use the filter and envelope parameters "
        "(or locks on them) for accents.", small)]

    s += [PageBreak(), Paragraph("6. Parameter reference", h1)]
    for section, rows in PARAMS:
        s.append(KeepTogether([Paragraph(section, h2),
                               table([["No.", "Name", "Range", "What it does"]] + [list(r) for r in rows],
                                     [16 * mm, 32 * mm, 16 * mm, W - 64 * mm])]))
    s += [Paragraph("Envelope stages (DEG 1-3)", h2), Paragraph(
        "Each DEG rises at the <b>Attack</b> rate to full level, falls at the <b>Decay</b> rate to the <b>Break point</b> "
        "level, then moves at the <b>Slope</b> rate toward the <b>Sustain</b> level, where it stays while the key is held. "
        "On release it falls at the <b>Release</b> rate. The break point and slope give the two-part decays typical of this instrument.", body)]
    s.append(table([
        ["Host-only parameter", "Range", "What it does"],
        ["Volume", "0-1", "Master output (the panel fader)."],
        ["Tune", "±100 cents", "Fine tuning (the panel slider)."],
        ["Joystick X / Joystick Y", "-1 to +1", "The panel joystick; automatable."],
        ["Bend Range", "1-12 st", "How far joystick X and the pitch wheel bend."],
        ["Key Assign", "Poly / Chord", "POLY or CHORD (chord memory)."],
        ["Hold", "off / on", "Key assign HOLD (sustain pedal too)."],
        ["Effect Type", "Chorus / Phaser / Ensemble", "Which effect P48 switches on."],
        ["Effect Speed / Intensity", "0-1", "Chorus or Phaser rate, or Ensemble intensity."],
    ], [45 * mm, 25 * mm, W - 70 * mm]))

    # 6. Factory programs.
    s += [PageBreak(), Paragraph("7. Factory programs", h1), Paragraph(
        "The 64 original factory programs, decoded from a preserved factory cassette. Numbers are bank (1-8) "
        "and program (1-8), as on the original.", body)]
    progs = list(names.items())
    rows = []
    for i in range(32):
        a = progs[i]
        b = progs[i + 32]
        rows.append([f"<b>{a[0]}</b>", a[1], f"<b>{b[0]}</b>", b[1]])
    s.append(table(rows, [12 * mm, W / 2 - 12 * mm, 12 * mm, W / 2 - 12 * mm], header=False))

    # 7. About.
    s += [PageBreak(), Paragraph("8. About P-800", h1), Paragraph(
        "P-800 is built on <b>ISLA Poly-800</b> by Interspock, an open-source reconstruction of the Korg Poly-800 MkI / "
        "EX-800 that recovers its modulation generator, envelope tables and factory data from the original firmware and "
        "cassette. The filter and parts of the signal path derive from Nick Copeland's Bristol synthesizer emulator.", body),
        Paragraph("Differences from the reconstruction it is built on", h2)]
    s += bullets([
        "Runs internally at 88.2/96 kHz at lower host rates, because the engine's lower-rate filter branch becomes "
        "unstable at full resonance.",
        "Adds joystick bend and modulation, HOLD, the hardware-style panel, and the chord memory and "
        "chorus, phaser and ensemble from Polytwelve (Ticco Ross).",
        "Keyboard tracking (P43) adds to the cutoff instead of replacing it, so P41 Cutoff works at every tracking "
        "setting, as on the original instrument.",
        "Noise (P33) is mixed at the oscillators' level; in the reconstruction it was about 19 dB too quiet to hear.",
        "Preset banks, with the original programs as the Factory bank.",
        "A new polyphonic step sequencer with parameter locks replaces the original's simple sequencer (P86-P88).",
        "Not included: cassette or MIDI SysEx program transfer.",
    ])
    s += [Paragraph("Licence", h2), Paragraph(
        "P-800 is free software under the <b>GNU General Public License v3.0 or later</b>. You may use, study, share and "
        "change it under that licence. The complete source code is at <b>github.com/Spunkmeyer77/Poly800</b>.", body),
        Paragraph("Korg and Poly-800 are trademarks of Korg Inc. P-800 is an independent project and is not affiliated "
                  "with or endorsed by Korg.", small)]

    doc.build(s, onFirstPage=on_page, onLaterPages=on_page)
    print(f"wrote {OUT}")


if __name__ == "__main__":
    build()
