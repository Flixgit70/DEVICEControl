#pragma once
#include <JuceHeader.h>

namespace er
{
inline juce::String makeContentPreview(juce::String text,
                                       int maxWords = 10,
                                       int maxCharacters = 90)
{
    text = text.replace("\r\n", " ")
               .replace("\n", " ")
               .replace("\r", " ")
               .replace("\t", " ")
               .trim();

    if (text.isEmpty())
        return "(empty)";

    juce::StringArray words;
    words.addTokens(text, " ", "");
    words.removeEmptyStrings();

    juce::String preview;
    bool truncated = false;
    const int wordLimit = juce::jmax(1, maxWords);
    const int charLimit = juce::jmax(8, maxCharacters);

    for (int i = 0; i < words.size(); ++i)
    {
        if (i >= wordLimit)
        {
            truncated = true;
            break;
        }

        const auto candidate = preview.isEmpty() ? words[i] : preview + " " + words[i];
        if (candidate.length() > charLimit)
        {
            if (preview.isEmpty())
                preview = words[i].substring(0, juce::jmax(1, charLimit - 3));
            truncated = true;
            break;
        }
        preview = candidate;
    }

    if (words.size() > wordLimit)
        truncated = true;

    if (preview.isEmpty())
        preview = text.substring(0, juce::jmax(1, charLimit - 3));

    return preview + (truncated ? "..." : juce::String());
}
}
