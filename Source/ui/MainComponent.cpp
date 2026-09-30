#include "MainComponent.h"
#include "../model/ProviderConfig.h"
#include "AboutDialog.h"
#include "../app/Theme.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace er
{
namespace
{
#if JUCE_LINUX
constexpr bool useNativeFileDialogs = false;
#else
constexpr bool useNativeFileDialogs = true;
#endif

class PageTabButton final : public juce::TextButton
{
public:
    explicit PageTabButton(const juce::String& text) : juce::TextButton(text) {}

    std::function<void(juce::Component*)> onPopupMenu;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!event.mods.isPopupMenu())
            juce::TextButton::mouseDown(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            if (onPopupMenu) onPopupMenu(this);
            return;
        }
        juce::TextButton::mouseUp(event);
    }
};

bool matrixAxisContainsSignal(const std::vector<MatrixAxisNode>& nodes, int number)
{
    for (const auto& node : nodes)
    {
        if (!node.folder && node.signalNumber == number) return true;
        if (node.folder && matrixAxisContainsSignal(node.children, number)) return true;
    }
    return false;
}
}
TreePageSplitter::TreePageSplitter()
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

void TreePageSplitter::paint(juce::Graphics& g)
{
    const auto colour = mouseInside ? Theme::accentBlue : Theme::outline.withAlpha(0.8f);
    const auto handle = getLocalBounds().toFloat().withSizeKeepingCentre(3.0f,
                                                                         juce::jmax(18.0f, (float) getHeight() - 18.0f));
    g.setColour(colour);
    g.fillRoundedRectangle(handle, 1.5f);
}

void TreePageSplitter::mouseDown(const juce::MouseEvent&)
{
    if (onDragStart) onDragStart();
}

void TreePageSplitter::mouseDrag(const juce::MouseEvent& event)
{
    if (onDragged) onDragged(event.getDistanceFromDragStartX());
}

void TreePageSplitter::mouseEnter(const juce::MouseEvent&)
{
    mouseInside = true;
    repaint();
}

void TreePageSplitter::mouseExit(const juce::MouseEvent&)
{
    mouseInside = false;
    repaint();
}

MainComponent::MainComponent()
{
    const auto logo = juce::ImageCache::getFromMemory(BinaryData::K2_Logo_png, BinaryData::K2_Logo_pngSize);
    if (logo.isValid())
        k2Logo.setImage(logo, juce::RectanglePlacement::centred);
    k2Logo.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(k2Logo);

    appTitle.onClick = [] { showAboutDialog(); };
    addAndMakeVisible(appTitle);

    providerManager.setProviders(settings.loadProviderConfigs());

    providersButton.setClickingTogglesState(true);
    providersButton.onClick = [this] { setProviderPanelVisible(providersButton.getToggleState()); };
    connectEnabledButton.onClick = [this]
    {
        providerManager.connectEnabled(false);
        for (const auto& cfg : providerManager.providerConfigs())
            if (providerManager.isProviderConnected(cfg.id)) settings.rememberHost(cfg.host);
        providerPanel.setRecentHosts(settings.recentHosts());
        providerPanel.refresh();
    };
    disconnectAllButton.onClick = [this]
    {
        providerManager.disconnectAll();
        providerPanel.refresh();
    };
    addAndMakeVisible(providersButton);
    addAndMakeVisible(connectEnabledButton);
    addAndMakeVisible(disconnectAllButton);

    providerSummary.setFont(juce::FontOptions(11.0f));
    providerSummary.setColour(juce::Label::textColourId, Theme::text);
    providerSummary.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(providerSummary);

    saveButton.onClick = [this] { saveProject(); };
    loadButton.onClick = [this] { loadProject(); };
    addAndMakeVisible(saveButton);
    addAndMakeVisible(loadButton);

    addAndMakeVisible(tabBar);
    tabBar.addAndMakeVisible(addPageButton);
    addPageButton.onClick = [this]
    {
        currentPage = pageSet.addPage();
        rebuildPageTabs();
        rebuildWorkspace();
    };

    treeBrowser.setParameterResolver([this] (const juce::String& path)
    {
        return providerManager.findParameter(path);
    });

    treeBrowser.onOpenDirectory = [this] (const juce::String& path)
    {
        providerManager.requestDirectory(path);
    };
    treeBrowser.onRefreshDirectory = [this] (const juce::String& path)
    {
        providerManager.refreshDirectory(path);
    };
    treeBrowser.onOpenMatrix = [this] (const juce::String& path)
    {
        openMatrixWindow(path);
    };
    treeBrowser.onAddMatrixControl = [this] (const juce::String& path)
    {
        addMatrixControl(path);
    };
    treeBrowser.onAddControls = [this] (const std::vector<juce::String>& paths, ControlType type)
    {
        addMappings(paths, type);
    };
    treeBrowser.onExpandAllSelection = [this] (const std::vector<juce::String>& paths)
    {
        beginBulkAction(paths, BulkAction::expandAll);
    };
    addAndMakeVisible(treeBrowser);

    treePageSplitter.onDragStart = [this]
    {
        treeBrowserWidthAtDragStart = treeBrowserWidth;
    };
    treePageSplitter.onDragged = [this] (int deltaX)
    {
        treeBrowserWidth = treeBrowserWidthAtDragStart + deltaX;
        resized();
    };
    addAndMakeVisible(treePageSplitter);

    addAndMakeVisible(workspace);
    workspace.onRemoveControl = [this] (const juce::String& parameterPath)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        if (!page.removeControl(parameterPath)) return;
        rebuildWorkspace();
        statusLabel.setText("Removed control: " + parameterPath, juce::dontSendNotification);
    };
    workspace.onRemoveControls = [this] (const std::vector<juce::String>& parameterPaths)
    {
        if (pageSet.pages().empty() || parameterPaths.empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        int removed = 0;
        for (const auto& path : parameterPaths)
            if (page.removeControl(path))
                ++removed;
        if (removed == 0) return;
        rebuildWorkspace();
        statusLabel.setText("Removed " + juce::String(removed) + " selected control(s) from Page "
                                + juce::String(currentPage + 1),
                            juce::dontSendNotification);
    };
    workspace.onReorderControls = [this] (const std::vector<juce::String>& paths, int insertionIndex)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        if (!page.moveControls(paths, insertionIndex)) return;
        rebuildWorkspace();
        statusLabel.setText("Moved " + juce::String((int) paths.size()) + " control(s) on Page "
                            + juce::String(currentPage + 1), juce::dontSendNotification);
    };
    workspace.onExportListCsv = [this] { exportPageCsv(); };
    workspace.onImportListCsv = [this] { importPageCsv(); };
    workspace.onControlMappingsChanged = [this] (const std::vector<ControlMapping>& mappings)
    {
        if (pageSet.pages().empty() || mappings.empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        bool changed = false;
        for (const auto& mapping : mappings)
        {
            changed = page.setControlType(mapping.parameterPath, mapping.type) || changed;
            changed = page.setControlThresholds(mapping.parameterPath,
                                                mapping.orangeAboveThresholdEnabled, mapping.orangeAboveThreshold,
                                                mapping.redAboveThresholdEnabled, mapping.redAboveThreshold,
                                                mapping.orangeBelowThresholdEnabled, mapping.orangeBelowThreshold,
                                                mapping.redBelowThresholdEnabled, mapping.redBelowThreshold) || changed;
            changed = page.setControlColour(mapping.parameterPath, mapping.colourArgb) || changed;
        }
        if (!changed) return;
        scheduleWorkspaceRebuild();
        statusLabel.setText("Updated " + juce::String((int) mappings.size()) + " selected control(s)",
                            juce::dontSendNotification);
    };
    workspace.onControlColourChanged = [this] (const juce::String& parameterPath, juce::uint32 colourArgb)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        if (page.setControlColour(parameterPath, colourArgb))
            scheduleWorkspaceRebuild();
        statusLabel.setText(colourArgb == 0 ? "Control colour reset: " + parameterPath
                                            : "Control colour changed: " + parameterPath,
                            juce::dontSendNotification);
    };
    workspace.onControlLabel2Changed = [this] (const juce::String& parameterPath, const juce::String& label2)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        page.setControlLabel2(parameterPath, label2);
        statusLabel.setText("Label 2 changed: " + parameterPath, juce::dontSendNotification);
    };
    workspace.onControlSizeChanged = [this] (const juce::String& parameterPath, int width, int height)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        page.setControlSize(parameterPath, width, height);
    };
    workspace.onMatrixConfigurationChanged = [this] (const juce::String& matrixPath,
                                                       const MatrixControlData& data)
    {
        if (pageSet.pages().empty()) return;
        auto& controls = pageSet.pages()[(size_t) currentPage].controls;
        const auto found = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& mapping)
        {
            return mapping.parameterPath == matrixPath && mapping.type == ControlType::matrix;
        });
        if (found != controls.end()) found->matrix = data;
    };
    workspace.onControlThresholdsChanged = [this] (const juce::String& parameterPath,
                                                    bool orangeAboveEnabled, double orangeAboveValue,
                                                    bool redAboveEnabled, double redAboveValue,
                                                    bool orangeBelowEnabled, double orangeBelowValue,
                                                    bool redBelowEnabled, double redBelowValue)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        if (page.setControlThresholds(parameterPath,
                                      orangeAboveEnabled, orangeAboveValue,
                                      redAboveEnabled, redAboveValue,
                                      orangeBelowEnabled, orangeBelowValue,
                                      redBelowEnabled, redBelowValue))
        {
            scheduleWorkspaceRebuild();
        }
    };
    workspace.onViewModeChanged = [this] (PageViewMode mode)
    {
        if (pageSet.pages().empty()) return;
        pageSet.pages()[(size_t) currentPage].viewMode = mode;
    };
    workspace.onClearPage = [this]
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        const auto removed = (int) page.controls.size();
        page.clearControls();
        rebuildWorkspace();
        statusLabel.setText("Cleared page " + juce::String(currentPage + 1) + " (" + juce::String(removed) + " controls)",
                            juce::dontSendNotification);
    };

    addAndMakeVisible(providerPanel);
    providerPanel.setRecentHosts(settings.recentHosts());
    providerPanel.onConfigurationChanged = [this]
    {
        settings.saveProviderConfigs(providerManager.providerConfigs());
        lastNavigationTreeRevision = 0;
        lastStoreRevision = 0;
        rebuildWorkspace();
    };
    providerPanel.onSuccessfulHost = [this] (const juce::String& host)
    {
        settings.rememberHost(host);
        providerPanel.setRecentHosts(settings.recentHosts());
    };

    statusLabel.setFont(juce::FontOptions(11.0f));
    statusLabel.setColour(juce::Label::textColourId, Theme::text);
    addAndMakeVisible(statusLabel);

    rebuildPageTabs();
    rebuildWorkspace();
    setProviderPanelVisible(false);
    setSize(1320, 780);
    startTimerHz(10);

    // Auto-connect only providers explicitly marked for it. This preserves a
    // deterministic startup while keeping all normal browsing fully lazy.
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(100, [safeThis]
    {
        if (safeThis == nullptr) return;
        safeThis->providerManager.connectEnabled(true);
        for (const auto& cfg : safeThis->providerManager.providerConfigs())
            if (safeThis->providerManager.isProviderConnected(cfg.id)) safeThis->settings.rememberHost(cfg.host);
        safeThis->providerPanel.setRecentHosts(safeThis->settings.recentHosts());
        safeThis->providerPanel.refresh();
    });
}

MainComponent::~MainComponent()
{
    stopTimer();
    matrixWindow.reset();
    providerManager.disconnectAll();
}

void MainComponent::openMatrixWindow(const juce::String& qualifiedPath)
{
    const auto entry = providerManager.findTreeEntry(qualifiedPath);
    if (!entry || entry->kind != TreeEntryKind::matrix)
    {
        showError("XY Matrix", "The selected Ember+ element is no longer available as a matrix.");
        return;
    }

    if (!entry->directoryLoaded)
        providerManager.requestDirectory(qualifiedPath);
    loadMatrixLabels(qualifiedPath);

    MatrixWindow::Callbacks callbacks;
    callbacks.matrixResolver = [this, qualifiedPath]
    {
        return providerManager.findTreeEntry(qualifiedPath);
    };
    callbacks.parameterSnapshot = [this]
    {
        return providerManager.parameterSnapshot();
    };
    callbacks.treeSnapshot = [this]
    {
        return providerManager.treeSnapshot();
    };
    callbacks.treeRevision = [this]
    {
        return providerManager.treeRevision();
    };
    callbacks.parameterStructureRevision = [this]
    {
        return providerManager.parameterStructureRevision();
    };
    callbacks.setCrosspoint = [this, qualifiedPath] (int target, int source, bool connect)
    {
        return providerManager.setMatrixCrosspoint(qualifiedPath, target, source, connect);
    };
    callbacks.refreshMatrix = [this, qualifiedPath]
    {
        providerManager.refreshDirectory(qualifiedPath);
    };
    callbacks.loadLabels = [this, qualifiedPath]
    {
        loadMatrixLabels(qualifiedPath);
    };
    callbacks.addSelectionToPage = [this, qualifiedPath]
        (const std::vector<std::pair<int, juce::String>>& targets,
         const std::vector<std::pair<int, juce::String>>& sources)
    {
        addMatrixSignalsToCurrentPage(qualifiedPath, targets, sources);
    };

    matrixWindow = std::make_unique<MatrixWindow>(qualifiedPath, std::move(callbacks));
}

void MainComponent::loadMatrixLabels(const juce::String& qualifiedPath)
{
    const auto entry = providerManager.findTreeEntry(qualifiedPath);
    if (!entry || entry->kind != TreeEntryKind::matrix) return;
    if (entry->matrix.labels.empty())
    {
        // Some providers omit the optional Matrix Labels field even though
        // their signal names exist as string parameters in a neighbouring
        // branch. The direct Matrix parent contains only the Matrix on the
        // observed provider, and the enclosing 5.1 VSM branch is empty too,
        // so scan the next controlled level (5 for a Matrix at 5.1.1.1).
        // MatrixWindow will match any discovered names
        // back to target/source numbers.
        providerManager.refreshDirectory(qualifiedPath);
        const auto providerId = ProviderPath::providerId(qualifiedPath);
        const auto localParent = ControlTreeModel::parentOf(ProviderPath::localPath(qualifiedPath));
        const auto enclosingBranch = ControlTreeModel::parentOf(localParent);
        const auto widerBranch = ControlTreeModel::parentOf(enclosingBranch);
        providerManager.startSubtreeScan({ ProviderPath::qualify(providerId,
                                                                  widerBranch.isNotEmpty() ? widerBranch
                                                                      : (enclosingBranch.isNotEmpty()
                                                                            ? enclosingBranch : localParent)) });
        return;
    }

    const auto providerId = ProviderPath::providerId(qualifiedPath);
    const auto& primary = entry->matrix.labels.front();
    const auto root = ProviderPath::qualify(providerId, primary.basePath);
    providerManager.requestDirectory(root + ".1"); // target labels
    providerManager.requestDirectory(root + ".2"); // source labels
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background);
    g.setColour(Theme::outline.withAlpha(0.72f));
    g.drawHorizontalLine(58, 0.0f, (float) getWidth());
    g.drawHorizontalLine(103, 0.0f, (float) getWidth());
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced(12);
    auto top = r.removeFromTop(40);
    k2Logo.setBounds(top.removeFromLeft(34).reduced(1));
    top.removeFromLeft(5);
    appTitle.setBounds(top.removeFromLeft(170));

    providersButton.setBounds(top.removeFromLeft(94).reduced(0, 3));
    top.removeFromLeft(5);
    connectEnabledButton.setBounds(top.removeFromLeft(112).reduced(0, 3));
    top.removeFromLeft(5);
    disconnectAllButton.setBounds(top.removeFromLeft(98).reduced(0, 3));
    top.removeFromLeft(6);
    providerSummary.setBounds(top.removeFromLeft(145));

    auto rightButtons = top;
    loadButton.setBounds(rightButtons.removeFromRight(58).reduced(0, 3));
    rightButtons.removeFromRight(4);
    saveButton.setBounds(rightButtons.removeFromRight(58).reduced(0, 3));

    r.removeFromTop(8);
    tabBar.setBounds(r.removeFromTop(36));
    r.removeFromTop(8);
    statusLabel.setBounds(r.removeFromBottom(24));
    r.removeFromBottom(6);

    constexpr int splitterWidth = 10;
    constexpr int minimumTreeWidth = 280;
    constexpr int minimumPagesWidth = 420;
    const int maximumTreeWidth = juce::jmax(minimumTreeWidth,
                                             r.getWidth() - splitterWidth - minimumPagesWidth);
    treeBrowserWidth = juce::jlimit(minimumTreeWidth, maximumTreeWidth, treeBrowserWidth);
    treeBrowser.setBounds(r.removeFromLeft(treeBrowserWidth));
    treePageSplitter.setBounds(r.removeFromLeft(splitterWidth));
    workspace.setBounds(r);
    providerPanel.setBounds(r);

    auto tr = tabBar.getLocalBounds();
    const int buttonGap = 2;
    const int addButtonWidth = 40;
    const int availableForTabs = juce::jmax(0, tr.getWidth() - addButtonWidth
                                               - buttonGap * (int) pageButtons.size());
    const int maximumTabWidth = pageButtons.empty()
        ? 54
        : juce::jlimit(46, 160, availableForTabs / (int) pageButtons.size());
    const juce::Font tabFont { juce::FontOptions(14.0f) };
    for (auto& b : pageButtons)
    {
        juce::GlyphArrangement tabText;
        tabText.addLineOfText(tabFont, b->getButtonText(), 0.0f, 0.0f);
        const int desiredWidth = (int) std::ceil(tabText.getBoundingBox(0, -1, true).getWidth()) + 28;
        const int tabWidth = juce::jlimit(46, maximumTabWidth, desiredWidth);
        b->setBounds(tr.removeFromLeft(tabWidth).reduced(2, 1));
        tr.removeFromLeft(buttonGap);
    }
    addPageButton.setBounds(tr.removeFromLeft(addButtonWidth).reduced(2, 1));
}

void MainComponent::timerCallback()
{
    const auto configs = providerManager.providerConfigs();

    // A successful reconnect starts with a deliberately empty parameter
    // cache. Reload the directories that contain controls already present on
    // any page, so their live values return without requiring the user to
    // browse the Ember+ tree again.
    std::set<juce::String> nowConnected;
    std::set<juce::String> newlyConnected;
    for (const auto& config : configs)
    {
        if (!providerManager.isProviderConnected(config.id)) continue;
        nowConnected.insert(config.id);
        if (connectedProviderIds.find(config.id) == connectedProviderIds.end())
            newlyConnected.insert(config.id);
    }
    connectedProviderIds = std::move(nowConnected);
    if (!newlyConnected.empty())
        refreshMappedParametersFor(newlyConnected);

    // The main UI timer runs at 10 Hz. Poll only the unique Ember+ directories
    // that feed meters on the visible page. These low-priority refreshes are
    // serialised behind normal browsing/tree requests by EmberClient.
    pollVisibleMeters();

    providerSummary.setText(juce::String(providerManager.connectedCount()) + "/" + juce::String((int) configs.size()) + " connected",
                            juce::dontSendNotification);
    treeBrowser.setConnected(providerManager.anyConnected());

    juce::String status = providerManager.statusText()
        + " | Selected Branches: " + juce::String(treeBrowser.selectedBranchCount())
        + " | Parameters: " + juce::String(treeBrowser.selectedParameterCount())
        + " | Page: " + juce::String(currentPage + 1);
    const auto selectedParameterName = treeBrowser.selectedParameterSummary();
    const auto selectedParameterPath = treeBrowser.selectedParameterPath();
    if (selectedParameterName.isNotEmpty())
    {
        status << " | Parameter: " << selectedParameterName;
        if (selectedParameterPath.isNotEmpty())
            if (const auto parameter = providerManager.findParameter(selectedParameterPath))
                status << " | Value: " << parameter->formattedValue();
    }
    statusLabel.setText(status, juce::dontSendNotification);

    const bool nowScanning = providerManager.isScanning();
    const auto navRev = providerManager.treeRevision();
    const bool deferTreeRebuild = nowScanning && pendingBulkAction != BulkAction::none;
    if (navRev != lastNavigationTreeRevision && !deferTreeRebuild)
    {
        treeBrowser.setEntries(providerManager.treeSnapshot());
        lastNavigationTreeRevision = navRev;
    }

    const auto structureRev = providerManager.parameterStructureRevision();
    if (structureRev != lastParameterStructureRevision)
        lastParameterStructureRevision = structureRev;

    const auto rev = providerManager.parameterRevision();
    if (rev != lastStoreRevision)
    {
        lastStoreRevision = rev;
        workspace.refreshValues();
        treeBrowser.repaintValues();
    }

    if ((wasScanning && !nowScanning) || (pendingBulkAction != BulkAction::none && !nowScanning))
    {
        const auto finalNavRev = providerManager.treeRevision();
        if (finalNavRev != lastNavigationTreeRevision)
        {
            treeBrowser.setEntries(providerManager.treeSnapshot());
            lastNavigationTreeRevision = finalNavRev;
        }
        finishBulkAction();
    }
    wasScanning = nowScanning;
    treeBrowser.applyPendingFullRebuildIfSafe();
}

void MainComponent::refreshMappedParametersFor(const std::set<juce::String>& providerIds)
{
    if (providerIds.empty()) return;

    std::set<juce::String> directories;
    for (const auto& page : pageSet.pages())
        for (const auto& mapping : page.controls)
            if (providerIds.find(ProviderPath::providerId(mapping.parameterPath)) != providerIds.end())
                directories.insert(ProviderPath::parentDirectory(mapping.parameterPath));

    for (const auto& directory : directories)
        providerManager.requestDirectory(directory);
}

void MainComponent::pollVisibleMeters()
{
    if (providerManager.isScanning()
        || pageSet.pages().empty()
        || currentPage < 0
        || currentPage >= (int) pageSet.pages().size())
        return;

    std::set<juce::String> directories;
    for (const auto& mapping : pageSet.pages()[(size_t) currentPage].controls)
    {
        if (mapping.type != ControlType::meter) continue;
        const auto providerId = ProviderPath::providerId(mapping.parameterPath);
        if (providerManager.isProviderConnected(providerId))
            directories.insert(ProviderPath::parentDirectory(mapping.parameterPath));
    }

    for (const auto& directory : directories)
        providerManager.refreshDirectory(directory);
}

void MainComponent::rebuildPageTabs()
{
    if (!pageSet.pages().empty())
        currentPage = juce::jlimit(0, (int) pageSet.pages().size() - 1, currentPage);
    pageButtons.clear();
    for (int i = 0; i < (int) pageSet.pages().size(); ++i)
    {
        auto button = std::make_unique<PageTabButton>(pageSet.pages()[(size_t) i].name);
        button->setClickingTogglesState(false);
        button->setToggleState(i == currentPage, juce::dontSendNotification);
        button->onClick = [this, i] { selectPage(i); };
        button->onPopupMenu = [this, i] (juce::Component* target)
        {
            showPageContextMenu(i, target);
        };
        tabBar.addAndMakeVisible(*button);
        pageButtons.push_back(std::move(button));
    }
    tabBar.toFront(false);
    addPageButton.toFront(false);
    resized();
}

void MainComponent::selectPage(int index)
{
    if (index < 0 || index >= (int) pageSet.pages().size()) return;
    currentPage = index;
    for (int i = 0; i < (int) pageButtons.size(); ++i)
        pageButtons[(size_t) i]->setToggleState(i == currentPage, juce::dontSendNotification);
    rebuildWorkspace();
}

void MainComponent::showPageContextMenu(int index, juce::Component* target)
{
    if (index < 0 || index >= (int) pageSet.pages().size()) return;

    juce::PopupMenu menu;
    menu.addItem(1, "Edit Page Label...");
    menu.addSeparator();
    menu.addItem(2, "Move Page Left", index > 0);
    menu.addItem(3, "Move Page Right", index + 1 < (int) pageSet.pages().size());
    menu.addSeparator();
    menu.addItem(4, "Delete Page...", pageSet.pages().size() > 1);

    juce::Component::SafePointer<MainComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target != nullptr ? target : &tabBar),
                       [safeThis, index] (int result)
                       {
                           if (safeThis == nullptr) return;
                           if (result == 1) safeThis->editPageLabel(index);
                           else if (result == 2) safeThis->movePage(index, -1);
                           else if (result == 3) safeThis->movePage(index, 1);
                           else if (result == 4) safeThis->confirmDeletePage(index);
                       });
}

void MainComponent::editPageLabel(int index)
{
    if (index < 0 || index >= (int) pageSet.pages().size()) return;

    auto* dialog = new juce::AlertWindow("Edit Page Label",
                                         "Enter a name for this page:",
                                         juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("pageName", pageSet.pages()[(size_t) index].name, "Page label:");
    if (auto* editor = dialog->getTextEditor("pageName"))
    {
        editor->setInputRestrictions(48);
        editor->selectAll();
    }
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Component::SafePointer<juce::AlertWindow> safeDialog(dialog);
    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([safeThis, safeDialog, index] (int result)
        {
            if (result != 1 || safeThis == nullptr || safeDialog == nullptr) return;
            const auto newName = safeDialog->getTextEditorContents("pageName").trim();
            if (newName.isEmpty())
            {
                safeThis->showError("Edit Page Label", "The page label must not be empty.");
                return;
            }
            if (!safeThis->pageSet.renamePage(index, newName)) return;
            safeThis->rebuildPageTabs();
            safeThis->rebuildWorkspace();
            safeThis->statusLabel.setText("Renamed page to: " + newName, juce::dontSendNotification);
        }), true);
}

void MainComponent::movePage(int index, int delta)
{
    const int target = index + delta;
    if (index < 0 || index >= (int) pageSet.pages().size()) return;
    const auto movedName = pageSet.pages()[(size_t) index].name;
    if (!pageSet.movePage(index, target)) return;

    if (currentPage == index) currentPage = target;
    else if (currentPage == target) currentPage = index;

    rebuildPageTabs();
    rebuildWorkspace();
    statusLabel.setText("Moved page: " + movedName,
                        juce::dontSendNotification);
}

void MainComponent::confirmDeletePage(int index)
{
    if (pageSet.pages().size() <= 1 || index < 0 || index >= (int) pageSet.pages().size()) return;

    const auto pageName = pageSet.pages()[(size_t) index].name;
    const auto controlCount = (int) pageSet.pages()[(size_t) index].controls.size();
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Delete Page")
        .withMessage("Delete page '" + pageName + "' and its "
                     + juce::String(controlCount) + " control(s)?")
        .withButton("Delete")
        .withButton("Cancel")
        .withAssociatedComponent(this);

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::AlertWindow::showAsync(options, [safeThis, index] (int result)
    {
        if (safeThis != nullptr && result == 1)
            safeThis->deletePage(index);
    });
}

void MainComponent::deletePage(int index)
{
    if (index < 0 || index >= (int) pageSet.pages().size()) return;
    const auto deletedName = pageSet.pages()[(size_t) index].name;
    if (!pageSet.removePage(index)) return;

    if (currentPage > index) --currentPage;
    else if (currentPage == index)
        currentPage = juce::jmin(index, (int) pageSet.pages().size() - 1);

    rebuildPageTabs();
    rebuildWorkspace();
    statusLabel.setText("Deleted page: " + deletedName, juce::dontSendNotification);
}

void MainComponent::rebuildWorkspace()
{
    if (pageSet.pages().empty()) return;
    currentPage = juce::jlimit(0, (int) pageSet.pages().size() - 1, currentPage);
    workspace.setPage(pageSet.pages()[(size_t) currentPage],
                      [this] (const juce::String& path) { return providerManager.findParameter(path); },
                      [this] (const juce::String& path, const juce::var& value)
                      {
                          if (!providerManager.setParameterValue(path, value))
                              statusLabel.setText("Write failed: " + path, juce::dontSendNotification);
                      },
                      [this] (const juce::String& path) { return providerManager.findTreeEntry(path); },
                      [this] { return providerManager.parameterSnapshot(); },
                      [this] (const juce::String& path, int target, int source, bool connect)
                      {
                          return providerManager.setMatrixCrosspoint(path, target, source, connect);
                      });
}

void MainComponent::scheduleWorkspaceRebuild()
{
    if (workspaceRebuildPending)
        return;

    workspaceRebuildPending = true;
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis]
    {
        if (safeThis == nullptr) return;
        safeThis->workspaceRebuildPending = false;
        safeThis->rebuildWorkspace();
    });
}

void MainComponent::addMappings(const std::vector<juce::String>& paths, ControlType type)
{
    const auto added = mappingController.addControlsToPage(paths, type, currentPage);
    rebuildWorkspace();
    if (added == 0)
        statusLabel.setText("Select one or more parameter leaves in the tree", juce::dontSendNotification);
}

void MainComponent::addMatrixControl(const juce::String& path)
{
    if (pageSet.pages().empty()) return;
    const auto entry = providerManager.findTreeEntry(path);
    if (!entry || entry->kind != TreeEntryKind::matrix)
    {
        statusLabel.setText("Selected tree entry is not a Matrix", juce::dontSendNotification);
        return;
    }

    auto& page = pageSet.pages()[(size_t) currentPage];
    if (std::any_of(page.controls.begin(), page.controls.end(), [&] (const ControlMapping& mapping)
        { return mapping.parameterPath == path; }))
    {
        statusLabel.setText("Matrix is already on this Page", juce::dontSendNotification);
        return;
    }

    ControlMapping mapping;
    mapping.parameterPath = path;
    mapping.label = entry->displayName();
    mapping.type = ControlType::matrix;
    mapping.tileWidth = 760;
    mapping.tileHeight = 440;
    page.controls.push_back(std::move(mapping));
    loadMatrixLabels(path);
    rebuildWorkspace();
    statusLabel.setText("Matrix added to Page " + juce::String(currentPage + 1),
                        juce::dontSendNotification);
}

void MainComponent::addMatrixSignalsToCurrentPage(
    const juce::String& path,
    const std::vector<std::pair<int, juce::String>>& targets,
    const std::vector<std::pair<int, juce::String>>& sources)
{
    if (pageSet.pages().empty() || (targets.empty() && sources.empty())) return;
    const auto entry = providerManager.findTreeEntry(path);
    if (!entry || entry->kind != TreeEntryKind::matrix)
    {
        statusLabel.setText("Selected Matrix is no longer available", juce::dontSendNotification);
        return;
    }

    auto& controls = pageSet.pages()[(size_t) currentPage].controls;
    auto mapping = std::find_if(controls.begin(), controls.end(), [&] (const ControlMapping& control)
    {
        return control.parameterPath == path && control.type == ControlType::matrix;
    });
    if (mapping == controls.end())
    {
        ControlMapping created;
        created.parameterPath = path;
        created.label = entry->displayName();
        created.type = ControlType::matrix;
        created.tileWidth = 760;
        created.tileHeight = 440;
        controls.push_back(std::move(created));
        mapping = std::prev(controls.end());
    }

    int addedTargets = 0;
    int addedSources = 0;
    for (const auto& [number, label] : targets)
        if (!matrixAxisContainsSignal(mapping->matrix.targets, number))
        {
            mapping->matrix.targets.push_back(MatrixAxisNode::signal(number, label));
            ++addedTargets;
        }
    for (const auto& [number, label] : sources)
        if (!matrixAxisContainsSignal(mapping->matrix.sources, number))
        {
            mapping->matrix.sources.push_back(MatrixAxisNode::signal(number, label));
            ++addedSources;
        }

    rebuildWorkspace();
    statusLabel.setText("Added " + juce::String(addedSources) + " source(s) and "
                        + juce::String(addedTargets) + " target(s) to Page "
                        + juce::String(currentPage + 1) + " Matrix",
                        juce::dontSendNotification);
}

void MainComponent::beginBulkAction(std::vector<juce::String> roots, BulkAction action)
{
    if (roots.empty())
    {
        statusLabel.setText("Select one or more branches or parameters in the tree", juce::dontSendNotification);
        return;
    }
    pendingBulkRoots = std::move(roots);
    pendingBulkAction = action;
    providerManager.startSubtreeScan(pendingBulkRoots);
    wasScanning = providerManager.isScanning();
    if (!wasScanning) finishBulkAction();
}

void MainComponent::finishBulkAction()
{
    if (pendingBulkAction == BulkAction::none) return;
    const auto roots = pendingBulkRoots;
    pendingBulkAction = BulkAction::none;
    pendingBulkRoots.clear();

    treeBrowser.expandLoadedBelow(roots);
    statusLabel.setText("Expanded selected Ember+ subtree(s) to loaded leaves", juce::dontSendNotification);
}

void MainComponent::setProviderPanelVisible(bool visible)
{
    providerPanelVisible = visible;
    providersButton.setToggleState(providerPanelVisible, juce::dontSendNotification);
    workspace.setVisible(!providerPanelVisible);
    providerPanel.setVisible(providerPanelVisible);
    if (providerPanelVisible)
    {
        providerPanel.setRecentHosts(settings.recentHosts());
        providerPanel.refresh();
    }
}

void MainComponent::exportPageCsv()
{
    if (pageSet.pages().empty()) return;
    const auto page = pageSet.pages()[(size_t) currentPage];
    const auto number = currentPage + 1;
    fileChooser = std::make_unique<juce::FileChooser>("Export current page mappings",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("DEVICEControl_Page_" + juce::String(number) + ".csv"), "*.csv");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, page, number] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File{}) return;
            if (file.getFileExtension().isEmpty()) file = file.withFileExtension("csv");
            juce::String error;
            if (!CsvService::exportPageMappings(file, page, number,
                    [this] (const juce::String& path) { return providerManager.findParameter(path); }, error))
                showError("CSV export", error);
        });
}

void MainComponent::importPageCsv()
{
    const int targetPage = currentPage;
    fileChooser = std::make_unique<juce::FileChooser>("Import CSV into current page (replace its controls)",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.csv");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, targetPage] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file == juce::File{}) return;
            PageSet imported;
            juce::String error;
            if (!CsvService::importMappings(file, imported, error)) { showError("CSV import", error); return; }
            if (!mappingController.qualifyLegacyImportedMappings(imported, error))
            { showError("CSV import", error); return; }

            // The list imports enabled CSV rows in file/page order into the
            // selected page, leaving every other page and its view mode intact.
            std::vector<ControlMapping> controls;
            for (auto& source : imported.pages())
                for (auto& control : source.controls)
                    controls.push_back(std::move(control));
            auto& page = pageSet.pages()[(size_t) targetPage];
            page.controls = std::move(controls);

            if (currentPage == targetPage) rebuildWorkspace();
            statusLabel.setText("Imported " + juce::String((int) page.controls.size())
                                + " control(s) into Page " + juce::String(targetPage + 1),
                                juce::dontSendNotification);
        });
}

void MainComponent::saveProject()
{
    fileChooser = std::make_unique<juce::FileChooser>("Save DEVICEControl project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Remote.emberremote"), "*.emberremote", useNativeFileDialogs);
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File{}) return;
            if (file.getFileExtension().isEmpty()) file = file.withFileExtension("emberremote");
            juce::String error;
            if (!ProjectService::save(file, pageSet, providerManager.providerConfigs(), error))
                showError("Save project", error);
        });
}

void MainComponent::loadProject()
{
    fileChooser = std::make_unique<juce::FileChooser>("Load DEVICEControl project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.emberremote", useNativeFileDialogs);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file == juce::File{}) return;
            juce::String error;
            PageSet loadedPages;
            std::vector<ProviderConfig> providers;
            if (!ProjectService::load(file, loadedPages, providers, error)) { showError("Load project", error); return; }

            providerManager.setProviders(providers);
            pageSet = std::move(loadedPages);
            settings.saveProviderConfigs(providerManager.providerConfigs());
            providerPanel.setRecentHosts(settings.recentHosts());
            providerPanel.refresh();
            currentPage = 0;
            lastNavigationTreeRevision = 0;
            lastStoreRevision = 0;
            rebuildPageTabs();
            rebuildWorkspace();

            providerManager.connectEnabled(true);
            for (const auto& cfg : providerManager.providerConfigs())
                if (providerManager.isProviderConnected(cfg.id)) settings.rememberHost(cfg.host);
        });
}

void MainComponent::showError(const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, message);
}
}
