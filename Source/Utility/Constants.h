#pragma once

#include <array>

/**
 * Global tuning constants for ShifterFX.
 *
 * Kept framework-independent (no JUCE includes) so the DSP core can be
 * unit tested without linking against JUCE.
 */
namespace shifterfx::constants
{
    /// Fallback tempo used before the host reports a valid BPM (e.g. standalone, no transport).
    inline constexpr double kDefaultBPM = 120.0;

    /// Lowest tempo the plugin guarantees full-range step buffering for. The RecordBuffer is
    /// sized against this floor combined with the slowest selectable Rate; slower tempos still
    /// work but the step length is clamped to the capacity that floor implies.
    inline constexpr double kMinSupportedBPM = 40.0;

    /// Highest tempo considered valid when sanity-checking host-reported transport info.
    inline constexpr double kMaxSupportedBPM = 300.0;

    /// Minimum BPM change (in BPM) required before ShifterEngine recomputes its reported
    /// latency. Filters out floating point jitter from host transport queries.
    inline constexpr double kTempoChangeThresholdBPM = 0.25;

    /// Number of audio channels the DSP core is laid out for (stereo in, stereo out).
    inline constexpr int kNumChannels = 2;

    // --- Step sequencer --------------------------------------------------------------------

    /// Number of steps in one pattern cycle (one bar of 16th notes at the default Rate).
    inline constexpr int kNumSteps = 16;

    /// Selectable step subdivisions, expressed as a fraction of one quarter note. Index order
    /// must match the "rate" choice parameter exactly (1/32, 1/16, 1/8).
    inline constexpr std::array<double, 3> kRateQuarterFractions { 0.125, 0.25, 0.5 };
    inline constexpr int kDefaultRateIndex = 1; // 1/16

    /// Effect lanes, in processing-chain order for the value-domain effects (Gate/Pan/Filter/
    /// Bitcrush run in this order after the time-remap stage picks Reverse/Repeat/passthrough;
    /// Echo is an always-on send tapped from the chain's output). Index order must match the
    /// per-lane step-pattern parameter IDs exactly - see PluginProcessor::createParameterLayout.
    enum class Lane
    {
        Gate = 0,
        Pan,
        Filter,
        Bitcrush,
        Repeat,
        Reverse,
        Echo,
        Count
    };
    inline constexpr int kNumLanes = static_cast<int>(Lane::Count);

    inline constexpr std::array<const char*, static_cast<std::size_t>(kNumLanes)> kLaneNames { {
        "GATE", "PAN", "FILTER", "CRUSH", "REPEAT", "REVERSE", "ECHO"
    } };

    /// Lowercase, whitespace-free identifiers used to build parameter IDs
    /// ("step_<id>_<step>", "<id>Amount"). Index order matches Lane exactly.
    inline constexpr std::array<const char*, static_cast<std::size_t>(kNumLanes)> kLaneIDs { {
        "gate", "pan", "filter", "crush", "repeat", "reverse", "echo"
    } };

    // --- Gate lane ---------------------------------------------------------------------------

    /// Length (ms) of the raised-cosine ramp at the start/end of a gated step, so ducking to
    /// (or recovering from) silence never clicks.
    inline constexpr float kGateRampMs = 3.0f;

    // --- Filter lane ---------------------------------------------------------------------------

    inline constexpr float kFilterOpenHz = 18000.0f;
    inline constexpr float kFilterMinHz = 250.0f;
    inline constexpr float kFilterMinQ = 0.7f;
    inline constexpr float kFilterMaxQ = 4.0f;
    /// Time constant (ms) of the one-pole smoother that slews the filter cutoff between the
    /// open (bypass) frequency and the swept target, avoiding zipper noise at step boundaries.
    inline constexpr float kFilterCutoffSlewMs = 5.0f;

    // --- Bitcrush lane -----------------------------------------------------------------------

    inline constexpr int kBitcrushMinHoldSamples = 1;
    inline constexpr int kBitcrushMaxHoldSamples = 40;
    inline constexpr float kBitcrushMinBits = 3.0f;
    inline constexpr float kBitcrushMaxBits = 16.0f;

    // --- Repeat lane -------------------------------------------------------------------------

    inline constexpr int kRepeatMinDivisions = 1;
    inline constexpr int kRepeatMaxDivisions = 8;

    // --- Echo lane ---------------------------------------------------------------------------

    inline constexpr float kEchoMaxFeedback = 0.85f;
    /// The echo tap time is a fixed fraction of the *current* step length, so the throw stays
    /// musically locked to tempo/rate without needing its own delay-time parameter.
    inline constexpr float kEchoDelayStepFraction = 0.5f;
}
