#pragma once

namespace shifterfx::dsp
{
    /**
     * Plain, framework-independent snapshot of host transport state.
     *
     * PluginProcessor translates JUCE's juce::AudioPlayHead::PositionInfo
     * into this struct once per block; every DSP-core class downstream
     * consumes only this POD, keeping the DSP core free of JUCE includes
     * and trivially testable with hand-constructed transport states.
     */
    struct TransportInfo
    {
        /// Host tempo in beats per minute. <= 0 means "unknown"; callers should fall back to a default.
        double bpm = 0.0;

        /// Host song position in quarter notes (PPQ) at the start of the current block.
        /// Negative means "unknown / not provided by host".
        double ppqPosition = -1.0;

        /// Whether the host transport is currently rolling.
        bool isPlaying = false;
    };
}
