#pragma once
#include <JuceHeader.h>
#include "../app/Theme.h"

namespace er
{
class BrandTitle final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;

    BrandTitle()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("About DEVICEControl");
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const auto font = juce::Font(juce::FontOptions(20.5f, juce::Font::bold | juce::Font::italic));

        g.setFont(font);
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawText("DEVICEControl", area.translated(1.5f, 1.5f), juce::Justification::centredLeft);

        g.setColour(Theme::accentBlue);
        g.drawText("DEVICEControl", area, juce::Justification::centredLeft);

        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawText("DEVICEControl", area.translated(0.0f, -0.7f), juce::Justification::centredLeft);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }
};
}
