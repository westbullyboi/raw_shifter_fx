#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <vector>

namespace shifterfx::dsp
{
    /**
     * RingBuffer - fixed-capacity circular sample store.
     *
     * The foundational storage primitive for every delay-based module in
     * ShifterFX (the step RecordBuffer, EchoEffect). Provides O(1) writes
     * and O(1) reads of a sample that is N positions behind the most
     * recently written one. Capacity is rounded up to a power of two so
     * wrap-around is a bitmask instead of a modulo.
     *
     * Not thread-safe by design - owned and exclusively touched by a
     * single audio-thread DSP object.
     */
    template <typename SampleType = float>
    class RingBuffer
    {
    public:
        /// Allocates storage for at least `minimumCapacity` samples (rounded up to
        /// a power of two). Not real-time safe - call during prepare(), never in processBlock().
        void resize(std::size_t minimumCapacity)
        {
            capacity = std::bit_ceil(std::max<std::size_t>(minimumCapacity, 2));
            indexMask = capacity - 1;
            buffer.assign(capacity, SampleType{});
            writeIndex = 0;
        }

        /// Zeroes the buffer and resets the write cursor without reallocating. Real-time safe.
        void clear() noexcept
        {
            std::fill(buffer.begin(), buffer.end(), SampleType{});
            writeIndex = 0;
        }

        /// Writes one sample and advances the cursor. Real-time safe.
        void push(SampleType sample) noexcept
        {
            buffer[writeIndex] = sample;
            writeIndex = (writeIndex + 1) & indexMask;
        }

        /// Returns the sample `delaySamples` behind the most recently pushed one
        /// (0 = most recent). `delaySamples` must be < capacity. Real-time safe.
        [[nodiscard]] SampleType readDelayed(std::size_t delaySamples) const noexcept
        {
            assert(delaySamples < capacity);
            const std::size_t index = (writeIndex - 1 - delaySamples) & indexMask;
            return buffer[index];
        }

        [[nodiscard]] std::size_t getCapacity() const noexcept { return capacity; }

    private:
        std::vector<SampleType> buffer;
        std::size_t capacity = 0;
        std::size_t indexMask = 0;
        std::size_t writeIndex = 0;
    };
}
