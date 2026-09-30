#include "PageListView.h"
#include "../model/ControlAlertPolicy.h"
#include "StringContentViewer.h"
#include "ContentPreview.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "../app/Theme.h"

namespace er
{
bool PageListView::ReorderTable::isInterestedInDragSource(const SourceDetails& details)
{
    return details.sourceComponent.get() == this
        && details.description.toString() == "DEVICEControl.PageListReorder";
}

void PageListView::ReorderTable::itemDragMove(const SourceDetails& details)
{
    const auto next = getInsertionIndexForPosition(details.localPosition.x, details.localPosition.y);
    if (next != insertionIndex) { insertionIndex = next; repaint(); }
}

void PageListView::ReorderTable::itemDragExit(const SourceDetails&)
{
    insertionIndex = -1;
    repaint();
}

void PageListView::ReorderTable::itemDropped(const SourceDetails& details)
{
    const auto target = getInsertionIndexForPosition(details.localPosition.x, details.localPosition.y);
    insertionIndex = -1;
    repaint();
    if (target >= 0 && onDrop) onDrop(target);
}

void PageListView::ReorderTable::paintOverChildren(juce::Graphics& g)
{
    juce::TableListBox::paintOverChildren(g);
    if (insertionIndex < 0 || getNumRows() == 0) return;
    const auto y = insertionIndex == getNumRows()
        ? getRowPosition(getNumRows() - 1, true).getBottom()
        : getRowPosition(insertionIndex, true).getY();
    g.setColour(Theme::accentBlue);
    g.fillRect(0, y - 2, getWidth(), 3);
}

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

struct ColourChoice
{
    int menuId;
    const char* name;
    juce::uint32 argb;
};

constexpr ColourChoice colourChoices[] = {
    { 101, "Orange", 0xffff9600 },
    { 102, "Blue",   0xff00beff },
    { 103, "Green",  0xff00ff78 },
    { 104, "Red",    0xffd95050 },
    { 105, "Purple", 0xff9a6ad6 },
    { 106, "Teal",   0xff2fa7a0 },
    { 107, "Yellow", 0xfffffc79 },
    { 108, "Grey",   0xff69727f }
};

juce::uint32 colourForMenuId(int menuId)
{
    if (menuId == 100) return 0;
    for (const auto& choice : colourChoices)
        if (choice.menuId == menuId) return choice.argb;
    return 0;
}

std::optional<juce::Colour> displayAccent(const ControlMapping& mapping,
                                          const std::optional<ParameterInfo>& info)
{
    if (info && ControlAlertPolicy::hasThresholds(mapping)
        && ControlAlertPolicy::supportsThresholds(*info))
        return alertColour(ControlAlertPolicy::evaluate(mapping, *info));
    if (mapping.colourArgb != 0)
        return juce::Colour(mapping.colourArgb);
    return std::nullopt;
}

class CommitEditor final : public juce::TextEditor
{
public:
    std::function<void(const juce::String&)> commit;
    std::function<void(const juce::MouseEvent&)> popup;
    std::function<bool()> returnPressed;

    CommitEditor()
    {
        setSelectAllWhenFocused(true);
        setColour(juce::TextEditor::textColourId, juce::Colours::white);
        setColour(juce::TextEditor::backgroundColourId, Theme::editor);
        setColour(juce::TextEditor::outlineColourId, Theme::outline);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBlue);
        onReturnKey = [this]
        {
            juce::Component::SafePointer<CommitEditor> safeThis(this);
            sendCommit();
            if (safeThis == nullptr)
                return;
            if (!returnPressed || !returnPressed())
                giveAwayKeyboardFocus();
        };
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

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() && popup && !hasKeyboardFocus(true))
        {
            popup(event);
            return;
        }
        juce::TextEditor::mouseDown(event);
    }

private:
    juce::String lastCommitted;
};

class ContextTextButton : public juce::TextButton
{
public:
    explicit ContextTextButton(const juce::String& text) : juce::TextButton(text)
    {
        setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
    std::function<void(const juce::MouseEvent&)> popup;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() && popup)
        {
            popup(event);
            return;
        }
        juce::TextButton::mouseDown(event);
    }
};

// Button-type controls use a small clickable value display in the List view.
// Draw its text explicitly so no LookAndFeel, enabled, toggle, or read-only
// state can reduce the value text opacity.
class PureWhiteValueButton final : public ContextTextButton
{
public:
    explicit PureWhiteValueButton(const juce::String& text) : ContextTextButton(text) {}

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto background = findColour(getToggleState()
                                               ? juce::TextButton::buttonOnColourId
                                               : juce::TextButton::buttonColourId);
        getLookAndFeel().drawButtonBackground(g, *this, background, highlighted, down);

        g.setColour(juce::Colours::white);
        g.setFont(getLookAndFeel().getTextButtonFont(*this, getHeight()));
        g.drawFittedText(getButtonText(), getLocalBounds().reduced(5, 2),
                         juce::Justification::centred, 1, 0.72f);
    }
};

class ContentButton final : public ContextTextButton
{
public:
    std::function<void()> show;

    ContentButton() : ContextTextButton("Show Content")
    {
        onClick = [this] { if (show) show(); };
    }
};

class SmallValueButton final : public juce::Component
{
public:
    std::function<void()> activate;
    std::function<void(const juce::MouseEvent&)> popup;

    SmallValueButton() : button("-")
    {
        addAndMakeVisible(button);
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        button.onClick = [this] { if (activate) activate(); };
        button.popup = [this] (const juce::MouseEvent& event)
        {
            if (popup) popup(event);
        };
    }

    void setText(const juce::String& text) { button.setButtonText(text); }
    void setButtonEnabled(bool enabled)
    {
        // JUCE fades disabled button text. Keep the component enabled for
        // full-white rendering and block mouse input separately when the
        // represented parameter cannot be operated.
        button.setEnabled(true);
        button.setInterceptsMouseClicks(enabled, enabled);
    }
    void setToggleState(bool state) { button.setToggleState(state, juce::dontSendNotification); }
    void setPopupTooltip(const juce::String& text) { button.setTooltip(text); }

    void setAccent(std::optional<juce::Colour> accent)
    {
        if (!accent)
        {
            button.removeColour(juce::TextButton::buttonColourId);
            button.removeColour(juce::TextButton::buttonOnColourId);
            return;
        }
        button.setColour(juce::TextButton::buttonColourId, accent->darker(0.28f));
        button.setColour(juce::TextButton::buttonOnColourId, accent->brighter(0.12f));
    }

    void resized() override
    {
        const int width = juce::jmin(110, juce::jmax(0, getWidth() - 8));
        button.setBounds((getWidth() - width) / 2, 3, width, juce::jmax(0, getHeight() - 6));
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() && popup)
            popup(event);
    }

private:
    PureWhiteValueButton button;
};

juce::String buttonText(const ParameterInfo& info)
{
    if (info.kind == ParameterKind::booleanValue)
        return static_cast<bool>(info.value) ? "ON" : "OFF";
    if (info.kind == ParameterKind::trigger)
        return "TRIGGER";
    if (info.kind == ParameterKind::enumeration)
    {
        const auto current = (int) (juce::int64) info.value;
        for (const auto& entry : info.enumEntries)
            if (entry.upToFirstOccurrenceOf(":", false, false).getIntValue() == current)
                return entry.fromFirstOccurrenceOf(":", false, false);
    }
    const auto value = info.formattedValue();
    return value.isNotEmpty() ? value : juce::String("-");
}

std::optional<juce::var> nextButtonValue(const ParameterInfo& info)
{
    switch (info.kind)
    {
        case ParameterKind::booleanValue:
            return juce::var(!static_cast<bool>(info.value));
        case ParameterKind::trigger:
            return juce::var(true);
        case ParameterKind::enumeration:
        {
            if (info.enumEntries.isEmpty()) return std::nullopt;
            const int current = (int) (juce::int64) info.value;
            int index = -1;
            for (int i = 0; i < info.enumEntries.size(); ++i)
                if (info.enumEntries[i].upToFirstOccurrenceOf(":", false, false).getIntValue() == current)
                {
                    index = i;
                    break;
                }
            const auto entry = info.enumEntries[(index + 1 + info.enumEntries.size()) % info.enumEntries.size()];
            return juce::var(entry.upToFirstOccurrenceOf(":", false, false).getIntValue());
        }
        case ParameterKind::integer:
        case ParameterKind::real:
            return juce::var(info.value.toString().getDoubleValue() == 0.0 ? 1 : 0);
        default:
            return std::nullopt;
    }
}

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

struct LabelSequence
{
    juce::String prefix;
    juce::int64 firstNumber = 0;
    int digitCount = 0;
    bool numbered = false;
};

LabelSequence parseLabelSequence(juce::String text)
{
    text = text.trimEnd();
    int digitStart = text.length();
    while (digitStart > 0 && juce::CharacterFunctions::isDigit(text[digitStart - 1]))
        --digitStart;

    LabelSequence result;
    if (digitStart == text.length())
    {
        result.prefix = text;
        return result;
    }

    const auto digits = text.substring(digitStart);
    result.prefix = text.substring(0, digitStart);
    result.firstNumber = digits.getLargeIntValue();
    result.digitCount = digits.length();
    result.numbered = true;
    return result;
}

juce::String labelForSequence(const LabelSequence& sequence, int offset)
{
    if (!sequence.numbered)
        return sequence.prefix;

    auto number = juce::String(sequence.firstNumber + (juce::int64) offset);
    if (number.length() < sequence.digitCount)
        number = number.paddedLeft('0', sequence.digitCount);
    return sequence.prefix + number;
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
    h.addColumn("Move", moveColumn, 48, 42, 70, juce::TableHeaderComponent::notSortable);
    h.addColumn("Type", typeColumn, 82, 60, -1);
    h.addColumn("Label 1", label1Column, 260, 120, -1);
    h.addColumn("Label 2", label2Column, 180, 100, -1);
    // Keep the value/content column comfortably wide by default. The table header
    // remains freely resizable by dragging its column dividers.
    h.addColumn("Value / Content", valueColumn, 280, 110, -1);
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
    table.onDrop = [this] (int index) { finishReorder(index); };

    selectionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    selectionLabel.setColour(juce::Label::textColourId, Theme::text);
    selectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(selectionLabel);

    asButtonButton.onClick = [this] { applyTypeToSelection(ControlType::button); };
    asEncoderButton.onClick = [this] { applyTypeToSelection(ControlType::encoder); };
    asContentButton.onClick = [this] { applyTypeToSelection(ControlType::showContent); };
    asMeterButton.onClick = [this] { applyTypeToSelection(ControlType::meter); };
    removeSelectedButton.onClick = [this] { removeSelection(); };
    exportCsvButton.onClick = [this] { if (onExportCsv) onExportCsv(); };
    importCsvButton.onClick = [this] { if (onImportCsv) onImportCsv(); };
    exportCsvButton.setTooltip("Export all controls on this page, in list order");
    importCsvButton.setTooltip("Replace this page with all enabled CSV rows; other pages remain unchanged");
    addAndMakeVisible(asButtonButton);
    addAndMakeVisible(asEncoderButton);
    addAndMakeVisible(asContentButton);
    addAndMakeVisible(asMeterButton);
    addAndMakeVisible(removeSelectedButton);
    addAndMakeVisible(exportCsvButton);
    addAndMakeVisible(importCsvButton);

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

void PageListView::setControlLabel2(const juce::String& path, const juce::String& label2)
{
    for (auto& mapping : pageSnapshot.controls)
        if (mapping.parameterPath == path)
        {
            mapping.label2 = label2;
            table.updateContent();
            table.repaint();
            return;
        }
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
    asButtonButton.setBounds(firstRow.removeFromLeft(68).reduced(1));
    asEncoderButton.setBounds(firstRow.removeFromLeft(72).reduced(1));
    asContentButton.setBounds(firstRow.removeFromLeft(68).reduced(1));
    asMeterButton.setBounds(firstRow.removeFromLeft(64).reduced(1));
    firstRow.removeFromLeft(8);
    removeSelectedButton.setBounds(firstRow.removeFromLeft(82).reduced(1));
    firstRow.removeFromLeft(8);
    exportCsvButton.setBounds(firstRow.removeFromLeft(94).reduced(1));
    importCsvButton.setBounds(firstRow.removeFromLeft(94).reduced(1));

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

juce::var PageListView::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    dragPaths.clear();
    for (int i = 0; i < rows.size(); ++i)
    {
        const int row = rows[i];
        if (const auto* mapping = mappingAt(row))
            dragPaths.push_back(mapping->parameterPath);
    }
    return dragPaths.empty() ? juce::var() : juce::var("DEVICEControl.PageListReorder");
}

void PageListView::finishReorder(int insertionIndex)
{
    auto paths = std::move(dragPaths);
    dragPaths.clear();
    if (!paths.empty() && onReorder) onReorder(paths, insertionIndex);
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

void PageListView::cellClicked(int row, int, const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        showRowContextMenu(row, event.eventComponent);
}

void PageListView::showRowContextMenu(int row, juce::Component* target)
{
    if (row < 0 || row >= (int) pageSnapshot.controls.size())
        return;

    if (!table.isRowSelected(row))
    {
        table.deselectAllRows();
        table.selectRow(row, false, true);
    }

    const auto rows = rowsAffectedByCellEdit(row);
    std::vector<juce::String> paths;
    paths.reserve(rows.size());
    std::optional<juce::uint32> commonColour;
    bool first = true;
    for (const auto selectedRow : rows)
    {
        if (selectedRow < 0 || selectedRow >= (int) pageSnapshot.controls.size())
            continue;
        const auto& mapping = pageSnapshot.controls[(size_t) selectedRow];
        paths.push_back(mapping.parameterPath);
        if (first)
        {
            commonColour = mapping.colourArgb;
            first = false;
        }
        else if (commonColour && *commonColour != mapping.colourArgb)
        {
            commonColour.reset();
        }
    }
    if (paths.empty())
        return;

    juce::PopupMenu colourMenu;
    colourMenu.addItem(100, "Default", true, commonColour && *commonColour == 0);
    colourMenu.addSeparator();
    for (const auto& choice : colourChoices)
        colourMenu.addItem(choice.menuId, choice.name, true,
                           commonColour && *commonColour == choice.argb);

    juce::PopupMenu menu;
    menu.addSubMenu("Colour", colourMenu);
    menu.addSeparator();
    menu.addItem(1, paths.size() > 1 ? "Remove Selected" : "Remove");

    juce::Component::SafePointer<PageListView> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target != nullptr ? target : &table),
                       [safeThis, paths] (int result)
                       {
                           if (safeThis == nullptr || result == 0)
                               return;
                           if (result == 1)
                           {
                               if (safeThis->onRemoveSelected)
                                   safeThis->onRemoveSelected(paths);
                               return;
                           }
                           if (result >= 100 && result < 200)
                               safeThis->applyColourToPaths(paths, colourForMenuId(result));
                       });
}

void PageListView::applyColourToPaths(const std::vector<juce::String>& paths,
                                      juce::uint32 colourArgb)
{
    std::vector<int> changedRows;
    for (int row = 0; row < (int) pageSnapshot.controls.size(); ++row)
    {
        auto& mapping = pageSnapshot.controls[(size_t) row];
        if (std::find(paths.begin(), paths.end(), mapping.parameterPath) == paths.end())
            continue;
        mapping.colourArgb = colourArgb;
        changedRows.push_back(row);
    }

    emitMappingsChanged(changedRows);
    table.repaint();
}

void PageListView::selectedRowsChanged(int)
{
    updateBatchControls();
}

void PageListView::returnKeyPressed(int)
{
    const auto rows = selectedRows();
    if (!rows.empty())
        beginLabel2Edit(rows.front());
}

void PageListView::beginLabel2Edit(int row)
{
    if (row < 0 || row >= (int) pageSnapshot.controls.size())
        return;

    table.scrollToEnsureRowIsOnscreen(row);
    table.scrollToEnsureColumnIsOnscreen(label2Column);

    juce::Component::SafePointer<PageListView> safeThis(this);
    juce::MessageManager::callAsync([safeThis, row]
    {
        if (safeThis == nullptr)
            return;
        if (auto* editor = dynamic_cast<CommitEditor*>(
                safeThis->table.getCellComponent(label2Column, row)))
        {
            editor->grabKeyboardFocus();
            editor->selectAll();
        }
    });
}

bool PageListView::beginNextSelectedLabel2Edit(int row)
{
    const auto rows = selectedRows();
    const bool currentRowIsSelected = std::find(rows.begin(), rows.end(), row) != rows.end();

    // With a single selection, Enter walks through every following list row.
    // Move the selection as well so repeated Enter presses can continue without
    // requiring another mouse click.
    if (rows.size() <= 1 || !currentRowIsSelected)
    {
        const int nextRow = row + 1;
        if (nextRow >= (int) pageSnapshot.controls.size())
            return false;

        table.deselectAllRows();
        table.selectRow(nextRow, false, true);
        beginLabel2Edit(nextRow);
        return true;
    }

    const auto next = std::find_if(rows.begin(), rows.end(), [row] (int candidate)
    {
        return candidate > row;
    });
    if (next == rows.end())
        return false;

    beginLabel2Edit(*next);
    return true;
}

void PageListView::commitLabel2(int row, const juce::String& text)
{
    auto rows = rowsAffectedByCellEdit(row);
    const auto firstTarget = std::find(rows.begin(), rows.end(), row);
    if (firstTarget == rows.end())
        return;

    const auto sequence = parseLabelSequence(text);
    std::vector<std::pair<juce::String, juce::String>> notifications;
    int offset = 0;
    for (auto target = firstTarget; target != rows.end(); ++target, ++offset)
    {
        if (*target < 0 || *target >= (int) pageSnapshot.controls.size())
            continue;

        auto& mapping = pageSnapshot.controls[(size_t) *target];
        const auto nextLabel = labelForSequence(sequence, offset);
        if (mapping.label2 == nextLabel)
            continue;

        mapping.label2 = nextLabel;
        notifications.emplace_back(mapping.parameterPath, nextLabel);
    }

    for (const auto& [path, label] : notifications)
        if (onLabel2Changed)
            onLabel2Changed(path, label);

    table.updateContent();
    table.repaint();
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
    asMeterButton.setEnabled(any);
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

    std::vector<int> changedRows;
    for (const auto row : rows)
    {
        auto& mapping = pageSnapshot.controls[(size_t) row];
        if (mapping.type == ControlType::matrix)
            continue;
        mapping.type = type;
        changedRows.push_back(row);
    }

    emitMappingsChanged(changedRows);
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

void PageListView::paintRowBackground(juce::Graphics& g, int row, int width, int height, bool selected)
{
    if (const auto* mapping = mappingAt(row); mapping && mapping->colourArgb != 0)
    {
        g.setColour(juce::Colour(mapping->colourArgb).darker(0.72f));
        g.fillRect(0, 0, width, height);
    }
    if (selected)
    {
        g.setColour(Theme::accentDark.withAlpha(0.82f));
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
        case moveColumn: text = "↕"; break;
        case typeColumn:
            text = mapping->type == ControlType::encoder ? "Encoder"
                 : mapping->type == ControlType::showContent ? "Show Content"
                 : mapping->type == ControlType::meter ? "Meter"
                 : mapping->type == ControlType::matrix ? "Matrix" : "Button";
            break;
        case label1Column: text = mapping->label; break;
        case statusColumn:
        {
            if (mapping->type == ControlType::matrix)
            {
                text = "MATRIX";
                g.setColour(Theme::accentDark.withAlpha(0.28f));
                g.fillRect(0, 0, width, height);
            }
            else if (!info || !info->online)
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
                text = "ONLINE";
                g.setColour(Theme::accentDark.withAlpha(0.28f));
                g.fillRect(0, 0, width, height);
            }
            else if (mapping->type == ControlType::meter)
            {
                text = "METER";
                g.setColour(Theme::accentDark.withAlpha(0.28f));
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
    const auto popup = [this, row] (const juce::MouseEvent& event)
    {
        showRowContextMenu(row, event.eventComponent);
    };

    if (columnId == valueColumn && mapping->type == ControlType::button)
    {
        auto* valueButton = dynamic_cast<SmallValueButton*>(existing);
        if (!valueButton)
        {
            delete existing;
            valueButton = new SmallValueButton();
        }

        const bool supported = info && info->online
                            && (info->kind == ParameterKind::booleanValue
                                || info->kind == ParameterKind::trigger
                                || info->kind == ParameterKind::enumeration
                                || info->kind == ParameterKind::integer
                                || info->kind == ParameterKind::real);
        valueButton->setText(supported ? buttonText(*info) : juce::String("Offline"));
        valueButton->setButtonEnabled(supported && info->writable && (bool) parameterWriter);
        valueButton->setToggleState(supported && info->kind == ParameterKind::booleanValue
                                    && static_cast<bool>(info->value));
        valueButton->setAccent(displayAccent(*mapping, info));
        valueButton->setPopupTooltip(supported ? "Click to operate this control" : "Control unavailable");
        valueButton->popup = popup;
        valueButton->activate = [this, path]
        {
            if (!parameterResolver || !parameterWriter) return;
            const auto current = parameterResolver(path);
            if (!current || !current->online || !current->writable) return;
            const auto next = nextButtonValue(*current);
            if (next) parameterWriter(path, *next);
        };
        return valueButton;
    }

    if (columnId == valueColumn && mapping->type == ControlType::showContent)
    {
        auto* contentButton = dynamic_cast<ContentButton*>(existing);
        if (!contentButton)
        {
            delete existing;
            contentButton = new ContentButton();
        }
        const bool available = info && info->online && info->kind == ParameterKind::stringValue;
        // Keep text at full opacity even when the content is unavailable.
        // Disabled JUCE buttons multiply their text alpha automatically.
        contentButton->setEnabled(true);
        contentButton->setInterceptsMouseClicks(available, available);
        const auto content = available ? info->value.toString() : juce::String();
        contentButton->setButtonText(available ? makeContentPreview(content, 12, 110) : juce::String("Offline"));
        contentButton->setTooltip(available ? "Click to open/edit full content" : "Content unavailable");
        contentButton->popup = popup;
        if (const auto accent = displayAccent(*mapping, info))
        {
            contentButton->setColour(juce::TextButton::buttonColourId, accent->darker(0.28f));
            contentButton->setColour(juce::TextButton::buttonOnColourId, accent->brighter(0.12f));
        }
        else
        {
            contentButton->removeColour(juce::TextButton::buttonColourId);
            contentButton->removeColour(juce::TextButton::buttonOnColourId);
        }
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
    editor->popup = popup;
    editor->returnPressed = {};

    if (columnId == label2Column)
    {
        editor->setEnabled(true);
        editor->setReadOnly(false);
        editor->setDisplayedText(mapping->label2);
        editor->setColour(juce::TextEditor::backgroundColourId, Theme::editor);
        editor->commit = [this, row] (const juce::String& text) { commitLabel2(row, text); };
        editor->returnPressed = [this, row] { return beginNextSelectedLabel2Edit(row); };
        return editor;
    }

    if (columnId == valueColumn)
    {
        const bool writable = mapping->type != ControlType::meter
                           && mapping->type != ControlType::matrix
                           && info && info->writable && info->online;
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
