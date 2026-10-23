#pragma once
#include <JuceHeader.h>
#include "../model/ControlTreeModel.h"
#include "../model/PageModel.h"
#include "../model/ParameterInfo.h"
#include <optional>
#include <map>
#include <set>
#include <vector>

namespace er
{
class ControlTreeBrowser final : public juce::Component
{
public:
    ControlTreeBrowser();
    ~ControlTreeBrowser() override;

    void setEntries(std::vector<TreeEntry> newEntries);
    void setConnected(bool connected);
    void setParameterResolver(std::function<std::optional<ParameterInfo>(const juce::String&)> resolver);
    void repaintValues();
    bool canSafelyReplaceTree() const;
    void applyPendingFullRebuildIfSafe();
    void expandLoadedBelow(const std::vector<juce::String>& roots);
    void collapseAllBelow(const std::vector<juce::String>& roots);
    void collapseAll();

    int selectedBranchCount() const;
    int selectedMatrixCount() const;
    int selectedParameterCount() const;
    juce::String selectedParameterSummary() const;
    juce::String selectedParameterPath() const;

    std::function<void(const juce::String&)> onOpenDirectory;
    std::function<void(const juce::String&)> onRefreshDirectory;
    std::function<void(const juce::String&)> onOpenMatrix;
    std::function<void(const juce::String&)> onAddMatrixControl;
    std::function<void(const std::vector<juce::String>&, ControlType)> onAddControls;
    std::function<void(const std::vector<juce::String>&)> onExpandAllSelection;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class BrowserItem;
    friend class BrowserItem;

    void rebuildTree();
    void rebuildEntryIndexes();
    void syncTreeWithoutDeletingItems();
    void addChildren(juce::TreeViewItem& parent, const juce::String& parentPath);
    void itemOpened(const juce::String& path);
    void openMatrix(const juce::String& path);
    void itemSelectionChanged(const juce::String& path, bool selected);
    std::vector<juce::String> selectedPaths() const;
    std::vector<juce::String> filteredParameterPaths() const;
    void selectFilterMatches();
    void deselectAll();
    void showFilterActionsMenu();
    void updateFilterActionState();
    void updateSelectionText();
    void showBranchContextMenu(const juce::String& path, juce::Component* target);
    std::vector<juce::String> rootsForContextPath(const juce::String& path) const;

    void recomputeFilter();
    bool shouldShowEntry(const juce::String& path) const;
    bool isFilterMatch(const juce::String& path) const;
    juce::String parameterValueText(const juce::String& path) const;

    juce::Label title;
    juce::TextButton showDescriptions { "Descriptions: ON" };
    juce::Label hint;
    juce::TextEditor filterEditor;
    juce::TextButton clearFilterButton { "X" };
    juce::TextButton selectMatchesButton { "Select Matches" };
    juce::TextButton deselectAllButton { "Deselect All" };
    juce::TextButton matchActionsButton { "Matches..." };
    juce::Label treeNameHeader { "TreeNameHeader", "Parameter" };
    juce::Label treeValueHeader { "TreeValueHeader", "Value / Matrix" };
    juce::TreeView tree;
    juce::TextButton addButton { "+ Button" };
    juce::TextButton addEncoder { "+ Encoder" };
    juce::TextButton addContent { "+ Content" };
    juce::TextButton addMeter { "+ Meter" };
    juce::TextButton expandAllButton { "Expand All" };
    juce::TextButton collapseAllButton { "Collapse All" };
    juce::Label selectionInfo;

    std::vector<TreeEntry> entries;
    std::map<juce::String, std::size_t> entryIndexByPath;
    std::map<juce::String, std::vector<std::size_t>> childEntryIndices;
    std::set<juce::String> openPaths;
    std::set<juce::String> selectedPathSet;
    std::set<juce::String> filterMatchPaths;
    std::set<juce::String> filterVisiblePaths;
    juce::String filterText;
    std::unique_ptr<BrowserItem> rootItem;
    bool rebuilding = false;
    bool fullRebuildPending = false;
    bool isConnected = false;
    std::function<std::optional<ParameterInfo>(const juce::String&)> parameterResolver;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlTreeBrowser)
};
}
