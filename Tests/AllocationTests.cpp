#include "TestFramework.h"

#include <atomic>
#include <cstdlib>
#include <new>

#include "../Source/DSP/ShifterEngine.h"

namespace
{
    std::atomic<bool> g_trackAllocations { false };
    std::atomic<std::size_t> g_allocationCount { 0 };
}

// Global replaceable allocation functions (standard C++, see [new.delete]). Overriding these
// for the whole test binary lets us count heap allocations that occur *while tracking is
// enabled*, which we flip on only around the call under test (ShifterEngine::process). Every
// other allocation in the binary (test bookkeeping, std::vector setup, etc.) happens outside
// that window and is invisible to the counter.
void* operator new(std::size_t size)
{
    if (g_trackAllocations.load(std::memory_order_relaxed))
        g_allocationCount.fetch_add(1, std::memory_order_relaxed);

    if (void* ptr = std::malloc(size == 0 ? 1 : size))
        return ptr;
    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* ptr) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { ::operator delete(ptr); }

using shifterfx::dsp::ShifterEngine;
using shifterfx::dsp::TransportInfo;
using shifterfx::constants::kNumChannels;
using shifterfx::constants::Lane;

TEST_CASE("ShifterEngine::process performs zero heap allocations per block")
{
    ShifterEngine engine;
    engine.prepare(48000.0, 512, kNumChannels);
    engine.setStepArmed(Lane::Gate, 0, true);
    engine.setStepArmed(Lane::Reverse, 4, true);
    engine.setStepArmed(Lane::Repeat, 8, true);
    engine.setStepArmed(Lane::Echo, 12, true);
    engine.setLaneAmount(Lane::Gate, 0.7f);
    engine.setLaneAmount(Lane::Filter, 0.6f);
    engine.setLaneAmount(Lane::Bitcrush, 0.5f);
    engine.setLaneAmount(Lane::Repeat, 0.4f);
    engine.setLaneAmount(Lane::Echo, 0.3f);
    engine.setMix(0.85f);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    std::array<float*, kNumChannels> pointers { left.data(), right.data() };
    const TransportInfo transport { 128.0, 4.5, true };

    // Warm up once outside the tracked window: this is where any one-time lazy
    // initialisation (that would be a real bug if it happened here) gets shaken out
    // without contaminating the measurement below.
    engine.process(pointers, 512, kNumChannels, transport);

    g_allocationCount.store(0, std::memory_order_relaxed);
    g_trackAllocations.store(true, std::memory_order_relaxed);

    for (int block = 0; block < 20; ++block)
        engine.process(pointers, 512, kNumChannels, transport);

    g_trackAllocations.store(false, std::memory_order_relaxed);

    EXPECT_TRUE(g_allocationCount.load(std::memory_order_relaxed) == 0);
}
