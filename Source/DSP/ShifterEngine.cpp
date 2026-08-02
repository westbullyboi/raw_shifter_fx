#include "ShifterEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shifterfx::dsp
{
    void ShifterEngine::prepare(double sampleRateIn, int /*maxBlockSize*/, int numChannels) noexcept
    {
        sampleRate = sampleRateIn;
        stepClock.prepare(sampleRate);

        const double worstCaseStepSeconds = constants::kRateQuarterFractions.back()
                                             * util::quarterNoteDurationSeconds(constants::kMinSupportedBPM);
        worstCaseStepLenSamples = static_cast<std::int64_t>(std::ceil(worstCaseStepSeconds * sampleRate)) + 1;
        recordBufferCapacity = static_cast<std::size_t>(worstCaseStepLenSamples) * 4;

        for (int ch = 0; ch < numChannels && ch < constants::kNumChannels; ++ch)
        {
            recordBuffer[static_cast<std::size_t>(ch)].resize(recordBufferCapacity);
            filter[static_cast<std::size_t>(ch)].prepare(sampleRate);
            filter[static_cast<std::size_t>(ch)].setCutoffSlewMs(constants::kFilterCutoffSlewMs);
            bitcrusher[static_cast<std::size_t>(ch)].reset();
            echo[static_cast<std::size_t>(ch)].prepare(static_cast<std::size_t>(worstCaseStepLenSamples) + 8);
        }

        lastAbsoluteStepIndex = std::numeric_limits<long long>::min();
        positionInStep = 0;
        currentPatternStepIndex = 0;
        lastRateIndex = stepClock.getRateIndex();
        lastBPM = constants::kDefaultBPM;
        updateForTempo(lastBPM);
    }

    void ShifterEngine::reset() noexcept
    {
        for (auto& buffer : recordBuffer)
            buffer.clear();
        for (auto& f : filter)
            f.reset();
        for (auto& b : bitcrusher)
            b.reset();
        for (auto& e : echo)
            e.reset();

        stepClock.reset();
        lastAbsoluteStepIndex = std::numeric_limits<long long>::min();
        positionInStep = 0;
    }

    void ShifterEngine::updateForTempo(double newBpm) noexcept
    {
        lastBPM = newBpm;
        lastRateIndex = stepClock.getRateIndex();

        const double raw = stepClock.stepLengthSamples(newBpm);
        currentStepLenSamples = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(raw)), 64,
                                                            worstCaseStepLenSamples);
    }

    void ShifterEngine::process(const std::array<float*, constants::kNumChannels>& channelPointers, int numSamples,
                                 int numChannels, const TransportInfo& transport) noexcept
    {
        const double rawBpm = transport.bpm > 0.0 ? transport.bpm : lastBPM;
        if (std::abs(rawBpm - lastBPM) > constants::kTempoChangeThresholdBPM
            || stepClock.getRateIndex() != lastRateIndex)
            updateForTempo(rawBpm);

        stepClock.resyncTo(transport.ppqPosition);

        const auto gateRampSamples = std::max<std::int64_t>(
            1, static_cast<std::int64_t>(util::msToSamples(constants::kGateRampMs, sampleRate)));
        const auto repeatDivisions = std::clamp(
            static_cast<int>(std::lround(util::lerp(static_cast<float>(constants::kRepeatMinDivisions),
                                                       static_cast<float>(constants::kRepeatMaxDivisions),
                                                       laneAmount[static_cast<std::size_t>(constants::Lane::Repeat)]))),
            constants::kRepeatMinDivisions, constants::kRepeatMaxDivisions);

        for (int n = 0; n < numSamples; ++n)
        {
            // The buffer read below fetches audio from `currentStepLenSamples` samples ago (see
            // the class doc comment's latency model), i.e. exactly one whole step behind the
            // live transport position - so the pattern flags that should govern *this* output
            // sample belong to the step *before* wherever the live clock currently sits, not the
            // live step itself (which is still being recorded into the buffer this same sample).
            const long long outputAbsoluteStep = stepClock.getAbsoluteStepIndex() - 1;
            if (outputAbsoluteStep != lastAbsoluteStepIndex)
            {
                lastAbsoluteStepIndex = outputAbsoluteStep;
                positionInStep = 0;
                currentPatternStepIndex = static_cast<int>(
                    ((outputAbsoluteStep % constants::kNumSteps) + constants::kNumSteps) % constants::kNumSteps);
                for (auto& b : bitcrusher)
                    b.reset();
            }

            const auto lane = [this](constants::Lane l) -> bool
            {
                return pattern[static_cast<std::size_t>(l)][static_cast<std::size_t>(currentPatternStepIndex)];
            };
            const bool gateArmed = lane(constants::Lane::Gate);
            const bool panArmed = lane(constants::Lane::Pan);
            const bool filterArmed = lane(constants::Lane::Filter);
            const bool crushArmed = lane(constants::Lane::Bitcrush);
            const bool repeatArmed = lane(constants::Lane::Repeat);
            const bool reverseArmed = lane(constants::Lane::Reverse);
            const bool echoArmed = lane(constants::Lane::Echo);

            std::int64_t sourceDelay = currentStepLenSamples;
            if (reverseArmed)
                sourceDelay = remap::reverseDelaySamples(currentStepLenSamples, positionInStep);
            else if (repeatArmed)
                sourceDelay = remap::repeatDelaySamples(currentStepLenSamples, positionInStep, repeatDivisions);
            sourceDelay = std::clamp<std::int64_t>(sourceDelay, 0, static_cast<std::int64_t>(recordBufferCapacity) - 1);

            const std::int64_t dryDelay = std::clamp<std::int64_t>(currentStepLenSamples, 0,
                                                                      static_cast<std::int64_t>(recordBufferCapacity) - 1);

            const float stepPhase = currentStepLenSamples > 0
                                       ? static_cast<float>(positionInStep) / static_cast<float>(currentStepLenSamples)
                                       : 0.0f;
            // Both the Amount->floor mapping and the phase->sweep within the step must move in
            // log-frequency space, not linear Hz: a linear lerp from 18kHz down to a low floor
            // spends almost the whole range sounding like "still basically open" to the ear,
            // which is why the Filter lane read as weak at moderate Amount/step-phase settings.
            const float filterFloorHz = util::lerpLogFrequency(
                constants::kFilterOpenHz, constants::kFilterMinHz,
                laneAmount[static_cast<std::size_t>(constants::Lane::Filter)]);
            const float targetCutoffHz = filterArmed
                                            ? util::lerpLogFrequency(constants::kFilterOpenHz, filterFloorHz, stepPhase)
                                            : constants::kFilterOpenHz;
            const float filterQ = util::lerp(constants::kFilterMinQ, constants::kFilterMaxQ,
                                              laneAmount[static_cast<std::size_t>(constants::Lane::Filter)]);

            const auto holdPeriodSamples = static_cast<int>(std::lround(
                util::lerp(static_cast<float>(constants::kBitcrushMinHoldSamples),
                           static_cast<float>(constants::kBitcrushMaxHoldSamples),
                           laneAmount[static_cast<std::size_t>(constants::Lane::Bitcrush)])));
            const float crushBits = util::lerp(constants::kBitcrushMaxBits, constants::kBitcrushMinBits,
                                                laneAmount[static_cast<std::size_t>(constants::Lane::Bitcrush)]);

            const float gateAmount = gateArmed ? laneAmount[static_cast<std::size_t>(constants::Lane::Gate)] : 0.0f;

            const auto echoDelaySamples = std::clamp<std::int64_t>(
                static_cast<std::int64_t>(std::llround(currentStepLenSamples * constants::kEchoDelayStepFraction)), 1,
                static_cast<std::int64_t>(worstCaseStepLenSamples));
            // Feedback decay is exponential in repeat count, so a linear Amount->feedback mapping
            // spends most of its lower range decaying below audibility within a step or two; a
            // sqrt curve front-loads the useful range so mid-range Amount already yields a tail
            // that survives several repeats.
            const float echoFeedback = std::sqrt(laneAmount[static_cast<std::size_t>(constants::Lane::Echo)])
                                        * constants::kEchoMaxFeedback;

            std::array<float, constants::kNumChannels> wetChannel {};
            std::array<float, constants::kNumChannels> dryChannel {};

            const int channelsThisSample = std::min(numChannels, constants::kNumChannels);
            for (int ch = 0; ch < channelsThisSample; ++ch)
            {
                auto& buffer = recordBuffer[static_cast<std::size_t>(ch)];
                buffer.push(channelPointers[static_cast<std::size_t>(ch)][n]);

                dryChannel[static_cast<std::size_t>(ch)] = buffer.readDelayed(static_cast<std::size_t>(dryDelay));

                float wet = buffer.readDelayed(static_cast<std::size_t>(sourceDelay));
                if (filterArmed)
                    wet = filter[static_cast<std::size_t>(ch)].process(wet, targetCutoffHz, filterQ);
                if (crushArmed)
                    wet = bitcrusher[static_cast<std::size_t>(ch)].process(wet, holdPeriodSamples, crushBits);
                wet *= gate::computeGain(positionInStep, currentStepLenSamples, gateRampSamples, gateAmount);

                wetChannel[static_cast<std::size_t>(ch)] = wet;
            }

            if (channelsThisSample == 2)
            {
                const auto [pannedL, pannedR] = pan::process(
                    wetChannel[0], wetChannel[1], outputAbsoluteStep,
                    panArmed ? laneAmount[static_cast<std::size_t>(constants::Lane::Pan)] : 0.0f);
                wetChannel[0] = pannedL;
                wetChannel[1] = pannedR;
            }

            for (int ch = 0; ch < channelsThisSample; ++ch)
            {
                const float echoTail = echo[static_cast<std::size_t>(ch)].process(
                    wetChannel[static_cast<std::size_t>(ch)], echoArmed, echoDelaySamples, echoFeedback);
                const float wetFinal = wetChannel[static_cast<std::size_t>(ch)] + echoTail;
                const float dry = dryChannel[static_cast<std::size_t>(ch)];

                channelPointers[static_cast<std::size_t>(ch)][n] = dry * (1.0f - mix) + wetFinal * mix;
            }

            stepClock.advance(lastBPM);
            ++positionInStep;
        }
    }
}
