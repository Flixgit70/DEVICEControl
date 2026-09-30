#pragma once
#include <JuceHeader.h>
#include "../model/PageModel.h"
#include "PageListView.h"
#include "controls/RemoteControlTile.h"
#include <optional>

namespace er
{
class ControlWorkspace final : public juce::Component
{
public:
    ControlWorkspace();
    void setPage(const PageModel&,
                 std::function<std::optional<ParameterInfo>(const juce::String&)> resolver,
                 std::function<void(const juce::String&, const juce::var&)> writer);
    void refreshValues();
    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void(const juce::String&)> onRemoveControl;
    std::function<void(const std::vector<juce::String>&)> onRemoveControls;
    std::function<void(const std::vector<ControlMapping>&)> onControlMappingsChanged;
    std::function<void(const juce::String&, juce::uint32)> onControlColourChanged;
    std::function<void(const juce::String&, const juce::String&)> onControlLabel2Changed;
    std::function<void(const juce::String&, bool, double, bool, double,
                       bool, double, bool, double)> onControlThresholdsChanged;
    std::function<void(PageViewMode)> onViewModeChanged;
    std::function<void()> onClearPage;

private:
    class Canvas final : public juce::Component {};
    void applyViewMode(PageViewMode mode, bool notify);

    juce::TextButton tilesButton { "Tiles" };
    juce::TextButton listButton { "List" };
    juce::TextButton clearAllButton { "Clear All" };
    juce::Viewport viewport;
    Canvas canvas;
    PageListView listView;
    PageViewMode viewMode = PageViewMode::tiles;
    std::function<std::optional<ParameterInfo>(const juce::String&)> parameterResolver;
    std::function<void(const juce::String&, const juce::var&)> parameterWriter;
    std::vector<std::unique_ptr<RemoteControlTile>> tiles;
};
}
