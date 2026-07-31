#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/ShifterEngine.h"

namespace shifterfx
{
    /**
     * PluginProcessor - JUCE AudioProcessor glue layer.
     *
     * Owns the plugin's parameter tree (juce::AudioProcessorValueTreeState),
     * translates JUCE's audio buffers and playhead into the DSP core's
     * framework-independent types, and reports latency to the host. All
     * step-sequencing/effect logic is delegated to dsp::ShifterEngine.
     */
    class PluginProcessor final : public juce::AudioProcessor
    {
    public:
        PluginProcessor();
        ~PluginProcessor() override = default;

        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

        using AudioProcessor::processBlock;
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return JucePlugin_Name; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}

        void getStateInformation(juce::MemoryBlock& destData) override;
        void setStateInformation(const void* data, int sizeInBytes) override;

        static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

        /// Parameter ID for lane/step toggle "step_<laneId>_<step>". Shared by the parameter
        /// layout and the editor's step-grid so the two can never drift apart.
        static juce::String stepParamID(int lane, int step);

        /// Parameter ID for a lane's "<laneId>Amount" knob (Reverse has none - it is binary).
        static juce::String amountParamID(int lane);

        juce::AudioProcessorValueTreeState apvts;

        struct StatusSnapshot
        {
            std::atomic<double> currentBPM { 120.0 };
            std::atomic<int> latencySamples { 0 };
            std::atomic<float> outputPeakLevel { 0.0f };
            std::atomic<int> currentStep { 0 };
        };

        [[nodiscard]] const StatusSnapshot& getStatusSnapshot() const noexcept { return status; }

    private:
        dsp::ShifterEngine shifterEngine;
        StatusSnapshot status;

        std::array<std::array<juce::AudioParameterBool*, constants::kNumSteps>, constants::kNumLanes>
            stepParams {};
        std::array<std::atomic<float>*, constants::kNumLanes> amountParams {};
        juce::AudioParameterChoice* rateParam = nullptr;
        std::atomic<float>* mixParam = nullptr;
        std::atomic<float>* outputGainParam = nullptr;

        double currentSampleRate = 44100.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
    };
}
