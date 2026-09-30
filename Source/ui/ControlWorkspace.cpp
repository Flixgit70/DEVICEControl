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
    clearAllButton.onClick = [this] { if (onClearPage) onClearPage(); };
    addAndMakeVisible(tilesButton);
    addAndMakeVisible(listButton);
    addAndMakeVisible(clearAllButton);

    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    addAndMakeVisible(listView);

    listView.onLabel2Changed = [this] (const juce::String& path, const juce::String& label2)
    {
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

    applyViewMode(PageViewMode::tiles, false);
}

void ControlWorkspace::setPage(const PageModel& page,
                               std::function<std::optional<ParameterInfo>(const juce::String&)> resolver,
                               std::function<void(const juce::String&, const juce::var&)> writer)
{
    parameterResolver = std::move(resolver);
    parameterWriter = std::move(writer);
    tiles.clear();
    canvas.removeAllChildren();
    for (const auto& mapping : page.controls)
    {
        std::unique_ptr<RemoteControlTile> tile;
        if (mapping.type == ControlType::encoder) tile = std::make_unique<EncoderTile>(mapping);
        else if (mapping.type == ControlType::showContent) tile = std::make_unique<ShowContentTile>(mapping);
        else tile = std::make_unique<ButtonTile>(mapping);
        tile->onWrite = parameterWriter;
        tile->onRemove = [this] (const juce::String& path)
        {
            if (onRemoveControl)
                onRemoveControl(path);
        };
        tile->onColourChange = [this] (const juce::String& path, juce::uint32 colourArgb)
        {
            if (onControlColourChanged)
                onControlColourChanged(path, colourArgb);
        };
        tile->onLabel2Change = [this] (const juce::String& path, const juce::String& label2)
        {
            if (onControlLabel2Changed)
                onControlLabel2Changed(path, label2);
        };
        canvas.addAndMakeVisible(*tile);
        tiles.push_back(std::move(tile));
    }

    listView.setPage(page, parameterResolver, parameterWriter);
    applyViewMode(page.viewMode, false);
    resized();
    refreshValues();
}

void ControlWorkspace::refreshValues()
{
    if (parameterResolver)
        for (auto& tile : tiles)
            if (auto p = parameterResolver(tile->mapping().parameterPath)) tile->refresh(*p);
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
        g.drawFittedText("Select a parameter and add a Button, Encoder or Show Content control to this page.",
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
    clearAllButton.setBounds(toolbar.removeFromLeft(92).reduced(1, 1));
    bounds.removeFromTop(4);

    viewport.setBounds(bounds);
    listView.setBounds(bounds);

    const int available = juce::jmax(320, viewport.getWidth() - 20);
    const int tileWidth = 135;
    const int tileHeight = 120;
    const int gap = 7;
    const int cols = juce::jmax(1, (available - gap) / (tileWidth + gap));
    const int rows = juce::jmax(1, ((int) tiles.size() + cols - 1) / cols);
    canvas.setSize(available, gap + rows * (tileHeight + gap));
    for (int i = 0; i < (int) tiles.size(); ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        tiles[(size_t) i]->setBounds(gap + col * (tileWidth + gap), gap + row * (tileHeight + gap), tileWidth, tileHeight);
    }
}

void ControlWorkspace::applyViewMode(PageViewMode mode, bool notify)
{
    viewMode = mode;
    const bool tilesVisible = mode == PageViewMode::tiles;
    viewport.setVisible(tilesVisible);
    listView.setVisible(!tilesVisible);
    tilesButton.setToggleState(tilesVisible, juce::dontSendNotification);
    listButton.setToggleState(!tilesVisible, juce::dontSendNotification);
    repaint();
    resized();
    if (notify && onViewModeChanged)
        onViewModeChanged(mode);
}
}
