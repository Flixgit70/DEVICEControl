#pragma once
#include <JuceHeader.h>
#include "../app/Theme.h"

namespace er
{
class AboutComponent final : public juce::Component
{
public:
    AboutComponent()
    {
        setSize(500, 430);
        setOpaque(false);
        addAndMakeVisible(okButton);
        okButton.onClick = [this]
        {
            if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
                dialog->exitModalState(0);
        };
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(8.0f);

        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillRoundedRectangle(bounds.translated(4.0f, 5.0f), 18.0f);

        g.setColour(Theme::background);
        g.fillRoundedRectangle(bounds, 18.0f);
        g.setColour(juce::Colours::white.withAlpha(0.78f));
        g.drawRoundedRectangle(bounds, 18.0f, 1.5f);

        auto title = bounds.reduced(30.0f, 20.0f);
        title.setHeight(58.0f);
        g.setFont(juce::Font(juce::FontOptions(29.0f, juce::Font::bold | juce::Font::italic)));
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawText("DEVICEControl", title.translated(2.0f, 2.0f), juce::Justification::centred);
        g.setColour(Theme::accentBlue);
        g.drawText("DEVICEControl", title, juce::Justification::centred);

        g.setColour(Theme::textBright);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText("Release Version " JUCE_APPLICATION_VERSION_STRING,
                   40, 91, getWidth() - 80, 24, juce::Justification::centred);

        g.setColour(Theme::text);
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("Created by Felix Krueckels (fkrueckels)",
                   40, 122, getWidth() - 80, 22, juce::Justification::centred);
        g.drawText("fkrueckels@gmail.com",
                   40, 145, getWidth() - 80, 22, juce::Justification::centred);
        g.drawText("K2Consulting",
                   40, 168, getWidth() - 80, 22, juce::Justification::centred);

        const auto panel = juce::Rectangle<float>(42.0f, 198.0f, (float) getWidth() - 84.0f, 150.0f);
        g.setColour(Theme::panel);
        g.fillRoundedRectangle(panel, 8.0f);
        g.setColour(Theme::outline);
        g.drawRoundedRectangle(panel, 8.0f, 1.0f);

        g.setColour(Theme::accentBlue);
        g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        g.drawText("Based on / third-party components", 58, 210, getWidth() - 116, 22,
                   juce::Justification::centredLeft);

        g.setColour(Theme::text);
        g.setFont(juce::FontOptions(13.0f));
        const juce::String libraries =
            "JUCE - application framework\n"
            "Lawo ember-plus v1.8.2 - Ember+ protocol implementation\n"
            "libember - Glow / Ember+ data model\n"
            "libs101 - S101 transport framing\n"
            "CMake - build system";
        g.drawFittedText(libraries, 58, 236, getWidth() - 116, 96,
                         juce::Justification::topLeft, 6, 1.0f);

        g.setColour(Theme::textDim);
        g.setFont(juce::FontOptions(10.8f));
        g.drawText("Third-party components are used under their respective licenses.",
                   42, 354, getWidth() - 84, 20, juce::Justification::centred);
    }

    void resized() override
    {
        okButton.setBounds(getWidth() / 2 - 44, getHeight() - 45, 88, 26);
    }

private:
    juce::TextButton okButton { "OK" };
};

inline void showAboutDialog()
{
    juce::DialogWindow::LaunchOptions options;

#if JUCE_LINUX
    options.dialogTitle = "About DEVICEControl";
    options.dialogBackgroundColour = Theme::background;
    options.useNativeTitleBar = true;
#else
    options.dialogTitle = "";
    options.dialogBackgroundColour = juce::Colours::transparentBlack;
    options.useNativeTitleBar = false;
#endif

    options.escapeKeyTriggersCloseButton = true;
    options.resizable = false;
    options.componentToCentreAround =
        juce::TopLevelWindow::getActiveTopLevelWindow();
    options.content.setOwned(new AboutComponent());

    if (auto* window = options.launchAsync())
    {
#if JUCE_LINUX
        window->toFront(true);
#else
        window->setOpaque(false);
        window->setUsingNativeTitleBar(false);
        window->setDropShadowEnabled(true);
#endif
    }
}
}
