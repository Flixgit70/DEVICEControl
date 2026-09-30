#include "PageModel.h"
#include <set>
#include <algorithm>
namespace er
{
MatrixAxisNode MatrixAxisNode::signal(int number, const juce::String& label)
{
    MatrixAxisNode node;
    node.id = "signal:" + juce::String(number);
    node.name = label.isNotEmpty() ? label : juce::String(number);
    node.signalNumber = number;
    return node;
}

MatrixAxisNode MatrixAxisNode::createFolder(const juce::String& label)
{
    MatrixAxisNode node;
    node.id = "folder:" + juce::Uuid().toString();
    node.name = label.isNotEmpty() ? label : juce::String("New Folder");
    node.folder = true;
    return node;
}

bool PageModel::removeControl(const juce::String& parameterPath)
{
    const auto oldSize = controls.size();
    controls.erase(std::remove_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    }), controls.end());
    return controls.size() != oldSize;
}

bool PageModel::moveControls(const std::vector<juce::String>& paths, int insertionIndex)
{
    if (paths.empty() || insertionIndex < 0 || insertionIndex > (int) controls.size()) return false;
    const std::set<juce::String> selection(paths.begin(), paths.end());
    std::vector<ControlMapping> moved, remaining;
    int beforeInsertion = 0;
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        const auto& mapping = controls[(size_t) i];
        if (selection.count(mapping.parameterPath) != 0)
        {
            if (i < insertionIndex) ++beforeInsertion;
            moved.push_back(mapping);
        }
        else remaining.push_back(mapping);
    }
    if (moved.empty()) return false;
    const auto target = (size_t) (insertionIndex - beforeInsertion);
    remaining.insert(remaining.begin() + (std::ptrdiff_t) target, moved.begin(), moved.end());
    if (std::equal(controls.begin(), controls.end(), remaining.begin(), [] (const auto& a, const auto& b)
    {
        return a.parameterPath == b.parameterPath;
    })) return false;
    controls = std::move(remaining);
    return true;
}

bool PageModel::setControlType(const juce::String& parameterPath, ControlType type)
{
    const auto it = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    });
    if (it == controls.end() || it->type == type
        || it->type == ControlType::matrix || type == ControlType::matrix)
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

bool PageModel::setControlSize(const juce::String& parameterPath, int width, int height)
{
    const auto it = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
    {
        return mapping.parameterPath == parameterPath;
    });
    if (it == controls.end())
        return false;

    width = juce::jmax(0, width);
    height = juce::jmax(0, height);
    if (it->tileWidth == width && it->tileHeight == height)
        return false;

    it->tileWidth = width;
    it->tileHeight = height;
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
    int n = (int) pageList.size() + 1;
    const auto nameExists = [this] (const juce::String& candidate)
    {
        return std::any_of(pageList.begin(), pageList.end(), [&candidate] (const PageModel& page)
        {
            return page.name == candidate;
        });
    };
    while (nameExists(juce::String(n)))
        ++n;
    pageList.push_back({juce::String(n), {}});
    return n - 1;
}

bool PageSet::removePage(int index)
{
    if (pageList.size() <= 1 || index < 0 || index >= (int) pageList.size())
        return false;

    pageList.erase(pageList.begin() + index);
    return true;
}

bool PageSet::movePage(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= (int) pageList.size()
        || toIndex < 0 || toIndex >= (int) pageList.size()
        || fromIndex == toIndex)
        return false;

    auto page = std::move(pageList[(size_t) fromIndex]);
    pageList.erase(pageList.begin() + fromIndex);
    pageList.insert(pageList.begin() + toIndex, std::move(page));
    return true;
}

bool PageSet::renamePage(int index, const juce::String& name)
{
    const auto trimmed = name.trim();
    if (index < 0 || index >= (int) pageList.size()
        || trimmed.isEmpty() || pageList[(size_t) index].name == trimmed)
        return false;

    pageList[(size_t) index].name = trimmed;
    return true;
}
}
