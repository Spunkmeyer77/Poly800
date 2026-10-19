#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds plugin/docs/P-800 Manual.pdf (reportlab). Run from the repo root."""
import csv
import pathlib

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
VERSION = "0.1"

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
        ("33", "Noise level", "0-15", "White noise into the filter, shaped by DEG 3."),
    ]),
    ("VCF (one filter shared by all voices)", [
        ("41", "Cutoff", "0-99", "Filter cutoff frequency."),
        ("42", "Resonance", "0-15", "Emphasis at the cutoff. Very high settings ring and whistle."),
        ("43", "Keyboard track", "0-2", "0 off, 1 half, 2 full: cutoff follows the highest note played."),
        ("44", "EG polarity", "1-2", "1 inverted, 2 normal direction of the DEG 3 sweep."),
        ("45", "EG intensity", "0-15", "How far DEG 3 sweeps the cutoff."),
        ("46", "Trigger", "1-2", "1 single: DEG 3 restarts only when all keys are released. 2 multi: every note restarts it."),
        ("48", "Chorus", "0-1", "Stereo BBD-style chorus on or off."),
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
        "footages, three six-stage envelopes, one shared resonant filter and a stereo chorus.", subtitle), PageBreak()]

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
        ["Key assign", "<b>POLY</b>: normal playing. <b>HOLD</b>: notes keep sounding after you let go, until you press POLY."],
        ["Programmer", "LED display (program, parameter, value), keypad, PROG/PARA key, <b>DOWN</b> / <b>UP</b>."],
        ["Performance", "<b>VOICES</b> switch (8 voices WHOLE / 4 voices DOUBLE, same as P18) and <b>CHORUS</b> on/off (P48)."],
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
        "preset in your host (in Live: the save button in the device title bar). Loading a factory program replaces "
        "the current edits.", body)]

    # 4. Playing.
    s += [Paragraph("4. Playing", h1), Paragraph("Joystick and controllers", h2)]
    s += bullets([
        "<b>Joystick X</b> bends pitch by up to ±<b>Bend Range</b> semitones (1-12, default 2; set it from your host's parameter list).",
        "<b>Joystick up</b> adds MG vibrato to the DCOs; <b>down</b> adds MG movement to the filter. Both add to the "
        "program's own MG depths (P83/P84) and follow the MG speed (P81).",
        "Your keyboard's <b>pitch wheel</b> adds to joystick X, and its <b>mod wheel</b> (CC1) works like pushing the joystick up.",
        "Joystick X and Y are host parameters, so you can automate them.",
    ])
    s += [Paragraph("Voices and the shared filter", h2), Paragraph(
        "WHOLE mode gives 8 voices with DCO 1 only; DOUBLE mode gives 4 voices, each with DCO 1 and DCO 2 for fatter, "
        "detuned or interval sounds. All voices go through <b>one shared filter</b>, as on the original, so a new note "
        "can retrigger the filter envelope for chords already sounding (see P46 Trigger). Keyboard tracking follows the "
        "highest note held.", body)]

    # 5. Parameter reference.
    s += [PageBreak(), Paragraph("5. Parameter reference", h1)]
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
        ["Hold", "off / on", "Key assign HOLD."],
    ], [45 * mm, 25 * mm, W - 70 * mm]))

    # 6. Factory programs.
    s += [PageBreak(), Paragraph("6. Factory programs", h1), Paragraph(
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
    s += [PageBreak(), Paragraph("7. About P-800", h1), Paragraph(
        "P-800 is built on <b>ISLA Poly-800</b> by Interspock, an open-source reconstruction of the Korg Poly-800 MkI / "
        "EX-800 that recovers its modulation generator, envelope tables and factory data from the original firmware and "
        "cassette. The filter and parts of the signal path derive from Nick Copeland's Bristol synthesizer emulator.", body),
        Paragraph("Differences from the reconstruction it is built on", h2)]
    s += bullets([
        "Runs internally at 88.2/96 kHz at lower host rates, because the engine's lower-rate filter branch becomes "
        "unstable at full resonance.",
        "Adds joystick bend and modulation, HOLD, and the hardware-style panel.",
        "Not included: the original's sequencer and chord memory (P86-P88 and KEY ASSIGN CHORD), and cassette or "
        "MIDI SysEx program transfer.",
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
