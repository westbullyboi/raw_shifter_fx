#pragma once

#include <cmath>

#include "../Utility/Constants.h"
#include "../Utility/Math.h"

namespace shifterfx::dsp
{
    /**
     * StepClock - beat-grid tracking for the step sequencer.
     *
     * Purpose:
     *   Owns ShifterFX's notion of "where are we in the pattern" - a
     *   running quarter-note (PPQ) position that free-runs at
     *   `bpm/60/sampleRate` quarter-notes per sample and is periodically
     *   resynced to the host's reported position, so it stays accurate
     *   under tempo/position changes without a host query every sample.
     *
     * DSP theory:
     *   The Rate parameter picks how many quarter notes one step spans
     *   (kRateQuarterFractions). A sample's position on the step grid is
     *   `floor(ppq / rateFraction)`; the pattern (kNumSteps long) repeats
     *   by taking that index modulo kNumSteps. The fractional remainder
     *   is the sample's phase within the current step, used by the Gate
     *   and Filter lanes to shape their envelope/sweep across the step.
     *
     * Thread safety:
     *   Not thread-safe; owned and driven exclusively by the audio thread.
     */
    class StepClock
    {
    public:
        void prepare(double sampleRateIn) noexcept
        {
            sampleRate = sampleRateIn;
            reset();
        }

        void reset() noexcept { runningPpq = 0.0; }

        /// Selects the step subdivision by "rate" choice-parameter index. Real-time safe.
        void setRateIndex(int index) noexcept
        {
            rateIndex = util::clamp(index, 0, static_cast<int>(constants::kRateQuarterFractions.size()) - 1);
        }
        [[nodiscard]] int getRateIndex() const noexcept { return rateIndex; }
        [[nodiscard]] double getRateQuarterFraction() const noexcept
        {
            return constants::kRateQuarterFractions[static_cast<std::size_t>(rateIndex)];
        }

        /// Snaps the beat clock to the host-reported PPQ position, if valid (>= 0). Real-time safe.
        void resyncTo(double ppqPosition) noexcept
        {
            if (ppqPosition >= 0.0)
                runningPpq = ppqPosition;
        }

        /// Advances the beat clock by one sample's worth of quarter notes. Real-time safe.
        void advance(double bpm) noexcept
        {
            runningPpq += (bpm / 60.0) / sampleRate;
        }

        /// Absolute (non-wrapped) step index at the clock's current position. Monotonically
        /// increasing while the transport runs forward; comparing successive calls detects
        /// step-boundary crossings the same way SwingFX's TimingEngine detects grid crossings.
        [[nodiscard]] long long getAbsoluteStepIndex() const noexcept
        {
            return static_cast<long long>(std::floor(runningPpq / getRateQuarterFraction()));
        }

        /// Position within the kNumSteps-long pattern cycle, in [0, kNumSteps).
        [[nodiscard]] int getPatternStepIndex() const noexcept
        {
            const auto raw = getAbsoluteStepIndex() % constants::kNumSteps;
            return static_cast<int>((raw + constants::kNumSteps) % constants::kNumSteps);
        }

        /// Fractional position within the current step, in [0, 1).
        [[nodiscard]] double getStepPhase01() const noexcept
        {
            const double stepPosition = runningPpq / getRateQuarterFraction();
            return stepPosition - std::floor(stepPosition);
        }

        /// Duration of one step, in samples, at the given tempo and the active Rate. Real-time safe.
        [[nodiscard]] double stepLengthSamples(double bpm) const noexcept
        {
            const double seconds = getRateQuarterFraction() * util::quarterNoteDurationSeconds(bpm);
            return seconds * sampleRate;
        }

    private:
        double sampleRate = 44100.0;
        double runningPpq = 0.0;
        int rateIndex = constants::kDefaultRateIndex;
    };
}
