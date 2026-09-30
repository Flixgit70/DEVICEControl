#include "PageModel.h"
#include <algorithm>
namespace er
{
bool PageModel::removeControl(const juce::String& parameterPath)
{
    const auto oldSize = controls.size();
    controls.erase(std::remove_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    }), controls.end());
    return controls.size() != oldSize;
}

bool PageModel::setControlType(const juce::String& parameterPath, ControlType type)
{
    const auto it = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    });
    if (it == controls.end() || it->type == type)
        return false;
    it->type = type;
    return true;
}

bool PageModel::setControlColour(const juce::String& parameterPath, juce::uint32 colourArgb)
{
    const auto it = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    });
    if (it == controls.end() || it->colourArgb == colourArgb)
        return false;
    it->colourArgb = colourArgb;
    return true;
}

bool PageModel::setControlLabel2(const juce::String& parameterPath, const juce::String& label2)
{
    const auto it = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    });
    if (it == controls.end() || it->label2 == label2)
        return false;
    it->label2 = label2;
    return true;
}

bool PageModel::setControlThresholds(const juce::String& parameterPath,
                                     bool orangeAboveEnabled, double orangeAboveValue,
                                     bool redAboveEnabled, double redAboveValue,
                                     bool orangeBelowEnabled, double orangeBelowValue,
                                     bool redBelowEnabled, double redBelowValue)
{
    const auto it = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    });
    if (it == controls.end())
        return false;

    const bool changed = it->orangeAboveThresholdEnabled != orangeAboveEnabled
                      || it->orangeAboveThreshold != orangeAboveValue
                      || it->redAboveThresholdEnabled != redAboveEnabled
                      || it->redAboveThreshold != redAboveValue
                      || it->orangeBelowThresholdEnabled != orangeBelowEnabled
                      || it->orangeBelowThreshold != orangeBelowValue
                      || it->redBelowThresholdEnabled != redBelowEnabled
                      || it->redBelowThreshold != redBelowValue;
    if (!changed)
        return false;

    it->orangeAboveThresholdEnabled = orangeAboveEnabled;
    it->orangeAboveThreshold = orangeAboveValue;
    it->redAboveThresholdEnabled = redAboveEnabled;
    it->redAboveThreshold = redAboveValue;
    it->orangeBelowThresholdEnabled = orangeBelowEnabled;
    it->orangeBelowThreshold = orangeBelowValue;
    it->redBelowThresholdEnabled = redBelowEnabled;
    it->redBelowThreshold = redBelowValue;
    return true;
}

void PageModel::clearControls()
{
    controls.clear();
}

PageSet::PageSet()
{
    pageList.push_back({"1", {}});
    pageList.push_back({"2", {}});
    pageList.push_back({"3", {}});
}
int PageSet::addPage()
{
    auto n = (int) pageList.size() + 1;
    pageList.push_back({juce::String(n), {}});
    return n - 1;
}
}
