#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include "Bitcrusher.h"
#include "EchoEffect.h"
#include "GateEnvelope.h"
#include "PanLaw.h"
#include "RingBuffer.h"
#include "StepClock.h"
#include "StepFilter.h"
#include "StepRemap.h"
#include "TransportInfo.h"
#include "../Utility/Constants.h"
#include "../Utility/Math.h"

namespace shifterfx::dsp
{
    /**
     * ShifterEngine - orchestrates the step-sequenced glitch effect.
     *
     * Purpose:
     *   An Effectrix-style multi-effect step sequencer: a pattern of
     *   kNumSteps steps, each of which can arm any combination of seven
     *   effect lanes (Gate, Pan, Filter, Bitcrush, Repeat, Reverse,
     *   Echo). Unarmed material passes through unchanged.
     *
     * Latency model:
     *   Every channel's input is continuously recorded into a RingBuffer,
     *   and output is emitted a constant `currentStepLenSamples` behind
     *   input - chosen so that, by the moment output *starts* a step, that
     *   step's audio has just finished being recorded in full. That is
     *   what lets Reverse and Repeat rearrange a step's own samples
     *   (which would otherwise not exist yet) while still reporting a
     *   single fixed plugin latency, exactly the "shared baseline delay"
     *   trick SwingFX's DelayEngine uses for its own bipolar shift range.
     *   Dry and wet are read/computed at that same constant delay, so the
     *   Mix knob never introduces comb filtering.
     *
     * Thread safety:
     *   process() runs on the audio thread and performs no allocation.
     *   Pattern/parameter setters are plain field writes, expected to be
     *   called once per block from the same thread that calls process().
     */
    class ShifterEngine
    {
    public:
        void prepare(double sampleRate, int maxBlockSize, int numChannels) noexcept;
        void reset() noexcept;

        void process(const std::array<float*, constants::kNumChannels>& channelPointers, int numSamples,
                     int numChannels, const TransportInfo& transport) noexcept;

        void setStepArmed(constants::Lane lane, int step, bool armed) noexcept
        {
            pattern[static_cast<std::size_t>(lane)][static_cast<std::size_t>(step)] = armed;
        }

        void setLaneAmount(constants::Lane lane, float amount0to1) noexcept
        {
            laneAmount[static_cast<std::size_t>(lane)] = util::clamp(amount0to1, 0.0f, 1.0f);
        }

        void setRateIndex(int index) noexcept { stepClock.setRateIndex(index); }
        void setMix(float mix0to1) noexcept { mix = util::clamp(mix0to1, 0.0f, 1.0f); }

        [[nodiscard]] int getLatencySamples() const noexcept { return static_cast<int>(currentStepLenSamples); }
        [[nodiscard]] double getCurrentBPM() const noexcept { return lastBPM; }
        [[nodiscard]] int getCurrentPatternStep() const noexcept { return currentPatternStepIndex; }

    private:
        void updateForTempo(double newBpm) noexcept;

        StepClock stepClock;
        std::array<RingBuffer<float>, constants::kNumChannels> recordBuffer;
        std::array<StepFilter, constants::kNumChannels> filter;
        std::array<Bitcrusher, constants::kNumChannels> bitcrusher;
        std::array<EchoEffect, constants::kNumChannels> echo;

        std::array<std::array<bool, constants::kNumSteps>, static_cast<std::size_t>(constants::kNumLanes)> pattern {};
        std::array<float, static_cast<std::size_t>(constants::kNumLanes)> laneAmount {};

        float mix = 1.0f;
        double sampleRate = 44100.0;
        double lastBPM = constants::kDefaultBPM;
        int lastRateIndex = constants::kDefaultRateIndex;

        std::size_t recordBufferCapacity = 0;
        std::int64_t worstCaseStepLenSamples = 0;
        std::int64_t currentStepLenSamples = 512;

        long long lastAbsoluteStepIndex = std::numeric_limits<long long>::min();
        std::int64_t positionInStep = 0;
        int currentPatternStepIndex = 0;
    };
}
