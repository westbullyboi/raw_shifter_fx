#pragma once

#include <juce_graphics/juce_graphics.h>

#include "../Utility/Constants.h"

namespace shifterfx::ui
{
    /// Colour palette + small paint helpers shared by every custom-painted control.
    namespace Theme
    {
        inline const juce::Colour background { 0xff14141c };
        inline const juce::Colour panel { 0xff1e1e29 };
        inline const juce::Colour gridLine { 0xff33333f };
        inline const juce::Colour text { 0xffe8e8f0 };
        inline const juce::Colour textDim { 0xff8a8a9a };
        inline const juce::Colour playhead { 0xff2a2a38 };

        /// One accent colour per effect lane, index order matches constants::Lane.
        inline const std::array<juce::Colour, static_cast<std::size_t>(constants::kNumLanes)> laneAccent { {
            juce::Colour { 0xffff4d6d }, // Gate
            juce::Colour { 0xff4da6ff }, // Pan
            juce::Colour { 0xff3fd6b0 }, // Filter
            juce::Colour { 0xffffc46b }, // Bitcrush
            juce::Colour { 0xff8b7bff }, // Repeat
            juce::Colour { 0xffff7ba6 }, // Reverse
            juce::Colour { 0xff6bffb0 }, // Echo
        } };

        inline juce::Font display(float height) { return juce::Font(juce::FontOptions { "Space Grotesk", height, juce::Font::plain }); }
        inline juce::Font mono(float height) { return juce::Font(juce::FontOptions { "JetBrains Mono", height, juce::Font::plain }); }
    }
}
