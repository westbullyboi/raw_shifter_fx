#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/AmountKnob.h"
#include "UI/StepGridView.h"
#include "UI/Theme.h"

namespace shifterfx
{
    /**
     * PluginEditor - RAW SHIFTER FX's custom-painted GUI.
     *
     * A step-grid glitch-sequencer view: dsp::constants::kNumLanes effect
     * lanes (Gate/Pan/Filter/Crush/Repeat/Reverse/Echo) x kNumSteps
     * click-to-arm cells, a scrolling playhead highlight synced to
     * ShifterEngine's current pattern step, a Rate selector, and one
     * AmountKnob per lane (except Reverse, which is binary) plus Mix and
     * Output. A GUI timer polls the processor's lock-free StatusSnapshot
     * to move the playhead; the audio thread is never touched directly.
     */
    class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit PluginEditor(PluginProcessor&);
        ~PluginEditor() override;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void cycleRate();

        /// Small pill button used for the Rate selector.
        class RateButton final : public juce::Button
        {
        public:
            RateButton() : juce::Button({}) {}
            void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

            juce::String text { "1/16" };
        };

        PluginProcessor& processorRef;

        juce::Label titleLabel, statusLabel;
        RateButton rateButton;
        ui::StepGridView stepGrid;

        std::vector<std::unique_ptr<ui::AmountKnob>> knobs;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
    };
}
