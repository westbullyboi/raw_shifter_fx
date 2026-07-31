#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "TestFramework.h"
#include "../Source/DSP/ShifterEngine.h"

using namespace shifterfx;
using namespace shifterfx::dsp;

TEST_CASE("ShifterEngine with no lanes armed is a pure constant-delay passthrough")
{
    ShifterEngine engine;
    engine.prepare(48000.0, 512, 1);
    // mix defaults to 1.0 (fully wet), which with nothing armed must equal fully dry.

    const int stepLenSamples = 6000; // 1/16 @ 120 BPM @ 48kHz
    const int numSamples = stepLenSamples * 2 + 50;

    std::vector<float> input(static_cast<std::size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        input[static_cast<std::size_t>(i)] = std::sin(0.017f * static_cast<float>(i)) * 0.5f
                                              + std::sin(0.31f * static_cast<float>(i)) * 0.2f;

    std::vector<float> buffer = input;
    std::array<float*, constants::kNumChannels> ptrs { buffer.data(), nullptr };

    TransportInfo transport;
    transport.bpm = 120.0;
    transport.ppqPosition = 0.0;
    transport.isPlaying = true;

    engine.process(ptrs, numSamples, 1, transport);

    const int latency = engine.getLatencySamples();
    EXPECT_TRUE(latency == stepLenSamples);

    for (int n = latency; n < numSamples; ++n)
        EXPECT_NEAR(buffer[static_cast<std::size_t>(n)], input[static_cast<std::size_t>(n - latency)], 1e-3);
}

TEST_CASE("ShifterEngine Gate lane mutes only its armed step's sustain region")
{
    ShifterEngine engine;
    engine.prepare(48000.0, 512, 1);
    engine.setStepArmed(constants::Lane::Gate, 0, true);
    engine.setLaneAmount(constants::Lane::Gate, 1.0f);

    TransportInfo transport;
    transport.bpm = 120.0;
    transport.ppqPosition = -1.0; // free-run across calls using accumulated engine state
    transport.isPlaying = true;

    int prevStep = -1;
    int counterInStep = 0;
    float mutedSustainPeak = 0.0f;
    float unmutedSustainPeak = 0.0f;
    bool sawMuted = false;
    bool sawUnmuted = false;

    const int totalSamples = 6000 * 3;
    for (int n = 0; n < totalSamples; ++n)
    {
        float sample = std::sin(2.0f * 3.14159265f * 300.0f * static_cast<float>(n) / 48000.0f);
        std::array<float*, constants::kNumChannels> ptrs { &sample, nullptr };
        engine.process(ptrs, 1, 1, transport);

        const int step = engine.getCurrentPatternStep();
        counterInStep = (step == prevStep) ? counterInStep + 1 : 0;
        prevStep = step;

        if (counterInStep >= 500 && counterInStep <= 5500)
        {
            if (step == 0)
            {
                mutedSustainPeak = std::max(mutedSustainPeak, std::abs(sample));
                sawMuted = true;
            }
            else if (step == 1)
            {
                // Step 15 (this run's very first pattern step) falls inside the constant-delay
                // warm-up window, where the record buffer's still-zeroed pre-roll makes *every*
                // step read as silent regardless of Gate - so step 1, which only appears after
                // real recorded audio has propagated through the delay, is the fair comparison.
                unmutedSustainPeak = std::max(unmutedSustainPeak, std::abs(sample));
                sawUnmuted = true;
            }
        }
    }

    EXPECT_TRUE(sawMuted);
    EXPECT_TRUE(sawUnmuted);
    EXPECT_TRUE(mutedSustainPeak < 0.05f);
    EXPECT_TRUE(unmutedSustainPeak > 0.5f);
}

TEST_CASE("ShifterEngine Mix=0 makes a fully-ducking Gate lane inaudible")
{
    ShifterEngine engine;
    engine.prepare(48000.0, 512, 1);
    engine.setStepArmed(constants::Lane::Gate, 0, true);
    engine.setLaneAmount(constants::Lane::Gate, 1.0f);
    engine.setMix(0.0f); // fully dry: the armed Gate lane must have no audible effect at all

    TransportInfo transport;
    transport.bpm = 120.0;
    transport.ppqPosition = -1.0;
    transport.isPlaying = true;

    int prevStep = -1;
    int counterInStep = 0;
    float step0SustainPeak = 0.0f;
    bool sawStep0 = false;

    const int totalSamples = 6000 * 3;
    for (int n = 0; n < totalSamples; ++n)
    {
        float sample = std::sin(2.0f * 3.14159265f * 300.0f * static_cast<float>(n) / 48000.0f);
        std::array<float*, constants::kNumChannels> ptrs { &sample, nullptr };
        engine.process(ptrs, 1, 1, transport);

        const int step = engine.getCurrentPatternStep();
        counterInStep = (step == prevStep) ? counterInStep + 1 : 0;
        prevStep = step;

        if (step == 0 && counterInStep >= 500 && counterInStep <= 5500)
        {
            step0SustainPeak = std::max(step0SustainPeak, std::abs(sample));
            sawStep0 = true;
        }
    }

    EXPECT_TRUE(sawStep0);
    // At mix=0 this must look like the unmuted case (>0.5), not the muted case (<0.05) - i.e.
    // Mix, not lane arming alone, gates whether an effect is actually audible.
    EXPECT_TRUE(step0SustainPeak > 0.5f);
}
