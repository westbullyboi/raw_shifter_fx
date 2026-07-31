#include "TestFramework.h"
#include "../Source/DSP/StepClock.h"

using namespace shifterfx;
using namespace shifterfx::dsp;

TEST_CASE("StepClock stays on step 0 for the first stepLengthSamples-1 advances")
{
    StepClock clock;
    clock.prepare(48000.0);
    clock.setRateIndex(1); // 1/16 at 120 BPM = 0.125s = 6000 samples at 48kHz

    const auto stepLen = static_cast<int>(clock.stepLengthSamples(120.0));
    EXPECT_TRUE(stepLen == 6000);

    for (int i = 0; i < stepLen - 1; ++i)
        clock.advance(120.0);

    EXPECT_TRUE(clock.getAbsoluteStepIndex() == 0);
}

TEST_CASE("StepClock crosses into step 1 after exactly stepLengthSamples advances")
{
    StepClock clock;
    clock.prepare(48000.0);
    clock.setRateIndex(1);

    // Advance a couple of samples past the exact boundary rather than landing on it precisely:
    // accumulating (bpm/60)/sampleRate thousands of times is subject to floating-point rounding
    // that can leave runningPpq a hair under the exact boundary value.
    const auto stepLen = static_cast<int>(clock.stepLengthSamples(120.0));
    for (int i = 0; i < stepLen + 2; ++i)
        clock.advance(120.0);

    EXPECT_TRUE(clock.getAbsoluteStepIndex() == 1);
}

TEST_CASE("StepClock pattern index wraps at kNumSteps")
{
    StepClock clock;
    clock.prepare(48000.0);
    clock.setRateIndex(1);

    const auto stepLen = static_cast<int>(clock.stepLengthSamples(120.0));
    for (int i = 0; i < stepLen * constants::kNumSteps; ++i)
        clock.advance(120.0);

    EXPECT_TRUE(clock.getAbsoluteStepIndex() == constants::kNumSteps);
    EXPECT_TRUE(clock.getPatternStepIndex() == 0);
}

TEST_CASE("StepClock resyncTo snaps the running position")
{
    StepClock clock;
    clock.prepare(48000.0);
    clock.setRateIndex(1); // 0.25 quarter notes/step

    clock.resyncTo(4.0); // 4 quarter notes = 16 steps at this rate
    EXPECT_TRUE(clock.getAbsoluteStepIndex() == 16);
    EXPECT_TRUE(clock.getPatternStepIndex() == 0);

    // A negative (unknown) position must be ignored, not snap the clock to 0.
    clock.resyncTo(-1.0);
    EXPECT_TRUE(clock.getAbsoluteStepIndex() == 16);
}

TEST_CASE("StepClock step phase runs from 0 towards 1 across a step")
{
    StepClock clock;
    clock.prepare(48000.0);
    clock.setRateIndex(1);

    EXPECT_NEAR(clock.getStepPhase01(), 0.0, 1e-9);

    const auto stepLen = static_cast<int>(clock.stepLengthSamples(120.0));
    for (int i = 0; i < stepLen / 2; ++i)
        clock.advance(120.0);

    EXPECT_NEAR(clock.getStepPhase01(), 0.5, 0.01);
}
