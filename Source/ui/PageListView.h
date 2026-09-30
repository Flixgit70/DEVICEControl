#pragma once
#include <JuceHeader.h>
#include "../model/PageModel.h"
#include "../model/ParameterInfo.h"
#include <optional>
#include <vector>

namespace er
{
class PageListView final : public juce::Component,
                           private juce::TableListBoxModel
{
public:
    PageListView();

    void setPage(const PageModel& page,
                 std::function<std::optional<ParameterInfo>(const juce::String&)> resolver,
                 std::function<void(const juce::String&, const juce::var&)> writer);
    void refreshValues();

    std::function<void(const juce::String&, const juce::String&)> onLabel2Changed;
    std::function<void(const std::vector<ControlMapping>&)> onMappingsChanged;
    std::function<void(const std::vector<juce::String>&)> onRemoveSelected;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum ColumnIds
    {
        typeColumn = 1,
        label1Column,
        label2Column,
        valueColumn,
        orangeAboveColumn,
        redAboveColumn,
        orangeBelowColumn,
        redBelowColumn,
        statusColumn
    };

    enum class ThresholdSlot { orangeAbove, redAbove, orangeBelow, redBelow };

    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;
    void selectedRowsChanged(int lastRowSelected) override;

    const ControlMapping* mappingAt(int row) const;
    std::optional<ParameterInfo> infoAt(int row) const;
    std::vector<int> selectedRows() const;
    std::vector<juce::String> selectedPaths() const;
    std::vector<int> rowsAffectedByCellEdit(int row) const;
    void emitMappingsChanged(const std::vector<int>& rows);
    void applyTypeToSelection(ControlType type);
    void removeSelection();
    void applyBatchThresholds();
    void updateBatchControls();
    void commitValue(int row, const juce::String& text);
    void commitThreshold(int row, ThresholdSlot slot, const juce::String& text);

    PageModel pageSnapshot;
    std::function<std::optional<ParameterInfo>(const juce::String&)> parameterResolver;
    std::function<void(const juce::String&, const juce::var&)> parameterWriter;

    juce::Label selectionLabel;
    juce::TextButton asButtonButton { "Button" };
    juce::TextButton asEncoderButton { "Encoder" };
    juce::TextButton asContentButton { "Content" };
    juce::TextButton removeSelectedButton { "Remove" };

    juce::Label orangeAboveLabel { {}, "Orange >" };
    juce::Label redAboveLabel { {}, "Red >" };
    juce::Label orangeBelowLabel { {}, "Orange <" };
    juce::Label redBelowLabel { {}, "Red <" };
    juce::TextEditor orangeAboveBatch;
    juce::TextEditor redAboveBatch;
    juce::TextEditor orangeBelowBatch;
    juce::TextEditor redBelowBatch;
    juce::TextButton applyThresholdsButton { "Apply Thresholds" };

    juce::TableListBox table { "PageList", this };
};
}
