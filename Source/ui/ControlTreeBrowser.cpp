#include "ControlTreeBrowser.h"
#include <algorithm>
#include "../app/Theme.h"

namespace er
{
class ControlTreeBrowser::BrowserItem final : public juce::TreeViewItem
{
public:
    BrowserItem(ControlTreeBrowser& o, TreeEntry e, bool root = false)
        : owner(o), entry(std::move(e)), isRoot(root)
    {
        // Let JUCE's selection background span the complete row, including
        // indentation and the disclosure triangle area.
        setDrawsInLeftMargin(true);
    }

    bool mightContainSubItems() override { return isRoot || entry.kind == TreeEntryKind::node; }
    juce::String getUniqueName() const override { return isRoot ? "__root__" : entry.path; }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (isRoot) return;
        const bool parameter = entry.kind == TreeEntryKind::parameter;
        const bool selected = isSelected();
        const bool filterMatch = owner.isFilterMatch(entry.path);

        if (filterMatch)
        {
            const auto orange = Theme::orange;
            if (!selected)
            {
                g.setColour(orange.withAlpha(0.16f));
                g.fillRoundedRectangle(0.0f, 1.0f, (float) width, (float) height - 2.0f, 3.0f);
            }
            g.setColour(orange);
            g.fillRect(0, 2, 3, juce::jmax(0, height - 4));
        }

        const auto textColour = selected ? juce::Colours::white
                                         : (filterMatch ? Theme::orange.brighter(0.2f)
                                                        : (!entry.online ? juce::Colours::grey
                                                                         : juce::Colours::white));
        g.setColour(textColour);
        g.setFont(juce::FontOptions(12.5f).withStyle((parameter && !selected && !filterMatch) ? "Plain" : "Bold"));
        auto name = entry.displayName();
        if (!parameter)
        {
            if (entry.directoryLoading) name << "  ...";
            else if (!entry.directoryLoaded) name << "  >";
        }

        const int valueWidth = parameter ? juce::jlimit(95, 165, width / 3) : 0;
        const int dividerX = width - valueWidth;
        const int nameWidth = parameter ? juce::jmax(20, dividerX - 12) : width - 10;
        g.drawText(name, 6, 0, nameWidth, height, juce::Justification::centredLeft, true);

        if (parameter)
        {
            g.setColour(Theme::grid);
            g.drawVerticalLine(dividerX, 2.0f, (float) height - 2.0f);
            g.setColour(selected ? juce::Colours::white : Theme::text);
            g.setFont(juce::FontOptions(11.5f));
            g.drawText(owner.parameterValueText(entry.path), dividerX + 7, 0, valueWidth - 12, height,
                       juce::Justification::centredRight, true);
        }
    }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (isRoot) return;
        if (!owner.rebuilding)
        {
            if (isNowOpen) owner.openPaths.insert(entry.path);
            else owner.openPaths.erase(entry.path);
            if (isNowOpen && entry.kind == TreeEntryKind::node)
                owner.itemOpened(entry.path);
        }
    }

    void itemSelectionChanged(bool isNowSelected) override
    {
        if (!isRoot && !owner.rebuilding)
            owner.itemSelectionChanged(entry.path, isNowSelected);
    }

    void itemClicked(const juce::MouseEvent& event) override
    {
        if (!isRoot && event.mods.isPopupMenu())
            owner.showBranchContextMenu(entry.path, event.eventComponent);
    }

private:
    ControlTreeBrowser& owner;
    TreeEntry entry;
    bool isRoot = false;
};

ControlTreeBrowser::ControlTreeBrowser()
{
    title.setText("EMBER+ TREE", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    addAndMakeVisible(title);

    hint.setText("Browse lazily; Expand All loads a subtree. Filter tokens are ANDed; prefix exclusions with -.", juce::dontSendNotification);
    hint.setColour(juce::Label::textColourId, Theme::text);
    hint.setFont(juce::FontOptions(10.5f));
    addAndMakeVisible(hint);

    filterEditor.setTextToShowWhenEmpty("Filter, e.g. Ravenna Input -Buffer", juce::Colours::grey);
    filterEditor.onTextChange = [this]
    {
        filterText = filterEditor.getText().trim();
        recomputeFilter();
        clearFilterButton.setEnabled(filterText.isNotEmpty());
        updateFilterActionState();
        rebuildTree();
    };
    addAndMakeVisible(filterEditor);

    clearFilterButton.setEnabled(false);
    clearFilterButton.onClick = [this] { filterEditor.clear(); };
    addAndMakeVisible(clearFilterButton);

    selectMatchesButton.onClick = [this] { selectFilterMatches(); };
    matchActionsButton.onClick = [this] { showFilterActionsMenu(); };
    addAndMakeVisible(selectMatchesButton);
    addAndMakeVisible(matchActionsButton);

    treeNameHeader.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
    treeValueHeader.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
    treeNameHeader.setColour(juce::Label::textColourId, Theme::text);
    treeValueHeader.setColour(juce::Label::textColourId, Theme::text);
    treeValueHeader.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(treeNameHeader);
    addAndMakeVisible(treeValueHeader);

    tree.setRootItemVisible(false);
    tree.setMultiSelectEnabled(true);
    tree.setDefaultOpenness(false);
    addAndMakeVisible(tree);

    addButton.onClick = [this]
    {
        if (onAddControls) onAddControls(selectedPaths(), ControlType::button);
    };
    addEncoder.onClick = [this]
    {
        if (onAddControls) onAddControls(selectedPaths(), ControlType::encoder);
    };
    addContent.onClick = [this]
    {
        if (onAddControls) onAddControls(selectedPaths(), ControlType::showContent);
    };
    collectButton.onClick = [this]
    {
        if (onCollectSelection) onCollectSelection(selectedPaths());
    };
    exportButton.onClick = [this]
    {
        if (onExportSelection) onExportSelection(selectedPaths());
    };
    expandAllButton.onClick = [this]
    {
        const auto roots = selectedPaths();
        if (onExpandAllSelection && !roots.empty()) onExpandAllSelection(roots);
    };
    collapseAllButton.onClick = [this]
    {
        collapseAllBelow(selectedPaths());
    };
    addAndMakeVisible(addButton);
    addAndMakeVisible(addEncoder);
    addAndMakeVisible(addContent);
    addAndMakeVisible(collectButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(expandAllButton);
    addAndMakeVisible(collapseAllButton);

    selectionInfo.setColour(juce::Label::textColourId, Theme::text);
    selectionInfo.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(selectionInfo);
    setConnected(false);
}

ControlTreeBrowser::~ControlTreeBrowser()
{
    tree.setRootItem(nullptr);
    rootItem.reset();
}

void ControlTreeBrowser::setConnected(bool connected)
{
    isConnected = connected;
    collectButton.setEnabled(connected);
    exportButton.setEnabled(connected);
    addButton.setEnabled(connected);
    addEncoder.setEnabled(connected);
    addContent.setEnabled(connected);
    expandAllButton.setEnabled(connected);
    // Collapse only changes the local UI state and remains available even if the provider disconnects.
    collapseAllButton.setEnabled(true);
    updateFilterActionState();
}

void ControlTreeBrowser::setParameterResolver(std::function<std::optional<ParameterInfo>(const juce::String&)> resolver)
{
    parameterResolver = std::move(resolver);
    tree.repaint();
}

void ControlTreeBrowser::repaintValues()
{
    tree.repaint();
}

juce::String ControlTreeBrowser::parameterValueText(const juce::String& path) const
{
    if (!parameterResolver) return {};
    const auto info = parameterResolver(path);
    if (!info) return {};
    auto text = info->formattedValue().replaceCharacters("\r\n\t", "   ");
    text = text.trim();
    constexpr int maxPreview = 56;
    if (text.length() > maxPreview)
        text = text.substring(0, maxPreview - 3) + "...";
    return text;
}

void ControlTreeBrowser::setEntries(std::vector<TreeEntry> newEntries)
{
    entries = std::move(newEntries);
    recomputeFilter();
    updateFilterActionState();
    rebuildTree();
}

void ControlTreeBrowser::paint(juce::Graphics& g)
{
    g.setColour(Theme::panelRaised);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
}

void ControlTreeBrowser::resized()
{
    auto r = getLocalBounds().reduced(10);
    title.setBounds(r.removeFromTop(22));
    hint.setBounds(r.removeFromTop(20));
    auto filterRow = r.removeFromTop(30);
    clearFilterButton.setBounds(filterRow.removeFromRight(30).reduced(1));
    filterRow.removeFromRight(4);
    filterEditor.setBounds(filterRow.reduced(0, 1));

    auto matchRow = r.removeFromTop(32);
    selectMatchesButton.setBounds(matchRow.removeFromLeft((matchRow.getWidth() - 6) * 2 / 3));
    matchRow.removeFromLeft(6);
    matchActionsButton.setBounds(matchRow);
    r.removeFromTop(5);

    auto bottom = r.removeFromBottom(142);
    auto treeHeader = r.removeFromTop(20);
    const int headerValueWidth = juce::jlimit(105, 155, treeHeader.getWidth() / 3);
    treeValueHeader.setBounds(treeHeader.removeFromRight(headerValueWidth));
    treeNameHeader.setBounds(treeHeader);
    tree.setBounds(r);
    selectionInfo.setBounds(bottom.removeFromTop(24));

    auto branch = bottom.removeFromTop(34);
    expandAllButton.setBounds(branch.removeFromLeft((branch.getWidth() - 6) / 2));
    branch.removeFromLeft(6);
    collapseAllButton.setBounds(branch);
    bottom.removeFromTop(4);

    auto direct = bottom.removeFromTop(34);
    const int directGap = 5;
    const int directWidth = juce::jmax(40, (direct.getWidth() - directGap * 2) / 3);
    addButton.setBounds(direct.removeFromLeft(directWidth));
    direct.removeFromLeft(directGap);
    addEncoder.setBounds(direct.removeFromLeft(directWidth));
    direct.removeFromLeft(directGap);
    addContent.setBounds(direct);
    bottom.removeFromTop(4);

    auto bulk = bottom.removeFromTop(34);
    collectButton.setBounds(bulk.removeFromLeft((bulk.getWidth() - 6) / 2));
    bulk.removeFromLeft(6);
    exportButton.setBounds(bulk);
}

void ControlTreeBrowser::recomputeFilter()
{
    const auto result = ControlTreeModel::buildFilter(entries, filterText);
    filterMatchPaths = result.matches;
    filterVisiblePaths = result.visible;
}

bool ControlTreeBrowser::shouldShowEntry(const juce::String& path) const
{
    return filterText.isEmpty() || filterVisiblePaths.count(path) != 0;
}

bool ControlTreeBrowser::isFilterMatch(const juce::String& path) const
{
    return filterText.isNotEmpty() && filterMatchPaths.count(path) != 0;
}

bool ControlTreeBrowser::hasVisibleDescendant(const juce::String& path) const
{
    if (filterText.isEmpty()) return false;
    for (const auto& candidate : filterVisiblePaths)
        if (candidate != path && ControlTreeModel::isSameOrDescendant(candidate, path)) return true;
    return false;
}

void ControlTreeBrowser::rebuildTree()
{
    rebuilding = true;
    tree.setRootItem(nullptr);
    rootItem.reset();

    TreeEntry root;
    root.kind = TreeEntryKind::node;
    root.path = {};
    root.identifier = "Root";
    rootItem = std::make_unique<BrowserItem>(*this, root, true);
    addChildren(*rootItem, {});
    tree.setRootItem(rootItem.get());
    rootItem->setOpen(true);
    rebuilding = false;
    updateSelectionText();
}

void ControlTreeBrowser::addChildren(juce::TreeViewItem& parent, const juce::String& parentPath)
{
    std::vector<TreeEntry> children;
    for (const auto& e : entries)
        if (e.parentPath == parentPath && shouldShowEntry(e.path)) children.push_back(e);

    std::sort(children.begin(), children.end(), [] (const TreeEntry& a, const TreeEntry& b)
    {
        if (a.kind != b.kind) return a.kind == TreeEntryKind::node;
        return a.displayName().compareNatural(b.displayName()) < 0;
    });

    for (const auto& e : children)
    {
        auto* item = new BrowserItem(*this, e);
        parent.addSubItem(item);
        addChildren(*item, e.path);
        const bool autoOpenForFilter = filterText.isNotEmpty() && hasVisibleDescendant(e.path);
        if (openPaths.count(e.path) != 0 || autoOpenForFilter) item->setOpen(true);
        if (selectedPathSet.count(e.path) != 0) item->setSelected(true, false);
    }
}

void ControlTreeBrowser::expandLoadedBelow(const std::vector<juce::String>& roots)
{
    if (roots.empty()) return;
    const auto nodes = ControlTreeModel::nodePathsUnderRoots(entries, roots);
    openPaths.insert(nodes.begin(), nodes.end());
    rebuildTree();
}

void ControlTreeBrowser::collapseAllBelow(const std::vector<juce::String>& roots)
{
    if (roots.empty()) return;
    const auto nodes = ControlTreeModel::nodePathsUnderRoots(entries, roots);
    for (const auto& path : nodes) openPaths.erase(path);
    rebuildTree();
}

std::vector<juce::String> ControlTreeBrowser::rootsForContextPath(const juce::String& path) const
{
    if (selectedPathSet.count(path) != 0 && !selectedPathSet.empty())
        return selectedPaths();
    return { path };
}

void ControlTreeBrowser::showBranchContextMenu(const juce::String& path, juce::Component* target)
{
    auto roots = rootsForContextPath(path);
    juce::PopupMenu menu;
    menu.addItem(1, "Expand All Below", isConnected);
    menu.addItem(2, "Collapse All Below", true);

    juce::Component::SafePointer<ControlTreeBrowser> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                       [safeThis, roots = std::move(roots)] (int result)
                       {
                           if (safeThis == nullptr || result == 0) return;
                           if (result == 1)
                           {
                               if (safeThis->onExpandAllSelection) safeThis->onExpandAllSelection(roots);
                           }
                           else if (result == 2)
                           {
                               safeThis->collapseAllBelow(roots);
                           }
                       });
}

void ControlTreeBrowser::itemOpened(const juce::String& path)
{
    if (isConnected && onOpenDirectory) onOpenDirectory(path);
}

void ControlTreeBrowser::itemSelectionChanged(const juce::String& path, bool selected)
{
    if (selected) selectedPathSet.insert(path);
    else selectedPathSet.erase(path);
    updateSelectionText();
}

std::vector<juce::String> ControlTreeBrowser::selectedPaths() const
{
    return { selectedPathSet.begin(), selectedPathSet.end() };
}

int ControlTreeBrowser::selectedBranchCount() const
{
    int count = 0;
    for (const auto& entry : entries)
        if (entry.kind == TreeEntryKind::node && selectedPathSet.count(entry.path) != 0)
            ++count;
    return count;
}

int ControlTreeBrowser::selectedParameterCount() const
{
    int count = 0;
    for (const auto& entry : entries)
        if (entry.kind == TreeEntryKind::parameter && selectedPathSet.count(entry.path) != 0)
            ++count;
    return count;
}

juce::String ControlTreeBrowser::selectedParameterSummary() const
{
    juce::String firstName;
    int count = 0;
    for (const auto& entry : entries)
    {
        if (entry.kind != TreeEntryKind::parameter || selectedPathSet.count(entry.path) == 0)
            continue;
        if (firstName.isEmpty())
            firstName = entry.displayName();
        ++count;
    }

    if (firstName.isEmpty())
        return {};
    if (count > 1)
        firstName << " (+" << juce::String(count - 1) << ")";
    return firstName;
}

juce::String ControlTreeBrowser::selectedParameterPath() const
{
    for (const auto& entry : entries)
        if (entry.kind == TreeEntryKind::parameter && selectedPathSet.count(entry.path) != 0)
            return entry.path;
    return {};
}

std::vector<juce::String> ControlTreeBrowser::filteredParameterPaths() const
{
    return ControlTreeModel::parameterPathsMatchingFilter(entries, filterText);
}

void ControlTreeBrowser::selectFilterMatches()
{
    const auto matches = filteredParameterPaths();
    if (filterText.isEmpty() || matches.empty())
        return;

    selectedPathSet.clear();
    selectedPathSet.insert(matches.begin(), matches.end());
    rebuildTree();
}

void ControlTreeBrowser::showFilterActionsMenu()
{
    const auto matches = filteredParameterPaths();
    if (!isConnected || filterText.isEmpty() || matches.empty())
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "Add matches to Mapping Table");
    menu.addSeparator();
    menu.addItem(2, "Add matches as Buttons");
    menu.addItem(3, "Add matches as Encoders");
    menu.addItem(4, "Add matches as Show Content");

    juce::Component::SafePointer<ControlTreeBrowser> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&matchActionsButton),
                       [safeThis, matches] (int result)
                       {
                           if (safeThis == nullptr || result == 0) return;
                           if (result == 1 && safeThis->onCollectSelection)
                               safeThis->onCollectSelection(matches);
                           else if (result == 2 && safeThis->onAddControls)
                               safeThis->onAddControls(matches, ControlType::button);
                           else if (result == 3 && safeThis->onAddControls)
                               safeThis->onAddControls(matches, ControlType::encoder);
                           else if (result == 4 && safeThis->onAddControls)
                               safeThis->onAddControls(matches, ControlType::showContent);
                       });
}

void ControlTreeBrowser::updateFilterActionState()
{
    const bool haveMatches = filterText.isNotEmpty() && !filteredParameterPaths().empty();
    selectMatchesButton.setEnabled(haveMatches);
    matchActionsButton.setEnabled(isConnected && haveMatches);
}

void ControlTreeBrowser::updateSelectionText()
{
    juce::String text;
    if (selectedPathSet.empty()) text = "No selection";
    else text = juce::String((int) selectedPathSet.size()) + " selected";

    if (filterText.isNotEmpty())
    {
        const auto parameterMatches = filteredParameterPaths();
        text << "  |  " << juce::String((int) filterMatchPaths.size()) << " filter matches / "
             << juce::String((int) parameterMatches.size()) << " parameters";
    }
    selectionInfo.setText(text, juce::dontSendNotification);
}
}
