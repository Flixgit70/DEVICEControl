#pragma once
#include <JuceHeader.h>
#include "../app/Theme.h"
#include <functional>

namespace er
{
class StringContentView final : public juce::Component
{
public:
    StringContentView(juce::String titleText,
                      juce::String content,
                      bool canWrite,
                      std::function<void(const juce::String&)> writeCallback)
        : writable(canWrite), onWrite(std::move(writeCallback))
    {
        title.setText(std::move(titleText), juce::dontSendNotification);
        title.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
        title.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(title);

        editor.setMultiLine(true, true);
        editor.setReturnKeyStartsNewLine(true);
        editor.setReadOnly(!writable);
        editor.setScrollbarsShown(true);
        editor.setCaretVisible(writable);
        editor.setSelectAllWhenFocused(false);
        editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        editor.setColour(juce::TextEditor::backgroundColourId, Theme::panelRaised);
        editor.setColour(juce::TextEditor::outlineColourId, Theme::outline);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBlue);
        editor.setText(std::move(content), false);
        originalContent = editor.getText();
        addAndMakeVisible(editor);

        status.setText(writable ? "Editable string parameter" : "Read only string parameter",
                       juce::dontSendNotification);
        status.setFont(juce::FontOptions(10.0f));
        status.setColour(juce::Label::textColourId, juce::Colours::grey);
        status.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(status);

        writeButton.setButtonText("Write");
        writeButton.setVisible(writable);
        writeButton.setEnabled(writable);
        writeButton.onClick = [this]
        {
            if (!writable || !onWrite)
                return;

            onWrite(editor.getText());
            originalContent = editor.getText();
            status.setText("Write requested", juce::dontSendNotification);
        };
        addAndMakeVisible(writeButton);

        revertButton.setButtonText("Revert");
        revertButton.setVisible(writable);
        revertButton.setEnabled(writable);
        revertButton.onClick = [this]
        {
            editor.setText(originalContent, false);
            status.setText("Changes reverted", juce::dontSendNotification);
        };
        addAndMakeVisible(revertButton);

        setSize(600, 380);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Theme::background);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10);
        title.setBounds(r.removeFromTop(24));
        r.removeFromTop(6);

        auto footer = r.removeFromBottom(30);
        status.setBounds(footer.removeFromLeft(juce::jmax(140, footer.getWidth() - (writable ? 180 : 0))));
        if (writable)
        {
            footer.removeFromLeft(6);
            revertButton.setBounds(footer.removeFromLeft(78));
            footer.removeFromLeft(6);
            writeButton.setBounds(footer.removeFromLeft(78));
        }

        r.removeFromBottom(6);
        editor.setBounds(r);
    }

private:
    bool writable = false;
    std::function<void(const juce::String&)> onWrite;
    juce::String originalContent;
    juce::Label title;
    juce::TextEditor editor;
    juce::Label status;
    juce::TextButton revertButton { "Revert" };
    juce::TextButton writeButton { "Write" };
};

inline void showStringContent(juce::Component& anchor,
                              const juce::String& title,
                              const juce::String& content,
                              bool writable = false,
                              std::function<void(const juce::String&)> onWrite = {})
{
    auto view = std::make_unique<StringContentView>(title, content, writable, std::move(onWrite));
    auto& box = juce::CallOutBox::launchAsynchronously(std::move(view), anchor.getScreenBounds(), nullptr);
    box.setDismissalMouseClicksAreAlwaysConsumed(true);
}
}
