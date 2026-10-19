// SPDX-License-Identifier: GPL-3.0-or-later
#include "PanelView.h"

namespace p800
{
namespace
{
const juce::Colour cBg       { 0xff101012 };
const juce::Colour cCream    { 0xffe8dfc8 };
const juce::Colour cDim      { 0xffa9a49a };
const juce::Colour cAccent   { 0xffe8902c };
const juce::Colour cLed      { 0xffff3a28 };
const juce::Colour cLedOff   { 0xff3a1512 };
const juce::Colour cSlot     { 0xff0b0b0c };
const juce::Colour cChartBg  { 0xff222326 };
const juce::Colour cDark     { 0xff1d1d1f };

juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font ledFont (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
}

juce::String two (int v) { return v < 10 && v >= 0 ? "0" + juce::String (v) : juce::String (v); }

juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

void drawText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f,
               juce::Colour c, juce::Justification j = juce::Justification::centred)
{
    g.setFont (f);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

// String literals in this file are UTF-8 (arrows, flat/sharp, middle dot).
void drawText (juce::Graphics& g, const char* s, juce::Rectangle<float> r, const juce::Font& f,
               juce::Colour c, juce::Justification j = juce::Justification::centred)
{
    drawText (g, utf8 (s), r, f, c, j);
}

// Red LED digits with a soft glow.
void drawLed (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float size, juce::Colour c)
{
    g.setFont (ledFont (size));
    g.setColour (c.withAlpha (0.18f));
    for (float d : { -2.0f, 2.0f })
    {
        g.drawText (s, r.translated (d, 0), juce::Justification::centred, false);
        g.drawText (s, r.translated (0, d), juce::Justification::centred, false);
    }
    g.setColour (c);
    g.drawText (s, r, juce::Justification::centred, false);
}

void drawSlot (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (cSlot);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (r.getX() + 2, r.getBottom() + 0.5f, r.getRight() - 2, r.getBottom() + 0.5f, 1.0f);
}

void drawCap (juce::Graphics& g, juce::Rectangle<float> r, bool lined = true)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0, 3), 3.0f);
    juce::ColourGradient grad (juce::Colour (0xfff4eedf), r.getX(), r.getY(),
                               juce::Colour (0xffc2baa5), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 3.0f);
    if (lined)
    {
        g.setColour (juce::Colour (0xff2a2a2c));
        g.fillRect (r.getX() + 3, r.getCentreY() - 1.0f, r.getWidth() - 6, 2.0f);
    }
}

// Tall cream key (programmer keys, key assign, up/down), lit when "on".
void drawKey (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour onColour)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 3), 3.0f);
    juce::ColourGradient grad (on ? onColour.brighter (0.25f) : juce::Colour (0xfff4eedf), r.getX(), r.getY(),
                               on ? onColour.darker (0.3f) : juce::Colour (0xffc2baa5), r.getRight(), r.getY(), false);
    grad.addColour (0.45, on ? onColour.brighter (0.4f) : juce::Colour (0xfff7f2e6));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff1c1c1e));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float angle)
{
    const auto r = juce::Rectangle<float> (12, 12).withCentre (c);
    juce::ColourGradient grad (juce::Colour (0xffd0d0d4), r.getX() + 3, r.getY() + 3,
                               juce::Colour (0xff3a3a3d), r.getRight(), r.getBottom(), true);
    g.setGradientFill (grad);
    g.fillEllipse (r);
    g.setColour (juce::Colour (0xff2a2a2c));
    juce::Path slot;
    slot.addRectangle (-5.0f, -1.0f, 10.0f, 2.0f);
    g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (c));
}

void drawSectionLabel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& s, bool accent)
{
    g.setColour (accent ? cAccent : juce::Colour (0xffd8d0bb));
    g.fillRoundedRectangle (r, 2.0f);
    drawText (g, s, r, uiFont (15.0f, true), juce::Colour (0xff2a2a2c));
}

struct ChartDef { int number; const char* label; const char* hi; const char* lo; };
struct GroupDef { const char* name; std::vector<ChartDef> cells; };

std::vector<GroupDef> rowOne()
{
    return {
        { "DCO 1", { { 11, "OCTAVE", "3 HI", "1 LO" }, { 12, "WAVE", "2 SAW", "1 SQR" }, { 13, "16'", "ON", "OFF" },
                     { 14, "8'", "ON", "OFF" }, { 15, "4'", "ON", "OFF" }, { 16, "2'", "ON", "OFF" }, { 17, "LEVEL", "31", "0" } } },
        { "MODE", { { 18, "DCO", "2 DBL", "1 WHL" } } },
        { "DCO 2", { { 21, "OCTAVE", "3 HI", "1 LO" }, { 22, "WAVE", "2 SAW", "1 SQR" }, { 23, "16'", "ON", "OFF" },
                     { 24, "8'", "ON", "OFF" }, { 25, "4'", "ON", "OFF" }, { 26, "2'", "ON", "OFF" }, { 27, "LEVEL", "31", "0" },
                     { 31, "INTRVL", "12", "0" }, { 32, "DETUNE", "3", "0" } } },
        { "NOISE", { { 33, "LEVEL", "15", "0" } } },
        { "VCF", { { 41, "CUTOFF", "99", "0" }, { 42, "RESO.", "15", "0" }, { 43, "KBD", "2 FULL", "0 OFF" },
                   { 44, "POLAR.", "2 POS", "1 NEG" }, { 45, "EG INT", "15", "0" }, { 46, "TRIG.", "2 MULTI", "1 SNGL" } } },
        { "FX", { { 48, "ON/OFF", "ON", "OFF" } } },
    };
}

std::vector<GroupDef> rowTwo()
{
    auto deg = [] (int b)
    {
        return std::vector<ChartDef> { { b + 1, "ATTACK", "31", "0" }, { b + 2, "DECAY", "31", "0" },
                                       { b + 3, "BRK P.", "31", "0" }, { b + 4, "SLOPE", "31", "0" },
                                       { b + 5, "SUST.", "31", "0" }, { b + 6, "REL.", "31", "0" } };
    };
    return {
        { "DEG 1 \xc2\xb7 DCO 1", deg (50) },
        { "DEG 2 \xc2\xb7 DCO 2", deg (60) },
        { "DEG 3 \xc2\xb7 VCF + NOISE", deg (70) },
        { "MG", { { 81, "FREQ", "15", "0" }, { 82, "DELAY", "15", "0" }, { 83, "DCO", "15", "0" }, { 84, "VCF", "15", "0" } } },
    };
}
} // namespace

int PanelView::indexOfParam (int number)
{
    for (int i = 0; i < numParams; ++i)
        if (params[i].number == number)
            return i;
    return -1;
}

PanelView::PanelView()
{
    buildLayout();

    // Brushed metal, rendered once at design size.
    metal = juce::Image (juce::Image::ARGB, width, height, true);
    juce::Graphics mg (metal);
    juce::ColourGradient base (juce::Colour (0xff5d5e63), 0, panel.getY(),
                               juce::Colour (0xff3d3e42), 0, panel.getBottom(), false);
    base.addColour (0.3, juce::Colour (0xff46474c));
    base.addColour (0.6, juce::Colour (0xff3a3b40));
    base.addColour (0.85, juce::Colour (0xff4b4c51));
    mg.setGradientFill (base);
    mg.fillRoundedRectangle (panel, 6.0f);
    juce::Random rng (800);
    mg.reduceClipRegion (panel.toNearestInt());
    for (float y = panel.getY(); y < panel.getBottom(); y += 1.0f)
    {
        const float a = rng.nextFloat() * 0.06f;
        mg.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
        mg.drawHorizontalLine ((int) y, panel.getX(), panel.getRight());
    }
}

void PanelView::buildLayout()
{
    panel = { 14, 14, 1412, 772 };

    // Left: tune, joystick, volume, key assign.
    tuneTrackR = { 40, 74, 170, 18 };
    joyBox = { 50, 150, 150, 150 };
    volTrack = { 251, 74, 22, 220 };
    polyR = { 310, 110, 36, 150 };
    chordR = { 358, 110, 36, 150 };
    holdR = { 406, 110, 36, 150 };
    memoryR = { 310, 314, 132, 26 };

    // Programmer.
    display = { 462, 56, 600, 112 };
    progPrevR = { 462, 176, 44, 22 };
    progNextR = { 1018, 176, 44, 22 };
    const int layoutDigits[9] = { 7, 8, -1, 4, 5, 6, 1, 2, 3 };
    for (int i = 0; i < 9; ++i)
    {
        keyDigit[(size_t) i] = layoutDigits[i];
        keyR[(size_t) i] = { 462.0f + (float) (i % 3) * 104.0f, 210.0f + (float) (i / 3) * 54.0f, 92.0f, 44.0f };
    }
    downR = { 800, 210, 40, 152 };
    upR = { 860, 210, 40, 152 };

    // Right: performance switches.
    voicesR = { 1100, 80, 90, 64 };
    chorusR = { 1200, 80, 90, 64 };
    for (int i = 0; i < 3; ++i)
        chorusTypeR[(size_t) i] = { 1300.0f, 72.0f + (float) i * 26.0f, 100.0f, 22.0f };
    speedTrack = { 1110, 200, 290, 16 };

    // Parameter chart.
    cells.clear();
    groups.clear();
    const float x0 = 30.0f, rowW = 1380.0f, gap = 6.0f;
    const float rowY[2] = { 422.0f, 602.0f }, rowH = 170.0f;
    const auto rows = { rowOne(), rowTwo() };
    int rowIndex = 0;
    for (const auto& row : rows)
    {
        int nCells = 0;
        for (const auto& g : row) nCells += (int) g.cells.size();
        const float cellW = (rowW - gap * (float) (row.size() - 1)) / (float) nCells;
        float x = x0;
        for (const auto& g : row)
        {
            const float w = cellW * (float) g.cells.size();
            groups.push_back ({ g.name, { x, rowY[rowIndex], w, rowH } });
            for (size_t c = 0; c < g.cells.size(); ++c)
            {
                const auto& d = g.cells[c];
                cells.push_back ({ indexOfParam (d.number), d.label, d.hi, d.lo,
                                   { x + cellW * (float) c, rowY[rowIndex], cellW, rowH - 22.0f } });
            }
            x += w + gap;
        }
        ++rowIndex;
    }
}

Hit PanelView::hitTest (juce::Point<float> p) const
{
    if (joyBox.contains (p)) return { HitKind::joystick };
    if (volTrack.expanded (14, 8).contains (p)) return { HitKind::volume };
    if (tuneTrackR.expanded (4, 8).contains (p)) return { HitKind::tune };
    if (polyR.contains (p)) return { HitKind::poly };
    if (chordR.contains (p)) return { HitKind::chord };
    if (holdR.contains (p)) return { HitKind::hold };
    if (memoryR.contains (p)) return { HitKind::memory };
    for (int i = 0; i < 3; ++i)
        if (chorusTypeR[(size_t) i].contains (p)) return { HitKind::chorusType, i };
    if (speedTrack.expanded (4, 10).contains (p)) return { HitKind::chorusSpeed };
    if (upR.contains (p)) return { HitKind::valueUp };
    if (downR.contains (p)) return { HitKind::valueDown };
    if (progPrevR.contains (p)) return { HitKind::progPrev };
    if (progNextR.contains (p)) return { HitKind::progNext };
    if (voicesR.contains (p)) return { HitKind::voices };
    if (chorusR.contains (p)) return { HitKind::chorus };
    for (size_t i = 0; i < keyR.size(); ++i)
        if (keyR[i].contains (p))
            return keyDigit[i] < 0 ? Hit { HitKind::progPara } : Hit { HitKind::key, keyDigit[i] };
    for (const auto& c : cells)
        if (c.r.contains (p))
            return { HitKind::cell, c.paramIndex };
    return {};
}

void PanelView::paint (juce::Graphics& g, const PanelState& s) const
{
    g.fillAll (cBg);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (panel.translated (0, 8), 8.0f);
    g.drawImageAt (metal, 0, 0);
    g.setColour (juce::Colour (0xff85868b));
    g.drawLine (panel.getX() + 6, panel.getY() + 0.5f, panel.getRight() - 6, panel.getY() + 0.5f, 1.0f);
    g.setColour (juce::Colour (0xff6a6b70));
    g.drawRoundedRectangle (panel, 6.0f, 1.0f);

    drawScrew (g, { panel.getX() + 13, panel.getY() + 13 }, 0.6f);
    drawScrew (g, { panel.getRight() - 13, panel.getY() + 13 }, -0.35f);
    drawScrew (g, { panel.getX() + 13, panel.getBottom() - 13 }, 1.2f);
    drawScrew (g, { panel.getRight() - 13, panel.getBottom() - 13 }, 0.15f);

    // Top strip.
    const auto strip = juce::Rectangle<float> (40, 22, 1335, 22);
    drawText (g, juce::String ("KEY ASSIGN: ") + (s.chordMode ? "CHORD" : "POLY") + (s.hold ? " + HOLD" : "")
                 + utf8 ("   |   CHORD MEMORY: ") + s.chordName + " (" + s.chordNotes + ")"
                 + utf8 ("   |   BEND \xc2\xb1") + juce::String (s.bendRange) + " ST",
              strip, uiFont (13.0f), cDim, juce::Justification::centredLeft);
    drawText (g, s.programName.toUpperCase(), strip, uiFont (14.0f, true), cCream, juce::Justification::centredRight);

    // Tune.
    drawText (g, "\xe2\x99\xad  \xc2\xb7  \xc2\xb7  \xc2\xb7  \xc2\xb7  0  \xc2\xb7  \xc2\xb7  \xc2\xb7  \xc2\xb7  \xe2\x99\xaf",
              tuneTrackR.translated (0, -20).withHeight (16), uiFont (12.0f), cDim);
    drawSlot (g, tuneTrackR);
    const float tuneX = tuneTrackR.getX() + 15 + (s.tune + 100.0f) / 200.0f * (tuneTrackR.getWidth() - 30);
    drawCap (g, juce::Rectangle<float> (30, 24).withCentre ({ tuneX, tuneTrackR.getCentreY() }), false);
    drawText (g, "TUNE", tuneTrackR.translated (0, 24).withHeight (18), uiFont (14.0f, true), cCream);

    // Joystick.
    drawText (g, "DCO MOD \xe2\x96\xb2", joyBox.translated (0, -22).withHeight (16), uiFont (12.0f, true), cCream);
    drawText (g, "VCF MOD \xe2\x96\xbc", joyBox.translated (0, joyBox.getHeight() + 6).withHeight (16), uiFont (12.0f, true), cCream);
    drawText (g, "\xe2\x97\x80", joyBox.translated (-22, 0).withWidth (18), uiFont (12.0f, true), cCream);
    drawText (g, "\xe2\x96\xb6", joyBox.translated (joyBox.getWidth() + 4, 0).withWidth (18), uiFont (12.0f, true), cCream);
    {
        juce::ColourGradient box (juce::Colour (0xff2c2d31), joyBox.getX(), joyBox.getY(),
                                  juce::Colour (0xff18181b), joyBox.getRight(), joyBox.getBottom(), false);
        g.setGradientFill (box);
        g.fillRoundedRectangle (joyBox, 14.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (joyBox.reduced (0.5f), 14.0f, 1.5f);
        g.setColour (juce::Colour (0xff3a3b40));
        g.drawLine (joyBox.getCentreX(), joyBox.getY() + 14, joyBox.getCentreX(), joyBox.getBottom() - 14, 2.0f);
        g.drawLine (joyBox.getX() + 14, joyBox.getCentreY(), joyBox.getRight() - 14, joyBox.getCentreY(), 2.0f);
        const auto boot = juce::Rectangle<float> (100, 100).withCentre (joyBox.getCentre());
        juce::ColourGradient bootGrad (juce::Colour (0xff0a0a0b), boot.getCentreX(), boot.getCentreY(),
                                      juce::Colour (0xff262629), boot.getRight(), boot.getCentreY(), true);
        bootGrad.addColour (0.55, juce::Colour (0xff2a2a2e));
        bootGrad.addColour (0.75, juce::Colour (0xff151517));
        g.setGradientFill (bootGrad);
        g.fillEllipse (boot);
        const auto tip = joyBox.getCentre() + juce::Point<float> (s.joyX * 40.0f, -s.joyY * 40.0f);
        g.setColour (juce::Colour (0xff0c0c0d));
        g.drawLine ({ joyBox.getCentre(), tip }, 11.0f);
        const auto ball = juce::Rectangle<float> (44, 44).withCentre (tip);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (ball.translated (0, 6));
        juce::ColourGradient ballGrad (juce::Colour (0xff9a9ba0), ball.getX() + 14, ball.getY() + 12,
                                      juce::Colour (0xff1c1c1f), ball.getRight(), ball.getBottom(), true);
        ballGrad.addColour (0.35, juce::Colour (0xff4a4b50));
        g.setGradientFill (ballGrad);
        g.fillEllipse (ball);
    }

    // Volume fader.
    drawText (g, "10", volTrack.translated (-10, -20).withSize (42, 16), uiFont (12.0f), cDim);
    drawSlot (g, volTrack);
    const float capY = volTrack.getBottom() - 13 - s.volume * (volTrack.getHeight() - 26);
    drawCap (g, juce::Rectangle<float> (40, 26).withCentre ({ volTrack.getCentreX(), capY }));
    drawText (g, "0", volTrack.translated (-10, volTrack.getHeight() + 4).withSize (42, 16), uiFont (12.0f), cDim);
    drawText (g, "VOLUME", volTrack.translated (-30, volTrack.getHeight() + 22).withSize (82, 18), uiFont (13.0f, true), cCream);

    // Key assign.
    for (auto [r, label, on] : { std::tuple { polyR, "POLY", ! s.chordMode }, std::tuple { chordR, "CHORD", s.chordMode },
                                 std::tuple { holdR, "HOLD", s.hold } })
    {
        g.setColour (on ? cLed : cLedOff);
        g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre ({ r.getCentreX(), r.getY() - 14 }));
        drawKey (g, r, false, cAccent);
        drawText (g, label, r.translated (-12, r.getHeight() + 6).withSize (60, 18), uiFont (13.0f, true), cCream);
    }
    g.setColour (juce::Colour (0xff2b2c2f));
    g.fillRoundedRectangle (memoryR, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (memoryR, 3.0f, 1.0f);
    drawText (g, "CHORD MEMORY", memoryR, uiFont (12.0f, true), s.chordMode ? cAccent : cCream);

    paintDisplay (g, s);

    // Keypad, up/down.
    for (size_t i = 0; i < keyR.size(); ++i)
    {
        const bool isPP = keyDigit[i] < 0;
        drawKey (g, keyR[i], isPP, cAccent);
        drawText (g, isPP ? (s.progEntry ? "PROG" : "PARA") : juce::String (keyDigit[i]), keyR[i],
                  ledFont (isPP ? 16.0f : 20.0f), isPP ? cDark : juce::Colour (0xff3a3530));
    }
    for (auto [r, label, arrow] : { std::tuple { downR, "DOWN", "\xe2\x96\xbc" }, std::tuple { upR, "UP", "\xe2\x96\xb2" } })
    {
        drawKey (g, r, false, cAccent);
        drawText (g, arrow, r, uiFont (16.0f), juce::Colour (0xff3a3530));
        drawText (g, label, r.translated (-10, r.getHeight() + 6).withSize (60, 18), uiFont (14.0f, true), cCream);
    }

    // Performance switches (P18 voices, P48 chorus).
    const int voicesIdx = indexOfParam (18), chorusIdx = indexOfParam (48);
    struct Sw { juce::Rectangle<float> r; const char* label; const char* top; const char* bot; bool up; };
    for (const auto& sw : { Sw { voicesR, "VOICES", "4 DBL", "8 WHL", s.values[(size_t) voicesIdx] == 2 },
                            Sw { chorusR, "EFFECT", "ON", "OFF", s.values[(size_t) chorusIdx] == 1 } })
    {
        const auto slot = juce::Rectangle<float> (22, 56).withPosition (sw.r.getX(), sw.r.getY());
        drawSlot (g, slot);
        drawCap (g, juce::Rectangle<float> (16, 22).withCentre ({ slot.getCentreX(), sw.up ? slot.getY() + 14 : slot.getBottom() - 14 }), false);
        drawText (g, sw.top, { slot.getRight() + 6, slot.getY(), 60, 16 }, uiFont (12.0f), cDim, juce::Justification::centredLeft);
        drawText (g, sw.bot, { slot.getRight() + 6, slot.getBottom() - 16, 60, 16 }, uiFont (12.0f), cDim, juce::Justification::centredLeft);
        drawText (g, sw.label, { sw.r.getX() - 10, slot.getBottom() + 8, 70, 18 }, uiFont (13.0f, true), cCream);
    }
    const char* typeNames[3] = { "CHORUS", "PHASER", "ENSEMBLE" };
    for (int i = 0; i < 3; ++i)
    {
        const auto& r = chorusTypeR[(size_t) i];
        const bool sel = s.chorusType == i;
        g.setColour (sel ? cAccent : juce::Colour (0xff2b2c2f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        drawText (g, typeNames[i], r, uiFont (11.0f, true), sel ? cDark : cCream);
    }
    drawText (g, "TYPE", { 1300, 152, 100, 16 }, uiFont (12.0f, true), cCream);

    drawSlot (g, speedTrack);
    const float spX = speedTrack.getX() + 15 + s.chorusSpeed * (speedTrack.getWidth() - 30);
    drawCap (g, juce::Rectangle<float> (30, 22).withCentre ({ spX, speedTrack.getCentreY() }), false);
    const char* speedNames[3] = { "CHORUS SPEED", "PHASER SPEED", "ENSEMBLE INTENSITY" };
    drawText (g, speedNames[juce::jlimit (0, 2, s.chorusType)], speedTrack.translated (0, 22).withHeight (18),
              uiFont (13.0f, true), cCream);

    // Section labels.
    drawSectionLabel (g, { 34, 386, 190, 22 }, "JOYSTICK / TUNE", false);
    drawSectionLabel (g, { 232, 386, 64, 22 }, "VOL", false);
    drawSectionLabel (g, { 304, 386, 146, 22 }, "KEY ASSIGN", false);
    drawSectionLabel (g, { 462, 386, 600, 22 }, utf8 ("PROGRAMMER \xc2\xb7 ") + (s.progEntry ? "PROGRAM ENTRY" : "PARAMETER ENTRY"), true);
    drawSectionLabel (g, { 1090, 386, 320, 22 }, "PERFORMANCE / EFFECTS", false);

    paintChart (g, s);
}

void PanelView::paintDisplay (juce::Graphics& g, const PanelState& s) const
{
    g.setColour (juce::Colour (0xff060606));
    g.fillRoundedRectangle (display, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (display, 4.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawLine (display.getX() + 4, display.getBottom() + 1, display.getRight() - 4, display.getBottom() + 1, 1.0f);

    drawText (g, "TICCO ROSS", { display.getX() + 20, display.getY() + 22, 200, 18 }, uiFont (14.0f, true), cAccent,
              juce::Justification::centredLeft);
    drawText (g, "P-800", { display.getX() + 18, display.getY() + 42, 200, 46 }, uiFont (40.0f, true),
              juce::Colour (0xffd8d0bb), juce::Justification::centredLeft);
    drawText (g, "v" + s.version, { display.getX() + 20, display.getY() + 88, 200, 14 }, uiFont (11.0f), cDim,
              juce::Justification::centredLeft);

    const juce::String buf = s.entryBuffer.isEmpty() ? juce::String() : s.entryBuffer + "-";
    const int sel = s.selectedParam;
    const juce::String leds[3] = {
        s.progEntry && buf.isNotEmpty() ? buf : two (s.programNumber),
        ! s.progEntry && buf.isNotEmpty() ? buf : two (params[sel].number),
        two (s.values[(size_t) sel]),
    };
    const char* names[3] = { "PROGRAM NO.", "PARAMETER NO.", "VALUE" };
    for (int i = 0; i < 3; ++i)
    {
        const auto col = juce::Rectangle<float> (display.getX() + 240 + (float) i * 120, display.getY() + 8, 110, 96);
        drawLed (g, leds[i], col.withHeight (66), 54.0f, cLed);
        drawText (g, names[i], col.withTrimmedTop (70), uiFont (12.0f, true), juce::Colour (0xffd8d0bb));
    }

    for (auto [r, t] : { std::pair { progPrevR, "\xe2\x97\x80 PROG" }, std::pair { progNextR, "PROG \xe2\x96\xb6" } })
    {
        g.setColour (juce::Colour (0xff2b2c2f));
        g.fillRoundedRectangle (r, 3.0f);
        drawText (g, t, r, uiFont (11.0f, true), cCream);
    }
}

void PanelView::paintChart (juce::Graphics& g, const PanelState& s) const
{
    const auto chart = juce::Rectangle<float> (22, 414, 1396, 364);
    g.setColour (cChartBg);
    g.fillRoundedRectangle (chart, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (chart, 4.0f, 1.5f);

    for (const auto& c : cells)
    {
        const auto& spec = params[c.paramIndex];
        const bool sel = c.paramIndex == s.selectedParam;
        auto r = c.r;
        auto labelR = r.removeFromTop (20);
        auto numR = r.removeFromTop (34);
        if (sel)
        {
            g.setColour (cAccent);
            g.fillRect (labelR.getUnion (numR));
            g.setColour (juce::Colour (0xff3a2a18));
            g.fillRect (r);
        }
        drawText (g, c.label, labelR, uiFont (12.0f, true), sel ? cDark : cCream);
        drawText (g, juce::String (spec.number), numR, ledFont (26.0f), sel ? cDark : cAccent);
        drawText (g, c.hi, r.removeFromTop (18), uiFont (11.0f), cDim);
        drawText (g, c.lo, r.removeFromBottom (18), uiFont (11.0f), cDim);
        drawLed (g, two (s.values[(size_t) c.paramIndex]), r, 22.0f, cLed);

        g.setColour (cAccent);
        g.drawLine (c.r.getX(), labelR.getBottom(), c.r.getRight(), labelR.getBottom(), 1.0f);
        g.drawLine (c.r.getX(), numR.getBottom(), c.r.getRight(), numR.getBottom(), 1.0f);
        g.drawLine (c.r.getX(), c.r.getY(), c.r.getX(), c.r.getBottom(), 1.0f);
    }

    for (const auto& gr : groups)
    {
        g.setColour (cAccent);
        g.drawRect (gr.r, 1.0f);
        const auto bar = gr.r.withTop (gr.r.getBottom() - 22);
        g.fillRect (bar);
        drawText (g, gr.name, bar, uiFont (15.0f, true), cDark);
    }
}
} // namespace p800
