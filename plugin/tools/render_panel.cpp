// SPDX-License-Identifier: GPL-3.0-or-later
// Renders the P-800 panel to a PNG without a host or window, for checking the
// look. Usage: render_panel out.png [scale]
#include <juce_graphics/juce_graphics.h>
#include "PanelView.h"
#include "FactoryPrograms.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI* noGui = nullptr; // not needed: graphics only
    juce::ignoreUnused (noGui);
    if (argc < 2)
        return 1;
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;

    p800::PanelView view;
    p800::PanelState s;
    for (int i = 0; i < p800::numParams; ++i)
        s.values[(size_t) i] = factory::programs[0].values[i];
    s.programNumber = factory::programs[0].number;
    s.programName = factory::programs[0].name;
    s.joyX = 0.35f;
    s.joyY = 0.4f;

    juce::Image img (juce::Image::RGB, juce::roundToInt (p800::PanelView::width * scale),
                     juce::roundToInt (p800::PanelView::height * scale), true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        view.paint (g, s);
    }
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    return png.writeImageToStream (img, os) ? 0 : 2;
}
