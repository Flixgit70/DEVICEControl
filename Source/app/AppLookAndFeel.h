#pragma once
#include <JuceHeader.h>
#include "Theme.h"

namespace er
{
class AppLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AppLookAndFeel()
    {
        auto scheme = getDarkColourScheme();
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::windowBackground, Theme::background);
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::widgetBackground, Theme::panelRaised);
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::menuBackground, Theme::panelRaised);
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::outline, Theme::outline);
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::defaultText, Theme::text);
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::defaultFill, Theme::accentDark);
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::highlightedText, juce::Colours::white);
        scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::UIColour::highlightedFill, Theme::accentBlue.withAlpha(0.55f));
        setColourScheme(scheme);

        setColour(juce::TextButton::buttonColourId, Theme::panelRaised);
        setColour(juce::TextButton::buttonOnColourId, Theme::accentDark);
        setColour(juce::TextButton::textColourOffId, Theme::text);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        setColour(juce::TextEditor::backgroundColourId, Theme::editor);
        setColour(juce::TextEditor::textColourId, Theme::textBright);
        setColour(juce::TextEditor::outlineColourId, Theme::outline);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBlue);

        setColour(juce::ComboBox::backgroundColourId, Theme::editor);
        setColour(juce::ComboBox::textColourId, Theme::textBright);
        setColour(juce::ComboBox::outlineColourId, Theme::outline);
        setColour(juce::ComboBox::arrowColourId, Theme::accentBlue);

        setColour(juce::PopupMenu::backgroundColourId, Theme::panelRaised);
        setColour(juce::PopupMenu::textColourId, Theme::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accentDark);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour(juce::Label::textColourId, Theme::text);
        setColour(juce::ListBox::backgroundColourId, Theme::panel);
        setColour(juce::TableHeaderComponent::backgroundColourId, Theme::panelRaised);
        setColour(juce::TableHeaderComponent::textColourId, Theme::text);
        setColour(juce::TableHeaderComponent::outlineColourId, Theme::outline);

        setColour(juce::Slider::rotarySliderOutlineColourId, Theme::outline);
        setColour(juce::Slider::rotarySliderFillColourId, Theme::accentBlue);
        setColour(juce::Slider::thumbColourId, Theme::textBright);
        setColour(juce::Slider::textBoxTextColourId, Theme::textBright);
        setColour(juce::Slider::textBoxBackgroundColourId, Theme::editor);
        setColour(juce::Slider::textBoxOutlineColourId, Theme::outline);

        setColour(juce::TreeView::backgroundColourId, Theme::panel);
        setColour(juce::TreeView::linesColourId, Theme::grid);
        setColour(juce::TreeView::selectedItemBackgroundColourId, Theme::accentDark);
        setColour(juce::TreeView::oddItemsColourId, juce::Colours::transparentBlack);
        setColour(juce::TreeView::evenItemsColourId, juce::Colours::transparentBlack);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto fill = backgroundColour;

        if (button.getToggleState())
            fill = Theme::accentDark;
        if (isMouseOverButton)
            fill = fill.interpolatedWith(Theme::accentBlue, 0.18f);
        if (isButtonDown)
            fill = fill.darker(0.18f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour((button.getToggleState() ? Theme::accentBlue : Theme::outline).withAlpha(0.95f));
        g.drawRoundedRectangle(bounds, 5.0f, button.getToggleState() ? 1.5f : 1.0f);
    }

    void drawTreeviewPlusMinusBox(juce::Graphics& g,
                                  const juce::Rectangle<float>& area,
                                  juce::Colour,
                                  bool isItemOpen,
                                  bool isMouseOver) override
    {
        auto box = area.reduced(5.0f, 4.0f);
        const auto cx = box.getCentreX();
        const auto cy = box.getCentreY();
        const auto halfW = juce::jmin(5.2f, box.getWidth() * 0.46f);
        const auto halfH = juce::jmin(4.4f, box.getHeight() * 0.46f);

        juce::Path triangle;
        if (isItemOpen)
        {
            triangle.startNewSubPath(cx - halfW, cy - halfH * 0.65f);
            triangle.lineTo(cx + halfW, cy - halfH * 0.65f);
            triangle.lineTo(cx, cy + halfH);
        }
        else
        {
            triangle.startNewSubPath(cx - halfW * 0.65f, cy - halfH);
            triangle.lineTo(cx + halfW, cy);
            triangle.lineTo(cx - halfW * 0.65f, cy + halfH);
        }
        triangle.closeSubPath();

        const auto colour = isMouseOver ? Theme::accentBlue.brighter(0.22f)
                                        : Theme::accentBlue;
        g.setColour(colour.withAlpha(0.18f));
        g.fillEllipse(area.getCentreX() - 7.0f, area.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour(colour);
        g.fillPath(triangle);
    }
};
}
