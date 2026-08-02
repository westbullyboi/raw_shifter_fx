#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace shifterfx::ui
{
    /// Small custom-painted rotary control bound directly to a juce::RangedAudioParameter.
    /// Vertical drag (120px = full range) writes through begin/endChangeGesture +
    /// setValueNotifyingHost, so it behaves exactly like host automation.
    class AmountKnob final : public juce::Component
    {
    public:
        AmountKnob(juce::RangedAudioParameter& p, juce::String labelText, juce::Colour accentColour,
                    juce::String unitSuffix = "%")
            : param(p), label(std::move(labelText)), accent(accentColour), unit(std::move(unitSuffix))
        {
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            const auto dial = bounds.removeFromTop(bounds.getWidth()).reduced(6.0f);

            g.setColour(Theme::panel);
            g.fillEllipse(dial);

            const float value01 = param.getValue();
            constexpr float startAngle = juce::MathConstants<float>::pi * 1.2f;
            constexpr float endAngle = juce::MathConstants<float>::pi * 2.8f;
            const float angle = startAngle + (endAngle - startAngle) * value01;

            juce::Path arc;
            arc.addArc(dial.getX(), dial.getY(), dial.getWidth(), dial.getHeight(), startAngle, angle, true);
            g.setColour(accent);
            g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour(Theme::gridLine);
            g.drawEllipse(dial, 1.0f);

            g.setColour(Theme::text);
            g.setFont(Theme::mono(11.0f));
            g.drawFittedText(juce::String(juce::roundToInt(param.getNormalisableRange().convertFrom0to1(value01)))
                                  + unit,
                              dial.toNearestInt(), juce::Justification::centred, 1);

            g.setColour(Theme::textDim);
            g.setFont(Theme::display(11.0f));
            g.drawFittedText(label, getLocalBounds().removeFromBottom(16), juce::Justification::centred, 1);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            startValue = param.getValue();
            startY = static_cast<float>(e.position.y);
            param.beginChangeGesture();
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            const float delta = (startY - static_cast<float>(e.position.y)) / 120.0f;
            param.setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, startValue + delta));
            repaint();
        }

        void mouseUp(const juce::MouseEvent&) override { param.endChangeGesture(); }

        void mouseDoubleClick(const juce::MouseEvent&) override
        {
            param.beginChangeGesture();
            param.setValueNotifyingHost(param.getDefaultValue());
            param.endChangeGesture();
            repaint();
        }

    private:
        juce::RangedAudioParameter& param;
        juce::String label;
        juce::Colour accent;
        juce::String unit;
        float startValue = 0.0f;
        float startY = 0.0f;
    };
}
