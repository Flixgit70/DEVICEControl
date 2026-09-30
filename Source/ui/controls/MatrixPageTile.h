#pragma once
#include "RemoteControlTile.h"
#include "../../model/ControlTreeModel.h"
#include <map>
#include <set>
#include <vector>

namespace er
{
class MatrixPageTile final : public RemoteControlTile,
                             private juce::ScrollBar::Listener
{
public:
    explicit MatrixPageTile(ControlMapping mappingToUse);
    ~MatrixPageTile() override;

    void refresh(const ParameterInfo&) override {}
    void refreshMatrix(const TreeEntry&, const std::vector<ParameterInfo>& parameters);
    void setControlLabel2(const juce::String&) override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    std::function<bool(int target, int source, bool connect)> onSetCrosspoint;
    std::function<void(const MatrixControlData&)> onMatrixConfigurationChanged;

private:
    struct AxisLeaf
    {
        int number = 0;
        juce::String label;
        juce::String folderPath;
    };

    void applyControlColour() override;
    void beginLabel2Edit() override;
    void openEditor();
    int targetHeaderIndexAt(juce::Point<float>) const;
    int sourceHeaderIndexAt(juce::Point<float>) const;
    void selectTarget(int index, const juce::ModifierKeys&);
    void selectSource(int index, const juce::ModifierKeys&);
    void showAxisMenu(bool targetsAxis, juce::Component* target);
    void commitMatrixConfiguration();
    void rebuildLeaves();
    void updateScrollRanges();
    void scrollBarMoved(juce::ScrollBar*, double) override;
    juce::Rectangle<int> gridBounds() const;
    static void flattenAxis(const std::vector<MatrixAxisNode>& nodes,
                            const juce::String& parentPath,
                            std::vector<AxisLeaf>& leaves,
                            const std::map<int, juce::String>& liveLabels);

    juce::Label title;
    juce::Label subtitle;
    SelectionTextButton editButton { "Configure Axes..." };
    juce::ScrollBar horizontalScroll { false };
    juce::ScrollBar verticalScroll { true };
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    TreeEntry matrixEntry;
    std::vector<ParameterInfo> parameterSnapshot;
    std::map<int, juce::String> targetLabels;
    std::map<int, juce::String> sourceLabels;
    std::map<int, std::set<int>> connections;
    std::vector<AxisLeaf> targets;
    std::vector<AxisLeaf> sources;
    std::set<int> selectedTargets;
    std::set<int> selectedSources;
    int targetSelectionAnchor = -1;
    int sourceSelectionAnchor = -1;
    int folderCounter = 0;
    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::ResizableCornerComponent resizeCorner;
};
}
