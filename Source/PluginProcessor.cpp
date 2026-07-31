#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Utility/Math.h"

namespace shifterfx
{
    juce::String PluginProcessor::stepParamID(int lane, int step)
    {
        return juce::String("step_") + constants::kLaneIDs[static_cast<std::size_t>(lane)] + "_"
               + juce::String(step);
    }

    juce::String PluginProcessor::amountParamID(int lane)
    {
        return juce::String(constants::kLaneIDs[static_cast<std::size_t>(lane)]) + "Amount";
    }

    PluginProcessor::PluginProcessor()
        : AudioProcessor(BusesProperties()
                              .withInput("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
    {
        for (int lane = 0; lane < constants::kNumLanes; ++lane)
        {
            for (int step = 0; step < constants::kNumSteps; ++step)
                stepParams[static_cast<std::size_t>(lane)][static_cast<std::size_t>(step)] =
                    dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(stepParamID(lane, step)));

            if (static_cast<constants::Lane>(lane) != constants::Lane::Reverse)
                amountParams[static_cast<std::size_t>(lane)] = apvts.getRawParameterValue(amountParamID(lane));
        }

        rateParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("rate"));
        mixParam = apvts.getRawParameterValue("mix");
        outputGainParam = apvts.getRawParameterValue("outputGain");
    }

    juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        for (int lane = 0; lane < constants::kNumLanes; ++lane)
        {
            for (int step = 0; step < constants::kNumSteps; ++step)
            {
                const auto id = stepParamID(lane, step);
                const auto name = juce::String(constants::kLaneNames[static_cast<std::size_t>(lane)]) + " "
                                   + juce::String(step + 1);
                params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, false));
            }

            if (static_cast<constants::Lane>(lane) != constants::Lane::Reverse)
            {
                const auto id = amountParamID(lane);
                const auto name = juce::String(constants::kLaneNames[static_cast<std::size_t>(lane)]) + " Amount";
                params.push_back(std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID { id, 1 }, name,
                    juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 50.0f,
                    juce::AudioParameterFloatAttributes {}.withLabel("%")));
            }
        }

        // Index order must match constants::kRateQuarterFractions exactly.
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "rate", 1 }, "Rate", juce::StringArray { "1/32", "1/16", "1/8" },
            constants::kDefaultRateIndex));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
            juce::AudioParameterFloatAttributes {}.withLabel("%")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "outputGain", 1 }, "Output Gain",
            juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
            juce::AudioParameterFloatAttributes {}.withLabel("dB")));

        return { params.begin(), params.end() };
    }

    void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;
        shifterEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
        setLatencySamples(shifterEngine.getLatencySamples());
        status.latencySamples.store(shifterEngine.getLatencySamples(), std::memory_order_relaxed);
    }

    void PluginProcessor::releaseResources() { shifterEngine.reset(); }

    bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
    {
        const auto mainIn = layouts.getMainInputChannelSet();
        const auto mainOut = layouts.getMainOutputChannelSet();

        if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
            return false;

        return mainIn == mainOut;
    }

    void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = juce::jmin(buffer.getNumChannels(), constants::kNumChannels);

        for (auto ch = buffer.getNumChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear(ch, 0, numSamples);

        dsp::TransportInfo transport;
        if (auto* currentPlayHead = getPlayHead())
        {
            if (const auto position = currentPlayHead->getPosition())
            {
                transport.bpm = position->getBpm().orFallback(0.0);
                transport.ppqPosition = position->getPpqPosition().orFallback(-1.0);
                transport.isPlaying = position->getIsPlaying();
            }
        }

        for (int lane = 0; lane < constants::kNumLanes; ++lane)
        {
            for (int step = 0; step < constants::kNumSteps; ++step)
            {
                if (auto* param = stepParams[static_cast<std::size_t>(lane)][static_cast<std::size_t>(step)])
                    shifterEngine.setStepArmed(static_cast<constants::Lane>(lane), step, param->get());
            }

            if (auto* amount = amountParams[static_cast<std::size_t>(lane)])
                shifterEngine.setLaneAmount(static_cast<constants::Lane>(lane),
                                             amount->load(std::memory_order_relaxed) * 0.01f);
        }

        if (rateParam != nullptr)
            shifterEngine.setRateIndex(rateParam->getIndex());
        shifterEngine.setMix(mixParam->load(std::memory_order_relaxed) * 0.01f);

        std::array<float*, constants::kNumChannels> channelPointers {};
        for (int ch = 0; ch < numChannels; ++ch)
            channelPointers[static_cast<std::size_t>(ch)] = buffer.getWritePointer(ch);

        shifterEngine.process(channelPointers, numSamples, numChannels, transport);

        const auto newLatency = shifterEngine.getLatencySamples();
        if (newLatency != getLatencySamples())
            setLatencySamples(newLatency);

        const float outputGainDb = outputGainParam->load(std::memory_order_relaxed);
        const auto outputGain = static_cast<float>(util::decibelsToGain(static_cast<double>(outputGainDb)));
        buffer.applyGain(0, numSamples, outputGain);

        status.currentBPM.store(shifterEngine.getCurrentBPM(), std::memory_order_relaxed);
        status.latencySamples.store(newLatency, std::memory_order_relaxed);
        status.outputPeakLevel.store(buffer.getMagnitude(0, numSamples), std::memory_order_relaxed);
        status.currentStep.store(shifterEngine.getCurrentPatternStep(), std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor(*this); }

    void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
    {
        if (const auto state = apvts.copyState(); state.isValid())
        {
            if (const auto xml = state.createXml())
                copyXmlToBinary(*xml, destData);
        }
    }

    void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
    {
        if (auto xml = getXmlFromBinary(data, sizeInBytes))
            if (xml->hasTagName(apvts.state.getType()))
                apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

// This function must exist in the global namespace and is the standard JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new shifterfx::PluginProcessor();
}
