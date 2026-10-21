#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds plugin/docs/P-800 Update History.pdf (reportlab). Run from the repo root.
Add a new entry to HISTORY at every release, together with the manual update."""
import pathlib
import re

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import KeepTogether, Paragraph, SimpleDocTemplate, Spacer

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "plugin" / "docs" / "P-800 Update History.pdf"
VERSION = re.search(r"project\(P800 VERSION ([0-9.]+)", (ROOT / "plugin" / "CMakeLists.txt").read_text()).group(1)

ACCENT = colors.HexColor("#c8741c")
INK = colors.HexColor("#1d1d1f")
GREY = colors.HexColor("#5a5650")

ss = getSampleStyleSheet()
body = ParagraphStyle("body", parent=ss["Normal"], fontName="Helvetica", fontSize=10, leading=14.5, textColor=INK, spaceAfter=4)
sub = ParagraphStyle("sub", parent=body, leftIndent=34, firstLineIndent=-10, spaceAfter=2)
item = ParagraphStyle("item", parent=body, leftIndent=20, firstLineIndent=-20)
h2 = ParagraphStyle("h2", parent=body, fontName="Helvetica-Bold", fontSize=13, leading=16, spaceBefore=12, spaceAfter=4, textColor=ACCENT)
title = ParagraphStyle("title", parent=body, fontName="Helvetica-Bold", fontSize=26, leading=30, alignment=TA_CENTER)
subtitle = ParagraphStyle("subtitle", parent=body, fontSize=11, leading=15, alignment=TA_CENTER, textColor=GREY)

# (version heading, [ (text, [sub-points]) ... ])  -- items are numbered across the whole history.
HISTORY = [
    ("0.1.0: First build", [
        ("The first playable plugin (AU/VST3, then called TRoss 800), wrapped around the ISLA Poly-800 engine. "
         "It runs the engine at twice the sample rate below 88 kHz so the filter stays stable, and it loads the 64 factory programs.", []),
        ("Renamed the plugin to Poly800.", []),
        ("Renamed it again to <b>P-800</b> (manufacturer Ticco Ross).", []),
        ("Added the joystick. X bends pitch with an adjustable bend range, Y up adds vibrato and Y down adds filter wobble. "
         "The MIDI pitch wheel and mod wheel are mapped to it too.", []),
        ("Added the hardware-style panel: brushed metal, LED display, keypad, parameter chart, joystick, volume fader and tune slider. "
         "Also added HOLD (the sustain pedal works as HOLD).", []),
        ("Added the PDF user manual.", []),
        ("Added the Windows (.exe installer) and Linux (.tar.gz) builds alongside the macOS .pkg.", []),
    ]),
    ("0.2.0: Chord memory and effects", [
        ("Brought over the chord memory, chorus and ensemble from Polytwelve.", []),
        ("Version 0.2.0.", []),
    ]),
    ("0.3.0", [
        ("Effects and filter fixes:", [
            "Replaced the original chorus with the Polytwelve phaser and effects.",
            "Fixed the cutoff: P41 hardly did anything when keyboard tracking was on.",
            "Fixed the cutoff interaction with EG intensity.",
            "Added a label under the speed slider.",
        ]),
    ]),
    ("0.4.0", [
        ("Presets and noise:", [
            "Added preset banks: a Factory bank, user banks, Favourites, save/delete and open file.",
            "Fixed the noise level, which was about 19 dB too quiet to hear.",
        ]),
    ]),
    ("0.5.0", [
        ("Added the step sequencer:", [
            "16 or 32 steps, 8 patterns, up to 8 notes per step.",
            "Rate, swing and gate, with host sync or its own BPM.",
            "Per-step probability and gate, and parameter locks.",
            "Patterns are saved in presets.",
            "New backlit-rubber button design.",
        ]),
    ]),
    ("0.5.1", [
        ("Sequencer fixes:", [
            "The sequencer now records CHORD mode as full chords.",
            "Parameter locks now hold for the whole step (before, they reset almost immediately).",
        ]),
    ]),
    ("0.5.2", [
        ("Sequencer recording and locks:", [
            "Live recording now records held notes at their real length.",
            "New TIE and REST buttons for step recording.",
            "New lockable controls: Effect Speed, Effect Type, Volume, Tune and Joystick X/Y.",
            "The lock area can be resized with the mouse.",
            "The manual is updated.",
        ]),
    ]),
]


def build():
    def on_page(c, doc):
        c.setFont("Helvetica", 8)
        c.setFillColor(GREY)
        c.drawString(18 * mm, 10 * mm, f"P-800 {VERSION} Update History · Ticco Ross")
        c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))

    story = [Paragraph("P-800 Update History", title), Spacer(1, 4),
             Paragraph(f"Ticco Ross · current version {VERSION}", subtitle), Spacer(1, 10)]
    n = 0
    for heading, items in HISTORY:
        block = [Paragraph(heading, h2)]
        for text, subs in items:
            n += 1
            block.append(Paragraph(f"<b>{n}.</b>&nbsp;&nbsp;{text}", item))
            block += [Paragraph("•&nbsp;&nbsp;" + s, sub) for s in subs]
        story.append(KeepTogether(block))
    story += [Spacer(1, 12), Paragraph(
        "From 0.3.0 on, each version also updated the manual and added tests. "
        "Every build is checked on macOS, Windows and Linux.", body)]
    SimpleDocTemplate(str(OUT), pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm,
                      topMargin=18 * mm, bottomMargin=18 * mm, title="P-800 Update History",
                      author="Ticco Ross").build(story, onFirstPage=on_page, onLaterPages=on_page)
    print("wrote", OUT)


if __name__ == "__main__":
    build()
