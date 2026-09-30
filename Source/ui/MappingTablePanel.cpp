#include "MappingTablePanel.h"
#include <algorithm>
#include "../app/Theme.h"

namespace er
{
namespace
{
class UseEditor final : public juce::ToggleButton
{
public:
    std::function<void(bool)> changed;
    UseEditor() { onClick = [this] { if (changed) changed(getToggleState()); }; }
};

class TypeEditor final : public juce::ComboBox
{
public:
    std::function<void(ControlType)> changed;
    TypeEditor()
    {
        addItem("Button", 1);
        addItem("Encoder", 2);
        addItem("Show Content", 3);
        onChange = [this]
        {
            if (!changed) return;
            if (getSelectedId() == 2) changed(ControlType::encoder);
            else if (getSelectedId() == 3) changed(ControlType::showContent);
            else changed(ControlType::button);
        };
    }
};

class LabelEditor final : public juce::TextEditor
{
public:
    std::function<void(const juce::String&)> changed;
    LabelEditor()
    {
        setSelectAllWhenFocused(true);
        onTextChange = [this] { if (changed) changed(getText()); };
    }
};
}

MappingTablePanel::MappingTablePanel(MappingDraft& draftToUse) : draft(draftToUse)
{
    title.setText("MAPPING TABLE", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    addAndMakeVisible(title);

    info.setColour(juce::Label::textColourId, Theme::text);
    info.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(info);

    auto& h = table.getHeader();
    // Keep user-resized widths exactly as chosen. Unlimited maximum widths let
    // long labels/paths expand to the right; TableListBox automatically grows
    // its minimum content width and exposes horizontal scrolling as required.
    h.setStretchToFitActive(false);
    h.addColumn("Use", useColumn, 54, 44, -1, juce::TableHeaderComponent::notSortable);
    h.addColumn("Provider", providerColumn, 130, 90, -1);
    h.addColumn("Label 1", labelColumn, 170, 90, -1);
    h.addColumn("Label 2", label2Column, 170, 90, -1);
    h.addColumn("Path", pathColumn, 160, 100, -1);
    h.addColumn("Kind", kindColumn, 92, 70, -1);
    h.addColumn("Control", typeColumn, 110, 90, -1);
    h.addColumn("Page", pageColumn, 70, 60, -1);
    h.addColumn("Access", accessColumn, 84, 70, -1);
    table.setRowHeight(30);
    table.setMultipleSelectionEnabled(true);
    table.setColour(juce::ListBox::backgroundColourId, Theme::background);
    if (auto* viewport = table.getViewport())
        viewport->setScrollBarsShown(true, true);
    addAndMakeVisible(table);

    selectedButton.onClick = [this]
    {
        for (int i = 0; i < table.getNumSelectedRows(); ++i)
            if (auto* r = rowAt(table.getSelectedRow(i))) { r->type = ControlType::button; r->enabled = true; }
        refresh();
    };
    selectedEncoder.onClick = [this]
    {
        for (int i = 0; i < table.getNumSelectedRows(); ++i)
            if (auto* r = rowAt(table.getSelectedRow(i))) { r->type = ControlType::encoder; r->enabled = true; }
        refresh();
    };
    selectedContent.onClick = [this]
    {
        for (int i = 0; i < table.getNumSelectedRows(); ++i)
            if (auto* r = rowAt(table.getSelectedRow(i))) { r->type = ControlType::showContent; r->enabled = true; }
        refresh();
    };
    addAndMakeVisible(selectedButton);
    addAndMakeVisible(selectedEncoder);
    addAndMakeVisible(selectedContent);

    applyButton.onClick = [this] { if (onApply) onApply(); };
    clearButton.onClick = [this]
    {
        draft.clear();
        refresh();
    };
    addAndMakeVisible(applyButton);
    addAndMakeVisible(clearButton);
    refresh();
}

void MappingTablePanel::setPageCount(int pages)
{
    pageCount = juce::jmax(1, pages);
    currentPage = juce::jlimit(1, pageCount, currentPage);
    applyButton.setButtonText("Apply to Page " + juce::String(currentPage));
    table.updateContent();
    table.repaint();
}

void MappingTablePanel::setCurrentPage(int page)
{
    currentPage = juce::jmax(1, page);
    applyButton.setButtonText("Apply to Page " + juce::String(currentPage));
    refresh();
}

void MappingTablePanel::refresh()
{
    table.updateContent();
    table.repaint();
    info.setText(juce::String((int) draft.rows().size())
                     + " parameters  |  Use rows will be assigned to Page " + juce::String(currentPage),
                 juce::dontSendNotification);
}

void MappingTablePanel::paint(juce::Graphics& g)
{
    g.setColour(Theme::panelRaised);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
}

void MappingTablePanel::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto top = r.removeFromTop(28);
    title.setBounds(top.removeFromLeft(150));
    auto buttons = top;
    clearButton.setBounds(buttons.removeFromRight(92).reduced(0, 1));
    buttons.removeFromRight(6);
    applyButton.setBounds(buttons.removeFromRight(136).reduced(0, 1));
    buttons.removeFromRight(12);
    selectedContent.setBounds(buttons.removeFromRight(152).reduced(0, 1));
    buttons.removeFromRight(6);
    selectedEncoder.setBounds(buttons.removeFromRight(142).reduced(0, 1));
    buttons.removeFromRight(6);
    selectedButton.setBounds(buttons.removeFromRight(136).reduced(0, 1));
    r.removeFromTop(4);
    info.setBounds(r.removeFromBottom(24));
    r.removeFromBottom(4);

    table.setBounds(r);
}

int MappingTablePanel::getNumRows() { return (int) draft.rows().size() + 1; }

MappingDraftRow* MappingTablePanel::rowAt(int rowNumber)
{
    if (rowNumber <= 0 || rowNumber > (int) draft.rows().size()) return nullptr;
    return &draft.rows()[(size_t) (rowNumber - 1)];
}

const MappingDraftRow* MappingTablePanel::rowAt(int rowNumber) const
{
    if (rowNumber <= 0 || rowNumber > (int) draft.rows().size()) return nullptr;
    return &draft.rows()[(size_t) (rowNumber - 1)];
}

int MappingTablePanel::rowForPath(const juce::String& path) const
{
    for (int i = 0; i < (int) draft.rows().size(); ++i)
        if (draft.rows()[(size_t) i].path == path) return i + 1;
    return -1;
}

void MappingTablePanel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool selected)
{
    if (rowNumber == 0)
    {
        g.setColour(Theme::editor);
        g.fillRect(0, 0, width, height);
        return;
    }
    if (selected)
    {
        g.setColour(Theme::accentDark);
        g.fillRect(0, 0, width, height);
    }
}

void MappingTablePanel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool)
{
    if (rowNumber == 0)
    {
        if (columnId == labelColumn)
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(11.5f).withStyle("Bold"));
            g.drawText("Select all", 6, 0, width - 10, height, juce::Justification::centredLeft, true);
        }
        return;
    }

    const auto* row = rowAt(rowNumber);
    if (!row) return;
    juce::String text;
    switch (columnId)
    {
        case providerColumn: text = row->provider; break;
        case pathColumn: text = row->path; break;
        case kindColumn: text = ParameterInfo::kindToString(row->kind); break;
        case pageColumn: text = juce::String(row->page); break;
        case accessColumn: text = row->writable ? "Write" : "Read only"; break;
        default: return;
    }
    g.setColour(row->writable ? juce::Colours::white : juce::Colours::grey);
    g.setFont(juce::FontOptions(11.5f));
    g.drawText(text, 6, 0, width - 10, height, juce::Justification::centredLeft, true);
}

juce::Component* MappingTablePanel::refreshComponentForCell(int rowNumber, int columnId, bool,
                                                              juce::Component* existing)
{
    if (rowNumber == 0)
    {
        if (columnId == useColumn)
        {
            auto* c = dynamic_cast<UseEditor*>(existing);
            if (!c) { delete existing; c = new UseEditor(); }
            c->setToggleState(draft.allEnabled(), juce::dontSendNotification);
            c->changed = [this] (bool value) { setUseForRowOrSelection(0, value); };
            return c;
        }
        delete existing;
        return nullptr;
    }

    auto* row = rowAt(rowNumber);
    if (!row) return nullptr;
    const auto path = row->path;

    if (columnId == useColumn)
    {
        auto* c = dynamic_cast<UseEditor*>(existing);
        if (!c) { delete existing; c = new UseEditor(); }
        c->setToggleState(row->enabled, juce::dontSendNotification);
        c->changed = [this, path] (bool value)
        {
            const auto i = rowForPath(path);
            if (i >= 0) setUseForRowOrSelection(i, value);
        };
        return c;
    }
    if (columnId == labelColumn || columnId == label2Column)
    {
        auto* c = dynamic_cast<LabelEditor*>(existing);
        if (!c) { delete existing; c = new LabelEditor(); }
        const auto currentText = columnId == labelColumn ? row->label : row->label2;
        if (c->getText() != currentText) c->setText(currentText, false);
        c->changed = [this, path, columnId] (const juce::String& value)
        {
            const auto i = rowForPath(path);
            if (auto* r = rowAt(i))
            {
                if (columnId == labelColumn) r->label = value;
                else r->label2 = value;
            }
        };
        return c;
    }
    if (columnId == typeColumn)
    {
        auto* c = dynamic_cast<TypeEditor*>(existing);
        if (!c) { delete existing; c = new TypeEditor(); }
        c->setSelectedId(row->type == ControlType::encoder ? 2
                       : row->type == ControlType::showContent ? 3 : 1, juce::dontSendNotification);
        c->changed = [this, path] (ControlType type)
        {
            const auto i = rowForPath(path); if (auto* r = rowAt(i)) r->type = type;
        };
        return c;
    }
    delete existing;
    return nullptr;
}

void MappingTablePanel::setUseForRowOrSelection(int rowNumber, bool enabled)
{
    if (rowNumber == 0)
    {
        draft.setAllEnabled(enabled);
        if (enabled && getNumRows() > 1)
            table.selectRangeOfRows(1, getNumRows() - 1);
        else if (!enabled)
            table.deselectAllRows();
        refresh();
        return;
    }

    std::vector<juce::String> paths;
    if (table.isRowSelected(rowNumber) && table.getNumSelectedRows() > 1)
    {
        paths.reserve((size_t) table.getNumSelectedRows());
        for (int i = 0; i < table.getNumSelectedRows(); ++i)
            if (const auto* row = rowAt(table.getSelectedRow(i)))
                paths.push_back(row->path);
    }
    else if (const auto* row = rowAt(rowNumber))
    {
        paths.push_back(row->path);
    }

    draft.setEnabledForPaths(paths, enabled);
    table.updateContent();
    table.repaint();
}

void MappingTablePanel::sortOrderChanged(int columnId, bool forwards)
{
    auto& rows = draft.rows();
    std::stable_sort(rows.begin(), rows.end(), [=] (const MappingDraftRow& a, const MappingDraftRow& b)
    {
        int cmp = 0;
        switch (columnId)
        {
            case providerColumn: cmp = a.provider.compareNatural(b.provider); break;
            case labelColumn: cmp = a.label.compareNatural(b.label); break;
            case label2Column: cmp = a.label2.compareNatural(b.label2); break;
            case pathColumn: cmp = a.path.compareNatural(b.path); break;
            case kindColumn: cmp = ParameterInfo::kindToString(a.kind).compareNatural(ParameterInfo::kindToString(b.kind)); break;
            case typeColumn: cmp = (int) a.type - (int) b.type; break;
            case pageColumn: cmp = a.page - b.page; break;
            case accessColumn: cmp = (int) a.writable - (int) b.writable; break;
            default: return false;
        }
        return forwards ? cmp < 0 : cmp > 0;
    });
    refresh();
}
}
