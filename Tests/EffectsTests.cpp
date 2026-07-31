#include <algorithm>
#include <cmath>
#include <tuple>

#include "TestFramework.h"
#include "../Source/DSP/Bitcrusher.h"
#include "../Source/DSP/EchoEffect.h"
#include "../Source/DSP/GateEnvelope.h"
#include "../Source/DSP/PanLaw.h"
#include "../Source/DSP/StepFilter.h"
#include "../Source/DSP/StepRemap.h"

using namespace shifterfx::dsp;

// --- StepRemap ---------------------------------------------------------------------------

TEST_CASE("remap::reverseDelaySamples plays the step back in reverse order")
{
    constexpr std::int64_t stepLen = 100;
    // Reading with this delay at output position p must land on step-relative sample (stepLen-1-p).
    // Verified indirectly: delay grows by 2 for each +1 in position, and equals 1 at p=0 (the very
    // last sample recorded, i.e. index stepLen-1 - matching a fully reversed readout).
    EXPECT_TRUE(remap::reverseDelaySamples(stepLen, 0) == 1);
    EXPECT_TRUE(remap::reverseDelaySamples(stepLen, 1) == 3);
    EXPECT_TRUE(remap::reverseDelaySamples(stepLen, 99) == 199);
}

TEST_CASE("remap::repeatDelaySamples loops a 1/divisions fragment from the step start")
{
    constexpr std::int64_t stepLen = 100;

    // The delay returned is relative to a write cursor that itself advances by 1 every time
    // `positionInStep` advances by 1 (see the class doc comment in StepRemap.h), so the actual
    // step-relative sample being targeted is `stepLen + positionInStep - delay`, not the raw
    // delay value itself.
    const auto targetOffset = [](std::int64_t positionInStep, int divisions)
    {
        return stepLen + positionInStep - remap::repeatDelaySamples(stepLen, positionInStep, divisions);
    };

    // 4 divisions -> a 25-sample fragment repeated 4x. Every fragment start (0, 25, 50, 75)
    // must resolve to the same target offset: the very first sample of the step.
    EXPECT_TRUE(targetOffset(0, 4) == 0);
    EXPECT_TRUE(targetOffset(25, 4) == 0);
    EXPECT_TRUE(targetOffset(50, 4) == 0);
    EXPECT_TRUE(targetOffset(75, 4) == 0);

    // One sample into a fragment targets one sample after the fragment start.
    EXPECT_TRUE(targetOffset(1, 4) == 1);
    EXPECT_TRUE(targetOffset(26, 4) == 1);
}

// --- GateEnvelope --------------------------------------------------------------------------

TEST_CASE("GateEnvelope gain is fully open at duckAmount 0")
{
    EXPECT_NEAR(gate::computeGain(0, 1000, 50, 0.0f), 1.0f, 1e-6);
    EXPECT_NEAR(gate::computeGain(500, 1000, 50, 0.0f), 1.0f, 1e-6);
}

TEST_CASE("GateEnvelope reaches full duck away from the step edges")
{
    const float gain = gate::computeGain(500, 1000, 50, 1.0f);
    EXPECT_NEAR(gain, 0.0f, 1e-4);
}

TEST_CASE("GateEnvelope is open exactly at the step edges regardless of duck amount")
{
    EXPECT_NEAR(gate::computeGain(0, 1000, 50, 1.0f), 1.0f, 1e-4);
    EXPECT_NEAR(gate::computeGain(999, 1000, 50, 1.0f), 1.0f, 1e-4);
}

// --- PanLaw ----------------------------------------------------------------------------------

TEST_CASE("PanLaw is a no-op at amount 0")
{
    const auto [outL, outR] = pan::process(0.3f, -0.6f, 0, 0.0f);
    EXPECT_NEAR(outL, 0.3f, 1e-6);
    EXPECT_NEAR(outR, -0.6f, 1e-6);
}

TEST_CASE("PanLaw hard-pans the mono sum, alternating by absolute step parity")
{
    const auto [leftL, leftR] = pan::process(1.0f, 1.0f, 0, 1.0f);
    EXPECT_NEAR(leftL, 1.0f, 1e-6);
    EXPECT_NEAR(leftR, 0.0f, 1e-6);

    const auto [rightL, rightR] = pan::process(1.0f, 1.0f, 1, 1.0f);
    EXPECT_NEAR(rightL, 0.0f, 1e-6);
    EXPECT_NEAR(rightR, 1.0f, 1e-6);
}

// --- StepFilter --------------------------------------------------------------------------------

TEST_CASE("StepFilter attenuates a high-frequency tone once cutoff sweeps low")
{
    StepFilter filter;
    filter.prepare(48000.0);
    filter.setCutoffSlewMs(0.01f); // near-instant slew for a deterministic test

    // Run a 10 kHz tone through a filter held at a 300 Hz cutoff; the settled output should be
    // much quieter than the input once the filter has had time to respond.
    float maxOut = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const float input = std::sin(2.0f * 3.14159265f * 10000.0f * static_cast<float>(i) / 48000.0f);
        const float out = filter.process(input, 300.0f, 0.7f);
        if (i > 1000)
            maxOut = std::max(maxOut, std::abs(out));
    }
    EXPECT_TRUE(maxOut < 0.3f);
}

// --- Bitcrusher --------------------------------------------------------------------------------

TEST_CASE("Bitcrusher holds a sample for holdPeriodSamples calls")
{
    Bitcrusher crusher;
    crusher.reset();

    const float first = crusher.process(0.5f, 4, 16.0f);
    const float second = crusher.process(0.9f, 4, 16.0f); // held: input ignored
    const float third = crusher.process(0.9f, 4, 16.0f);
    const float fourth = crusher.process(0.9f, 4, 16.0f);

    EXPECT_NEAR(first, second, 1e-6);
    EXPECT_NEAR(second, third, 1e-6);
    EXPECT_NEAR(third, fourth, 1e-6);
}

TEST_CASE("Bitcrusher quantizeBits collapses nearby values onto a shared coarse step")
{
    // 2 bits -> 3 quantization levels (2^2 - 1) spaced 1/3 apart. 0.60 and 0.62 fall in the same
    // bin and must quantize identically; 0.95 falls in the next bin up and must differ.
    const float a = bitcrush::quantizeBits(0.60f, 2.0f);
    const float b = bitcrush::quantizeBits(0.62f, 2.0f);
    const float c = bitcrush::quantizeBits(0.95f, 2.0f);

    EXPECT_NEAR(a, b, 1e-6);
    EXPECT_TRUE(std::abs(a - c) > 0.1f);
    EXPECT_NEAR(a, 2.0f / 3.0f, 1e-6);
    EXPECT_NEAR(c, 1.0f, 1e-6);
}

// --- EchoEffect --------------------------------------------------------------------------------

TEST_CASE("EchoEffect produces a decaying tail after the feed stops")
{
    EchoEffect echo;
    echo.prepare(256);
    echo.reset();

    for (int i = 0; i < 10; ++i)
        std::ignore = echo.process(1.0f, true, 10, 0.5f);

    float lastTail = 1.0f;
    bool sawNonZero = false;
    for (int i = 0; i < 200; ++i)
    {
        const float tail = echo.process(0.0f, false, 10, 0.5f);
        if (std::abs(tail) > 1e-6f)
            sawNonZero = true;
        lastTail = tail;
    }
    EXPECT_TRUE(sawNonZero);
    EXPECT_TRUE(std::abs(lastTail) < 0.01f); // decayed away well before 200 more samples
}
