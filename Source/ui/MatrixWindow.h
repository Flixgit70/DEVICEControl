#pragma once
#include <JuceHeader.h>
#include "../model/ControlTreeModel.h"
#include "../model/ParameterInfo.h"
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace er
{
class MatrixWindow final : public juce::DocumentWindow
{
public:
    struct Callbacks
    {
        std::function<std::optional<TreeEntry>()> matrixResolver;
        std::function<std::vector<ParameterInfo>()> parameterSnapshot;
        std::function<std::vector<TreeEntry>()> treeSnapshot;
        std::function<std::uint64_t()> treeRevision;
        std::function<std::uint64_t()> parameterStructureRevision;
        std::function<bool(int target, int source, bool connect)> setCrosspoint;
        std::function<void()> refreshMatrix;
        std::function<void()> loadLabels;
        std::function<void(const std::vector<std::pair<int, juce::String>>& targets,
                           const std::vector<std::pair<int, juce::String>>& sources)> addSelectionToPage;
    };

    MatrixWindow(const juce::String& qualifiedMatrixPath, Callbacks callbacks);
    void closeButtonPressed() override;

private:
    class MatrixView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MatrixWindow)
};
}
