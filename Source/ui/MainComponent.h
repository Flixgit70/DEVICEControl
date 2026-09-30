#pragma once
#include <JuceHeader.h>
#include "../backend/ProviderManager.h"
#include "../model/PageModel.h"
#include "../model/MappingDraft.h"
#include "../services/CsvService.h"
#include "../services/ProjectService.h"
#include "../services/AppSettingsService.h"
#include "../controllers/MappingController.h"
#include "ControlTreeBrowser.h"
#include "MappingTablePanel.h"
#include "ProviderConfigPanel.h"
#include "ControlWorkspace.h"
#include "BrandTitle.h"

namespace er
{
class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum class BulkAction { none, addToMappingTable, exportCsv, expandAll };

    void timerCallback() override;
    void rebuildPageTabs();
    void selectPage(int index);
    void rebuildWorkspace();
    void addMappings(const std::vector<juce::String>& paths, ControlType type);
    void exportLoadedCsv();
    void importCsv();
    void saveProject();
    void loadProject();
    void showError(const juce::String& title, const juce::String& message);

    void beginBulkAction(std::vector<juce::String> roots, BulkAction action);
    void finishBulkAction();
    void setMappingTableVisible(bool visible);
    void setProviderPanelVisible(bool visible);

    ProviderManager providerManager;
    PageSet pageSet;
    MappingDraft mappingDraft;
    MappingController mappingController { providerManager, pageSet };
    AppSettingsService settings;
    int currentPage = 0;
    std::uint64_t lastStoreRevision = 0;
    std::uint64_t lastParameterStructureRevision = 0;
    std::uint64_t lastNavigationTreeRevision = 0;
    bool mappingTableVisible = false;
    bool providerPanelVisible = false;
    bool wasScanning = false;
    BulkAction pendingBulkAction = BulkAction::none;
    std::vector<juce::String> pendingBulkRoots;

    juce::ImageComponent k2Logo;
    BrandTitle appTitle;
    juce::TextButton providersButton { "Providers..." };
    juce::TextButton connectEnabledButton { "Connect Enabled" };
    juce::TextButton disconnectAllButton { "Disconnect All" };
    juce::Label providerSummary;
    juce::TextButton mappingViewButton { "Mapping Table" };
    juce::TextButton exportButton { "Export Loaded" };
    juce::TextButton importButton { "Import CSV" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };

    juce::Component tabBar;
    std::vector<std::unique_ptr<juce::TextButton>> pageButtons;
    juce::TextButton addPageButton { "+" };

    ControlTreeBrowser treeBrowser;
    ControlWorkspace workspace;
    MappingTablePanel mappingPanel { mappingDraft };
    ProviderConfigPanel providerPanel { providerManager };
    juce::Label statusLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
}
