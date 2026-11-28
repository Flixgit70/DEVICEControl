#pragma once
#include <JuceHeader.h>
#include "../backend/ProviderManager.h"
#include "../model/PageModel.h"
#include "../model/MappingIdentityIndex.h"
#include "../services/CsvService.h"
#include "../services/ProjectService.h"
#include "../services/AppSettingsService.h"
#include "../controllers/MappingController.h"
#include "ControlTreeBrowser.h"
#include "ProviderConfigPanel.h"
#include "ControlWorkspace.h"
#include "BrandTitle.h"
#include "MatrixWindow.h"
#include "../cli/CliTransport.h"
#include <set>

namespace er
{
class TreePageSplitter final : public juce::Component
{
public:
    TreePageSplitter();

    std::function<void()> onDragStart;
    std::function<void(int)> onDragged;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    bool mouseInside = false;
};

class MainComponent final : public juce::Component,
                            public juce::DragAndDropContainer,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class BulkAction { none, expandAll };

    void timerCallback() override;
    void rebuildPageTabs();
    void selectPage(int index);
    void showPageContextMenu(int index, juce::Component* target);
    void editPageLabel(int index);
    void movePage(int index, int delta);
    void confirmDeletePage(int index);
    void deletePage(int index);
    void rebuildWorkspace();
    void scheduleWorkspaceRebuild();
    void refreshMappedParametersFor(const std::set<juce::String>& providerIds);
    bool ensureMappingIdentityIndex();
    bool isNameMappingResolved(const juce::String& path, bool matrix = false);
    void captureAndResolveNameMappings(bool captureMissingIdentity = false);
    void pollVisibleMeters();
    void addMappings(const std::vector<juce::String>& paths, ControlType type);
    void addMatrixControl(const juce::String& path);
    void addMatrixSignalsToCurrentPage(
        const juce::String& path,
        const std::vector<std::pair<int, juce::String>>& targets,
        const std::vector<std::pair<int, juce::String>>& sources);
    void exportPageCsv();
    void importPageCsv();
    void saveProject();
    void loadProject();
    void showError(const juce::String& title, const juce::String& message);

    void beginBulkAction(std::vector<juce::String> roots, BulkAction action);
    void finishBulkAction();
    void setProviderPanelVisible(bool visible);
    void openMatrixWindow(const juce::String& qualifiedPath);
    void loadMatrixLabels(const juce::String& qualifiedPath);
    CliSnapshot makeCliSnapshot() const;
    CliCommandResult handleCliCommand(const juce::String& line, CliCommandModel::Mode mode);

    ProviderManager providerManager;
    PageSet pageSet;
    MappingController mappingController { providerManager, pageSet };
    AppSettingsService settings;
    int currentPage = 0;
    std::uint64_t lastStoreRevision = 0;
    std::uint64_t lastParameterStructureRevision = 0;
    std::uint64_t lastNavigationTreeRevision = 0;
    bool providerPanelVisible = false;
    bool workspaceRebuildPending = false;
    bool wasScanning = false;
    BulkAction pendingBulkAction = BulkAction::none;
    std::vector<juce::String> pendingBulkRoots;
    std::set<juce::String> connectedProviderIds;
    MappingIdentityIndex mappingIdentityIndex;
    bool mappingIdentityIndexValid = false;
    bool captureNameIdentitiesPending = false;
    std::uint64_t indexedParameterStructureRevision = 0, indexedTreeIdentityRevision = 0;
    std::map<juce::String, juce::String> visibleParameterNames, visibleMatrixNames;

    juce::ImageComponent k2Logo;
    BrandTitle appTitle;
    juce::TextButton providersButton { "Providers..." };
    juce::TextButton connectEnabledButton { "Connect Enabled" };
    juce::TextButton disconnectAllButton { "Disconnect All" };
    juce::Label providerSummary;
    juce::Label mappingIdentityLabel;
    juce::ComboBox mappingIdentityMode;
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };

    juce::Component tabBar;
    std::vector<std::unique_ptr<juce::TextButton>> pageButtons;
    juce::TextButton addPageButton { "+" };

    ControlTreeBrowser treeBrowser;
    TreePageSplitter treePageSplitter;
    ControlWorkspace workspace;
    ProviderConfigPanel providerPanel { providerManager };
    juce::Label statusLabel;

    int treeBrowserWidth = 420;
    int treeBrowserWidthAtDragStart = 420;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<MatrixWindow> matrixWindow;
    std::unique_ptr<CliServer> cliServer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
}
