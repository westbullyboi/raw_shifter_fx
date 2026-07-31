#include "TestFramework.h"
#include "../Source/DSP/RingBuffer.h"

using namespace shifterfx::dsp;

TEST_CASE("RingBuffer readDelayed(0) returns the most recently pushed sample")
{
    RingBuffer<float> buffer;
    buffer.resize(16);
    buffer.push(1.0f);
    buffer.push(2.0f);
    buffer.push(3.0f);

    EXPECT_NEAR(buffer.readDelayed(0), 3.0f, 1e-9);
    EXPECT_NEAR(buffer.readDelayed(1), 2.0f, 1e-9);
    EXPECT_NEAR(buffer.readDelayed(2), 1.0f, 1e-9);
}

TEST_CASE("RingBuffer capacity is rounded up to a power of two")
{
    RingBuffer<float> buffer;
    buffer.resize(10);
    EXPECT_TRUE(buffer.getCapacity() == 16);
}

TEST_CASE("RingBuffer wraps correctly past capacity")
{
    RingBuffer<float> buffer;
    buffer.resize(4);
    for (int i = 0; i < 10; ++i)
        buffer.push(static_cast<float>(i));

    // Last pushed value was 9; readDelayed(0) must be 9 regardless of wraparound.
    EXPECT_NEAR(buffer.readDelayed(0), 9.0f, 1e-9);
    EXPECT_NEAR(buffer.readDelayed(3), 6.0f, 1e-9);
}

TEST_CASE("RingBuffer clear zeroes the buffer without reallocating")
{
    RingBuffer<float> buffer;
    buffer.resize(8);
    buffer.push(5.0f);
    const auto capacityBefore = buffer.getCapacity();
    buffer.clear();

    EXPECT_TRUE(buffer.getCapacity() == capacityBefore);
    EXPECT_NEAR(buffer.readDelayed(0), 0.0f, 1e-9);
}
