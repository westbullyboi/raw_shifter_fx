#include "PluginEditor.h"

namespace shifterfx
{
    void PluginEditor::RateButton::paintButton(juce::Graphics& g, bool highlighted, bool /*down*/)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(highlighted ? ui::Theme::gridLine.brighter(0.2f) : ui::Theme::panel);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(ui::Theme::gridLine);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        g.setColour(ui::Theme::text);
        g.setFont(ui::Theme::mono(14.0f));
        g.drawFittedText("RATE " + text, getLocalBounds(), juce::Justification::centred, 1);
    }

    PluginEditor::PluginEditor(PluginProcessor& p)
        : AudioProcessorEditor(&p), processorRef(p), stepGrid(p.apvts)
    {
        titleLabel.setText("RAW SHIFTER FX", juce::dontSendNotification);
        titleLabel.setFont(ui::Theme::display(22.0f));
        titleLabel.setColour(juce::Label::textColourId, ui::Theme::text);
        addAndMakeVisible(titleLabel);

        statusLabel.setText({}, juce::dontSendNotification);
        statusLabel.setFont(ui::Theme::mono(13.0f));
        statusLabel.setColour(juce::Label::textColourId, ui::Theme::textDim);
        statusLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(statusLabel);

        if (auto* rateParam = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("rate")))
            rateButton.text = rateParam->getCurrentChoiceName();
        rateButton.onClick = [this] { cycleRate(); };
        addAndMakeVisible(rateButton);

        addAndMakeVisible(stepGrid);

        const auto addKnob = [this](const juce::String& paramID, const juce::String& label, juce::Colour accent,
                                      const juce::String& unit = "%")
        {
            if (auto* param = processorRef.apvts.getParameter(paramID))
            {
                auto knob = std::make_unique<ui::AmountKnob>(*param, label, accent, unit);
                addAndMakeVisible(*knob);
                knobs.push_back(std::move(knob));
            }
        };

        addKnob("mix", "MIX", ui::Theme::text);
        addKnob("outputGain", "OUTPUT", ui::Theme::text, "dB");
        for (int lane = 0; lane < constants::kNumLanes; ++lane)
        {
            if (static_cast<constants::Lane>(lane) == constants::Lane::Reverse)
                continue;
            addKnob(PluginProcessor::amountParamID(lane), constants::kLaneNames[static_cast<std::size_t>(lane)],
                    ui::Theme::laneAccent[static_cast<std::size_t>(lane)]);
        }

        setSize(900, 560);
        startTimerHz(30);
    }

    PluginEditor::~PluginEditor() { stopTimer(); }

    void PluginEditor::cycleRate()
    {
        if (auto* rateParam = dynamic_cast<juce::AudioParameterChoice*>(processorRef.apvts.getParameter("rate")))
        {
            const int next = (rateParam->getIndex() + 1) % rateParam->choices.size();
            rateParam->beginChangeGesture();
            rateParam->setValueNotifyingHost(rateParam->convertTo0to1(static_cast<float>(next)));
            rateParam->endChangeGesture();
            rateButton.text = rateParam->getCurrentChoiceName();
            rateButton.repaint();
        }
    }

    void PluginEditor::paint(juce::Graphics& g) { g.fillAll(ui::Theme::background); }

    void PluginEditor::resized()
    {
        auto bounds = getLocalBounds().reduced(16);

        auto header = bounds.removeFromTop(48);
        titleLabel.setBounds(header.removeFromLeft(320));
        rateButton.setBounds(header.removeFromRight(120));
        statusLabel.setBounds(header);

        bounds.removeFromTop(12);

        auto knobRow = bounds.removeFromBottom(110);
        bounds.removeFromBottom(12);

        stepGrid.setBounds(bounds);

        const int knobWidth = knobRow.getWidth() / juce::jmax(1, static_cast<int>(knobs.size()));
        for (auto& knob : knobs)
            knob->setBounds(knobRow.removeFromLeft(knobWidth).reduced(6));
    }

    void PluginEditor::timerCallback()
    {
        const auto& status = processorRef.getStatusSnapshot();
        stepGrid.setCurrentStep(status.currentStep.load(std::memory_order_relaxed));

        const auto bpm = status.currentBPM.load(std::memory_order_relaxed);
        const auto latencyMs = status.latencySamples.load(std::memory_order_relaxed) * 1000.0
                                / juce::jmax(1.0, processorRef.getSampleRate());
        statusLabel.setText(juce::String(bpm, 1) + " BPM   LATENCY " + juce::String(latencyMs, 1) + " MS",
                              juce::dontSendNotification);
    }
}
