#pragma once

#include <algorithm>
#include <cmath>

/**
 * Small, allocation-free numeric helpers shared across the DSP core.
 * Pure functions only - no state, no JUCE dependency, safe to call
 * from the audio thread and trivially unit-testable.
 */
namespace shifterfx::util
{
    /// Clamps `value` to the closed interval [lo, hi].
    template <typename T>
    [[nodiscard]] constexpr T clamp(T value, T lo, T hi) noexcept
    {
        return std::min(std::max(value, lo), hi);
    }

    /// Linear interpolation between `a` and `b` at position `t` (unclamped).
    template <typename T>
    [[nodiscard]] constexpr T lerp(T a, T b, T t) noexcept
    {
        return a + (b - a) * t;
    }

    /// Converts a duration in milliseconds to a sample count at `sampleRate`.
    [[nodiscard]] constexpr double msToSamples(double milliseconds, double sampleRate) noexcept
    {
        return milliseconds * 0.001 * sampleRate;
    }

    /// Duration, in seconds, of one quarter note at `bpm`.
    [[nodiscard]] constexpr double quarterNoteDurationSeconds(double bpm) noexcept
    {
        return bpm > 0.0 ? 60.0 / bpm : 0.0;
    }

    /// Converts a decibel value to a linear gain multiplier.
    [[nodiscard]] inline double decibelsToGain(double db) noexcept
    {
        return std::pow(10.0, db / 20.0);
    }

    /// Equal-power (constant loudness across a linear crossfade) gain pair for position
    /// `t` in [0, 1]: 0 = fully A, 1 = fully B. gainA^2 + gainB^2 == 1 for every t.
    inline void equalPowerGains(float t, float& gainA, float& gainB) noexcept
    {
        constexpr float halfPi = 1.5707963267948966f;
        const float clamped = clamp(t, 0.0f, 1.0f);
        gainA = std::cos(clamped * halfPi);
        gainB = std::sin(clamped * halfPi);
    }
}
