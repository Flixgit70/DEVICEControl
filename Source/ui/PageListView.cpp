#include "PageListView.h"
#include "../model/ControlAlertPolicy.h"
#include "StringContentViewer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "../app/Theme.h"

namespace er
{
namespace
{
juce::Colour alertColour(AlertState state)
{
    switch (state)
    {
        case AlertState::orange: return Theme::orange;
        case AlertState::red:    return Theme::red;
        case AlertState::normal: return Theme::green;
    }
    return Theme::green;
}

class CommitEditor final : public juce::TextEditor
{
public:
    std::function<void(const juce::String&)> commit;

    CommitEditor()
    {
        setSelectAllWhenFocused(true);
        setColour(juce::TextEditor::textColourId, juce::Colours::white);
        setColour(juce::TextEditor::backgroundColourId, Theme::editor);
        setColour(juce::TextEditor::outlineColourId, Theme::outline);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBlue);
        onReturnKey = [this] { sendCommit(); giveAwayKeyboardFocus(); };
        onFocusLost = [this] { sendCommit(); };
    }

    void setDisplayedText(const juce::String& newText)
    {
        if (hasKeyboardFocus(true))
            return;
        if (getText() != newText)
            setText(newText, false);
        lastCommitted = getText();
    }

    void sendCommit()
    {
        const auto now = getText();
        if (now == lastCommitted)
            return;
        lastCommitted = now;
        if (commit)
            commit(now);
    }

private:
    juce::String lastCommitted;
};

class ContentButton final : public juce::TextButton
{
public:
    std::function<void()> show;

    ContentButton() : juce::TextButton("Show Content")
    {
        onClick = [this] { if (show) show(); };
    }
};

std::optional<double> parseDoubleStrict(juce::String text)
{
    text = text.trim();
    if (text.isEmpty())
        return std::nullopt;
    if (text.containsChar(',') && !text.containsChar('.'))
        text = text.replaceCharacter(',', '.');

    const auto utf8 = text.toStdString();
    char* end = nullptr;
    const double value = std::strtod(utf8.c_str(), &end);
    if (end == utf8.c_str() || *end != '\0' || !std::isfinite(value))
        return std::nullopt;
    return value;
}

std::optional<juce::var> parseValueForParameter(const ParameterInfo& info, juce::String text)
{
    text = text.trim();
    if (text.isEmpty())
        return std::nullopt;

    switch (info.kind)
    {
        case ParameterKind::booleanValue:
            if (text.equalsIgnoreCase("true") || text.equalsIgnoreCase("on") || text.equalsIgnoreCase("yes") || text == "1")
                return juce::var(true);
            if (text.equalsIgnoreCase("false") || text.equalsIgnoreCase("off") || text.equalsIgnoreCase("no") || text == "0")
                return juce::var(false);
            return std::nullopt;

        case ParameterKind::trigger:
            return juce::var(true);

        case ParameterKind::enumeration:
        {
            for (const auto& entry : info.enumEntries)
            {
                const auto number = entry.upToFirstOccurrenceOf(":", false, false).getIntValue();
                const auto label = entry.fromFirstOccurrenceOf(":", false, false).trim();
                if (text.equalsIgnoreCase(label) || text == juce::String(number))
                    return juce::var(number);
            }
            return std::nullopt;
        }

        case ParameterKind::integer:
        case ParameterKind::real:
        {
            const auto numeric = parseDoubleStrict(text);
            if (!numeric) return std::nullopt;
            const double factor = info.factor == 0.0 ? 1.0 : info.factor;
            const auto raw = *numeric * factor;
            if (info.kind == ParameterKind::integer)
                return juce::var((juce::int64) std::llround(raw));
            return juce::var(raw);
        }

        case ParameterKind::stringValue:
            return juce::var(text);

        default:
            return std::nullopt;
    }
}

juce::String thresholdText(bool enabled, double value)
{
    if (!enabled)
        return {};
    auto text = juce::String(value, 3);
    if (text.containsChar('.'))
        text = text.trimCharactersAtEnd("0").trimCharactersAtEnd(".");
    return text;
}

void configureBatchEditor(juce::TextEditor& editor)
{
    editor.setSelectAllWhenFocused(true);
    editor.setJustification(juce::Justification::centred);
    editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    editor.setColour(juce::TextEditor::backgroundColourId, Theme::editor);
    editor.setColour(juce::TextEditor::outlineColourId, Theme::outline);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBlue);
    editor.setTextToShowWhenEmpty("off", Theme::textDim);
}
}

PageListView::PageListView()
{
    auto& h = table.getHeader();
    h.setStretchToFitActive(false);
    h.addColumn("Type", typeColumn, 82, 60, -1);
    h.addColumn("Label 1", label1Column, 260, 120, -1);
    h.addColumn("Label 2", label2Column, 180, 100, -1);
    h.addColumn("Value", valueColumn, 115, 85, -1);
    h.addColumn("Orange >", orangeAboveColumn, 100, 78, -1);
    h.addColumn("Red >", redAboveColumn, 100, 78, -1);
    h.addColumn("Orange <", orangeBelowColumn, 100, 78, -1);
    h.addColumn("Red <", redBelowColumn, 100, 78, -1);
    h.addColumn("Status", statusColumn, 92, 70, -1);
    table.setRowHeight(30);
    table.setMultipleSelectionEnabled(true);
    table.setColour(juce::ListBox::backgroundColourId, Theme::background);
    if (auto* viewport = table.getViewport())
        viewport->setScrollBarsShown(true, true);
    addAndMakeVisible(table);

    selectionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    selectionLabel.setColour(juce::Label::textColourId, Theme::text);
    selectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(selectionLabel);

    asButtonButton.onClick = [this] { applyTypeToSelection(ControlType::button); };
    asEncoderButton.onClick = [this] { applyTypeToSelection(ControlType::encoder); };
    asContentButton.onClick = [this] { applyTypeToSelection(ControlType::showContent); };
    removeSelectedButton.onClick = [this] { removeSelection(); };
    addAndMakeVisible(asButtonButton);
    addAndMakeVisible(asEncoderButton);
    addAndMakeVisible(asContentButton);
    addAndMakeVisible(removeSelectedButton);

    auto configureLabel = [this] (juce::Label& label)
    {
        label.setFont(juce::FontOptions(10.5f));
        label.setColour(juce::Label::textColourId, Theme::textDim);
        label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(label);
    };
    configureLabel(orangeAboveLabel);
    configureLabel(redAboveLabel);
    configureLabel(orangeBelowLabel);
    configureLabel(redBelowLabel);

    configureBatchEditor(orangeAboveBatch);
    configureBatchEditor(redAboveBatch);
    configureBatchEditor(orangeBelowBatch);
    configureBatchEditor(redBelowBatch);
    addAndMakeVisible(orangeAboveBatch);
    addAndMakeVisible(redAboveBatch);
    addAndMakeVisible(orangeBelowBatch);
    addAndMakeVisible(redBelowBatch);

    applyThresholdsButton.onClick = [this] { applyBatchThresholds(); };
    addAndMakeVisible(applyThresholdsButton);

    updateBatchControls();
}

void PageListView::setPage(const PageModel& page,
                           std::function<std::optional<ParameterInfo>(const juce::String&)> resolver,
                           std::function<void(const juce::String&, const juce::var&)> writer)
{
    const auto previouslySelected = selectedPaths();
    pageSnapshot = page;
    parameterResolver = std::move(resolver);
    parameterWriter = std::move(writer);
    table.updateContent();

    table.deselectAllRows();
    for (int row = 0; row < (int) pageSnapshot.controls.size(); ++row)
    {
        const auto& path = pageSnapshot.controls[(size_t) row].parameterPath;
        if (std::find(previouslySelected.begin(), previouslySelected.end(), path) != previouslySelected.end())
            table.selectRow(row, true, false);
    }

    updateBatchControls();
    table.repaint();
}

void PageListView::refreshValues()
{
    table.updateContent();
    table.repaint();
}

void PageListView::paint(juce::Graphics& g)
{
    g.setColour(Theme::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    const auto dividerY = 70.0f;
    g.setColour(Theme::outline.withAlpha(0.65f));
    g.drawHorizontalLine((int) dividerY, 6.0f, (float) getWidth() - 6.0f);
}

void PageListView::resized()
{
    auto bounds = getLocalBounds().reduced(6);

    auto firstRow = bounds.removeFromTop(28);
    selectionLabel.setBounds(firstRow.removeFromLeft(120));
    firstRow.removeFromLeft(5);
    asButtonButton.setBounds(firstRow.removeFromLeft(74).reduced(1));
    asEncoderButton.setBounds(firstRow.removeFromLeft(78).reduced(1));
    asContentButton.setBounds(firstRow.removeFromLeft(78).reduced(1));
    firstRow.removeFromLeft(8);
    removeSelectedButton.setBounds(firstRow.removeFromLeft(82).reduced(1));

    bounds.removeFromTop(3);
    auto secondRow = bounds.removeFromTop(28);
    auto placeThreshold = [&] (juce::Label& label, juce::TextEditor& editor, int labelWidth)
    {
        label.setBounds(secondRow.removeFromLeft(labelWidth));
        editor.setBounds(secondRow.removeFromLeft(64).reduced(1));
        secondRow.removeFromLeft(4);
    };
    placeThreshold(orangeAboveLabel, orangeAboveBatch, 58);
    placeThreshold(redAboveLabel, redAboveBatch, 42);
    placeThreshold(orangeBelowLabel, orangeBelowBatch, 58);
    placeThreshold(redBelowLabel, redBelowBatch, 42);
    secondRow.removeFromLeft(5);
    applyThresholdsButton.setBounds(secondRow.removeFromLeft(126).reduced(1));

    bounds.removeFromTop(9);
    table.setBounds(bounds);
}

int PageListView::getNumRows()
{
    return (int) pageSnapshot.controls.size();
}

const ControlMapping* PageListView::mappingAt(int row) const
{
    if (row < 0 || row >= (int) pageSnapshot.controls.size())
        return nullptr;
    return &pageSnapshot.controls[(size_t) row];
}

std::optional<ParameterInfo> PageListView::infoAt(int row) const
{
    const auto* mapping = mappingAt(row);
    if (!mapping || !parameterResolver)
        return std::nullopt;
    return parameterResolver(mapping->parameterPath);
}

std::vector<int> PageListView::selectedRows() const
{
    std::vector<int> rows;
    rows.reserve((size_t) table.getNumSelectedRows());
    for (int i = 0; i < table.getNumSelectedRows(); ++i)
    {
        const int row = table.getSelectedRow(i);
        if (row >= 0 && row < (int) pageSnapshot.controls.size())
            rows.push_back(row);
    }
    return rows;
}

std::vector<juce::String> PageListView::selectedPaths() const
{
    std::vector<juce::String> paths;
    for (const auto row : selectedRows())
        paths.push_back(pageSnapshot.controls[(size_t) row].parameterPath);
    return paths;
}

std::vector<int> PageListView::rowsAffectedByCellEdit(int row) const
{
    const auto rows = selectedRows();
    const bool clickedRowIsSelected = std::find(rows.begin(), rows.end(), row) != rows.end();
    if (clickedRowIsSelected && rows.size() > 1)
        return rows;
    return { row };
}

void PageListView::emitMappingsChanged(const std::vector<int>& rows)
{
    if (!onMappingsChanged)
        return;

    std::vector<ControlMapping> changed;
    changed.reserve(rows.size());
    for (const auto row : rows)
        if (row >= 0 && row < (int) pageSnapshot.controls.size())
            changed.push_back(pageSnapshot.controls[(size_t) row]);

    if (!changed.empty())
        onMappingsChanged(changed);
}

void PageListView::selectedRowsChanged(int)
{
    updateBatchControls();
}

void PageListView::updateBatchControls()
{
    const auto rows = selectedRows();
    const int count = (int) rows.size();
    selectionLabel.setText(count == 1 ? "1 selected" : juce::String(count) + " selected", juce::dontSendNotification);

    const bool any = count > 0;
    asButtonButton.setEnabled(any);
    asEncoderButton.setEnabled(any);
    asContentButton.setEnabled(any);
    removeSelectedButton.setEnabled(any);

    bool anyNumeric = false;
    for (const auto row : rows)
    {
        if (const auto info = infoAt(row); info && ControlAlertPolicy::supportsThresholds(*info))
        {
            anyNumeric = true;
            break;
        }
    }
    applyThresholdsButton.setEnabled(anyNumeric);
    orangeAboveBatch.setEnabled(anyNumeric);
    redAboveBatch.setEnabled(anyNumeric);
    orangeBelowBatch.setEnabled(anyNumeric);
    redBelowBatch.setEnabled(anyNumeric);

    if (count == 1)
    {
        const auto& mapping = pageSnapshot.controls[(size_t) rows.front()];
        orangeAboveBatch.setText(thresholdText(mapping.orangeAboveThresholdEnabled, mapping.orangeAboveThreshold), false);
        redAboveBatch.setText(thresholdText(mapping.redAboveThresholdEnabled, mapping.redAboveThreshold), false);
        orangeBelowBatch.setText(thresholdText(mapping.orangeBelowThresholdEnabled, mapping.orangeBelowThreshold), false);
        redBelowBatch.setText(thresholdText(mapping.redBelowThresholdEnabled, mapping.redBelowThreshold), false);
    }
    else if (count != 1)
    {
        orangeAboveBatch.clear();
        redAboveBatch.clear();
        orangeBelowBatch.clear();
        redBelowBatch.clear();
    }
}

void PageListView::applyTypeToSelection(ControlType type)
{
    const auto rows = selectedRows();
    if (rows.empty())
        return;

    for (const auto row : rows)
        pageSnapshot.controls[(size_t) row].type = type;

    emitMappingsChanged(rows);
    table.updateContent();
    table.repaint();
}

void PageListView::removeSelection()
{
    const auto paths = selectedPaths();
    if (paths.empty() || !onRemoveSelected)
        return;
    onRemoveSelected(paths);
}

void PageListView::applyBatchThresholds()
{
    auto parseField = [] (juce::TextEditor& editor) -> std::optional<std::pair<bool, double>>
    {
        const auto text = editor.getText().trim();
        if (text.isEmpty())
            return std::pair<bool, double> { false, 0.0 };
        const auto value = parseDoubleStrict(text);
        if (!value)
            return std::nullopt;
        return std::pair<bool, double> { true, *value };
    };

    const auto oa = parseField(orangeAboveBatch);
    const auto ra = parseField(redAboveBatch);
    const auto ob = parseField(orangeBelowBatch);
    const auto rb = parseField(redBelowBatch);
    if (!oa || !ra || !ob || !rb)
        return;

    std::vector<int> changedRows;
    for (const auto row : selectedRows())
    {
        const auto info = infoAt(row);
        if (!info || !ControlAlertPolicy::supportsThresholds(*info))
            continue;

        auto& mapping = pageSnapshot.controls[(size_t) row];
        mapping.orangeAboveThresholdEnabled = oa->first;
        mapping.orangeAboveThreshold = oa->second;
        mapping.redAboveThresholdEnabled = ra->first;
        mapping.redAboveThreshold = ra->second;
        mapping.orangeBelowThresholdEnabled = ob->first;
        mapping.orangeBelowThreshold = ob->second;
        mapping.redBelowThresholdEnabled = rb->first;
        mapping.redBelowThreshold = rb->second;
        changedRows.push_back(row);
    }

    emitMappingsChanged(changedRows);
    table.repaint();
}

void PageListView::paintRowBackground(juce::Graphics& g, int, int width, int height, bool selected)
{
    if (selected)
    {
        g.setColour(Theme::accentDark);
        g.fillRect(0, 0, width, height);
    }
}

void PageListView::paintCell(juce::Graphics& g, int row, int columnId, int width, int height, bool)
{
    const auto* mapping = mappingAt(row);
    if (!mapping)
        return;

    const auto info = infoAt(row);
    juce::String text;
    switch (columnId)
    {
        case typeColumn:
            text = mapping->type == ControlType::encoder ? "Encoder"
                 : mapping->type == ControlType::showContent ? "Show Content" : "Button";
            break;
        case label1Column: text = mapping->label; break;
        case statusColumn:
        {
            if (!info)
                text = "Offline";
            else if (ControlAlertPolicy::hasThresholds(*mapping) && ControlAlertPolicy::supportsThresholds(*info))
            {
                const auto state = ControlAlertPolicy::evaluate(*mapping, *info);
                text = state == AlertState::red ? "RED" : state == AlertState::orange ? "ORANGE" : "GREEN";
                g.setColour(alertColour(state).withAlpha(0.25f));
                g.fillRect(0, 0, width, height);
            }
            else if (mapping->type == ControlType::showContent)
            {
                text = info->online ? "ONLINE" : "Offline";
                g.setColour((info->online ? Theme::accentDark : Theme::outline).withAlpha(0.28f));
                g.fillRect(0, 0, width, height);
            }
            else
            {
                text = "GREEN";
                g.setColour(alertColour(AlertState::normal).withAlpha(0.18f));
                g.fillRect(0, 0, width, height);
            }
            break;
        }
        default: return;
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(11.5f));
    g.drawText(text, 6, 0, width - 10, height, juce::Justification::centredLeft, true);
}

juce::Component* PageListView::refreshComponentForCell(int row, int columnId, bool,
                                                        juce::Component* existing)
{
    const auto* mapping = mappingAt(row);
    if (!mapping)
    {
        delete existing;
        return nullptr;
    }
    const auto info = infoAt(row);

    if (columnId != label2Column && columnId != valueColumn
        && columnId != orangeAboveColumn && columnId != redAboveColumn
        && columnId != orangeBelowColumn && columnId != redBelowColumn)
    {
        delete existing;
        return nullptr;
    }

    const auto path = mapping->parameterPath;

    if (columnId == valueColumn && mapping->type == ControlType::showContent)
    {
        auto* contentButton = dynamic_cast<ContentButton*>(existing);
        if (!contentButton)
        {
            delete existing;
            contentButton = new ContentButton();
        }
        const bool available = info && info->online && info->kind == ParameterKind::stringValue;
        contentButton->setEnabled(available);
        contentButton->setButtonText("Show Content");
        contentButton->show = [this, path, contentButton]
        {
            if (!parameterResolver) return;
            const auto current = parameterResolver(path);
            if (!current || !current->online || current->kind != ParameterKind::stringValue) return;
            const auto title = current->description.isNotEmpty() ? current->description : current->fullTreeLabel();
            const bool writable = current->writable;
            const auto writer = parameterWriter;
            showStringContent(*contentButton, title, current->value.toString(), writable,
                              [writer, path] (const juce::String& text)
                              {
                                  if (writer) writer(path, juce::var(text));
                              });
        };
        return contentButton;
    }

    auto* editor = dynamic_cast<CommitEditor*>(existing);
    if (!editor)
    {
        delete existing;
        editor = new CommitEditor();
    }

    if (columnId == label2Column)
    {
        editor->setEnabled(true);
        editor->setReadOnly(false);
        editor->setDisplayedText(mapping->label2);
        editor->setColour(juce::TextEditor::backgroundColourId, Theme::editor);
        editor->commit = [this, path] (const juce::String& text)
        {
            for (auto& m : pageSnapshot.controls)
                if (m.parameterPath == path) m.label2 = text;
            if (onLabel2Changed) onLabel2Changed(path, text);
        };
        return editor;
    }

    if (columnId == valueColumn)
    {
        const bool writable = info && info->writable && info->online;
        editor->setEnabled(true);
        editor->setReadOnly(!writable);
        editor->setDisplayedText(info ? info->formattedValue() : juce::String("-"));
        auto state = AlertState::normal;
        if (info && ControlAlertPolicy::hasThresholds(*mapping))
            state = ControlAlertPolicy::evaluate(*mapping, *info);
        editor->setColour(juce::TextEditor::backgroundColourId,
                          info ? alertColour(state).darker(0.25f) : Theme::outline);
        editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
        editor->commit = [this, row] (const juce::String& text) { commitValue(row, text); };
        return editor;
    }

    const bool numeric = info && ControlAlertPolicy::supportsThresholds(*info);
    editor->setEnabled(true);
    editor->setReadOnly(!numeric);
    editor->setColour(juce::TextEditor::backgroundColourId, Theme::editor);
    if (columnId == orangeAboveColumn)
    {
        editor->setDisplayedText(thresholdText(mapping->orangeAboveThresholdEnabled, mapping->orangeAboveThreshold));
        editor->commit = [this, row] (const juce::String& text) { commitThreshold(row, ThresholdSlot::orangeAbove, text); };
    }
    else if (columnId == redAboveColumn)
    {
        editor->setDisplayedText(thresholdText(mapping->redAboveThresholdEnabled, mapping->redAboveThreshold));
        editor->commit = [this, row] (const juce::String& text) { commitThreshold(row, ThresholdSlot::redAbove, text); };
    }
    else if (columnId == orangeBelowColumn)
    {
        editor->setDisplayedText(thresholdText(mapping->orangeBelowThresholdEnabled, mapping->orangeBelowThreshold));
        editor->commit = [this, row] (const juce::String& text) { commitThreshold(row, ThresholdSlot::orangeBelow, text); };
    }
    else
    {
        editor->setDisplayedText(thresholdText(mapping->redBelowThresholdEnabled, mapping->redBelowThreshold));
        editor->commit = [this, row] (const juce::String& text) { commitThreshold(row, ThresholdSlot::redBelow, text); };
    }
    return editor;
}

void PageListView::commitValue(int row, const juce::String& text)
{
    const auto* mapping = mappingAt(row);
    const auto info = infoAt(row);
    if (!mapping || !info || !info->writable || !parameterWriter)
        return;
    const auto parsed = parseValueForParameter(*info, text);
    if (parsed)
        parameterWriter(mapping->parameterPath, *parsed);
}

void PageListView::commitThreshold(int row, ThresholdSlot slot, const juce::String& text)
{
    const auto trimmed = text.trim();
    const bool enabled = trimmed.isNotEmpty();
    double value = 0.0;
    if (enabled)
    {
        const auto parsed = parseDoubleStrict(trimmed);
        if (!parsed) return;
        value = *parsed;
    }

    std::vector<int> changedRows;
    for (const auto targetRow : rowsAffectedByCellEdit(row))
    {
        if (targetRow < 0 || targetRow >= (int) pageSnapshot.controls.size())
            continue;
        const auto info = infoAt(targetRow);
        if (!info || !ControlAlertPolicy::supportsThresholds(*info))
            continue;

        auto& mapping = pageSnapshot.controls[(size_t) targetRow];
        switch (slot)
        {
            case ThresholdSlot::orangeAbove:
                mapping.orangeAboveThresholdEnabled = enabled; mapping.orangeAboveThreshold = value; break;
            case ThresholdSlot::redAbove:
                mapping.redAboveThresholdEnabled = enabled; mapping.redAboveThreshold = value; break;
            case ThresholdSlot::orangeBelow:
                mapping.orangeBelowThresholdEnabled = enabled; mapping.orangeBelowThreshold = value; break;
            case ThresholdSlot::redBelow:
                mapping.redBelowThresholdEnabled = enabled; mapping.redBelowThreshold = value; break;
        }
        changedRows.push_back(targetRow);
    }

    emitMappingsChanged(changedRows);
    updateBatchControls();
    table.repaint();
}

}
