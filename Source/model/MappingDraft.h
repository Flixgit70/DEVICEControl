#pragma once
#include <JuceHeader.h>
#include "PageModel.h"
#include "ParameterInfo.h"
#include "ProviderConfig.h"
#include <vector>
#include <optional>

namespace er
{
class ParameterStore;

struct MappingDraftRow
{
    bool enabled = true;
    juce::String path;
    juce::String provider;
    juce::String label;
    juce::String label2;
    ControlType type = ControlType::button;
    int page = 1;
    ParameterKind kind = ParameterKind::unknown;
    bool writable = false;
    juce::uint32 colourArgb = 0;
    bool orangeAboveThresholdEnabled = false;
    double orangeAboveThreshold = 0.0;
    bool redAboveThresholdEnabled = false;
    double redAboveThreshold = 0.0;
    bool orangeBelowThresholdEnabled = false;
    double orangeBelowThreshold = 0.0;
    bool redBelowThresholdEnabled = false;
    double redBelowThreshold = 0.0;
};

class MappingDraft
{
public:
    void clear();
    void addParameters(const std::vector<ParameterInfo>& params, int defaultPage);
    void replaceFromPages(const PageSet& pages, const ParameterStore* store = nullptr);
    void replaceFromPages(const PageSet& pages, std::function<std::optional<ParameterInfo>(const juce::String&)> resolver);
    void setAllEnabled(bool enabled);
    void setEnabledForPaths(const std::vector<juce::String>& paths, bool enabled);
    bool allEnabled() const;
    void removeRowsForPage(int page);
    bool setLabel2ForPath(const juce::String& path, const juce::String& label2);
    bool setTypeForPath(const juce::String& path, ControlType type);
    bool setThresholdsForPath(const juce::String& path,
                              bool orangeAboveEnabled, double orangeAboveValue,
                              bool redAboveEnabled, double redAboveValue,
                              bool orangeBelowEnabled, double orangeBelowValue,
                              bool redBelowEnabled, double redBelowValue);
    std::vector<MappingDraftRow>& rows() noexcept { return rowList; }
    const std::vector<MappingDraftRow>& rows() const noexcept { return rowList; }
    void applyTo(PageSet& pages) const;
    void applyToPage(PageSet& pages, int pageNumber);

private:
    std::vector<MappingDraftRow> rowList;
};
}
