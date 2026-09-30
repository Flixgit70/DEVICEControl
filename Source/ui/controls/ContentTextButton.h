#pragma once
#include <JuceHeader.h>
#include "../ContentTextLayout.h"

namespace er
{
// Keep the normal TextButton click, keyboard and context-menu behaviour,
// but paint the complete content using the available width AND height.
class ContentTextButton final : public juce::TextButton
{
public:
    ContentTextButton() : juce::TextButton("Content") {}

    std::function<bool(const juce::MouseEvent&)> selectionMouseDown;

    void mouseDown(const juce::MouseEvent& event) override
    {
        selectionGesture = selectionMouseDown && selectionMouseDown(event);
        if (!selectionGesture)
            juce::TextButton::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!selectionGesture)
            juce::TextButton::mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (selectionGesture)
        {
            selectionGesture = false;
            return;
        }
        juce::TextButton::mouseUp(event);
    }

    void setContentText(const juce::String& text)
    {
        if (content == text) return;
        content = text;
        rebuildLines();
    }

    void resized() override
    {
        juce::TextButton::resized();
        rebuildLines();
    }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground(g, *this,
            findColour(getToggleState() ? buttonOnColourId : buttonColourId), over, down);
        const auto area = getLocalBounds().reduced(8, 6);
        g.saveState();
        g.reduceClipRegion(area);
        g.setFont(contentFont());
        g.setColour(findColour(textColourOffId).withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f));
        int y = area.getY();
        for (const auto& line : visibleLines)
        {
            // No automatic extra ellipsis: only layoutContentText adds "..."
            // to the last visible line when content actually overflows.
            g.drawText(line, area.getX(), y, area.getWidth(), lineHeight,
                       juce::Justification::centredLeft, false);
            y += lineHeight;
        }
        g.restoreState();
    }

private:
    static juce::Font contentFont() { return juce::Font(juce::FontOptions(13.0f)); }

    static juce::String toJuceString(const std::u32string& text)
    {
        juce::String result;
        for (const auto c : text) result += juce::String::charToString((juce::juce_wchar) c);
        return result;
    }

    void rebuildLines()
    {
        const auto area = getLocalBounds().reduced(8, 6);
        std::u32string characters;
        auto ptr = content.getCharPointer();
        while (!ptr.isEmpty()) characters += (char32_t) ptr.getAndAdvance();
        if (characters.empty()) characters = U"(empty)";
        const auto font = contentFont();
        const auto layout = layoutContentText(characters, (float) area.getWidth(),
            juce::jmax(0, area.getHeight() / lineHeight), [&font] (const std::u32string& line)
            {
                juce::GlyphArrangement glyphs;
                glyphs.addLineOfText(font, toJuceString(line), 0.0f, 0.0f);
                return glyphs.getBoundingBox(0, -1, true).getRight();
            });
        visibleLines.clear();
        for (const auto& line : layout.lines) visibleLines.add(toJuceString(line));
        repaint();
    }

    static constexpr int lineHeight = 17;
    bool selectionGesture = false;
    juce::String content;
    juce::StringArray visibleLines;
};
}
