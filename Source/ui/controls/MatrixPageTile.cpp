#include "MatrixPageTile.h"
#include "../../app/Theme.h"
#include "../../model/ProviderConfig.h"
#include <algorithm>
#include <map>
#include <set>

namespace er
{
namespace
{
constexpr int rowHeaderWidth = 175;
constexpr int columnHeaderHeight = 66;
constexpr int cellWidth = 62;
constexpr int cellHeight = 25;
constexpr int scrollBarSize = 14;

bool axisContainsSignal(const std::vector<MatrixAxisNode>& nodes, int number)
{
    for (const auto& node : nodes)
    {
        if (!node.folder && node.signalNumber == number) return true;
        if (node.folder && axisContainsSignal(node.children, number)) return true;
    }
    return false;
}

bool removeAxisNodes(std::vector<MatrixAxisNode>& nodes, const std::set<juce::String>& ids)
{
    const auto oldSize = nodes.size();
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&ids] (const MatrixAxisNode& node)
    {
        return ids.count(node.id) != 0;
    }), nodes.end());
    bool changed = nodes.size() != oldSize;
    for (auto& node : nodes)
        if (node.folder) changed = removeAxisNodes(node.children, ids) || changed;
    return changed;
}

bool renameAxisNode(std::vector<MatrixAxisNode>& nodes, const juce::String& id,
                    const juce::String& newName)
{
    for (auto& node : nodes)
    {
        if (node.id == id) { node.name = newName; return true; }
        if (node.folder && renameAxisNode(node.children, id, newName)) return true;
    }
    return false;
}

bool toggleAxisFolder(std::vector<MatrixAxisNode>& nodes, const juce::String& id)
{
    for (auto& node : nodes)
    {
        if (node.folder && node.id == id)
        {
            node.expanded = !node.expanded;
            return true;
        }
        if (node.folder && toggleAxisFolder(node.children, id)) return true;
    }
    return false;
}

void collectAxisSignals(const std::vector<MatrixAxisNode>& nodes, std::set<int>& signals)
{
    for (const auto& node : nodes)
    {
        if (node.folder) collectAxisSignals(node.children, signals);
        else signals.insert(node.signalNumber);
    }
}

void drawDisclosureTriangle(juce::Graphics& g, juce::Rectangle<float> bounds, bool expanded)
{
    juce::Path triangle;
    if (expanded)
    {
        triangle.startNewSubPath(bounds.getX(), bounds.getY());
        triangle.lineTo(bounds.getRight(), bounds.getY());
        triangle.lineTo(bounds.getCentreX(), bounds.getBottom());
    }
    else
    {
        triangle.startNewSubPath(bounds.getX(), bounds.getY());
        triangle.lineTo(bounds.getRight(), bounds.getCentreY());
        triangle.lineTo(bounds.getX(), bounds.getBottom());
    }
    triangle.closeSubPath();
    g.fillPath(triangle);
}

bool groupAtCommonLevel(std::vector<MatrixAxisNode>& nodes,
                        const std::set<juce::String>& selected,
                        const juce::String& folderName)
{
    int selectedHere = 0;
    int firstIndex = -1;
    for (int i = 0; i < (int) nodes.size(); ++i)
        if (selected.count(nodes[(size_t) i].id) != 0)
        {
            if (firstIndex < 0) firstIndex = i;
            ++selectedHere;
        }

    if (selectedHere == (int) selected.size() && selectedHere > 0)
    {
        auto folder = MatrixAxisNode::createFolder(folderName);
        int insertionIndex = 0;
        for (int i = 0; i < firstIndex; ++i)
            if (selected.count(nodes[(size_t) i].id) == 0)
                ++insertionIndex;
        std::vector<MatrixAxisNode> remaining;
        remaining.reserve(nodes.size() - (size_t) selectedHere + 1);
        for (auto& node : nodes)
            if (selected.count(node.id) != 0) folder.children.push_back(std::move(node));
            else remaining.push_back(std::move(node));
        remaining.insert(remaining.begin() + insertionIndex, std::move(folder));
        nodes = std::move(remaining);
        return true;
    }

    for (auto& node : nodes)
        if (node.folder && groupAtCommonLevel(node.children, selected, folderName)) return true;
    return false;
}

void selectAxisLeaf(std::set<int>& selection, const std::vector<int>& visible,
                    int index, int& anchor, const juce::ModifierKeys& modifiers)
{
    if (index < 0 || index >= (int) visible.size()) return;
    const auto number = visible[(size_t) index];
    const bool additive = modifiers.isCommandDown();
    if (modifiers.isShiftDown() && anchor >= 0 && anchor < (int) visible.size())
    {
        if (!additive) selection.clear();
        const auto first = juce::jmin(anchor, index);
        const auto last = juce::jmax(anchor, index);
        for (int i = first; i <= last; ++i) selection.insert(visible[(size_t) i]);
    }
    else if (additive)
    {
        if (selection.count(number) != 0) selection.erase(number);
        else selection.insert(number);
        anchor = index;
    }
    else
    {
        selection.clear();
        selection.insert(number);
        anchor = index;
    }
}

class MatrixAxisEditor final : public juce::Component,
                               private juce::Timer
{
public:
    MatrixAxisEditor(const TreeEntry& matrix, const std::map<int, juce::String>& targetNames,
                     const std::map<int, juce::String>& sourceNames, MatrixControlData initial,
                     std::function<void(const MatrixControlData&)> changed)
        : data(std::move(initial)), onChanged(std::move(changed)),
          targetModel(matrix.matrix.targets, targetNames), sourceModel(matrix.matrix.sources, sourceNames),
          targetAvailable("Available targets", &targetModel), sourceAvailable("Available sources", &sourceModel),
          targetTree(*this, true), sourceTree(*this, false)
    {
        instruction.setText("Select signals with Command (macOS) or Ctrl (Windows/Linux). "
                            "Right-click selected axis entries to create nested folders.",
                            juce::dontSendNotification);
        instruction.setColour(juce::Label::textColourId, Theme::textDim);
        instruction.setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(instruction);

        targetAvailable.setMultipleSelectionEnabled(true);
        sourceAvailable.setMultipleSelectionEnabled(true);
        addAndMakeVisible(targetAvailable);
        addAndMakeVisible(sourceAvailable);
        addAndMakeVisible(targetTree);
        addAndMakeVisible(sourceTree);

        targetTitle.setText("TARGETS — X axis", juce::dontSendNotification);
        sourceTitle.setText("SOURCES — Y axis", juce::dontSendNotification);
        for (auto* label : { &targetTitle, &sourceTitle })
        {
            label->setFont(juce::FontOptions(14.0f).withStyle("Bold"));
            label->setColour(juce::Label::textColourId, Theme::textBright);
            addAndMakeVisible(*label);
        }

        addTargets.setButtonText("Add selected targets  →");
        addSources.setButtonText("Add selected sources  →");
        addTargets.onClick = [this] { addSelected(true); };
        addSources.onClick = [this] { addSelected(false); };
        addAndMakeVisible(addTargets);
        addAndMakeVisible(addSources);
        rebuildTrees();
        startTimerHz(4);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        instruction.setBounds(area.removeFromTop(28));
        auto targetArea = area.removeFromTop(area.getHeight() / 2).reduced(0, 4);
        auto sourceArea = area.reduced(0, 4);
        layoutAxis(targetArea, targetTitle, targetAvailable, addTargets, targetTree);
        layoutAxis(sourceArea, sourceTitle, sourceAvailable, addSources, sourceTree);
    }

private:
    void timerCallback() override
    {
        // Matrix metadata and labels may arrive after the editor opens.
        // Let ListBox refresh its cached row count without discarding the
        // current Command/Ctrl multi-selection.
        targetAvailable.updateContent();
        sourceAvailable.updateContent();
        targetAvailable.repaint();
        sourceAvailable.repaint();
    }

    class AvailableModel final : public juce::ListBoxModel
    {
    public:
        AvailableModel(const std::vector<int>& numbersToUse, const std::map<int, juce::String>& namesToUse)
            : numbers(numbersToUse), names(namesToUse) {}
        int getNumRows() override { return (int) numbers.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            if (row < 0 || row >= (int) numbers.size()) return;
            if (selected) { g.setColour(Theme::accentBlue.withAlpha(0.35f)); g.fillRect(0, 0, width, height); }
            const auto number = numbers[(size_t) row];
            const auto found = names.find(number);
            const auto label = found != names.end() ? found->second : juce::String(number);
            g.setColour(Theme::text);
            g.setFont(juce::FontOptions(12.0f));
            g.drawText(label, 6, 0, width - 94, height, juce::Justification::centredLeft, true);
            g.setColour(Theme::textDim);
            g.drawText(juce::String(number), width - 88, 0, 82, height,
                       juce::Justification::centredRight, true);
        }
        const std::vector<int>& numbers;
        const std::map<int, juce::String>& names;
    };

    class AxisTree;
    class NodeItem final : public juce::TreeViewItem
    {
    public:
        NodeItem(AxisTree& ownerToUse, MatrixAxisNode& nodeToUse)
            : node(nodeToUse), owner(ownerToUse) {}
        bool mightContainSubItems() override { return node.folder; }
        juce::String getUniqueName() const override { return node.id; }
        void paintItem(juce::Graphics& g, int width, int height) override;
        void itemOpennessChanged(bool open) override;
        void itemClicked(const juce::MouseEvent& event) override;
        MatrixAxisNode& node;
    private:
        AxisTree& owner;
    };

    class RootItem final : public juce::TreeViewItem
    {
    public:
        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return "root"; }
    };

    class AxisTree final : public juce::TreeView
    {
    public:
        AxisTree(MatrixAxisEditor& ownerToUse, bool targetsToUse)
            : owner(ownerToUse), targetsAxis(targetsToUse)
        {
            setRootItemVisible(false);
            setMultiSelectEnabled(true);
            setDefaultOpenness(true);
        }

        ~AxisTree() override
        {
            detachRoot();
        }

        void detachRoot()
        {
            setRootItem(nullptr);
            root.reset();
        }

        void rebuild()
        {
            rebuilding = true;
            detachRoot();
            root = std::make_unique<RootItem>();
            addNodes(*root, axis());
            setRootItem(root.get());
            root->setOpen(true);
            rebuilding = false;
        }

        void opennessChanged()
        {
            if (!rebuilding && owner.onChanged) owner.onChanged(owner.data);
        }

        void showMenu(NodeItem& clicked, juce::Component* target)
        {
            if (!clicked.isSelected()) clicked.setSelected(true, false);
            std::set<juce::String> selected;
            for (int i = 0; i < getNumSelectedItems(); ++i)
                if (auto* item = dynamic_cast<NodeItem*>(getSelectedItem(i))) selected.insert(item->node.id);

            juce::PopupMenu menu;
            menu.addItem(1, "Group selected into new folder", !selected.empty());
            menu.addItem(2, "Remove selected from axis", !selected.empty());
            menu.addSeparator();
            menu.addItem(3, "Rename folder...", selected.size() == 1 && clicked.node.folder);
            juce::Component::SafePointer<MatrixAxisEditor> safeOwner(&owner);
            const auto clickedId = clicked.node.id;
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                               [safeOwner, selected, clickedId, targetAxis = targetsAxis] (int result)
                               {
                                   if (safeOwner == nullptr || result == 0) return;
                                   auto& nodes = targetAxis ? safeOwner->data.targets : safeOwner->data.sources;
                                   if (result == 1)
                                   {
                                       const auto name = "Folder " + juce::String(++safeOwner->folderCounter);
                                       safeOwner->detachTrees();
                                       if (groupAtCommonLevel(nodes, selected, name)) safeOwner->changed();
                                       else safeOwner->rebuildTrees();
                                   }
                                   else if (result == 2)
                                   {
                                       safeOwner->detachTrees();
                                       if (removeAxisNodes(nodes, selected)) safeOwner->changed();
                                       else safeOwner->rebuildTrees();
                                   }
                                   else if (result == 3)
                                   {
                                       safeOwner->promptRename(targetAxis, clickedId);
                                   }
                               });
        }

    private:
        std::vector<MatrixAxisNode>& axis() { return targetsAxis ? owner.data.targets : owner.data.sources; }
        void addNodes(juce::TreeViewItem& parent, std::vector<MatrixAxisNode>& nodes)
        {
            for (auto& node : nodes)
            {
                auto* item = new NodeItem(*this, node);
                parent.addSubItem(item);
                if (node.folder)
                {
                    addNodes(*item, node.children);
                    item->setOpen(node.expanded);
                }
            }
        }
        MatrixAxisEditor& owner;
        bool targetsAxis = false;
        bool rebuilding = false;
        std::unique_ptr<RootItem> root;
        friend class NodeItem;
    };

    void layoutAxis(juce::Rectangle<int> area, juce::Label& heading, juce::ListBox& available,
                    juce::TextButton& add, juce::TreeView& tree)
    {
        heading.setBounds(area.removeFromTop(24));
        auto left = area.removeFromLeft(area.getWidth() / 2 - 65);
        auto middle = area.removeFromLeft(130);
        available.setBounds(left.reduced(0, 2));
        add.setBounds(middle.reduced(7, middle.getHeight() / 2 - 18));
        tree.setBounds(area.reduced(0, 2));
    }

    void addSelected(bool targetAxis)
    {
        auto& list = targetAxis ? targetAvailable : sourceAvailable;
        const auto& model = targetAxis ? targetModel : sourceModel;
        auto& axis = targetAxis ? data.targets : data.sources;
        const auto selected = list.getSelectedRows();
        std::vector<std::pair<int, juce::String>> additions;
        for (int i = 0; i < selected.size(); ++i)
        {
            const auto row = selected[i];
            if (row < 0 || row >= (int) model.numbers.size()) continue;
            const auto number = model.numbers[(size_t) row];
            if (axisContainsSignal(axis, number)) continue;
            const auto found = model.names.find(number);
            additions.emplace_back(number,
                found != model.names.end() ? found->second : juce::String(number));
        }

        if (additions.empty()) return;
        // Tree items keep references to nodes in data. Detach them before a
        // vector reallocation can invalidate those references.
        detachTrees();
        for (const auto& addition : additions)
            axis.push_back(MatrixAxisNode::signal(addition.first, addition.second));
        changed();
    }

    void promptRename(bool targetAxis, const juce::String& id)
    {
        auto& axis = targetAxis ? data.targets : data.sources;
        juce::String currentName = "Folder";
        std::function<bool(const std::vector<MatrixAxisNode>&)> findName;
        findName = [&] (const std::vector<MatrixAxisNode>& nodes)
        {
            for (const auto& node : nodes)
            {
                if (node.id == id)
                {
                    currentName = node.name;
                    return true;
                }
                if (node.folder && findName(node.children)) return true;
            }
            return false;
        };
        findName(axis);

        auto* alert = new juce::AlertWindow("Rename Matrix folder", "Enter the new folder name:",
                                            juce::MessageBoxIconType::NoIcon);
        alert->addTextEditor("name", currentName);
        alert->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<MatrixAxisEditor> safeThis(this);
        alert->enterModalState(true, juce::ModalCallbackFunction::create(
            [safeThis, alert, targetAxis, id] (int result)
            {
                if (safeThis == nullptr || result != 1) return;
                const auto name = alert->getTextEditorContents("name").trim();
                if (name.isEmpty()) return;
                auto& axis = targetAxis ? safeThis->data.targets : safeThis->data.sources;
                safeThis->detachTrees();
                if (renameAxisNode(axis, id, name)) safeThis->changed();
                else safeThis->rebuildTrees();
            }), true);
    }

    void changed()
    {
        rebuildTrees();
        if (onChanged) onChanged(data);
    }

    void rebuildTrees()
    {
        targetTree.rebuild();
        sourceTree.rebuild();
    }

    void detachTrees()
    {
        targetTree.detachRoot();
        sourceTree.detachRoot();
    }

    MatrixControlData data;
    std::function<void(const MatrixControlData&)> onChanged;
    AvailableModel targetModel;
    AvailableModel sourceModel;
    juce::ListBox targetAvailable;
    juce::ListBox sourceAvailable;
    AxisTree targetTree;
    AxisTree sourceTree;
    juce::Label instruction;
    juce::Label targetTitle;
    juce::Label sourceTitle;
    juce::TextButton addTargets;
    juce::TextButton addSources;
    int folderCounter = 0;
};

void MatrixAxisEditor::NodeItem::paintItem(juce::Graphics& g, int width, int height)
{
    g.setColour(isSelected() ? Theme::accentBlue.withAlpha(0.35f) : juce::Colours::transparentBlack);
    g.fillRect(0, 0, width, height);
    g.setColour(node.folder ? Theme::textBright : Theme::text);
    g.setFont(juce::FontOptions(12.0f).withStyle(node.folder ? "Bold" : "Regular"));
    auto text = node.folder ? ("Folder: " + node.name)
                            : (node.name + "  [" + juce::String(node.signalNumber) + "]");
    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

void MatrixAxisEditor::NodeItem::itemClicked(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu()) owner.showMenu(*this, event.eventComponent);
}

void MatrixAxisEditor::NodeItem::itemOpennessChanged(bool open)
{
    if (node.expanded == open) return;
    node.expanded = open;
    owner.opennessChanged();
}

class MatrixEditorWindow final : public juce::DocumentWindow
{
public:
    MatrixEditorWindow(const TreeEntry& matrix, const std::map<int, juce::String>& targets,
                       const std::map<int, juce::String>& sources, MatrixControlData data,
                       std::function<void(const MatrixControlData&)> changed)
        : juce::DocumentWindow("Configure Page Matrix", Theme::background,
                               juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setResizeLimits(850, 540, 1800, 1200);
        setContentOwned(new MatrixAxisEditor(matrix, targets, sources, std::move(data), std::move(changed)), true);
        centreWithSize(1180, 760);
        setVisible(true);
        toFront(true);
    }
    void closeButtonPressed() override { setVisible(false); }
};
}

MatrixPageTile::MatrixPageTile(ControlMapping mappingToUse)
    : RemoteControlTile(std::move(mappingToUse)),
      resizeCorner(this, &resizeConstrainer)
{
    resizeConstrainer.setSizeLimits(340, 220, 2200, 1400);
    title.setText(controlMapping.label.isNotEmpty() ? controlMapping.label : "Matrix", juce::dontSendNotification);
    title.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    title.setColour(juce::Label::textColourId, Theme::textBright);
    addAndMakeVisible(title);
    subtitle.setText(controlMapping.label2, juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, Theme::textDim);
    subtitle.setFont(juce::FontOptions(10.0f));
    subtitle.setEditable(false, true, false);
    subtitle.onTextChange = [this]
    {
        controlMapping.label2 = subtitle.getText();
        if (onLabel2Change) onLabel2Change(controlMapping.parameterPath, controlMapping.label2);
    };
    addAndMakeVisible(subtitle);
    editButton.onClick = [this] { openEditor(); };
    editButton.selectionMouseDown = [this] (const juce::MouseEvent& event)
    {
        return !event.mods.isPopupMenu() && requestSelection(event.mods, false);
    };
    addAndMakeVisible(editButton);
    horizontalScroll.addListener(this);
    verticalScroll.addListener(this);
    horizontalScroll.setAutoHide(false);
    verticalScroll.setAutoHide(false);
    addAndMakeVisible(horizontalScroll);
    addAndMakeVisible(verticalScroll);
    addAndMakeVisible(resizeCorner);
    resizeCorner.setAlwaysOnTop(true);
    watchForContextMenu(title);
    watchForContextMenu(subtitle);
    applyPathTooltip();
    rebuildLeaves();
}

MatrixPageTile::~MatrixPageTile()
{
    horizontalScroll.removeListener(this);
    verticalScroll.removeListener(this);
    editorWindow.reset();
}

void MatrixPageTile::setControlLabel2(const juce::String& text)
{
    controlMapping.label2 = text;
    subtitle.setText(text, juce::dontSendNotification);
}

void MatrixPageTile::applyControlColour() { repaint(); }
void MatrixPageTile::beginLabel2Edit() { subtitle.setEditable(false, true, false); subtitle.showEditor(); }

void MatrixPageTile::refreshMatrix(const TreeEntry& newEntry,
                                   const std::vector<ParameterInfo>& parameters)
{
    matrixEntry = newEntry;
    if (matrixEntry.matrix.targets.empty())
        for (const auto& connection : matrixEntry.matrix.connections)
            matrixEntry.matrix.targets.push_back(connection.target);
    if (matrixEntry.matrix.sources.empty())
        for (const auto& connection : matrixEntry.matrix.connections)
            matrixEntry.matrix.sources.insert(matrixEntry.matrix.sources.end(),
                                               connection.sources.begin(), connection.sources.end());
    std::sort(matrixEntry.matrix.targets.begin(), matrixEntry.matrix.targets.end());
    matrixEntry.matrix.targets.erase(std::unique(matrixEntry.matrix.targets.begin(),
                                                  matrixEntry.matrix.targets.end()),
                                     matrixEntry.matrix.targets.end());
    std::sort(matrixEntry.matrix.sources.begin(), matrixEntry.matrix.sources.end());
    matrixEntry.matrix.sources.erase(std::unique(matrixEntry.matrix.sources.begin(),
                                                  matrixEntry.matrix.sources.end()),
                                     matrixEntry.matrix.sources.end());
    parameterSnapshot = parameters;
    targetLabels.clear();
    sourceLabels.clear();
    if (!matrixEntry.matrix.labels.empty())
    {
        const auto provider = ProviderPath::providerId(controlMapping.parameterPath);
        const auto root = ProviderPath::qualify(provider, matrixEntry.matrix.labels.front().basePath);
        const auto targetPrefix = root + ".1.";
        const auto sourcePrefix = root + ".2.";
        for (const auto& parameter : parameters)
        {
            const bool target = parameter.path.startsWith(targetPrefix);
            const bool source = parameter.path.startsWith(sourcePrefix);
            if (!target && !source) continue;
            const auto& prefix = target ? targetPrefix : sourcePrefix;
            const auto tail = parameter.path.substring(prefix.length());
            if (tail.isEmpty() || tail.containsChar('.') || !tail.containsOnly("0123456789")) continue;
            const auto text = parameter.formattedValue();
            if (target) targetLabels[tail.getIntValue()] = text;
            else sourceLabels[tail.getIntValue()] = text;
        }
    }
    connections.clear();
    for (const auto& connection : matrixEntry.matrix.connections)
        connections[connection.target].insert(connection.sources.begin(), connection.sources.end());
    rebuildLeaves();
}

void MatrixPageTile::flattenAxis(const std::vector<MatrixAxisNode>& nodes,
                                 int depth,
                                 std::vector<AxisItem>& items,
                                 const std::map<int, juce::String>& liveLabels)
{
    for (const auto& node : nodes)
    {
        if (node.folder)
        {
            items.push_back({ true, node.id, -1, node.name, depth, node.expanded });
            if (node.expanded)
                flattenAxis(node.children, depth + 1, items, liveLabels);
        }
        else
        {
            const auto found = liveLabels.find(node.signalNumber);
            items.push_back({ false, node.id, node.signalNumber,
                              found != liveLabels.end() && found->second.isNotEmpty() ? found->second : node.name,
                              depth, true });
        }
    }
}

void MatrixPageTile::rebuildLeaves()
{
    targets.clear();
    sources.clear();
    flattenAxis(controlMapping.matrix.targets, 0, targets, targetLabels);
    flattenAxis(controlMapping.matrix.sources, 0, sources, sourceLabels);
    std::set<int> availableTargets;
    std::set<int> availableSources;
    collectAxisSignals(controlMapping.matrix.targets, availableTargets);
    collectAxisSignals(controlMapping.matrix.sources, availableSources);
    for (auto it = selectedTargets.begin(); it != selectedTargets.end();)
        if (availableTargets.count(*it) == 0) it = selectedTargets.erase(it);
        else ++it;
    for (auto it = selectedSources.begin(); it != selectedSources.end();)
        if (availableSources.count(*it) == 0) it = selectedSources.erase(it);
        else ++it;
    const auto visibleTargetSignals = (int) std::count_if(targets.begin(), targets.end(),
                                                          [] (const AxisItem& item) { return !item.folder; });
    const auto visibleSourceSignals = (int) std::count_if(sources.begin(), sources.end(),
                                                          [] (const AxisItem& item) { return !item.folder; });
    if (targetSelectionAnchor >= visibleTargetSignals) targetSelectionAnchor = -1;
    if (sourceSelectionAnchor >= visibleSourceSignals) sourceSelectionAnchor = -1;
    updateScrollRanges();
    repaint();
}

juce::Rectangle<int> MatrixPageTile::gridBounds() const
{
    return getLocalBounds().reduced(6).withTrimmedTop(34).withTrimmedRight(scrollBarSize)
                          .withTrimmedBottom(scrollBarSize);
}

void MatrixPageTile::paint(juce::Graphics& g)
{
    paintFrame(g, nullptr);
    const auto matrix = gridBounds();
    const auto body = matrix.withTrimmedLeft(rowHeaderWidth).withTrimmedTop(columnHeaderHeight);
    g.setColour(Theme::editor);
    g.fillRect(matrix);
    g.setColour(Theme::panelRaised);
    g.fillRect(matrix.getX(), matrix.getY(), rowHeaderWidth, columnHeaderHeight);
    g.fillRect(body.getX(), matrix.getY(), body.getWidth(), columnHeaderHeight);
    g.fillRect(matrix.getX(), body.getY(), rowHeaderWidth, body.getHeight());
    g.setColour(Theme::textBright);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawFittedText("SOURCES \\ TARGETS", matrix.getX() + 6, matrix.getY() + 4,
                     rowHeaderWidth - 12, columnHeaderHeight - 8, juce::Justification::centredLeft, 2);

    const auto xOffset = horizontalScroll.getCurrentRangeStart();
    const auto yOffset = verticalScroll.getCurrentRangeStart();
    const int firstColumn = juce::jmax(0, (int) (xOffset / cellWidth));
    const int firstRow = juce::jmax(0, (int) (yOffset / cellHeight));
    const int lastColumn = juce::jmin((int) targets.size(), firstColumn + body.getWidth() / cellWidth + 2);
    const int lastRow = juce::jmin((int) sources.size(), firstRow + body.getHeight() / cellHeight + 2);

    for (int column = firstColumn; column < lastColumn; ++column)
    {
        const int x = body.getX() + column * cellWidth - (int) xOffset;
        const auto& target = targets[(size_t) column];
        if (target.folder)
        {
            g.setColour(Theme::accentDark.withAlpha(0.72f));
            g.fillRect(x, matrix.getY(), cellWidth, columnHeaderHeight);
            g.setColour(Theme::accentBlue);
            drawDisclosureTriangle(g,
                                   { (float) x + 6.0f + juce::jmin(target.depth, 2) * 5.0f,
                                     (float) matrix.getY() + 7.0f, 8.0f, 8.0f },
                                   target.expanded);
            g.setColour(Theme::textBright);
            g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
            g.drawFittedText(target.label, x + 4, matrix.getY() + 19, cellWidth - 8,
                             columnHeaderHeight - 22, juce::Justification::centred, 3);
            g.setColour(Theme::outline);
            g.drawRect(x, matrix.getY(), cellWidth, columnHeaderHeight);
            continue;
        }
        if (selectedTargets.count(target.number) != 0)
        {
            g.setColour(Theme::accentBlue.withAlpha(0.48f));
            g.fillRect(x, matrix.getY(), cellWidth, columnHeaderHeight);
        }
        g.setColour(Theme::textBright);
        g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
        g.drawFittedText(target.label, x + 3, matrix.getY() + 2, cellWidth - 6, 46,
                         juce::Justification::centred, 3);
        g.setColour(Theme::textDim);
        g.drawText(juce::String(target.number), x + 2, matrix.getY() + 48, cellWidth - 4, 15,
                   juce::Justification::centred, true);
    }

    for (int row = firstRow; row < lastRow; ++row)
    {
        const int y = body.getY() + row * cellHeight - (int) yOffset;
        const auto& source = sources[(size_t) row];
        if (source.folder)
        {
            g.setColour(Theme::accentDark.withAlpha(0.72f));
            g.fillRect(matrix.getX(), y, rowHeaderWidth, cellHeight);
            g.setColour(Theme::accentBlue);
            const auto displayDepth = juce::jmin(source.depth, 6);
            const float triangleX = (float) matrix.getX() + 7.0f + displayDepth * 12.0f;
            drawDisclosureTriangle(g, { triangleX, (float) y + 8.0f, 8.0f, 8.0f }, source.expanded);
            g.setColour(Theme::textBright);
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            const int textX = (int) triangleX + 14;
            g.drawFittedText(source.label, textX, y, matrix.getX() + rowHeaderWidth - textX - 5,
                             cellHeight, juce::Justification::centredLeft, 1);
            g.setColour(Theme::outline);
            g.drawRect(matrix.getX(), y, rowHeaderWidth, cellHeight);
        }
        else if (selectedSources.count(source.number) != 0)
        {
            g.setColour(Theme::accentBlue.withAlpha(0.48f));
            g.fillRect(matrix.getX(), y, rowHeaderWidth, cellHeight);
        }
        if (!source.folder)
        {
            const int textX = matrix.getX() + 5 + juce::jmin(source.depth, 6) * 12;
            g.setColour(Theme::textBright);
            g.setFont(juce::FontOptions(10.0f));
            g.drawFittedText(source.label, textX, y,
                             matrix.getX() + rowHeaderWidth - 76 - textX, cellHeight,
                             juce::Justification::centredLeft, 1);
            g.setColour(Theme::textDim);
            g.drawText(juce::String(source.number), matrix.getX() + rowHeaderWidth - 70, y, 65, cellHeight,
                       juce::Justification::centredRight, true);
        }

        for (int column = firstColumn; column < lastColumn; ++column)
        {
            const int x = body.getX() + column * cellWidth - (int) xOffset;
            const auto& targetItem = targets[(size_t) column];
            if (source.folder || targetItem.folder)
            {
                g.setColour(Theme::panelRaised.darker(0.18f));
                g.fillRect(x, y, cellWidth, cellHeight);
                g.setColour(Theme::outline);
                g.drawRect(x, y, cellWidth, cellHeight);
                continue;
            }
            const auto target = targetItem.number;
            const auto active = connections.find(target);
            const bool connected = active != connections.end() && active->second.count(source.number) != 0;
            g.setColour((row % 2) == 0 ? Theme::panel : Theme::panelRaised);
            g.fillRect(x, y, cellWidth, cellHeight);
            g.setColour(Theme::grid);
            g.drawRect(x, y, cellWidth, cellHeight);
            if (connected)
            {
                g.setColour(Theme::green);
                g.fillEllipse((float) x + cellWidth * 0.5f - 6.0f,
                              (float) y + cellHeight * 0.5f - 6.0f, 12.0f, 12.0f);
            }
        }
    }
    const bool hasVisibleTargets = std::any_of(targets.begin(), targets.end(),
                                               [] (const AxisItem& item) { return !item.folder; });
    const bool hasVisibleSources = std::any_of(sources.begin(), sources.end(),
                                               [] (const AxisItem& item) { return !item.folder; });
    if (!hasVisibleTargets || !hasVisibleSources)
    {
        g.setColour(Theme::textDim);
        g.setFont(juce::FontOptions(14.0f));
        const bool hasFolders = std::any_of(targets.begin(), targets.end(),
                                            [] (const AxisItem& item) { return item.folder; })
                             || std::any_of(sources.begin(), sources.end(),
                                            [] (const AxisItem& item) { return item.folder; });
        g.drawFittedText(hasFolders ? "Open a folder to show its Matrix signals."
                                    : "Select signals in the full Matrix or use Configure Axes...",
                         body.reduced(20), juce::Justification::centred, 2);
    }
    g.setColour(Theme::outline);
    g.drawRect(matrix);
}

void MatrixPageTile::resized()
{
    auto top = getLocalBounds().reduced(7).removeFromTop(27);
    title.setBounds(top.removeFromLeft(220));
    subtitle.setBounds(top.removeFromLeft(220));
    editButton.setBounds(top.removeFromRight(140));
    const auto matrix = gridBounds();
    horizontalScroll.setBounds(matrix.getX() + rowHeaderWidth, matrix.getBottom(),
                               juce::jmax(1, matrix.getWidth() - rowHeaderWidth), scrollBarSize);
    verticalScroll.setBounds(matrix.getRight(), matrix.getY() + columnHeaderHeight,
                             scrollBarSize, juce::jmax(1, matrix.getHeight() - columnHeaderHeight));
    resizeCorner.setBounds(getWidth() - 18, getHeight() - 18, 18, 18);
    resizeCorner.toFront(false);
    updateScrollRanges();

    if (getWidth() > 0 && getHeight() > 0
        && (controlMapping.tileWidth != getWidth() || controlMapping.tileHeight != getHeight()))
    {
        const int widthDelta = controlMapping.tileWidth > 0 ? getWidth() - controlMapping.tileWidth : 0;
        const int heightDelta = controlMapping.tileHeight > 0 ? getHeight() - controlMapping.tileHeight : 0;
        controlMapping.tileWidth = getWidth();
        controlMapping.tileHeight = getHeight();
        if (onSizeChange)
            onSizeChange(controlMapping.parameterPath, getWidth(), getHeight(), widthDelta, heightDelta);
    }
}

void MatrixPageTile::updateScrollRanges()
{
    const auto body = gridBounds().withTrimmedLeft(rowHeaderWidth).withTrimmedTop(columnHeaderHeight);
    const auto visibleWidth = (double) juce::jmax(1, body.getWidth());
    const auto visibleHeight = (double) juce::jmax(1, body.getHeight());
    horizontalScroll.setRangeLimits(0.0, juce::jmax(visibleWidth, (double) targets.size() * cellWidth));
    verticalScroll.setRangeLimits(0.0, juce::jmax(visibleHeight, (double) sources.size() * cellHeight));
    horizontalScroll.setCurrentRange(horizontalScroll.getCurrentRangeStart(), visibleWidth,
                                     juce::dontSendNotification);
    verticalScroll.setCurrentRange(verticalScroll.getCurrentRangeStart(), visibleHeight,
                                   juce::dontSendNotification);
}

void MatrixPageTile::scrollBarMoved(juce::ScrollBar*, double) { repaint(gridBounds()); }

int MatrixPageTile::targetHeaderIndexAt(juce::Point<float> position) const
{
    const auto matrix = gridBounds();
    const auto body = matrix.withTrimmedLeft(rowHeaderWidth).withTrimmedTop(columnHeaderHeight);
    const juce::Rectangle<float> header((float) body.getX(), (float) matrix.getY(),
                                         (float) body.getWidth(), (float) columnHeaderHeight);
    if (!header.contains(position)) return -1;
    const auto index = (int) ((position.x - body.getX()
                              + horizontalScroll.getCurrentRangeStart()) / cellWidth);
    return index >= 0 && index < (int) targets.size() ? index : -1;
}

int MatrixPageTile::sourceHeaderIndexAt(juce::Point<float> position) const
{
    const auto matrix = gridBounds();
    const auto body = matrix.withTrimmedLeft(rowHeaderWidth).withTrimmedTop(columnHeaderHeight);
    const juce::Rectangle<float> header((float) matrix.getX(), (float) body.getY(),
                                         (float) rowHeaderWidth, (float) body.getHeight());
    if (!header.contains(position)) return -1;
    const auto index = (int) ((position.y - body.getY()
                              + verticalScroll.getCurrentRangeStart()) / cellHeight);
    return index >= 0 && index < (int) sources.size() ? index : -1;
}

void MatrixPageTile::selectTarget(int index, const juce::ModifierKeys& modifiers)
{
    if (index < 0 || index >= (int) targets.size()) return;
    if (targets[(size_t) index].folder) return;
    if (modifiers.isPopupMenu() && selectedTargets.count(targets[(size_t) index].number) != 0) return;
    std::vector<int> numbers;
    numbers.reserve(targets.size());
    int signalIndex = -1;
    for (int itemIndex = 0; itemIndex < (int) targets.size(); ++itemIndex)
    {
        const auto& target = targets[(size_t) itemIndex];
        if (target.folder) continue;
        if (itemIndex == index) signalIndex = (int) numbers.size();
        numbers.push_back(target.number);
    }
    selectAxisLeaf(selectedTargets, numbers, signalIndex, targetSelectionAnchor, modifiers);
    repaint(gridBounds());
}

void MatrixPageTile::selectSource(int index, const juce::ModifierKeys& modifiers)
{
    if (index < 0 || index >= (int) sources.size()) return;
    if (sources[(size_t) index].folder) return;
    if (modifiers.isPopupMenu() && selectedSources.count(sources[(size_t) index].number) != 0) return;
    std::vector<int> numbers;
    numbers.reserve(sources.size());
    int signalIndex = -1;
    for (int itemIndex = 0; itemIndex < (int) sources.size(); ++itemIndex)
    {
        const auto& source = sources[(size_t) itemIndex];
        if (source.folder) continue;
        if (itemIndex == index) signalIndex = (int) numbers.size();
        numbers.push_back(source.number);
    }
    selectAxisLeaf(selectedSources, numbers, signalIndex, sourceSelectionAnchor, modifiers);
    repaint(gridBounds());
}

void MatrixPageTile::toggleFolder(bool targetsAxis, const juce::String& id)
{
    auto& nodes = targetsAxis ? controlMapping.matrix.targets : controlMapping.matrix.sources;
    if (!toggleAxisFolder(nodes, id)) return;
    rebuildLeaves();
    if (onMatrixConfigurationChanged)
        onMatrixConfigurationChanged(controlMapping.matrix);
}

void MatrixPageTile::showFolderMenu(bool targetsAxis, const AxisItem& folder,
                                    juce::Component* target)
{
    juce::PopupMenu menu;
    menu.addItem(1, folder.expanded ? "Close folder" : "Open folder");
    menu.addItem(2, "Rename folder...");

    juce::Component::SafePointer<MatrixPageTile> safeThis(this);
    const auto folderId = folder.id;
    const auto folderName = folder.label;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                       [safeThis, targetsAxis, folderId, folderName] (int result)
                       {
                           if (safeThis == nullptr || result == 0) return;
                           if (result == 1)
                               safeThis->toggleFolder(targetsAxis, folderId);
                           else if (result == 2)
                               safeThis->promptRenameFolder(targetsAxis, folderId, folderName);
                       });
}

void MatrixPageTile::promptRenameFolder(bool targetsAxis, const juce::String& id,
                                        const juce::String& currentName)
{
    auto* alert = new juce::AlertWindow("Rename Matrix folder", "Enter the new folder name:",
                                        juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("name", currentName);
    alert->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<MatrixPageTile> safeThis(this);
    alert->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, alert, targetsAxis, id, currentName] (int result)
        {
            if (safeThis == nullptr || result != 1) return;
            const auto name = alert->getTextEditorContents("name").trim();
            if (name.isEmpty() || name == currentName) return;

            auto& nodes = targetsAxis ? safeThis->controlMapping.matrix.targets
                                      : safeThis->controlMapping.matrix.sources;
            if (renameAxisNode(nodes, id, name))
                safeThis->commitMatrixConfiguration();
        }), true);
}

void MatrixPageTile::commitMatrixConfiguration()
{
    editorWindow.reset();
    rebuildLeaves();
    if (onMatrixConfigurationChanged) onMatrixConfigurationChanged(controlMapping.matrix);
}

void MatrixPageTile::showAxisMenu(bool targetsAxis, juce::Component* target)
{
    const auto& selected = targetsAxis ? selectedTargets : selectedSources;
    juce::PopupMenu menu;
    menu.addItem(1, "Group selected signals into new folder", !selected.empty());
    menu.addItem(2, "Remove selected signals from Page Matrix", !selected.empty());
    menu.addSeparator();
    menu.addItem(3, "Clear axis selection", !selected.empty());
    juce::Component::SafePointer<MatrixPageTile> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                       [safeThis, targetsAxis] (int result)
                       {
                           if (safeThis == nullptr || result == 0) return;
                           auto& selection = targetsAxis ? safeThis->selectedTargets
                                                         : safeThis->selectedSources;
                           if (result == 3)
                           {
                               selection.clear();
                               safeThis->repaint(safeThis->gridBounds());
                               return;
                           }

                           std::set<juce::String> ids;
                           for (const auto number : selection) ids.insert("signal:" + juce::String(number));
                           auto& nodes = targetsAxis ? safeThis->controlMapping.matrix.targets
                                                     : safeThis->controlMapping.matrix.sources;
                           if (result == 1)
                           {
                               const auto name = "Folder " + juce::String(++safeThis->folderCounter);
                               if (!groupAtCommonLevel(nodes, ids, name))
                               {
                                   juce::AlertWindow::showMessageBoxAsync(
                                       juce::MessageBoxIconType::InfoIcon,
                                       "Group Matrix signals",
                                       "Selected signals must be on the same folder level.");
                                   return;
                               }
                               safeThis->commitMatrixConfiguration();
                           }
                           else if (result == 2 && removeAxisNodes(nodes, ids))
                           {
                               selection.clear();
                               safeThis->commitMatrixConfiguration();
                           }
                       });
}

void MatrixPageTile::mouseDown(const juce::MouseEvent& event)
{
    if (const auto targetIndex = targetHeaderIndexAt(event.position); targetIndex >= 0)
    {
        if (targets[(size_t) targetIndex].folder)
        {
            if (event.mods.isPopupMenu())
                showFolderMenu(true, targets[(size_t) targetIndex], event.eventComponent);
            else
                toggleFolder(true, targets[(size_t) targetIndex].id);
            return;
        }
        selectTarget(targetIndex, event.mods);
        if (event.mods.isPopupMenu()) showAxisMenu(true, event.eventComponent);
        return;
    }
    if (const auto sourceIndex = sourceHeaderIndexAt(event.position); sourceIndex >= 0)
    {
        if (sources[(size_t) sourceIndex].folder)
        {
            if (event.mods.isPopupMenu())
                showFolderMenu(false, sources[(size_t) sourceIndex], event.eventComponent);
            else
                toggleFolder(false, sources[(size_t) sourceIndex].id);
            return;
        }
        selectSource(sourceIndex, event.mods);
        if (event.mods.isPopupMenu()) showAxisMenu(false, event.eventComponent);
        return;
    }
    if (event.mods.isPopupMenu()) { RemoteControlTile::mouseDown(event); return; }
    const auto body = gridBounds().withTrimmedLeft(rowHeaderWidth).withTrimmedTop(columnHeaderHeight);
    if (!body.contains(event.getPosition())) { RemoteControlTile::mouseDown(event); return; }
    const int column = (int) ((event.x - body.getX() + horizontalScroll.getCurrentRangeStart()) / cellWidth);
    const int row = (int) ((event.y - body.getY() + verticalScroll.getCurrentRangeStart()) / cellHeight);
    if (column < 0 || column >= (int) targets.size() || row < 0 || row >= (int) sources.size()) return;
    if (targets[(size_t) column].folder || sources[(size_t) row].folder) return;
    const auto target = targets[(size_t) column].number;
    const auto source = sources[(size_t) row].number;
    const auto active = connections.find(target);
    const bool connected = active != connections.end() && active->second.count(source) != 0;
    if (onSetCrosspoint && onSetCrosspoint(target, source, !connected))
    {
        if (connected)
        {
            auto& activeSources = connections[target];
            activeSources.erase(source);
            if (activeSources.empty()) connections.erase(target);
        }
        else
        {
            connections[target].insert(source);
        }
        repaint(body);
    }
}

void MatrixPageTile::openEditor()
{
    if (matrixEntry.kind != TreeEntryKind::matrix) return;
    if (editorWindow) { editorWindow->setVisible(true); editorWindow->toFront(true); return; }
    juce::Component::SafePointer<MatrixPageTile> safeThis(this);
    editorWindow = std::make_unique<MatrixEditorWindow>(
        matrixEntry, targetLabels, sourceLabels, controlMapping.matrix,
        [safeThis] (const MatrixControlData& data)
        {
            if (safeThis == nullptr) return;
            safeThis->controlMapping.matrix = data;
            safeThis->rebuildLeaves();
            if (safeThis->onMatrixConfigurationChanged)
                safeThis->onMatrixConfigurationChanged(data);
        });
}
}
