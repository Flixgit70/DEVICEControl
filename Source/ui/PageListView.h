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
    void setControlLabel2(const juce::String& path, const juce::String& label2);
    void refreshValues();

    std::function<void(const juce::String&, const juce::String&)> onLabel2Changed;
    std::function<void(const std::vector<ControlMapping>&)> onMappingsChanged;
    std::function<void(const std::vector<juce::String>&)> onRemoveSelected;
    std::function<void(const std::vector<juce::String>&, int)> onReorder;
    std::function<void()> onExportCsv;
    std::function<void()> onImportCsv;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum ColumnIds
    {
        moveColumn = 1,
        typeColumn,
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
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void returnKeyPressed(int lastRowSelected) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override;
    bool mayDragToExternalWindows() const override { return false; }

    class ReorderTable final : public juce::TableListBox, public juce::DragAndDropTarget
    {
    public:
        ReorderTable(const juce::String& name, juce::TableListBoxModel* model)
            : juce::TableListBox(name, model) {}
        std::function<void(int)> onDrop;
        bool isInterestedInDragSource(const SourceDetails& details) override;
        void itemDragMove(const SourceDetails& details) override;
        void itemDragExit(const SourceDetails&) override;
        void itemDropped(const SourceDetails& details) override;
        void paintOverChildren(juce::Graphics& g) override;
    private:
        int insertionIndex = -1;
    };
    void finishReorder(int insertionIndex);

    const ControlMapping* mappingAt(int row) const;
    std::optional<ParameterInfo> infoAt(int row) const;
    std::vector<int> selectedRows() const;
    std::vector<juce::String> selectedPaths() const;
    std::vector<int> rowsAffectedByCellEdit(int row) const;
    void emitMappingsChanged(const std::vector<int>& rows);
    void showRowContextMenu(int row, juce::Component* target);
    void applyColourToPaths(const std::vector<juce::String>& paths, juce::uint32 colourArgb);
    void applyTypeToSelection(ControlType type);
    void removeSelection();
    void applyBatchThresholds();
    void updateBatchControls();
    void beginLabel2Edit(int row);
    bool beginNextSelectedLabel2Edit(int row);
    void commitLabel2(int row, const juce::String& text);
    void commitValue(int row, const juce::String& text);
    void commitThreshold(int row, ThresholdSlot slot, const juce::String& text);

    PageModel pageSnapshot;
    std::function<std::optional<ParameterInfo>(const juce::String&)> parameterResolver;
    std::function<void(const juce::String&, const juce::var&)> parameterWriter;

    juce::Label selectionLabel;
    juce::TextButton asButtonButton { "Button" };
    juce::TextButton asEncoderButton { "Encoder" };
    juce::TextButton asContentButton { "Content" };
    juce::TextButton asMeterButton { "Meter" };
    juce::TextButton removeSelectedButton { "Remove" };
    juce::TextButton exportCsvButton { "Export CSV" };
    juce::TextButton importCsvButton { "Import CSV" };

    juce::Label orangeAboveLabel { {}, "Orange >" };
    juce::Label redAboveLabel { {}, "Red >" };
    juce::Label orangeBelowLabel { {}, "Orange <" };
    juce::Label redBelowLabel { {}, "Red <" };
    juce::TextEditor orangeAboveBatch;
    juce::TextEditor redAboveBatch;
    juce::TextEditor orangeBelowBatch;
    juce::TextEditor redBelowBatch;
    juce::TextButton applyThresholdsButton { "Apply Thresholds" };

    ReorderTable table { "PageList", this };
    std::vector<juce::String> dragPaths;
};
}
