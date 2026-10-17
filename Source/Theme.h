#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Colour themes. Every colour of the interface comes from here.
struct Theme
{
    juce::String name;
    bool gloss = false;          // smooth lacquer instead of grain
    bool lightBg = false;        // dark text on a light background
    juce::Colour bg, speck, ink, mute, accent, accentHi, accentDim, logo, vignette, panelFill, panelStroke;

    juce::Colour glow (float t, float alpha = 1.0f) const
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return accentDim.interpolatedWith (accentHi, t).withAlpha ((0.45f + 0.55f * t) * alpha);
    }

    static const std::vector<Theme>& all()
    {
        using C = juce::Colour;
        static const std::vector<Theme> themes {
            { "Asphalt", false, false, C (24, 25, 28), C (150, 150, 158), C (0xffececf0), C (0xffa5a5ad),
              C (0xffff73b2), C (0xffff7db9), C (0xff965f7d), C (0xffff73b2),
              C (0x73000000), C (0x40000000), C (0x0fffffff) },
            { "Mango", false, true, C (246, 168, 32), C (196, 110, 12), C (0xff2b1a00), C (0xff6b4a10),
              C (0xff0fb5c9), C (0xff5ae1f0), C (0xff468c9b), C (0xff5a3205),
              C ((juce::uint8) 120, 50, 0, 0.30f), C ((juce::uint8) 110, 50, 0, 0.13f), C ((juce::uint8) 80, 40, 0, 0.18f) },
            { "Rage", false, false, C (16, 12, 12), C (96, 58, 58), C (0xfff2e6e2), C (0xffb09a96),
              C (0xffe8263a), C (0xffff4655), C (0xff822d34), C (0xffff3045),
              C (0x8c000000), C (0x4d000000), C ((juce::uint8) 255, 80, 90, 0.10f) },
            { "Ultraviolet", false, false, C (34, 18, 58), C (112, 92, 146), C (0xfff3ecff), C (0xffb9a6d9),
              C (0xffff8c3c), C (0xffffaa5a), C (0xffa05f46), C (0xffff9d4d),
              C ((juce::uint8) 8, 0, 20, 0.55f), C ((juce::uint8) 10, 0, 25, 0.28f), C (0x12ffffff) },
            { "Luxe", true, false, C (10, 10, 12), C (40, 40, 44), C (0xfff4eedc), C (0xffb8a77a),
              C (0xffd6af50), C (0xffffdc8c), C (0xff96783c), C (0xffe8c46a),
              C (0x59000000), C (0x08ffffff), C ((juce::uint8) 232, 196, 106, 0.22f) },
        };
        return themes;
    }

    static const Theme& byName (const juce::String& n)
    {
        for (const auto& t : all())
            if (t.name == n)
                return t;
        return all().front();
    }
};
