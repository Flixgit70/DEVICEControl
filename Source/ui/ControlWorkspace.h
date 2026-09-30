#pragma once
#include <JuceHeader.h>
#include "../model/PageModel.h"
#include "PageListView.h"
#include "controls/RemoteControlTile.h"
#include "controls/MeterTile.h"
#include "controls/MatrixPageTile.h"
#include <optional>
#include <set>

namespace er
{
class ControlWorkspace final : public juce::Component
{
public:
    ControlWorkspace();
    void setPage(const PageModel&,
                 std::function<std::optional<ParameterInfo>(const juce::String&)> resolver,
                 std::function<void(const juce::String&, const juce::var&)> writer,
                 std::function<std::optional<TreeEntry>(const juce::String&)> matrixResolver,
                 std::function<std::vector<ParameterInfo>()> parameterSnapshot,
                 std::function<bool(const juce::String&, int, int, bool)> matrixWriter);
    void refreshValues();
    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void(const juce::String&)> onRemoveControl;
    std::function<void(const std::vector<juce::String>&)> onRemoveControls;
    std::function<void(const std::vector<juce::String>&, int)> onReorderControls;
    std::function<void()> onExportListCsv;
    std::function<void()> onImportListCsv;
    std::function<void(const std::vector<ControlMapping>&)> onControlMappingsChanged;
    std::function<void(const juce::String&, juce::uint32)> onControlColourChanged;
    std::function<void(const juce::String&, const juce::String&)> onControlLabel2Changed;
    std::function<void(const juce::String&, int, int)> onControlSizeChanged;
    std::function<void(const juce::String&, const MatrixControlData&)> onMatrixConfigurationChanged;
    std::function<void(const juce::String&, bool, double, bool, double,
                       bool, double, bool, double)> onControlThresholdsChanged;
    std::function<void(PageViewMode)> onViewModeChanged;
    std::function<void()> onClearPage;

private:
    class Canvas final : public juce::Component {};
    void applyViewMode(PageViewMode mode, bool notify);
    void setTileSelected(const juce::String& path, bool selected);
    void applyTileSelectionGesture(const juce::String& path, TileSelectionGesture gesture);
    void refreshTileSelectionState();
    void selectAllTiles();
    void updateSelectionButton();
    void applyColourToSelectedTiles(const juce::String& sourcePath, juce::uint32 colourArgb);
    void resizeSelectedContentTiles(const juce::String& sourcePath, int widthDelta, int heightDelta);

    juce::TextButton tilesButton { "Tiles" };
    juce::TextButton listButton { "List" };
    juce::TextButton selectAllButton { "Select All" };
    juce::TextButton clearAllButton { "Clear All" };
    juce::Viewport viewport;
    Canvas canvas;
    PageListView listView;
    PageViewMode viewMode = PageViewMode::tiles;
    std::function<std::optional<ParameterInfo>(const juce::String&)> parameterResolver;
    std::function<void(const juce::String&, const juce::var&)> parameterWriter;
    std::function<std::optional<TreeEntry>(const juce::String&)> matrixResolver;
    std::function<std::vector<ParameterInfo>()> parameterSnapshot;
    std::function<bool(const juce::String&, int, int, bool)> matrixWriter;
    std::vector<std::unique_ptr<RemoteControlTile>> tiles;
    std::set<juce::String> selectedTilePaths;
    juce::String selectionAnchorPath;
    juce::String currentPageName;
    bool layingOutTiles = false;
    bool applyingGroupResize = false;
};
}
