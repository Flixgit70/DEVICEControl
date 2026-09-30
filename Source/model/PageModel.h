#pragma once
#include <JuceHeader.h>

namespace er
{
enum class ControlType { button, encoder, showContent };
enum class PageViewMode { tiles, list };

struct ControlMapping
{
    juce::String parameterPath;
    juce::String label;
    ControlType type = ControlType::button;
    juce::String label2;
    juce::uint32 colourArgb = 0; // 0 = use application default accent

    bool orangeAboveThresholdEnabled = false;
    double orangeAboveThreshold = 0.0;
    bool redAboveThresholdEnabled = false;
    double redAboveThreshold = 0.0;
    bool orangeBelowThresholdEnabled = false;
    double orangeBelowThreshold = 0.0;
    bool redBelowThresholdEnabled = false;
    double redBelowThreshold = 0.0;
};

struct PageModel
{
    juce::String name;
    std::vector<ControlMapping> controls;
    PageViewMode viewMode = PageViewMode::tiles;

    bool removeControl(const juce::String& parameterPath);
    bool setControlColour(const juce::String& parameterPath, juce::uint32 colourArgb);
    bool setControlType(const juce::String& parameterPath, ControlType type);
    bool setControlLabel2(const juce::String& parameterPath, const juce::String& label2);
    bool setControlThresholds(const juce::String& parameterPath,
                              bool orangeAboveEnabled, double orangeAboveValue,
                              bool redAboveEnabled, double redAboveValue,
                              bool orangeBelowEnabled, double orangeBelowValue,
                              bool redBelowEnabled, double redBelowValue);
    void clearControls();
};

class PageSet
{
public:
    PageSet();
    std::vector<PageModel>& pages() noexcept { return pageList; }
    const std::vector<PageModel>& pages() const noexcept { return pageList; }
    int addPage();
private:
    std::vector<PageModel> pageList;
};
}
