#include "MainComponent.h"
#include "../model/ControlTreeModel.h"
#include "../model/ProviderConfig.h"
#include "AboutDialog.h"
#include "../app/Theme.h"
#include <BinaryData.h>
#include <algorithm>

namespace er
{
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

    mappingViewButton.setClickingTogglesState(true);
    mappingViewButton.onClick = [this] { setMappingTableVisible(mappingViewButton.getToggleState()); };
    exportButton.onClick = [this] { exportLoadedCsv(); };
    importButton.onClick = [this] { importCsv(); };
    saveButton.onClick = [this] { saveProject(); };
    loadButton.onClick = [this] { loadProject(); };
    addAndMakeVisible(mappingViewButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(importButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(loadButton);

    addAndMakeVisible(tabBar);
    tabBar.addAndMakeVisible(addPageButton);
    addPageButton.onClick = [this]
    {
        currentPage = pageSet.addPage();
        rebuildPageTabs();
        rebuildWorkspace();
        mappingPanel.setPageCount((int) pageSet.pages().size());
    };

    treeBrowser.setParameterResolver([this] (const juce::String& path)
    {
        return providerManager.findParameter(path);
    });

    treeBrowser.onOpenDirectory = [this] (const juce::String& path)
    {
        providerManager.requestDirectory(path);
    };
    treeBrowser.onAddControls = [this] (const std::vector<juce::String>& paths, ControlType type)
    {
        addMappings(paths, type);
    };
    treeBrowser.onCollectSelection = [this] (const std::vector<juce::String>& paths)
    {
        beginBulkAction(paths, BulkAction::addToMappingTable);
    };
    treeBrowser.onExportSelection = [this] (const std::vector<juce::String>& paths)
    {
        beginBulkAction(paths, BulkAction::exportCsv);
    };
    treeBrowser.onExpandAllSelection = [this] (const std::vector<juce::String>& paths)
    {
        beginBulkAction(paths, BulkAction::expandAll);
    };
    addAndMakeVisible(treeBrowser);

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
            mappingDraft.setTypeForPath(mapping.parameterPath, mapping.type);
            mappingDraft.setThresholdsForPath(mapping.parameterPath,
                                              mapping.orangeAboveThresholdEnabled, mapping.orangeAboveThreshold,
                                              mapping.redAboveThresholdEnabled, mapping.redAboveThreshold,
                                              mapping.orangeBelowThresholdEnabled, mapping.orangeBelowThreshold,
                                              mapping.redBelowThresholdEnabled, mapping.redBelowThreshold);
        }
        if (!changed) return;
        mappingPanel.refresh();
        rebuildWorkspace();
        statusLabel.setText("Updated " + juce::String((int) mappings.size()) + " selected control(s)",
                            juce::dontSendNotification);
    };
    workspace.onControlColourChanged = [this] (const juce::String& parameterPath, juce::uint32 colourArgb)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        page.setControlColour(parameterPath, colourArgb);
        statusLabel.setText(colourArgb == 0 ? "Control colour reset: " + parameterPath
                                            : "Control colour changed: " + parameterPath,
                            juce::dontSendNotification);
    };
    workspace.onControlLabel2Changed = [this] (const juce::String& parameterPath, const juce::String& label2)
    {
        if (pageSet.pages().empty()) return;
        auto& page = pageSet.pages()[(size_t) currentPage];
        page.setControlLabel2(parameterPath, label2);
        mappingDraft.setLabel2ForPath(parameterPath, label2);
        mappingPanel.refresh();
        statusLabel.setText("Label 2 changed: " + parameterPath, juce::dontSendNotification);
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
            mappingDraft.setThresholdsForPath(parameterPath,
                                              orangeAboveEnabled, orangeAboveValue,
                                              redAboveEnabled, redAboveValue,
                                              orangeBelowEnabled, orangeBelowValue,
                                              redBelowEnabled, redBelowValue);
            mappingPanel.refresh();
            rebuildWorkspace();
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
        mappingDraft.removeRowsForPage(currentPage + 1);
        mappingPanel.refresh();
        rebuildWorkspace();
        statusLabel.setText("Cleared page " + juce::String(currentPage + 1) + " (" + juce::String(removed) + " controls)",
                            juce::dontSendNotification);
    };

    addAndMakeVisible(mappingPanel);
    mappingPanel.onApply = [this]
    {
        const int targetPage = currentPage + 1;
        mappingDraft.applyToPage(pageSet, targetPage);
        rebuildPageTabs();
        mappingPanel.setCurrentPage(targetPage);
        mappingPanel.refresh();
        rebuildWorkspace();
        statusLabel.setText("Mapping table applied to Page " + juce::String(targetPage), juce::dontSendNotification);
    };
    mappingPanel.setPageCount((int) pageSet.pages().size());
    mappingPanel.setCurrentPage(currentPage + 1);

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
    setMappingTableVisible(false);
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
    providerManager.disconnectAll();
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
    top.removeFromLeft(5);
    mappingViewButton.setBounds(top.removeFromLeft(106).reduced(0, 3));

    auto rightButtons = top;
    loadButton.setBounds(rightButtons.removeFromRight(58).reduced(0, 3));
    rightButtons.removeFromRight(4);
    saveButton.setBounds(rightButtons.removeFromRight(58).reduced(0, 3));
    rightButtons.removeFromRight(4);
    importButton.setBounds(rightButtons.removeFromRight(82).reduced(0, 3));
    rightButtons.removeFromRight(4);
    exportButton.setBounds(rightButtons.removeFromRight(98).reduced(0, 3));

    r.removeFromTop(8);
    tabBar.setBounds(r.removeFromTop(36));
    r.removeFromTop(8);
    statusLabel.setBounds(r.removeFromBottom(24));
    r.removeFromBottom(6);

    const int browserWidth = juce::jlimit(330, 460, getWidth() / 3);
    treeBrowser.setBounds(r.removeFromLeft(browserWidth));
    r.removeFromLeft(10);
    workspace.setBounds(r);
    mappingPanel.setBounds(r);
    providerPanel.setBounds(r);

    auto tr = tabBar.getLocalBounds();
    const int tabW = 54;
    for (auto& b : pageButtons)
    {
        b->setBounds(tr.removeFromLeft(tabW).reduced(2, 1));
        tr.removeFromLeft(2);
    }
    addPageButton.setBounds(tr.removeFromLeft(40).reduced(2, 1));
}

void MainComponent::timerCallback()
{
    const auto configs = providerManager.providerConfigs();
    providerSummary.setText(juce::String(providerManager.connectedCount()) + "/" + juce::String((int) configs.size()) + " connected",
                            juce::dontSendNotification);
    treeBrowser.setConnected(providerManager.anyConnected());
    exportButton.setEnabled(providerManager.discoveredParameterCount() > 0);

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
        lastNavigationTreeRevision = navRev;
        treeBrowser.setEntries(providerManager.treeSnapshot());
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
            lastNavigationTreeRevision = finalNavRev;
            treeBrowser.setEntries(providerManager.treeSnapshot());
        }
        finishBulkAction();
    }
    wasScanning = nowScanning;
}

void MainComponent::rebuildPageTabs()
{
    pageButtons.clear();
    for (int i = 0; i < (int) pageSet.pages().size(); ++i)
    {
        auto button = std::make_unique<juce::TextButton>(pageSet.pages()[(size_t) i].name);
        button->setClickingTogglesState(false);
        button->setToggleState(i == currentPage, juce::dontSendNotification);
        button->onClick = [this, i] { selectPage(i); };
        tabBar.addAndMakeVisible(*button);
        pageButtons.push_back(std::move(button));
    }
    tabBar.toFront(false);
    addPageButton.toFront(false);
    mappingPanel.setPageCount((int) pageSet.pages().size());
    mappingPanel.setCurrentPage(currentPage + 1);
    resized();
}

void MainComponent::selectPage(int index)
{
    if (index < 0 || index >= (int) pageSet.pages().size()) return;
    currentPage = index;
    for (int i = 0; i < (int) pageButtons.size(); ++i)
        pageButtons[(size_t) i]->setToggleState(i == currentPage, juce::dontSendNotification);
    mappingPanel.setCurrentPage(currentPage + 1);
    rebuildWorkspace();
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
                      });
}

void MainComponent::addMappings(const std::vector<juce::String>& paths, ControlType type)
{
    const auto added = mappingController.addControlsToPage(paths, type, currentPage);
    rebuildWorkspace();
    if (added == 0)
        statusLabel.setText("Select one or more parameter leaves in the tree", juce::dontSendNotification);
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
    const auto action = pendingBulkAction;
    const auto roots = pendingBulkRoots;
    pendingBulkAction = BulkAction::none;
    pendingBulkRoots.clear();

    if (action == BulkAction::expandAll)
    {
        treeBrowser.expandLoadedBelow(roots);
        statusLabel.setText("Expanded selected Ember+ subtree(s) to loaded leaves", juce::dontSendNotification);
        return;
    }

    auto params = mappingController.parametersUnderRoots(roots);
    if (action == BulkAction::addToMappingTable)
    {
        mappingDraft.addParameters(params, currentPage + 1);
        mappingPanel.refresh();
        setMappingTableVisible(true);
        statusLabel.setText(juce::String((int) params.size()) + " parameters added to mapping table", juce::dontSendNotification);
        return;
    }

    if (action == BulkAction::exportCsv)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Export selected Ember+ branches",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ember_selection.csv"), "*.csv");
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, params = std::move(params)] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file == juce::File{}) return;
                if (file.getFileExtension().isEmpty()) file = file.withFileExtension("csv");
                juce::String error;
                if (!CsvService::exportParameters(file, params, error)) showError("CSV export", error);
            });
    }
}

void MainComponent::setMappingTableVisible(bool visible)
{
    mappingTableVisible = visible;
    if (visible) providerPanelVisible = false;
    mappingViewButton.setToggleState(mappingTableVisible, juce::dontSendNotification);
    providersButton.setToggleState(providerPanelVisible, juce::dontSendNotification);
    workspace.setVisible(!mappingTableVisible && !providerPanelVisible);
    mappingPanel.setVisible(mappingTableVisible);
    providerPanel.setVisible(providerPanelVisible);
    if (mappingTableVisible) mappingPanel.refresh();
}

void MainComponent::setProviderPanelVisible(bool visible)
{
    providerPanelVisible = visible;
    if (visible) mappingTableVisible = false;
    providersButton.setToggleState(providerPanelVisible, juce::dontSendNotification);
    mappingViewButton.setToggleState(mappingTableVisible, juce::dontSendNotification);
    workspace.setVisible(!mappingTableVisible && !providerPanelVisible);
    mappingPanel.setVisible(mappingTableVisible);
    providerPanel.setVisible(providerPanelVisible);
    if (providerPanelVisible)
    {
        providerPanel.setRecentHosts(settings.recentHosts());
        providerPanel.refresh();
    }
}

void MainComponent::exportLoadedCsv()
{
    fileChooser = std::make_unique<juce::FileChooser>("Export loaded Ember+ parameters",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ember_loaded.csv"), "*.csv");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file == juce::File{}) return;
            if (file.getFileExtension().isEmpty()) file = file.withFileExtension("csv");
            juce::String error;
            if (!CsvService::exportParameters(file, providerManager.parameterSnapshot(), error)) showError("CSV export", error);
        });
}

void MainComponent::importCsv()
{
    fileChooser = std::make_unique<juce::FileChooser>("Import control mapping CSV",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.csv");
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file == juce::File{}) return;
            PageSet importedPages;
            juce::String error;
            if (!CsvService::importMappings(file, importedPages, error)) { showError("CSV import", error); return; }
            if (!mappingController.qualifyLegacyImportedMappings(importedPages, error)) { showError("CSV import", error); return; }
            pageSet = std::move(importedPages);
            mappingDraft.replaceFromPages(pageSet, [this] (const juce::String& path) { return providerManager.findParameter(path); });

            currentPage = 0;
            rebuildPageTabs();
            rebuildWorkspace();
            mappingPanel.setPageCount((int) pageSet.pages().size());
            mappingPanel.refresh();
            setMappingTableVisible(false);
            statusLabel.setText("Control mapping imported: " + file.getFileName(), juce::dontSendNotification);
        });
}

void MainComponent::saveProject()
{
    fileChooser = std::make_unique<juce::FileChooser>("Save DEVICEControl project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Remote.emberremote"), "*.emberremote");
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
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.emberremote");
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
            mappingDraft.replaceFromPages(pageSet, [this] (const juce::String& path) { return providerManager.findParameter(path); });
            currentPage = 0;
            lastNavigationTreeRevision = 0;
            lastStoreRevision = 0;
            rebuildPageTabs();
            rebuildWorkspace();
            mappingPanel.setPageCount((int) pageSet.pages().size());
            mappingPanel.refresh();

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
