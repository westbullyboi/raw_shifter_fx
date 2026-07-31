#pragma once

#include <algorithm>
#include <utility>

namespace shifterfx::dsp
{
    /**
     * PanLaw - pure function implementing the Pan lane.
     *
     * An armed step collapses the stereo image to its mono sum and
     * places it hard to one side, alternating left/right by the
     * *absolute* step index (even = left, odd = right) so a run of
     * consecutive armed steps ping-pongs rather than sitting on one
     * side. `amount01` crossfades between the untouched input (0) and
     * the fully hard-panned mono sum (1), so partial settings widen
     * rather than snap.
     */
    namespace pan
    {
        [[nodiscard]] inline std::pair<float, float> process(float inL, float inR, long long absoluteStepIndex,
                                                                float amount01) noexcept
        {
            const float amount = std::clamp(amount01, 0.0f, 1.0f);
            if (amount <= 0.0f)
                return { inL, inR };

            const float monoSum = 0.5f * (inL + inR);
            const bool panLeft = ((absoluteStepIndex % 2) + 2) % 2 == 0;
            const float targetL = panLeft ? monoSum : 0.0f;
            const float targetR = panLeft ? 0.0f : monoSum;

            return { inL + (targetL - inL) * amount, inR + (targetR - inR) * amount };
        }
    }
}
