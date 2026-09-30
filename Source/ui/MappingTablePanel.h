#pragma once
#include <JuceHeader.h>
#include "../model/MappingDraft.h"

namespace er
{
class MappingTablePanel final : public juce::Component,
                                private juce::TableListBoxModel
{
public:
    explicit MappingTablePanel(MappingDraft& draftToUse);

    void refresh();
    void setPageCount(int pages);
    void setCurrentPage(int page);
    std::function<void()> onApply;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum ColumnIds { useColumn = 1, providerColumn, labelColumn, label2Column, pathColumn, kindColumn, typeColumn, pageColumn, accessColumn };

    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

    void setUseForRowOrSelection(int rowNumber, bool enabled);

    int rowForPath(const juce::String& path) const;
    MappingDraftRow* rowAt(int rowNumber);
    const MappingDraftRow* rowAt(int rowNumber) const;

    MappingDraft& draft;
    int pageCount = 3;
    int currentPage = 1;
    juce::Label title;
    juce::Label info;
    juce::TableListBox table { "MappingTable", this };
    juce::TextButton selectedButton { "Selected -> Button" };
    juce::TextButton selectedEncoder { "Selected -> Encoder" };
    juce::TextButton selectedContent { "Selected -> Content" };
    juce::TextButton applyButton { "Apply to Page 1" };
    juce::TextButton clearButton { "Clear Table" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingTablePanel)
};
}
