#include "ControlTreeBrowser.h"
#include <algorithm>
#include "../app/Theme.h"
#include "../model/ProviderConfig.h"

namespace er
{
namespace
{
bool isTechnicalIdentifier(const juce::String& identifier)
{
    // Some providers expose a chain such as /_1/_101/_3e8 as an identifier.
    // In that case the Ember number path is more useful in the tree.
    juce::StringArray parts;
    parts.addTokens(identifier, "/", "");
    parts.removeEmptyStrings();
    if (parts.isEmpty()) return false;
    for (const auto& part : parts)
        if (!part.startsWithChar('_') || part.length() < 2
            || !part.substring(1).containsOnly("0123456789abcdefABCDEF"))
            return false;
    return true;
}
}

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

    void ensureChildren()
    {
        if (childrenBuilt || (!isRoot && entry.kind != TreeEntryKind::node)) return;
        childrenBuilt = true;
        owner.addChildren(*this, isRoot ? juce::String() : entry.path);
    }

    void syncFromOwner()
    {
        if (!isRoot)
        {
            const auto found = owner.entryIndexByPath.find(entry.path);
            if (found != owner.entryIndexByPath.end())
                entry = owner.entries[found->second];
        }

        if (isRoot || isOpen())
        {
            owner.addChildren(*this, isRoot ? juce::String() : entry.path);
            childrenBuilt = true;
        }

        const int childCount = getNumSubItems();
        for (int i = 0; i < childCount; ++i)
            if (auto* child = dynamic_cast<BrowserItem*>(getSubItem(i)))
                child->syncFromOwner();
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (isRoot) return;
        const bool parameter = entry.kind == TreeEntryKind::parameter;
        const bool matrix = entry.kind == TreeEntryKind::matrix;
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
        const auto localPath = ProviderPath::localPath(entry.path);
        if (localPath.isNotEmpty())
        {
            name = "[" + localPath + "]";
            if (entry.identifier.isNotEmpty() && !isTechnicalIdentifier(entry.identifier))
                name << "  " << entry.identifier;
        }
        if (matrix) name << "  [MATRIX]";
        if (owner.showDescriptions.getToggleState() && entry.description.isNotEmpty()
            && !(entry.path.startsWithChar('@') && !entry.path.containsChar('.'))
            && entry.description != entry.identifier)
            name << "  —  " << entry.description;
        if (entry.kind != TreeEntryKind::parameter)
        {
            if (entry.directoryLoading) name << "  ...";
            else if (!entry.directoryLoaded) name << "  >";
        }

        const bool hasValueColumn = parameter || matrix;
        const int valueWidth = hasValueColumn ? juce::jlimit(105, 175, width / 3) : 0;
        const int dividerX = width - valueWidth;
        const int nameWidth = hasValueColumn ? juce::jmax(20, dividerX - 12) : width - 10;
        g.drawText(name, 6, 0, nameWidth, height, juce::Justification::centredLeft, true);

        if (hasValueColumn)
        {
            g.setColour(Theme::grid);
            g.drawVerticalLine(dividerX, 2.0f, (float) height - 2.0f);
            g.setColour(selected ? juce::Colours::white : (matrix ? Theme::accentBlue : Theme::text));
            g.setFont(juce::FontOptions(11.5f));
            g.drawText(matrix ? entry.matrix.summary() : owner.parameterValueText(entry.path),
                       dividerX + 7, 0, valueWidth - 12, height,
                       juce::Justification::centredRight, true);
        }
    }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (isNowOpen) ensureChildren();
        if (isRoot) return;
        if (!owner.rebuilding)
        {
            if (isNowOpen) owner.openPaths.insert(entry.path);
            else owner.openPaths.erase(entry.path);
            if (isNowOpen && entry.kind == TreeEntryKind::node)
                owner.itemOpened(entry.path);
            owner.updateFilterActionState();
            owner.updateSelectionText();
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

    void itemDoubleClicked(const juce::MouseEvent&) override
    {
        if (!isRoot && entry.kind == TreeEntryKind::matrix)
            owner.openMatrix(entry.path);
    }

private:
    ControlTreeBrowser& owner;
    TreeEntry entry;
    bool isRoot = false;
    bool childrenBuilt = false;
};

ControlTreeBrowser::ControlTreeBrowser()
{
    title.setText("EMBER+ TREE", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    addAndMakeVisible(title);

    showDescriptions.setClickingTogglesState(true);
    showDescriptions.setToggleState(true, juce::dontSendNotification);
    showDescriptions.setColour(juce::TextButton::buttonOnColourId, Theme::accentBlue);
    showDescriptions.onClick = [this]
    {
        showDescriptions.setButtonText(showDescriptions.getToggleState() ? "Descriptions: ON" : "Descriptions: OFF");
        tree.repaint();
    };
    addAndMakeVisible(showDescriptions);

    hint.setText("Browse lazily; selecting a Matrix loads its directory. Filter tokens are ANDed; prefix exclusions with -.", juce::dontSendNotification);
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
    deselectAllButton.setEnabled(false);
    deselectAllButton.setTooltip("Clear all selections in the tree, including hidden filter matches");
    deselectAllButton.onClick = [this] { deselectAll(); };
    matchActionsButton.onClick = [this] { showFilterActionsMenu(); };
    addAndMakeVisible(selectMatchesButton);
    addAndMakeVisible(deselectAllButton);
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
    addMeter.onClick = [this]
    {
        if (onAddControls) onAddControls(selectedPaths(), ControlType::meter);
    };
    expandAllButton.onClick = [this]
    {
        const auto roots = selectedPaths();
        if (onExpandAllSelection && !roots.empty()) onExpandAllSelection(roots);
    };
    collapseAllButton.onClick = [this]
    {
        collapseAll();
    };
    addAndMakeVisible(addButton);
    addAndMakeVisible(addEncoder);
    addAndMakeVisible(addContent);
    addAndMakeVisible(addMeter);
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
    addButton.setEnabled(connected);
    addEncoder.setEnabled(connected);
    addContent.setEnabled(connected);
    addMeter.setEnabled(connected);
    expandAllButton.setEnabled(connected);
    // Collapse only changes the local UI state and remains available even if the provider disconnects.
    collapseAllButton.setEnabled(true);

    // Connection polling must not rebuild the complete filter result.
    // The match state is recalculated when the filter or tree changes.
    if (filterText.isEmpty())
        selectMatchesButton.setEnabled(false);

    matchActionsButton.setEnabled(isConnected
                                  && filterText.isNotEmpty()
                                  && selectMatchesButton.isEnabled());
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

bool ControlTreeBrowser::canSafelyReplaceTree() const
{
    // JUCE keeps an internal pointer to the TreeView row currently under the
    // mouse. Replacing the root while that row is hovered can leave a queued
    // Linux mouse event referring to an already deleted row component.
    return !tree.isMouseOverOrDragging(true);
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
    if (rootItem && !canSafelyReplaceTree())
    {
        rebuildEntryIndexes();
        syncTreeWithoutDeletingItems();
        fullRebuildPending = true;
    }
    else
    {
        rebuildTree();
    }
}

void ControlTreeBrowser::paint(juce::Graphics& g)
{
    g.setColour(Theme::panelRaised);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
}

void ControlTreeBrowser::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto titleRow = r.removeFromTop(22);
    showDescriptions.setBounds(titleRow.removeFromRight(145));
    title.setBounds(titleRow);
    hint.setBounds(r.removeFromTop(20));
    auto filterRow = r.removeFromTop(30);
    clearFilterButton.setBounds(filterRow.removeFromRight(30).reduced(1));
    filterRow.removeFromRight(4);
    filterEditor.setBounds(filterRow.reduced(0, 1));

    auto matchRow = r.removeFromTop(32);
    constexpr int matchButtonGap = 6;
    const int matchButtonSpace = juce::jmax(0, matchRow.getWidth() - 2 * matchButtonGap);
    selectMatchesButton.setBounds(matchRow.removeFromLeft(matchButtonSpace * 42 / 100));
    matchRow.removeFromLeft(matchButtonGap);
    deselectAllButton.setBounds(matchRow.removeFromLeft(matchButtonSpace * 34 / 100));
    matchRow.removeFromLeft(matchButtonGap);
    matchActionsButton.setBounds(matchRow);
    r.removeFromTop(5);

    auto bottom = r.removeFromBottom(104);
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
    const int directWidth = juce::jmax(40, (direct.getWidth() - directGap * 3) / 4);
    addButton.setBounds(direct.removeFromLeft(directWidth));
    direct.removeFromLeft(directGap);
    addEncoder.setBounds(direct.removeFromLeft(directWidth));
    direct.removeFromLeft(directGap);
    addContent.setBounds(direct.removeFromLeft(directWidth));
    direct.removeFromLeft(directGap);
    addMeter.setBounds(direct);
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

void ControlTreeBrowser::applyPendingFullRebuildIfSafe()
{
    if (fullRebuildPending && canSafelyReplaceTree())
        rebuildTree();
}

void ControlTreeBrowser::rebuildTree()
{
    rebuilding = true;
    tree.setRootItem(nullptr);
    rootItem.reset();
    rebuildEntryIndexes();

    TreeEntry root;
    root.kind = TreeEntryKind::node;
    root.path = {};
    root.identifier = "Root";
    rootItem = std::make_unique<BrowserItem>(*this, root, true);
    rootItem->ensureChildren();
    tree.setRootItem(rootItem.get());
    rootItem->setOpen(true);
    rebuilding = false;
    fullRebuildPending = false;
    updateSelectionText();
}

void ControlTreeBrowser::rebuildEntryIndexes()
{
    entryIndexByPath.clear();
    childEntryIndices.clear();

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        entryIndexByPath[entry.path] = i;
        if (shouldShowEntry(entry.path))
            childEntryIndices[entry.parentPath].push_back(i);
    }

    for (auto& [parentPath, indices] : childEntryIndices)
    {
        juce::ignoreUnused(parentPath);
        std::sort(indices.begin(), indices.end(), [this] (std::size_t left, std::size_t right)
        {
            const auto& a = entries[left];
            const auto& b = entries[right];
            // Synthetic provider roots have no Ember number; sort those by name.
            if (a.path.startsWithChar('@') && !a.path.containsChar('.')
                && b.path.startsWithChar('@') && !b.path.containsChar('.'))
                return a.displayName().compareNatural(b.displayName()) < 0;
            return ControlTreeModel::numericPathLess(a.path, b.path);
        });
    }
}

void ControlTreeBrowser::syncTreeWithoutDeletingItems()
{
    if (!rootItem) return;

    juce::ScopedValueSetter<bool> rebuildGuard(rebuilding, true);
    rootItem->syncFromOwner();
    tree.repaint();
    updateSelectionText();
}

void ControlTreeBrowser::addChildren(juce::TreeViewItem& parent, const juce::String& parentPath)
{
    const auto children = childEntryIndices.find(parentPath);
    if (children == childEntryIndices.end()) return;

    std::map<juce::String, BrowserItem*> existing;
    for (int i = 0; i < parent.getNumSubItems(); ++i)
        if (auto* item = dynamic_cast<BrowserItem*>(parent.getSubItem(i)))
            existing[item->getUniqueName()] = item;

    int insertionPosition = 0;
    for (const auto index : children->second)
    {
        const auto& e = entries[index];
        BrowserItem* item = nullptr;
        bool added = false;
        if (const auto found = existing.find(e.path); found != existing.end())
        {
            item = found->second;
        }
        else
        {
            item = new BrowserItem(*this, e);
            parent.addSubItem(item, insertionPosition);
            added = true;
        }

        if (openPaths.count(e.path) != 0)
        {
            item->ensureChildren();
            item->setOpen(true);
        }
        if (added && selectedPathSet.count(e.path) != 0) item->setSelected(true, false);
        ++insertionPosition;
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

void ControlTreeBrowser::collapseAll()
{
    openPaths.clear();
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
    const auto found = entryIndexByPath.find(path);
    if (found != entryIndexByPath.end() && entries[found->second].kind == TreeEntryKind::matrix)
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Open XY Matrix", isConnected);
        menu.addItem(2, "Add Matrix to Current Page", isConnected);
        menu.addItem(3, "Reload Matrix (GetDirectory)", isConnected);
        menu.addSeparator();
        const auto details = entries[found->second].matrix.details();
        menu.addItem(4, "Copy Matrix Details");

        juce::Component::SafePointer<ControlTreeBrowser> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                           [safeThis, path, details] (int result)
                           {
                               if (safeThis == nullptr || result == 0) return;
                               if (result == 1)
                                   safeThis->openMatrix(path);
                               else if (result == 2 && safeThis->onAddMatrixControl)
                                   safeThis->onAddMatrixControl(path);
                               else if (result == 3 && safeThis->onRefreshDirectory)
                                   safeThis->onRefreshDirectory(path);
                               else if (result == 4)
                                   juce::SystemClipboard::copyTextToClipboard("Matrix " + path + "\n" + details);
                           });
        return;
    }

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

void ControlTreeBrowser::openMatrix(const juce::String& path)
{
    if (isConnected && onOpenMatrix) onOpenMatrix(path);
}

void ControlTreeBrowser::itemSelectionChanged(const juce::String& path, bool selected)
{
    if (selected) selectedPathSet.insert(path);
    else selectedPathSet.erase(path);

    if (selected && isConnected && onOpenDirectory)
    {
        const auto found = entryIndexByPath.find(path);
        if (found != entryIndexByPath.end())
        {
            const auto& entry = entries[found->second];
            if (entry.kind == TreeEntryKind::matrix && !entry.directoryLoaded && !entry.directoryLoading)
                onOpenDirectory(path);
        }
    }
    updateSelectionText();
}

std::vector<juce::String> ControlTreeBrowser::selectedPaths() const
{
    return { selectedPathSet.begin(), selectedPathSet.end() };
}

int ControlTreeBrowser::selectedBranchCount() const
{
    int count = 0;
    for (const auto& path : selectedPathSet)
        if (const auto it = entryIndexByPath.find(path);
            it != entryIndexByPath.end() && entries[it->second].kind == TreeEntryKind::node)
            ++count;
    return count;
}

int ControlTreeBrowser::selectedParameterCount() const
{
    int count = 0;
    for (const auto& path : selectedPathSet)
        if (const auto it = entryIndexByPath.find(path);
            it != entryIndexByPath.end() && entries[it->second].kind == TreeEntryKind::parameter)
            ++count;
    return count;
}

int ControlTreeBrowser::selectedMatrixCount() const
{
    int count = 0;
    for (const auto& path : selectedPathSet)
        if (const auto it = entryIndexByPath.find(path);
            it != entryIndexByPath.end() && entries[it->second].kind == TreeEntryKind::matrix)
            ++count;
    return count;
}

juce::String ControlTreeBrowser::selectedParameterSummary() const
{
    juce::String firstName;
    int count = 0;
    for (const auto& path : selectedPathSet)
    {
        const auto it = entryIndexByPath.find(path);
        if (it == entryIndexByPath.end()) continue;
        const auto& entry = entries[it->second];
        if (entry.kind != TreeEntryKind::parameter) continue;
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
    for (const auto& path : selectedPathSet)
        if (const auto it = entryIndexByPath.find(path);
            it != entryIndexByPath.end() && entries[it->second].kind == TreeEntryKind::parameter)
            return path;
    return {};
}

std::vector<juce::String> ControlTreeBrowser::filteredParameterPaths() const
{
    return ControlTreeModel::parameterPathsMatchingFilterInOpenBranches(entries, filterText, openPaths);
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

void ControlTreeBrowser::deselectAll()
{
    if (selectedPathSet.empty())
        return;

    selectedPathSet.clear();
    rebuildTree();
}

void ControlTreeBrowser::showFilterActionsMenu()
{
    const auto matches = filteredParameterPaths();
    if (!isConnected || filterText.isEmpty() || matches.empty())
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "Add matches as Buttons");
    menu.addItem(2, "Add matches as Encoders");
    menu.addItem(3, "Add matches as Show Content");
    menu.addItem(4, "Add matches as Meters");

    juce::Component::SafePointer<ControlTreeBrowser> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&matchActionsButton),
                       [safeThis, matches] (int result)
                       {
                           if (safeThis == nullptr || result == 0) return;
                           if (result == 1 && safeThis->onAddControls)
                               safeThis->onAddControls(matches, ControlType::button);
                           else if (result == 2 && safeThis->onAddControls)
                               safeThis->onAddControls(matches, ControlType::encoder);
                           else if (result == 3 && safeThis->onAddControls)
                               safeThis->onAddControls(matches, ControlType::showContent);
                           else if (result == 4 && safeThis->onAddControls)
                               safeThis->onAddControls(matches, ControlType::meter);
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
    deselectAllButton.setEnabled(!selectedPathSet.empty());

    juce::String text;
    if (selectedPathSet.empty()) text = "No selection";
    else if (selectedPathSet.size() == 1)
    {
        const auto found = entryIndexByPath.find(*selectedPathSet.begin());
        if (found != entryIndexByPath.end() && entries[found->second].kind == TreeEntryKind::matrix)
        {
            const auto& entry = entries[found->second];
            text = "Matrix " + entry.displayName() + " [" + entry.path + "]";
            const auto summary = entry.matrix.summary();
            if (summary.isNotEmpty()) text << "  |  " << summary;
        }
        else text = "1 selected";
    }
    else text = juce::String((int) selectedPathSet.size()) + " selected";

    if (filterText.isNotEmpty())
    {
        const auto parameterMatches = filteredParameterPaths();
        text << "  |  " << juce::String((int) filterMatchPaths.size()) << " filter matches / "
             << juce::String((int) parameterMatches.size()) << " visible parameters";
    }
    selectionInfo.setText(text, juce::dontSendNotification);
}
}
