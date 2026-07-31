#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "../PluginProcessor.h"

namespace shifterfx::ui
{
    /// The step-sequencer grid: dsp::constants::kNumLanes rows x kNumSteps columns of
    /// click-to-toggle cells, one per effect lane, plus a scrolling playhead highlight.
    /// Writes directly to the bound bool parameters via begin/endChangeGesture +
    /// setValueNotifyingHost, exactly like host automation.
    class StepGridView final : public juce::Component
    {
    public:
        explicit StepGridView(juce::AudioProcessorValueTreeState& stateIn) : state(stateIn)
        {
            for (int lane = 0; lane < constants::kNumLanes; ++lane)
                for (int step = 0; step < constants::kNumSteps; ++step)
                    cells[static_cast<std::size_t>(lane)][static_cast<std::size_t>(step)] =
                        dynamic_cast<juce::AudioParameterBool*>(
                            state.getParameter(PluginProcessor::stepParamID(lane, step)));
        }

        void setCurrentStep(int step) noexcept
        {
            if (step != currentStep)
            {
                currentStep = step;
                repaint();
            }
        }

        void paint(juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float labelWidth = 78.0f;
            const float cellWidth = (bounds.getWidth() - labelWidth) / static_cast<float>(constants::kNumSteps);
            const float rowHeight = bounds.getHeight() / static_cast<float>(constants::kNumLanes);

            for (int lane = 0; lane < constants::kNumLanes; ++lane)
            {
                const float rowY = static_cast<float>(lane) * rowHeight;
                g.setColour(Theme::textDim);
                g.setFont(Theme::display(11.0f));
                g.drawFittedText(constants::kLaneNames[static_cast<std::size_t>(lane)],
                                  juce::Rectangle<int>(0, static_cast<int>(rowY), static_cast<int>(labelWidth),
                                                        static_cast<int>(rowHeight)),
                                  juce::Justification::centredLeft, 1);

                for (int step = 0; step < constants::kNumSteps; ++step)
                {
                    const float cellX = labelWidth + static_cast<float>(step) * cellWidth;
                    const juce::Rectangle<float> cell { cellX + 1.5f, rowY + 1.5f, cellWidth - 3.0f,
                                                          rowHeight - 3.0f };

                    if (step == currentStep)
                    {
                        g.setColour(Theme::playhead);
                        g.fillRect(juce::Rectangle<float>(cellX, rowY, cellWidth, rowHeight));
                    }

                    const bool armed = isArmed(lane, step);
                    g.setColour(armed ? Theme::laneAccent[static_cast<std::size_t>(lane)]
                                        : Theme::panel);
                    g.fillRoundedRectangle(cell, 3.0f);

                    // Beat markers (every 4th step) get a visibly brighter outline so the pattern
                    // reads at a glance instead of requiring the viewer to count cells.
                    g.setColour(step % 4 == 0 ? Theme::gridLine.brighter(0.3f) : Theme::gridLine);
                    g.drawRoundedRectangle(cell, 3.0f, 1.0f);
                }
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            const auto hit = cellAt(e.position);
            if (hit.first < 0)
                return;

            dragSetValue = !isArmed(hit.first, hit.second);
            setArmed(hit.first, hit.second, dragSetValue);
            lastDragged = hit;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            const auto hit = cellAt(e.position);
            if (hit.first < 0 || hit == lastDragged)
                return;

            setArmed(hit.first, hit.second, dragSetValue);
            lastDragged = hit;
        }

    private:
        [[nodiscard]] bool isArmed(int lane, int step) const noexcept
        {
            const auto* p = cells[static_cast<std::size_t>(lane)][static_cast<std::size_t>(step)];
            return p != nullptr && p->get();
        }

        void setArmed(int lane, int step, bool armed)
        {
            if (auto* p = cells[static_cast<std::size_t>(lane)][static_cast<std::size_t>(step)])
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(armed ? 1.0f : 0.0f);
                p->endChangeGesture();
                repaint();
            }
        }

        [[nodiscard]] std::pair<int, int> cellAt(juce::Point<float> position) const noexcept
        {
            const float labelWidth = 78.0f;
            if (position.x < labelWidth)
                return { -1, -1 };

            const float cellWidth = (static_cast<float>(getWidth()) - labelWidth)
                                     / static_cast<float>(constants::kNumSteps);
            const float rowHeight = static_cast<float>(getHeight()) / static_cast<float>(constants::kNumLanes);

            const int lane = static_cast<int>(position.y / rowHeight);
            const int step = static_cast<int>((position.x - labelWidth) / cellWidth);

            if (lane < 0 || lane >= constants::kNumLanes || step < 0 || step >= constants::kNumSteps)
                return { -1, -1 };
            return { lane, step };
        }

        juce::AudioProcessorValueTreeState& state;
        std::array<std::array<juce::AudioParameterBool*, constants::kNumSteps>, constants::kNumLanes> cells {};
        int currentStep = -1;
        bool dragSetValue = true;
        std::pair<int, int> lastDragged { -1, -1 };
    };
}
