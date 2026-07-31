#pragma once

#include <algorithm>
#include <cstdint>

#include "RingBuffer.h"

namespace shifterfx::dsp
{
    /**
     * EchoEffect - feedback delay "throw" for the Echo lane.
     *
     * Purpose:
     *   Unlike the other lanes, Echo is always running: an armed step
     *   feeds its audio into the delay line, but the resulting tail keeps
     *   decaying (and gets mixed in) for as long as feedback sustains it,
     *   independent of whether the *current* step is armed. That is what
     *   makes a "throw" audible after the triggering step ends.
     *
     * Thread safety:
     *   Not thread-safe; one instance per audio channel.
     */
    class EchoEffect
    {
    public:
        void prepare(std::size_t maxDelaySamples) { buffer.resize(maxDelaySamples + 1); }
        void reset() noexcept { buffer.clear(); }

        [[nodiscard]] float process(float input, bool feedInput, std::int64_t delaySamples, float feedback) noexcept
        {
            const auto delay = static_cast<std::size_t>(std::max<std::int64_t>(delaySamples, 1));
            const float delayed = buffer.readDelayed(std::min<std::size_t>(delay - 1, buffer.getCapacity() - 1));
            const float toPush = (feedInput ? input : 0.0f) + delayed * feedback;
            buffer.push(toPush);
            return delayed;
        }

    private:
        RingBuffer<float> buffer;
    };
}
