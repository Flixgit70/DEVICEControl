#include "ControlWorkspace.h"
#include "../app/Theme.h"

namespace er
{
ControlWorkspace::ControlWorkspace()
{
    tilesButton.setClickingTogglesState(false);
    listButton.setClickingTogglesState(false);
    tilesButton.onClick = [this] { applyViewMode(PageViewMode::tiles, true); };
    listButton.onClick = [this] { applyViewMode(PageViewMode::list, true); };
    selectAllButton.onClick = [this] { selectAllTiles(); };
    selectAllButton.setTooltip("Select or deselect all controls on this page for group colour changes");
    clearAllButton.onClick = [this] { if (onClearPage) onClearPage(); };
    addAndMakeVisible(tilesButton);
    addAndMakeVisible(listButton);
    addAndMakeVisible(selectAllButton);
    addAndMakeVisible(clearAllButton);

    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);
    addAndMakeVisible(listView);

    listView.onLabel2Changed = [this] (const juce::String& path, const juce::String& label2)
    {
        for (auto& tile : tiles)
            if (tile->mapping().parameterPath == path)
                tile->setControlLabel2(label2);
        if (onControlLabel2Changed) onControlLabel2Changed(path, label2);
    };
    listView.onMappingsChanged = [this] (const std::vector<ControlMapping>& mappings)
    {
        if (onControlMappingsChanged)
            onControlMappingsChanged(mappings);
    };
    listView.onRemoveSelected = [this] (const std::vector<juce::String>& paths)
    {
        if (onRemoveControls)
            onRemoveControls(paths);
        else if (onRemoveControl)
            for (const auto& path : paths)
                onRemoveControl(path);
    };
    listView.onReorder = [this] (const std::vector<juce::String>& paths, int position)
    {
        if (onReorderControls) onReorderControls(paths, position);
    };
    listView.onExportCsv = [this] { if (onExportListCsv) onExportListCsv(); };
    listView.onImportCsv = [this] { if (onImportListCsv) onImportListCsv(); };

    applyViewMode(PageViewMode::tiles, false);
}

void ControlWorkspace::setPage(const PageModel& page,
                               std::function<std::optional<ParameterInfo>(const juce::String&)> resolver,
                               std::function<void(const juce::String&, const juce::var&)> writer,
                               std::function<std::optional<TreeEntry>(const juce::String&)> matrixResolverToUse,
                               std::function<std::vector<ParameterInfo>()> parameterSnapshotToUse,
                               std::function<bool(const juce::String&, int, int, bool)> matrixWriterToUse)
{
    parameterResolver = std::move(resolver);
    parameterWriter = std::move(writer);
    matrixResolver = std::move(matrixResolverToUse);
    parameterSnapshot = std::move(parameterSnapshotToUse);
    matrixWriter = std::move(matrixWriterToUse);
    if (currentPageName != page.name)
    {
        selectedTilePaths.clear();
        selectionAnchorPath.clear();
    }
    else
    {
        std::set<juce::String> availablePaths;
        for (const auto& mapping : page.controls)
            availablePaths.insert(mapping.parameterPath);
        for (auto it = selectedTilePaths.begin(); it != selectedTilePaths.end();)
            if (availablePaths.count(*it) == 0) it = selectedTilePaths.erase(it);
            else ++it;
        if (availablePaths.count(selectionAnchorPath) == 0)
            selectionAnchorPath.clear();
    }
    currentPageName = page.name;
    tiles.clear();
    canvas.removeAllChildren();
    for (const auto& mapping : page.controls)
    {
        std::unique_ptr<RemoteControlTile> tile;
        if (mapping.type == ControlType::matrix) tile = std::make_unique<MatrixPageTile>(mapping);
        else if (mapping.type == ControlType::encoder) tile = std::make_unique<EncoderTile>(mapping);
        else if (mapping.type == ControlType::showContent) tile = std::make_unique<ShowContentTile>(mapping);
        else if (mapping.type == ControlType::meter) tile = std::make_unique<MeterTile>(mapping);
        else tile = std::make_unique<ButtonTile>(mapping);
        tile->onWrite = parameterWriter;
        tile->onRemove = [this] (const juce::String& path)
        {
            if (onRemoveControl)
                onRemoveControl(path);
        };
        tile->onColourChange = [this] (const juce::String& path, juce::uint32 colourArgb)
        {
            applyColourToSelectedTiles(path, colourArgb);
        };
        tile->onLabel2Change = [this] (const juce::String& path, const juce::String& label2)
        {
            listView.setControlLabel2(path, label2);
            if (onControlLabel2Changed)
                onControlLabel2Changed(path, label2);
        };
        tile->onSelectionChange = [this] (const juce::String& path, bool selected)
        {
            setTileSelected(path, selected);
        };
        tile->onSelectionGesture = [this] (const juce::String& path, TileSelectionGesture gesture)
        {
            applyTileSelectionGesture(path, gesture);
        };
        tile->onSizeChange = [this] (const juce::String& path, int width, int height,
                                     int widthDelta, int heightDelta)
        {
            if (onControlSizeChanged)
                onControlSizeChanged(path, width, height);
            if (layingOutTiles || applyingGroupResize) return;

            resizeSelectedContentTiles(path, widthDelta, heightDelta);
            resized();
        };
        if (auto* matrixTile = dynamic_cast<MatrixPageTile*>(tile.get()))
        {
            matrixTile->onSetCrosspoint = [this, path = mapping.parameterPath]
                (int target, int source, bool connect)
            {
                return matrixWriter && matrixWriter(path, target, source, connect);
            };
            matrixTile->onMatrixConfigurationChanged = [this, path = mapping.parameterPath]
                (const MatrixControlData& data)
            {
                if (onMatrixConfigurationChanged) onMatrixConfigurationChanged(path, data);
            };
        }
        if (selectedTilePaths.count(mapping.parameterPath) != 0)
            tile->setTileSelected(true);
        canvas.addAndMakeVisible(*tile);
        tiles.push_back(std::move(tile));
    }

    listView.setPage(page, parameterResolver, parameterWriter);
    applyViewMode(page.viewMode, false);
    updateSelectionButton();
    resized();
    refreshValues();
}

void ControlWorkspace::setTileSelected(const juce::String& path, bool selected)
{
    if (selected) selectedTilePaths.insert(path);
    else selectedTilePaths.erase(path);
    selectionAnchorPath = path;

    refreshTileSelectionState();
}

void ControlWorkspace::applyTileSelectionGesture(const juce::String& path,
                                                 TileSelectionGesture gesture)
{
    int clickedIndex = -1;
    int anchorIndex = -1;
    for (int i = 0; i < (int) tiles.size(); ++i)
    {
        const auto& tilePath = tiles[(size_t) i]->mapping().parameterPath;
        if (tilePath == path) clickedIndex = i;
        if (tilePath == selectionAnchorPath) anchorIndex = i;
    }

    if (clickedIndex < 0)
        return;

    if (gesture == TileSelectionGesture::replace)
    {
        selectedTilePaths.clear();
        selectedTilePaths.insert(path);
        selectionAnchorPath = path;
    }
    else if (gesture == TileSelectionGesture::toggle)
    {
        if (selectedTilePaths.count(path) != 0)
            selectedTilePaths.erase(path);
        else
            selectedTilePaths.insert(path);
        selectionAnchorPath = path;
    }
    else
    {
        if (anchorIndex < 0)
            anchorIndex = clickedIndex;
        if (gesture == TileSelectionGesture::range)
            selectedTilePaths.clear();

        const int first = juce::jmin(anchorIndex, clickedIndex);
        const int last = juce::jmax(anchorIndex, clickedIndex);
        for (int i = first; i <= last; ++i)
            selectedTilePaths.insert(tiles[(size_t) i]->mapping().parameterPath);

        if (selectionAnchorPath.isEmpty())
            selectionAnchorPath = path;
    }

    refreshTileSelectionState();
}

void ControlWorkspace::refreshTileSelectionState()
{

    for (auto& tile : tiles)
        tile->setTileSelected(selectedTilePaths.count(tile->mapping().parameterPath) != 0);

    updateSelectionButton();
}

void ControlWorkspace::selectAllTiles()
{
    const bool selectAll = !tiles.empty() && selectedTilePaths.size() < tiles.size();
    selectedTilePaths.clear();
    for (auto& tile : tiles)
    {
        if (selectAll)
            selectedTilePaths.insert(tile->mapping().parameterPath);
        tile->setTileSelected(selectAll);
    }

    selectionAnchorPath = selectAll && !tiles.empty()
        ? tiles.front()->mapping().parameterPath
        : juce::String();

    updateSelectionButton();
}

void ControlWorkspace::updateSelectionButton()
{
    const bool allSelected = !tiles.empty() && selectedTilePaths.size() == tiles.size();
    selectAllButton.setButtonText(allSelected ? "Deselect All" : "Select All");
    selectAllButton.setEnabled(viewMode == PageViewMode::tiles && !tiles.empty());
}

void ControlWorkspace::applyColourToSelectedTiles(const juce::String& sourcePath,
                                                  juce::uint32 colourArgb)
{
    const bool applyToSelection = selectedTilePaths.count(sourcePath) != 0;
    if (!applyToSelection)
    {
        if (onControlColourChanged)
            onControlColourChanged(sourcePath, colourArgb);
        return;
    }

    for (auto& tile : tiles)
    {
        const auto& mapping = tile->mapping();
        if (selectedTilePaths.count(mapping.parameterPath) == 0)
            continue;

        tile->setControlColour(colourArgb);
        if (onControlColourChanged)
            onControlColourChanged(mapping.parameterPath, colourArgb);
    }
}

void ControlWorkspace::resizeSelectedContentTiles(const juce::String& sourcePath,
                                                  int widthDelta,
                                                  int heightDelta)
{
    int selectedContentCount = 0;
    for (const auto& tile : tiles)
        if (tile->mapping().type == ControlType::showContent
            && selectedTilePaths.count(tile->mapping().parameterPath) != 0)
            ++selectedContentCount;

    if (selectedContentCount < 2
        || selectedTilePaths.count(sourcePath) == 0
        || (widthDelta == 0 && heightDelta == 0))
        return;

    juce::ScopedValueSetter<bool> groupResizeGuard(applyingGroupResize, true);
    for (auto& tile : tiles)
    {
        const auto& mapping = tile->mapping();
        if (mapping.type != ControlType::showContent
            || mapping.parameterPath == sourcePath
            || selectedTilePaths.count(mapping.parameterPath) == 0)
            continue;

        const int width = juce::jlimit(180, 720, tile->getWidth() + widthDelta);
        const int height = juce::jlimit(110, 420, tile->getHeight() + heightDelta);
        tile->setSize(width, height);
    }
}

void ControlWorkspace::refreshValues()
{
    std::vector<ParameterInfo> parameters;
    if (parameterSnapshot) parameters = parameterSnapshot();
    for (auto& tile : tiles)
    {
        if (tile->mapping().type == ControlType::matrix)
        {
            if (matrixResolver)
                if (auto entry = matrixResolver(tile->mapping().parameterPath))
                    if (auto* matrixTile = dynamic_cast<MatrixPageTile*>(tile.get()))
                        matrixTile->refreshMatrix(*entry, parameters);
        }
        else if (parameterResolver)
            if (auto p = parameterResolver(tile->mapping().parameterPath)) tile->refresh(*p);
    }
    listView.refreshValues();
}

void ControlWorkspace::paint(juce::Graphics& g)
{
    g.setColour(Theme::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    if (viewMode == PageViewMode::tiles && tiles.empty())
    {
        g.setColour(juce::Colours::grey);
        g.setFont(juce::FontOptions(16.0f));
        g.drawFittedText("Select a parameter and add a Button, Encoder, Content or Meter control to this page.",
                         getLocalBounds().reduced(30), juce::Justification::centred, 2);
    }
}

void ControlWorkspace::resized()
{
    auto bounds = getLocalBounds().reduced(6);
    auto toolbar = bounds.removeFromTop(30);
    tilesButton.setBounds(toolbar.removeFromLeft(72).reduced(1, 1));
    listButton.setBounds(toolbar.removeFromLeft(72).reduced(1, 1));
    toolbar.removeFromLeft(8);
    selectAllButton.setBounds(toolbar.removeFromLeft(98).reduced(1, 1));
    toolbar.removeFromLeft(6);
    clearAllButton.setBounds(toolbar.removeFromLeft(92).reduced(1, 1));
    bounds.removeFromTop(4);

    viewport.setBounds(bounds);
    listView.setBounds(bounds);

    const int available = juce::jmax(320, viewport.getWidth() - 20);
    const int defaultTileWidth = 135;
    const int defaultTileHeight = 120;
    const int defaultContentWidth = 240;
    const int defaultContentHeight = 130;
    const int defaultMeterWidth = 68;
    const int defaultMeterHeight = 180;
    const int gap = 7;

    int x = gap;
    int y = gap;
    int rowHeight = 0;
    int widestRight = available;

    juce::ScopedValueSetter<bool> layoutGuard(layingOutTiles, true);
    for (auto& tile : tiles)
    {
        const auto& mapping = tile->mapping();
        const bool content = mapping.type == ControlType::showContent;
        const bool meter = mapping.type == ControlType::meter;
        const bool matrix = mapping.type == ControlType::matrix;
        const int width = matrix
            ? juce::jlimit(340, 2200, mapping.tileWidth > 0 ? mapping.tileWidth : 760)
            : content
            ? juce::jlimit(180, 720, mapping.tileWidth > 0 ? mapping.tileWidth : defaultContentWidth)
            : (meter ? defaultMeterWidth : defaultTileWidth);
        const int height = matrix
            ? juce::jlimit(220, 1400, mapping.tileHeight > 0 ? mapping.tileHeight : 440)
            : content
            ? juce::jlimit(110, 420, mapping.tileHeight > 0 ? mapping.tileHeight : defaultContentHeight)
            : (meter ? defaultMeterHeight : defaultTileHeight);

        if (x > gap && x + width + gap > available)
        {
            x = gap;
            y += rowHeight + gap;
            rowHeight = 0;
        }

        tile->setBounds(x, y, width, height);
        widestRight = juce::jmax(widestRight, x + width + gap);
        x += width + gap;
        rowHeight = juce::jmax(rowHeight, height);
    }

    canvas.setSize(widestRight, juce::jmax(defaultTileHeight + 2 * gap, y + rowHeight + gap));
}

void ControlWorkspace::applyViewMode(PageViewMode mode, bool notify)
{
    viewMode = mode;
    const bool tilesVisible = mode == PageViewMode::tiles;
    viewport.setVisible(tilesVisible);
    listView.setVisible(!tilesVisible);
    tilesButton.setToggleState(tilesVisible, juce::dontSendNotification);
    listButton.setToggleState(!tilesVisible, juce::dontSendNotification);
    selectAllButton.setVisible(tilesVisible);
    updateSelectionButton();
    repaint();
    resized();
    if (notify && onViewModeChanged)
        onViewModeChanged(mode);
}
}
