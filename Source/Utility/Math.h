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

    /// Interpolates between two frequencies (Hz) *logarithmically* rather than linearly: pitch
    /// and filter cutoff are perceived on a log scale, so a plain `lerp(startHz, endHz, t)`
    /// between e.g. 18 kHz and 250 Hz barely sounds different from the open end until `t` is
    /// most of the way to 1 - the low end of that range is a tiny fraction of the raw Hz span
    /// but a large fraction of the perceived pitch/brightness change. This is the correct
    /// interpolation for any user-facing frequency sweep/knob.
    [[nodiscard]] inline float lerpLogFrequency(float startHz, float endHz, float t) noexcept
    {
        const float a = std::max(startHz, 1.0f);
        const float b = std::max(endHz, 1.0f);
        return a * std::pow(b / a, clamp(t, 0.0f, 1.0f));
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
