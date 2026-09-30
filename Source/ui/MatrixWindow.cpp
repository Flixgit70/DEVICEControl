#include "MatrixWindow.h"
#include "../app/Theme.h"
#include "../model/ProviderConfig.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <unordered_set>

namespace er
{
namespace
{
constexpr int rowHeaderWidth = 230;
constexpr int columnHeaderHeight = 68;
constexpr int cellWidth = 62;
constexpr int cellHeight = 27;
constexpr int scrollBarThickness = 15;

juce::String compactLabel(juce::String text)
{
    text = text.replaceCharacters("\r\n\t", "   ").trim();
    return text;
}
}

class MatrixWindow::MatrixView final : public juce::Component,
                                       private juce::Timer,
                                       private juce::ScrollBar::Listener
{
public:
    MatrixView(juce::String pathToUse, Callbacks callbacksToUse)
        : path(std::move(pathToUse)), callbacks(std::move(callbacksToUse)),
          horizontalScroll(false), verticalScroll(true)
    {
        targetSearch.setTextToShowWhenEmpty("Filter targets by name or ID", Theme::textDim);
        sourceSearch.setTextToShowWhenEmpty("Filter sources by name or ID", Theme::textDim);
        targetSearch.onTextChange = [this] { rebuildFilteredAxes(true); };
        sourceSearch.onTextChange = [this] { rebuildFilteredAxes(true); };
        addAndMakeVisible(targetSearch);
        addAndMakeVisible(sourceSearch);

        activeOnly.setButtonText("Connected targets only");
        activeOnly.onClick = [this] { rebuildFilteredAxes(true); };
        addAndMakeVisible(activeOnly);

        reloadButton.setButtonText("Reload Matrix");
        reloadButton.onClick = [this]
        {
            if (callbacks.refreshMatrix) callbacks.refreshMatrix();
            status.setText("Matrix GetDirectory requested ...", juce::dontSendNotification);
        };
        addAndMakeVisible(reloadButton);

        labelsButton.setButtonText("Reload Labels");
        labelsButton.onClick = [this]
        {
            if (callbacks.loadLabels) callbacks.loadLabels();
            status.setText("Target/source labels requested ...", juce::dontSendNotification);
        };
        addAndMakeVisible(labelsButton);

        debugButton.setButtonText("Copy Label Debug");
        debugButton.onClick = [this] { copyLabelDebug(); };
        addAndMakeVisible(debugButton);

        addToPageButton.setButtonText("Add Selection to Page");
        addToPageButton.onClick = [this] { addSelectedSignalsToPage(); };
        addToPageButton.setEnabled(false);
        addAndMakeVisible(addToPageButton);

        status.setColour(juce::Label::textColourId, Theme::text);
        status.setFont(juce::FontOptions(11.5f));
        status.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(status);

        horizontalScroll.addListener(this);
        verticalScroll.addListener(this);
        horizontalScroll.setAutoHide(false);
        verticalScroll.setAutoHide(false);
        addAndMakeVisible(horizontalScroll);
        addAndMakeVisible(verticalScroll);

        refreshMatrixData();
        refreshLabels();
        if (callbacks.loadLabels) callbacks.loadLabels();
        startTimerHz(2);
    }

    ~MatrixView() override
    {
        horizontalScroll.removeListener(this);
        verticalScroll.removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Theme::background);
        g.setColour(Theme::panel);
        g.fillRect(matrixBounds);

        const auto body = bodyBounds();
        if (body.getWidth() <= 0 || body.getHeight() <= 0) return;

        g.setColour(Theme::panelRaised);
        g.fillRect(matrixBounds.getX(), matrixBounds.getY(), rowHeaderWidth, columnHeaderHeight);
        g.fillRect(body.getX(), matrixBounds.getY(), body.getWidth(), columnHeaderHeight);
        g.fillRect(matrixBounds.getX(), body.getY(), rowHeaderWidth, body.getHeight());

        g.setColour(Theme::textBright);
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawFittedText("SOURCES  \\  TARGETS",
                         matrixBounds.getX() + 8, matrixBounds.getY() + 4,
                         rowHeaderWidth - 16, columnHeaderHeight - 8,
                         juce::Justification::centredLeft, 2);

        const auto xOffset = horizontalScroll.getCurrentRangeStart();
        const auto yOffset = verticalScroll.getCurrentRangeStart();
        const int firstColumn = juce::jmax(0, (int) (xOffset / cellWidth));
        const int firstRow = juce::jmax(0, (int) (yOffset / cellHeight));
        const int visibleColumns = body.getWidth() / cellWidth + 2;
        const int visibleRows = body.getHeight() / cellHeight + 2;
        const int lastColumn = juce::jmin((int) visibleTargets.size(), firstColumn + visibleColumns);
        const int lastRow = juce::jmin((int) visibleSources.size(), firstRow + visibleRows);

        for (int column = firstColumn; column < lastColumn; ++column)
        {
            const int x = body.getX() + column * cellWidth - (int) xOffset;
            const auto target = visibleTargets[(size_t) column];
            const auto label = signalLabel(targetLabels, target);
            if (selectedTargets.count(target) != 0)
            {
                g.setColour(Theme::accentBlue.withAlpha(0.48f));
                g.fillRect(x, matrixBounds.getY(), cellWidth, columnHeaderHeight);
            }
            g.setColour(Theme::grid);
            g.drawVerticalLine(x, (float) matrixBounds.getY(), (float) matrixBounds.getBottom());
            g.setColour(Theme::textBright);
            g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
            g.drawFittedText(label, x + 4, matrixBounds.getY() + 3,
                             cellWidth - 8, 39, juce::Justification::centred, 3);
            g.setColour(Theme::textDim);
            g.setFont(juce::FontOptions(9.5f));
            g.drawText(juce::String(target), x + 3, matrixBounds.getY() + 47,
                       cellWidth - 6, 17, juce::Justification::centred, true);
        }

        for (int row = firstRow; row < lastRow; ++row)
        {
            const int y = body.getY() + row * cellHeight - (int) yOffset;
            const auto source = visibleSources[(size_t) row];
            const auto label = signalLabel(sourceLabels, source);

            g.setColour(selectedSources.count(source) != 0
                            ? Theme::accentBlue.withAlpha(0.48f)
                            : ((row % 2) == 0 ? Theme::panelRaised : Theme::panel));
            g.fillRect(matrixBounds.getX(), y, rowHeaderWidth, cellHeight);
            g.setColour(Theme::textBright);
            g.setFont(juce::FontOptions(10.8f).withStyle("Bold"));
            g.drawFittedText(label, matrixBounds.getX() + 7, y + 1,
                             rowHeaderWidth - 91, cellHeight - 2,
                             juce::Justification::centredLeft, 1);
            g.setColour(Theme::textDim);
            g.setFont(juce::FontOptions(9.5f));
            g.drawText(juce::String(source), matrixBounds.getRight() - 84, y + 1,
                       77, cellHeight - 2, juce::Justification::centredRight, true);

            for (int column = firstColumn; column < lastColumn; ++column)
            {
                const int x = body.getX() + column * cellWidth - (int) xOffset;
                const auto target = visibleTargets[(size_t) column];
                const auto active = connections.find(target);
                const bool connected = active != connections.end()
                                     && active->second.count(source) != 0;
                const bool hovered = row == hoveredRow && column == hoveredColumn;

                g.setColour((row % 2) == 0 ? Theme::editor : Theme::panelRaised.darker(0.18f));
                g.fillRect(x, y, cellWidth, cellHeight);
                g.setColour(Theme::grid);
                g.drawRect(x, y, cellWidth, cellHeight, 1);

                if (connected)
                {
                    g.setColour(Theme::green.withAlpha(hovered ? 1.0f : 0.85f));
                    g.fillEllipse((float) x + cellWidth * 0.5f - 7.0f,
                                  (float) y + cellHeight * 0.5f - 7.0f, 14.0f, 14.0f);
                }
                if (hovered)
                {
                    g.setColour(Theme::accentBlue);
                    g.drawRect(x + 1, y + 1, cellWidth - 2, cellHeight - 2, 2);
                }
            }
        }

        g.setColour(Theme::outline);
        g.drawRect(matrixBounds, 1);
        g.drawVerticalLine(body.getX(), (float) matrixBounds.getY(), (float) matrixBounds.getBottom());
        g.drawHorizontalLine(body.getY(), (float) matrixBounds.getX(), (float) matrixBounds.getRight());

        if (visibleSources.empty() || visibleTargets.empty())
        {
            g.setColour(Theme::textDim);
            g.setFont(juce::FontOptions(15.0f));
            g.drawText("No signals match the current filters", body,
                       juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        auto controls = area.removeFromTop(32);
        targetSearch.setBounds(controls.removeFromLeft(245));
        controls.removeFromLeft(6);
        sourceSearch.setBounds(controls.removeFromLeft(245));
        controls.removeFromLeft(8);
        activeOnly.setBounds(controls.removeFromLeft(165));
        controls.removeFromLeft(6);
        reloadButton.setBounds(controls.removeFromLeft(118));
        controls.removeFromLeft(6);
        labelsButton.setBounds(controls.removeFromLeft(112));
        controls.removeFromLeft(6);
        debugButton.setBounds(controls.removeFromLeft(126));
        controls.removeFromLeft(6);
        addToPageButton.setBounds(controls.removeFromLeft(166));

        status.setBounds(area.removeFromTop(25));
        area.removeFromTop(4);
        auto bottom = area.removeFromBottom(scrollBarThickness);
        auto right = area.removeFromRight(scrollBarThickness);
        matrixBounds = area;
        horizontalScroll.setBounds(bottom.withTrimmedLeft(rowHeaderWidth));
        verticalScroll.setBounds(right.withTrimmedTop(columnHeaderHeight));
        updateScrollRanges();
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        updateHover(event.position);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoveredRow = -1;
        hoveredColumn = -1;
        repaint(matrixBounds);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (const auto targetIndex = targetHeaderIndexAt(event.position); targetIndex >= 0)
        {
            selectTarget(targetIndex, event.mods);
            if (event.mods.isPopupMenu()) showSelectionMenu(event.eventComponent);
            return;
        }
        if (const auto sourceIndex = sourceHeaderIndexAt(event.position); sourceIndex >= 0)
        {
            selectSource(sourceIndex, event.mods);
            if (event.mods.isPopupMenu()) showSelectionMenu(event.eventComponent);
            return;
        }
        if (event.mods.isPopupMenu()) return;
        updateHover(event.position);
        if (hoveredRow < 0 || hoveredColumn < 0) return;

        const auto target = visibleTargets[(size_t) hoveredColumn];
        const auto source = visibleSources[(size_t) hoveredRow];
        const auto current = connections.find(target);
        const bool isConnected = current != connections.end() && current->second.count(source) != 0;
        const bool connect = !isConnected;

        if (!callbacks.setCrosspoint || !callbacks.setCrosspoint(target, source, connect))
        {
            status.setText("Crosspoint command could not be sent", juce::dontSendNotification);
            return;
        }

        status.setText((connect ? "Connect " : "Disconnect ")
                       + signalLabel(sourceLabels, source) + " -> "
                       + signalLabel(targetLabels, target) + " sent",
                       juce::dontSendNotification);
    }

private:
    int targetHeaderIndexAt(juce::Point<float> position) const
    {
        const auto body = bodyBounds();
        const juce::Rectangle<float> header((float) body.getX(), (float) matrixBounds.getY(),
                                             (float) body.getWidth(), (float) columnHeaderHeight);
        if (!header.contains(position)) return -1;
        const auto index = (int) ((position.x - body.getX()
                                  + horizontalScroll.getCurrentRangeStart()) / cellWidth);
        return index >= 0 && index < (int) visibleTargets.size() ? index : -1;
    }

    int sourceHeaderIndexAt(juce::Point<float> position) const
    {
        const auto body = bodyBounds();
        const juce::Rectangle<float> header((float) matrixBounds.getX(), (float) body.getY(),
                                             (float) rowHeaderWidth, (float) body.getHeight());
        if (!header.contains(position)) return -1;
        const auto index = (int) ((position.y - body.getY()
                                  + verticalScroll.getCurrentRangeStart()) / cellHeight);
        return index >= 0 && index < (int) visibleSources.size() ? index : -1;
    }

    template <typename Set>
    static void selectAxisItem(Set& selection, const std::vector<int>& visible, int index,
                               int& anchor, const juce::ModifierKeys& modifiers)
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

    void selectTarget(int index, const juce::ModifierKeys& modifiers)
    {
        // A popup click on an unselected item should make it the context item,
        // while a popup click on an existing multi-selection keeps the group.
        if (modifiers.isPopupMenu() && selectedTargets.count(visibleTargets[(size_t) index]) != 0)
            return;
        selectAxisItem(selectedTargets, visibleTargets, index, targetSelectionAnchor, modifiers);
        selectionChanged();
    }

    void selectSource(int index, const juce::ModifierKeys& modifiers)
    {
        if (modifiers.isPopupMenu() && selectedSources.count(visibleSources[(size_t) index]) != 0)
            return;
        selectAxisItem(selectedSources, visibleSources, index, sourceSelectionAnchor, modifiers);
        selectionChanged();
    }

    void selectionChanged()
    {
        addToPageButton.setEnabled(!selectedTargets.empty() || !selectedSources.empty());
        updateStatusText();
        repaint(matrixBounds);
    }

    void showSelectionMenu(juce::Component* target)
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Add selected signals to current Page Matrix",
                     !selectedTargets.empty() || !selectedSources.empty());
        menu.addItem(2, "Clear axis selection",
                     !selectedTargets.empty() || !selectedSources.empty());
        juce::Component::SafePointer<MatrixView> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                           [safeThis] (int result)
                           {
                               if (safeThis == nullptr) return;
                               if (result == 1) safeThis->addSelectedSignalsToPage();
                               else if (result == 2)
                               {
                                   safeThis->selectedTargets.clear();
                                   safeThis->selectedSources.clear();
                                   safeThis->selectionChanged();
                               }
                           });
    }

    void addSelectedSignalsToPage()
    {
        if (!callbacks.addSelectionToPage || (selectedTargets.empty() && selectedSources.empty())) return;
        std::vector<std::pair<int, juce::String>> targetsToAdd;
        std::vector<std::pair<int, juce::String>> sourcesToAdd;
        for (const auto number : allTargets)
            if (selectedTargets.count(number) != 0)
                targetsToAdd.emplace_back(number, signalLabel(targetLabels, number));
        for (const auto number : allSources)
            if (selectedSources.count(number) != 0)
                sourcesToAdd.emplace_back(number, signalLabel(sourceLabels, number));
        callbacks.addSelectionToPage(targetsToAdd, sourcesToAdd);
        status.setText(juce::String((int) sourcesToAdd.size()) + " source(s) and "
                       + juce::String((int) targetsToAdd.size())
                       + " target(s) added to current Page Matrix",
                       juce::dontSendNotification);
    }

    void timerCallback() override
    {
        const auto newTreeRevision = callbacks.treeRevision ? callbacks.treeRevision() : 0;
        if (newTreeRevision != lastTreeRevision)
        {
            lastTreeRevision = newTreeRevision;
            refreshMatrixData();
        }

        const auto newParameterRevision = callbacks.parameterStructureRevision
                                        ? callbacks.parameterStructureRevision() : 0;
        if (newParameterRevision != lastParameterStructureRevision)
        {
            lastParameterStructureRevision = newParameterRevision;
            refreshLabels();
        }
    }

    void scrollBarMoved(juce::ScrollBar*, double) override
    {
        repaint(matrixBounds);
    }

    juce::Rectangle<int> bodyBounds() const
    {
        return matrixBounds.withTrimmedLeft(rowHeaderWidth).withTrimmedTop(columnHeaderHeight);
    }

    static juce::String signalLabel(const std::map<int, juce::String>& labels, int number)
    {
        const auto found = labels.find(number);
        return found != labels.end() && found->second.isNotEmpty()
             ? found->second : juce::String(number);
    }

    void copyLabelDebug()
    {
        juce::String report;
        report << "DEVICEControl Matrix Label Debug\n"
               << "Matrix: " << path << "\n"
               << "Sources (Y): " << juce::String((int) allSources.size()) << "\n"
               << "Targets (X): " << juce::String((int) allTargets.size()) << "\n"
               << "Resolved source labels: " << juce::String((int) sourceLabels.size()) << "\n"
               << "Resolved target labels: " << juce::String((int) targetLabels.size()) << "\n"
               << "Current mode: " << labelStatus << "\n";

        report << "Matrix Labels metadata: ";
        if (entry.matrix.labels.empty()) report << "not reported\n";
        else
        {
            report << juce::String((int) entry.matrix.labels.size()) << " set(s)\n";
            for (const auto& label : entry.matrix.labels)
                report << "  " << label.description << " -> " << label.basePath << "\n";
        }

        struct Group
        {
            juce::String parent;
            int count = 0;
            int first = 0;
            int last = 0;
            int consecutive = 0;
            int sourceMatches = 0;
            int targetMatches = 0;
            juce::StringArray samples;
        };

        const auto providerId = ProviderPath::providerId(path);
        const std::unordered_set<int> sourceNumbers(allSources.begin(), allSources.end());
        const std::unordered_set<int> targetNumbers(allTargets.begin(), allTargets.end());
        std::map<juce::String, std::map<int, juce::String>> grouped;
        int providerParameters = 0;
        int providerStringParameters = 0;

        if (callbacks.parameterSnapshot)
        {
            for (const auto& parameter : callbacks.parameterSnapshot())
            {
                if (parameter.providerId != providerId) continue;
                ++providerParameters;
                if (parameter.kind != ParameterKind::stringValue || parameter.value.isVoid()) continue;
                ++providerStringParameters;

                const auto localPath = parameter.localPath.isNotEmpty()
                                     ? parameter.localPath : ProviderPath::localPath(parameter.path);
                const auto tail = localPath.fromLastOccurrenceOf(".", false, false);
                if (tail.isEmpty() || !tail.containsOnly("0123456789")) continue;
                const auto wideNumber = tail.getLargeIntValue();
                if (wideNumber < 0 || wideNumber > std::numeric_limits<int>::max()) continue;
                auto value = compactLabel(parameter.formattedValue());
                if (value.isEmpty() || value == "-") value = compactLabel(parameter.displayName());
                grouped[ControlTreeModel::parentOf(parameter.path)][(int) wideNumber] = value;
            }
        }

        std::vector<Group> groups;
        groups.reserve(grouped.size());
        for (const auto& [parent, values] : grouped)
        {
            if (values.empty()) continue;
            Group group;
            group.parent = parent;
            group.count = (int) values.size();
            group.first = values.begin()->first;
            group.last = values.rbegin()->first;
            int previous = group.first;
            bool firstValue = true;
            for (const auto& [number, value] : values)
            {
                if (!firstValue && number == previous + 1) ++group.consecutive;
                firstValue = false;
                previous = number;
                if (sourceNumbers.count(number) != 0) ++group.sourceMatches;
                if (targetNumbers.count(number) != 0) ++group.targetMatches;
                if (group.samples.size() < 3)
                    group.samples.add(juce::String(number) + "='" + value.substring(0, 80) + "'");
            }
            groups.push_back(std::move(group));
        }

        std::sort(groups.begin(), groups.end(), [] (const Group& a, const Group& b)
        {
            if (a.count != b.count) return a.count > b.count;
            return (a.sourceMatches + a.targetMatches) > (b.sourceMatches + b.targetMatches);
        });

        report << "\nLoaded provider parameters: " << juce::String(providerParameters)
               << " (string: " << juce::String(providerStringParameters) << ")\n"
               << "Numeric string-parameter groups: " << juce::String((int) groups.size()) << "\n";

        const auto groupLimit = juce::jmin(80, (int) groups.size());
        for (int i = 0; i < groupLimit; ++i)
        {
            const auto& group = groups[(size_t) i];
            const auto sequentialPercent = group.count > 1
                ? (100 * group.consecutive) / (group.count - 1) : 100;
            report << "\n[" << juce::String(i + 1) << "] " << group.parent
                   << "\n  count=" << juce::String(group.count)
                   << " range=" << juce::String(group.first) << ".." << juce::String(group.last)
                   << " sequential=" << juce::String(sequentialPercent) << "%"
                   << " source-ID-matches=" << juce::String(group.sourceMatches)
                   << " target-ID-matches=" << juce::String(group.targetMatches)
                   << "\n  samples: " << group.samples.joinIntoString(" | ") << "\n";
        }

        report << "\nRelevant loaded tree entries in enclosing search branch:\n";
        if (callbacks.treeSnapshot)
        {
            const auto localParent = ControlTreeModel::parentOf(ProviderPath::localPath(path));
            const auto enclosingBranch = ControlTreeModel::parentOf(localParent);
            const auto widerBranch = ControlTreeModel::parentOf(enclosingBranch);
            const auto searchRoot = widerBranch.isNotEmpty() ? widerBranch
                                  : (enclosingBranch.isNotEmpty() ? enclosingBranch : localParent);
            int written = 0;
            for (const auto& treeEntry : callbacks.treeSnapshot())
            {
                if (ProviderPath::providerId(treeEntry.path) != providerId) continue;
                const auto local = ProviderPath::localPath(treeEntry.path);
                if (!ControlTreeModel::isSameOrDescendant(local, searchRoot)) continue;
                const auto relative = local.substring(searchRoot.length()).trimCharactersAtStart(".");
                juce::StringArray relativeParts;
                relativeParts.addTokens(relative, ".", "");
                const auto text = (treeEntry.identifier + " " + treeEntry.description).toLowerCase();
                const bool nearby = relativeParts.size() <= 3;
                const bool relevant = text.contains("label") || text.contains("name")
                                   || text.contains("source") || text.contains("target")
                                   || text.contains("input") || text.contains("output");
                if (!nearby && !relevant) continue;
                report << "  " << local << " | " << treeEntry.identifier
                       << " | " << treeEntry.description << "\n";
                if (++written >= 160) { report << "  ... truncated ...\n"; break; }
            }
            if (written == 0) report << "  none\n";
        }
        else report << "  tree snapshot unavailable\n";

        juce::SystemClipboard::copyTextToClipboard(report);
        status.setText("Label diagnostic copied to clipboard", juce::dontSendNotification);
        status.setTooltip(report.substring(0, 1000));
    }

    void refreshMatrixData()
    {
        if (!callbacks.matrixResolver) return;
        const auto current = callbacks.matrixResolver();
        if (!current || current->kind != TreeEntryKind::matrix) return;
        entry = *current;

        if (!entry.matrix.labels.empty())
        {
            const auto labelRoot = entry.matrix.labels.front().basePath;
            if (labelRoot != lastRequestedLabelRoot)
            {
                lastRequestedLabelRoot = labelRoot;
                if (callbacks.loadLabels) callbacks.loadLabels();
            }
        }

        connections.clear();
        for (const auto& connection : entry.matrix.connections)
            connections[connection.target].insert(connection.sources.begin(), connection.sources.end());

        allSources = entry.matrix.sources;
        allTargets = entry.matrix.targets;
        if (allTargets.empty())
            for (const auto& connection : entry.matrix.connections) allTargets.push_back(connection.target);
        if (allSources.empty())
            for (const auto& connection : entry.matrix.connections)
                allSources.insert(allSources.end(), connection.sources.begin(), connection.sources.end());

        std::sort(allSources.begin(), allSources.end());
        allSources.erase(std::unique(allSources.begin(), allSources.end()), allSources.end());
        std::sort(allTargets.begin(), allTargets.end());
        allTargets.erase(std::unique(allTargets.begin(), allTargets.end()), allTargets.end());
        for (auto it = selectedTargets.begin(); it != selectedTargets.end();)
            if (!std::binary_search(allTargets.begin(), allTargets.end(), *it)) it = selectedTargets.erase(it);
            else ++it;
        for (auto it = selectedSources.begin(); it != selectedSources.end();)
            if (!std::binary_search(allSources.begin(), allSources.end(), *it)) it = selectedSources.erase(it);
            else ++it;
        addToPageButton.setEnabled(!selectedTargets.empty() || !selectedSources.empty());
        rebuildFilteredAxes(false);
    }

    void refreshLabels()
    {
        if (!callbacks.parameterSnapshot)
        {
            labelStatus = "parameter snapshot unavailable";
            updateStatusText();
            return;
        }

        const auto providerId = ProviderPath::providerId(path);
        juce::String targetPrefix;
        juce::String sourcePrefix;
        if (!entry.matrix.labels.empty())
        {
            const auto labelRoot = ProviderPath::qualify(providerId, entry.matrix.labels.front().basePath);
            targetPrefix = labelRoot + ".1.";
            sourcePrefix = labelRoot + ".2.";
        }

        std::map<int, juce::String> newTargets;
        std::map<int, juce::String> newSources;
        targetLabelsAreOrdinal = false;
        sourceLabelsAreOrdinal = false;
        const auto parameters = callbacks.parameterSnapshot();
        for (const auto& parameter : parameters)
        {
            const bool isTarget = targetPrefix.isNotEmpty() && parameter.path.startsWith(targetPrefix);
            const bool isSource = sourcePrefix.isNotEmpty() && parameter.path.startsWith(sourcePrefix);
            if (!isTarget && !isSource) continue;
            const auto prefixLength = isTarget ? targetPrefix.length() : sourcePrefix.length();
            const auto tail = parameter.path.substring(prefixLength);
            if (tail.containsChar('.')) continue;
            auto label = compactLabel(parameter.formattedValue());
            if (label.isEmpty()) label = compactLabel(parameter.displayName());
            if (isTarget) newTargets[tail.getIntValue()] = label;
            else newSources[tail.getIntValue()] = label;
        }

        juce::String discoveredTargetPath;
        juce::String discoveredSourcePath;
        if (newTargets.size() < 4 || newSources.size() < 4)
        {
            struct Candidate
            {
                std::map<int, juce::String> targets;
                std::map<int, juce::String> sources;
                std::map<int, juce::String> orderedLabels;
                juce::String searchText;
            };

            const std::unordered_set<int> targetNumbers(allTargets.begin(), allTargets.end());
            const std::unordered_set<int> sourceNumbers(allSources.begin(), allSources.end());
            std::map<juce::String, Candidate> candidates;

            for (const auto& parameter : parameters)
            {
                if (parameter.providerId != providerId || parameter.value.isVoid()
                    || parameter.kind != ParameterKind::stringValue) continue;
                const auto localPath = parameter.localPath.isNotEmpty()
                                     ? parameter.localPath : ProviderPath::localPath(parameter.path);
                const auto tail = localPath.fromLastOccurrenceOf(".", false, false);
                if (tail.isEmpty() || !tail.containsOnly("0123456789")) continue;
                const auto wideNumber = tail.getLargeIntValue();
                if (wideNumber < 0 || wideNumber > std::numeric_limits<int>::max()) continue;
                const auto number = (int) wideNumber;

                auto label = compactLabel(parameter.formattedValue());
                if (label.isEmpty() || label == "-") label = compactLabel(parameter.displayName());
                if (label.isEmpty() || label == juce::String(number)) continue;

                const auto parent = ControlTreeModel::parentOf(parameter.path);
                auto& candidate = candidates[parent];
                candidate.searchText = (parent + " " + parameter.treePath + " "
                                      + parameter.identifier + " " + parameter.description).toLowerCase();
                candidate.orderedLabels[number] = label;
                if (targetNumbers.count(number) != 0) candidate.targets[number] = label;
                if (sourceNumbers.count(number) != 0) candidate.sources[number] = label;
            }

            auto bestCandidate = [&candidates] (bool targets)
            {
                juce::String bestPath;
                int bestScore = 0;
                for (const auto& [candidatePath, candidate] : candidates)
                {
                    const int count = (int) (targets ? candidate.targets.size() : candidate.sources.size());
                    if (count == 0) continue;
                    int score = count * 100;
                    const auto& text = candidate.searchText;
                    if (targets && (text.contains("target") || text.contains("destination")
                                    || text.contains("output"))) score += 40;
                    if (!targets && (text.contains("source") || text.contains("input"))) score += 40;
                    if (text.contains("label") || text.contains("name")) score += 20;
                    if (score > bestScore) { bestScore = score; bestPath = candidatePath; }
                }
                return bestPath;
            };

            auto bestOrdinalCandidate = [&candidates] (bool targets, int expectedCount)
            {
                juce::String bestPath;
                long long bestScore = std::numeric_limits<long long>::min();
                if (expectedCount < 2) return bestPath;

                for (const auto& [candidatePath, candidate] : candidates)
                {
                    const auto count = (int) candidate.orderedLabels.size();
                    if (count < 2) continue;

                    int consecutive = 0;
                    int previous = 0;
                    bool first = true;
                    for (const auto& [number, unused] : candidate.orderedLabels)
                    {
                        juce::ignoreUnused(unused);
                        if (!first && number == previous + 1) ++consecutive;
                        first = false;
                        previous = number;
                    }
                    if (consecutive * 10 < (count - 1) * 8) continue;

                    const auto difference = std::abs((long long) count - (long long) expectedCount);
                    const auto tolerance = juce::jmax(16, expectedCount / 10);
                    if (difference > tolerance) continue;

                    long long score = 5000000LL - difference * 100LL;
                    const auto& text = candidate.searchText;
                    if (targets && (text.contains("target") || text.contains("destination")
                                    || text.contains("output"))) score += 20000;
                    if (!targets && (text.contains("source") || text.contains("input"))) score += 20000;
                    if (text.contains("label") || text.contains("name")) score += 10000;
                    if (score > bestScore) { bestScore = score; bestPath = candidatePath; }
                }
                return bestPath;
            };

            auto applyOrdinalLabels = [&candidates] (const juce::String& candidatePath,
                                                     const std::vector<int>& signals,
                                                     std::map<int, juce::String>& destination)
            {
                const auto found = candidates.find(candidatePath);
                if (found == candidates.end()) return false;
                destination.clear();
                auto signal = signals.begin();
                for (const auto& [unusedNumber, label] : found->second.orderedLabels)
                {
                    juce::ignoreUnused(unusedNumber);
                    if (signal == signals.end()) break;
                    destination[*signal++] = label;
                }
                return !destination.empty();
            };

            if (newTargets.empty())
            {
                discoveredTargetPath = bestCandidate(true);
                if (auto found = candidates.find(discoveredTargetPath); found != candidates.end())
                    newTargets = found->second.targets;
            }
            if (newTargets.size() < 4)
            {
                const auto ordinalPath = bestOrdinalCandidate(true, (int) allTargets.size());
                if (applyOrdinalLabels(ordinalPath, allTargets, newTargets))
                {
                    discoveredTargetPath = ordinalPath;
                    targetLabelsAreOrdinal = true;
                }
            }
            if (newSources.empty())
            {
                discoveredSourcePath = bestCandidate(false);
                if (auto found = candidates.find(discoveredSourcePath); found != candidates.end())
                    newSources = found->second.sources;
            }
            if (newSources.size() < 4)
            {
                const auto ordinalPath = bestOrdinalCandidate(false, (int) allSources.size());
                if (applyOrdinalLabels(ordinalPath, allSources, newSources))
                {
                    discoveredSourcePath = ordinalPath;
                    sourceLabelsAreOrdinal = true;
                }
            }
        }

        if (!entry.matrix.labels.empty() && discoveredTargetPath.isEmpty() && discoveredSourcePath.isEmpty())
            labelStatus = "standard path " + entry.matrix.labels.front().basePath;
        else if (targetLabelsAreOrdinal || sourceLabelsAreOrdinal)
            labelStatus = "ordinal parameter labels: " + discoveredTargetPath
                        + (discoveredTargetPath != discoveredSourcePath ? " / " + discoveredSourcePath : juce::String());
        else if (!discoveredTargetPath.isEmpty() || !discoveredSourcePath.isEmpty())
            labelStatus = "ID-matched parameter labels";
        else if (entry.matrix.labels.empty())
        {
            const auto directParent = ControlTreeModel::parentOf(ProviderPath::localPath(path));
            const auto enclosingBranch = ControlTreeModel::parentOf(directParent);
            const auto widerBranch = ControlTreeModel::parentOf(enclosingBranch);
            labelStatus = "no Matrix label path; searching "
                        + (widerBranch.isNotEmpty() ? widerBranch
                                                   : (enclosingBranch.isNotEmpty()
                                                          ? enclosingBranch : directParent));
        }
        else
            labelStatus = "label path reported, no values received yet";

        if (newTargets != targetLabels || newSources != sourceLabels)
        {
            targetLabels = std::move(newTargets);
            sourceLabels = std::move(newSources);
            rebuildFilteredAxes(false);
        }
        else updateStatusText();
    }

    void rebuildFilteredAxes(bool resetScroll)
    {
        const auto targetNeedle = targetSearch.getText().trim().toLowerCase();
        const auto sourceNeedle = sourceSearch.getText().trim().toLowerCase();

        visibleTargets.clear();
        visibleSources.clear();
        for (const auto target : allTargets)
        {
            if (activeOnly.getToggleState()
                && (connections.find(target) == connections.end() || connections.at(target).empty()))
                continue;
            const auto text = (signalLabel(targetLabels, target) + " " + juce::String(target)).toLowerCase();
            if (targetNeedle.isEmpty() || text.contains(targetNeedle)) visibleTargets.push_back(target);
        }
        for (const auto source : allSources)
        {
            const auto text = (signalLabel(sourceLabels, source) + " " + juce::String(source)).toLowerCase();
            if (sourceNeedle.isEmpty() || text.contains(sourceNeedle)) visibleSources.push_back(source);
        }

        if (resetScroll)
        {
            horizontalScroll.setCurrentRangeStart(0.0, juce::dontSendNotification);
            verticalScroll.setCurrentRangeStart(0.0, juce::dontSendNotification);
        }
        updateScrollRanges();
        updateStatusText();
        repaint(matrixBounds);
    }

    void updateScrollRanges()
    {
        const auto body = bodyBounds();
        const double contentWidth = (double) visibleTargets.size() * cellWidth;
        const double contentHeight = (double) visibleSources.size() * cellHeight;
        const double visibleWidth = juce::jmax(1.0, (double) body.getWidth());
        const double visibleHeight = juce::jmax(1.0, (double) body.getHeight());
        horizontalScroll.setRangeLimits(0.0, juce::jmax(contentWidth, visibleWidth));
        verticalScroll.setRangeLimits(0.0, juce::jmax(contentHeight, visibleHeight));
        horizontalScroll.setCurrentRange(horizontalScroll.getCurrentRangeStart(),
                                         visibleWidth, juce::dontSendNotification);
        verticalScroll.setCurrentRange(verticalScroll.getCurrentRangeStart(),
                                       visibleHeight, juce::dontSendNotification);
        horizontalScroll.setSingleStepSize(cellWidth);
        verticalScroll.setSingleStepSize(cellHeight);
    }

    void updateStatusText()
    {
        const auto text = juce::String((int) visibleSources.size()) + "/" + juce::String((int) allSources.size())
                        + " S  |  " + juce::String((int) visibleTargets.size()) + "/"
                        + juce::String((int) allTargets.size()) + " T  |  "
                        + "selected " + juce::String((int) selectedSources.size()) + " S / "
                        + juce::String((int) selectedTargets.size()) + " T  |  "
                        + juce::String(entry.matrix.activeCrosspointCount()) + " XP  |  labels "
                        + juce::String((int) sourceLabels.size()) + "/" + juce::String((int) targetLabels.size())
                        + "  |  " + labelStatus;
        status.setText(text, juce::dontSendNotification);
        status.setTooltip(text);
    }

    void updateHover(juce::Point<float> position)
    {
        const auto body = bodyBounds();
        int newRow = -1;
        int newColumn = -1;
        if (body.toFloat().contains(position))
        {
            newColumn = (int) ((position.x - body.getX() + horizontalScroll.getCurrentRangeStart()) / cellWidth);
            newRow = (int) ((position.y - body.getY() + verticalScroll.getCurrentRangeStart()) / cellHeight);
            if (newColumn < 0 || newColumn >= (int) visibleTargets.size()) newColumn = -1;
            if (newRow < 0 || newRow >= (int) visibleSources.size()) newRow = -1;
        }
        if (newRow != hoveredRow || newColumn != hoveredColumn)
        {
            hoveredRow = newRow;
            hoveredColumn = newColumn;
            repaint(matrixBounds);
        }
    }

    juce::String path;
    Callbacks callbacks;
    TreeEntry entry;
    juce::TextEditor targetSearch;
    juce::TextEditor sourceSearch;
    juce::ToggleButton activeOnly;
    juce::TextButton reloadButton;
    juce::TextButton labelsButton;
    juce::TextButton debugButton;
    juce::TextButton addToPageButton;
    juce::Label status;
    juce::ScrollBar horizontalScroll;
    juce::ScrollBar verticalScroll;
    juce::Rectangle<int> matrixBounds;
    std::vector<int> allSources;
    std::vector<int> allTargets;
    std::vector<int> visibleSources;
    std::vector<int> visibleTargets;
    std::map<int, juce::String> sourceLabels;
    std::map<int, juce::String> targetLabels;
    std::map<int, std::set<int>> connections;
    std::set<int> selectedSources;
    std::set<int> selectedTargets;
    std::uint64_t lastTreeRevision = 0;
    std::uint64_t lastParameterStructureRevision = 0;
    juce::String lastRequestedLabelRoot;
    juce::String labelStatus { "waiting for Matrix metadata" };
    bool targetLabelsAreOrdinal = false;
    bool sourceLabelsAreOrdinal = false;
    int hoveredRow = -1;
    int hoveredColumn = -1;
    int sourceSelectionAnchor = -1;
    int targetSelectionAnchor = -1;
};

MatrixWindow::MatrixWindow(const juce::String& qualifiedMatrixPath, Callbacks callbacks)
    : juce::DocumentWindow("DEVICEControl XY Matrix",
                           Theme::background,
                           juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
{
    setUsingNativeTitleBar(true);
    setResizable(true, false);
    setResizeLimits(760, 480, 2400, 1600);
    setContentOwned(new MatrixView(qualifiedMatrixPath, std::move(callbacks)), true);
    centreWithSize(1280, 820);
    setVisible(true);
    toFront(true);
}

void MatrixWindow::closeButtonPressed()
{
    setVisible(false);
}
}
