#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace shifterfx::dsp
{
    /**
     * GateEnvelope - pure function computing the Gate lane's per-sample gain.
     *
     * An armed step ducks towards `1 - duckAmount01` with a short
     * raised-cosine ramp at both the start and end of the step, so full
     * mute (duckAmount01 = 1) never clicks. Not a stateful class: the gain
     * at any sample is a pure function of where that sample sits within
     * its step, so it can be evaluated directly from ShifterEngine's
     * existing step-position counter with no extra state to keep in sync.
     */
    namespace gate
    {
        [[nodiscard]] inline float computeGain(std::int64_t positionInStep, std::int64_t stepLenSamples,
                                                 std::int64_t rampSamples, float duckAmount01) noexcept
        {
            const float sustain = 1.0f - std::clamp(duckAmount01, 0.0f, 1.0f);
            if (stepLenSamples <= 0)
                return sustain;

            const std::int64_t ramp = std::clamp<std::int64_t>(rampSamples, 1, stepLenSamples / 2);
            const std::int64_t distanceFromEdge = std::min(positionInStep, stepLenSamples - 1 - positionInStep);

            if (distanceFromEdge >= ramp)
                return sustain;

            const float t = static_cast<float>(distanceFromEdge) / static_cast<float>(ramp);
            const float smooth = 0.5f * (1.0f - std::cos(t * std::numbers::pi_v<float>));
            return 1.0f + (sustain - 1.0f) * smooth;
        }
    }
}
