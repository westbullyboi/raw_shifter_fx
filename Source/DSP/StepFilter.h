#pragma once

#include <cmath>
#include <numbers>

#include "../Utility/Math.h"

namespace shifterfx::dsp
{
    /**
     * StepFilter - resonant low-pass sweep for the Filter lane.
     *
     * Purpose:
     *   A Chamberlin state-variable low-pass whose cutoff continuously
     *   slews (one-pole smoother, kFilterCutoffSlewMs) towards a target
     *   frequency. ShifterEngine sets that target every sample from the
     *   step phase (open when not armed, closing across an armed step),
     *   so entering/leaving the effect is always a smooth glide rather
     *   than a discontinuous jump - avoiding zipper noise at step
     *   boundaries without needing to reset filter state.
     *
     * DSP theory:
     *   Chamberlin's topology updates two state variables (low, band)
     *   per sample from a normalised frequency coefficient
     *   f = 2*sin(pi*Fc/Fs), stable while Fc stays below roughly Fs/6.
     *   Resonance is the inverse of Q as the feedback damping term.
     *
     * Thread safety:
     *   Not thread-safe; one instance per audio channel, driven
     *   exclusively by the audio thread.
     */
    class StepFilter
    {
    public:
        void prepare(double sampleRateIn) noexcept
        {
            sampleRate = sampleRateIn;
            reset();
        }

        void reset() noexcept
        {
            low = 0.0f;
            band = 0.0f;
            smoothedCutoffHz = 20000.0f;
        }

        void setCutoffSlewMs(float ms) noexcept
        {
            slewCoefficient = ms > 0.0f
                                 ? 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * ms * 0.001f))
                                 : 1.0f;
        }

        [[nodiscard]] float process(float input, float targetCutoffHz, float q) noexcept
        {
            smoothedCutoffHz += (targetCutoffHz - smoothedCutoffHz) * slewCoefficient;

            const float nyquistGuard = static_cast<float>(sampleRate) * 0.16f;
            const float cutoff = util::clamp(smoothedCutoffHz, 20.0f, nyquistGuard);
            const float f = 2.0f * std::sin(std::numbers::pi_v<float> * cutoff / static_cast<float>(sampleRate));
            const float damping = q > 0.0f ? 1.0f / q : 1.0f;

            low += f * band;
            const float high = input - low - damping * band;
            band += f * high;

            return low;
        }

    private:
        double sampleRate = 44100.0;
        float low = 0.0f;
        float band = 0.0f;
        float smoothedCutoffHz = 20000.0f;
        float slewCoefficient = 1.0f;
    };
}
