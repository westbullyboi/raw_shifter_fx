#pragma once

#include <cmath>

namespace shifterfx::dsp
{
    /**
     * Bitcrusher - sample-and-hold downsampler + bit-depth quantizer for the Crush lane.
     *
     * Purpose:
     *   Two independent lo-fi degradations stacked together: holding the
     *   last sample for `holdPeriodSamples` calls approximates sample-rate
     *   reduction (a zero-order-hold downsampler), and `quantizeBits`
     *   rounds the held value to a coarse mid-tread staircase.
     *
     * Thread safety:
     *   Not thread-safe; one instance per audio channel.
     */
    namespace bitcrush
    {
        [[nodiscard]] inline float quantizeBits(float x, float bits) noexcept
        {
            const float levels = std::exp2(bits) - 1.0f;
            if (levels <= 0.0f)
                return x;
            return std::round(x * levels) / levels;
        }
    }

    class Bitcrusher
    {
    public:
        void reset() noexcept
        {
            holdCounter = 0;
            heldValue = 0.0f;
        }

        [[nodiscard]] float process(float input, int holdPeriodSamples, float bits) noexcept
        {
            if (holdCounter <= 0)
            {
                heldValue = bitcrush::quantizeBits(input, bits);
                holdCounter = holdPeriodSamples > 0 ? holdPeriodSamples : 1;
            }
            --holdCounter;
            return heldValue;
        }

    private:
        int holdCounter = 0;
        float heldValue = 0.0f;
    };
}
