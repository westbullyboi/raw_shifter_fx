#pragma once

#include <algorithm>
#include <cstdint>

namespace shifterfx::dsp
{
    /**
     * StepRemap - pure functions computing, for a sample at `positionInStep`
     * samples into a step of length `stepLenSamples`, how many samples
     * behind the RecordBuffer's write cursor to read from instead of the
     * plain passthrough delay (which always reads `stepLenSamples` behind,
     * i.e. this step's own audio played back in order).
     *
     * Derivation: ShifterEngine's output lags its input by a constant
     * `stepLenSamples` (see ShifterEngine.h), chosen so that by the time
     * output begins a step, that step's audio has just finished being
     * recorded in full. At output position `p` within step N, the write
     * cursor sits at `stepStart(N) + stepLenSamples + p`. Reading the
     * step's own sample `p` back in order therefore needs delay
     * `stepLenSamples` (constant); reading a *different* offset `p'`
     * within the same step needs delay `stepLenSamples + p - p'`.
     *
     * Both functions are bounded to at most `2 * stepLenSamples`, which is
     * why RecordBuffer is sized with that much headroom.
     */
    namespace remap
    {
        /// Delay (samples) to play the step back reverse: target offset = stepLen-1-p.
        [[nodiscard]] constexpr std::int64_t reverseDelaySamples([[maybe_unused]] std::int64_t stepLenSamples,
                                                                    std::int64_t positionInStep) noexcept
        {
            return 2 * positionInStep + 1;
        }

        /// Delay (samples) to loop a `1/divisions`-length fragment from the step's start,
        /// `divisions` times, for the classic stutter/beat-repeat glitch.
        [[nodiscard]] constexpr std::int64_t repeatDelaySamples(std::int64_t stepLenSamples,
                                                                   std::int64_t positionInStep,
                                                                   int divisions) noexcept
        {
            const std::int64_t fragLen = std::max<std::int64_t>(stepLenSamples / std::max(divisions, 1), 1);
            return stepLenSamples + positionInStep - (positionInStep % fragLen);
        }
    }
}
