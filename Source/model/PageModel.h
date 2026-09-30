#pragma once
#include <JuceHeader.h>

namespace er
{
enum class ControlType { button, encoder, showContent, meter, matrix };
enum class PageViewMode { tiles, list };

/** One entry on a page Matrix axis. Folders may contain signals and more
    folders recursively; signalNumber is used only when folder is false. */
struct MatrixAxisNode
{
    juce::String id;
    juce::String name;
    bool folder = false;
    int signalNumber = -1;
    bool expanded = true;
    std::vector<MatrixAxisNode> children;

    static MatrixAxisNode signal(int number, const juce::String& label);
    static MatrixAxisNode createFolder(const juce::String& label = "New Folder");
};

struct MatrixControlData
{
    // Ember targets are displayed horizontally (X), sources vertically (Y).
    std::vector<MatrixAxisNode> targets;
    std::vector<MatrixAxisNode> sources;
};

struct ControlMapping
{
    juce::String parameterPath;
    juce::String label;
    ControlType type = ControlType::button;
    juce::String label2;
    juce::uint32 colourArgb = 0; // 0 = use application default accent

    // Optional per-control tile size. Zero keeps the control-type default.
    int tileWidth = 0;
    int tileHeight = 0;

    bool orangeAboveThresholdEnabled = false;
    double orangeAboveThreshold = 0.0;
    bool redAboveThresholdEnabled = false;
    double redAboveThreshold = 0.0;
    bool orangeBelowThresholdEnabled = false;
    double orangeBelowThreshold = 0.0;
    bool redBelowThresholdEnabled = false;
    double redBelowThreshold = 0.0;

    MatrixControlData matrix;
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
    bool setControlSize(const juce::String& parameterPath, int width, int height);
    bool setControlThresholds(const juce::String& parameterPath,
                              bool orangeAboveEnabled, double orangeAboveValue,
                              bool redAboveEnabled, double redAboveValue,
                              bool orangeBelowEnabled, double orangeBelowValue,
                              bool redBelowEnabled, double redBelowValue);
    bool moveControls(const std::vector<juce::String>& paths, int insertionIndex);
    void clearControls();
};

class PageSet
{
public:
    PageSet();
    std::vector<PageModel>& pages() noexcept { return pageList; }
    const std::vector<PageModel>& pages() const noexcept { return pageList; }
    int addPage();
    bool removePage(int index);
    bool movePage(int fromIndex, int toIndex);
    bool renamePage(int index, const juce::String& name);
private:
    std::vector<PageModel> pageList;
};
}
