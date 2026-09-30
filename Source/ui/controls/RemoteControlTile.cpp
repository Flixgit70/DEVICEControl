#include "RemoteControlTile.h"
#include <cmath>
#include "../../model/ControlValuePolicy.h"
#include "../../model/ControlAlertPolicy.h"
#include "../StringContentViewer.h"
#include "../../app/Theme.h"

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
}

RemoteControlTile::RemoteControlTile(ControlMapping mappingToUse) : controlMapping(std::move(mappingToUse)) {}

void RemoteControlTile::setTileSelected(bool selected)
{
    if (tileSelected == selected) return;
    tileSelected = selected;
    repaint();
}

void RemoteControlTile::setControlColour(juce::uint32 colourArgb)
{
    controlMapping.colourArgb = colourArgb;
    applyControlColour();
    repaint();
}

void RemoteControlTile::watchForContextMenu(juce::Component& component)
{
    component.addMouseListener(this, false);
}

juce::Colour RemoteControlTile::accentColour() const
{
    if (lastInfo && ControlAlertPolicy::hasThresholds(controlMapping)
        && ControlAlertPolicy::supportsThresholds(*lastInfo))
        return alertColour(ControlAlertPolicy::evaluate(controlMapping, *lastInfo));

    return controlMapping.colourArgb != 0 ? juce::Colour(controlMapping.colourArgb)
                                          : Theme::accentBlue;
}

void RemoteControlTile::applyPathTooltip()
{
    const auto tooltip = controlMapping.label.isNotEmpty() ? controlMapping.label : controlMapping.parameterPath;
    setTooltip(tooltip);
    for (auto* child : getChildren())
        if (auto* tooltipTarget = dynamic_cast<juce::SettableTooltipClient*>(child))
            tooltipTarget->setTooltip(tooltip);
}

bool RemoteControlTile::requestSelection(const juce::ModifierKeys& modifiers,
                                         bool allowPlainSelection)
{
    if (!onSelectionGesture)
        return false;

    TileSelectionGesture gesture;
    if (modifiers.isShiftDown())
        gesture = modifiers.isCommandDown() ? TileSelectionGesture::addRange
                                            : TileSelectionGesture::range;
    else if (modifiers.isCommandDown())
        gesture = TileSelectionGesture::toggle;
    else if (allowPlainSelection)
        gesture = TileSelectionGesture::replace;
    else
        return false;

    onSelectionGesture(controlMapping.parameterPath, gesture);
    return true;
}

void RemoteControlTile::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isPopupMenu())
    {
        // Interactive children intercept modified clicks themselves so that a
        // selection gesture never changes a value or opens a content window.
        const bool interactiveChild = dynamic_cast<juce::Button*>(event.eventComponent) != nullptr
                                   || dynamic_cast<juce::Slider*>(event.eventComponent) != nullptr;
        if (!interactiveChild)
            requestSelection(event.mods, true);
        return;
    }

    juce::PopupMenu colourMenu;
    colourMenu.addItem(100, "Default", true, controlMapping.colourArgb == 0);
    colourMenu.addSeparator();
    for (const auto& choice : colourChoices)
        colourMenu.addItem(choice.menuId, choice.name, true, controlMapping.colourArgb == choice.argb);

    juce::PopupMenu menu;
    menu.addItem(3, tileSelected ? "Deselect" : "Select");
    menu.addItem(2, "Edit Label 2...");
    menu.addSubMenu("Colour", colourMenu);
    menu.addSeparator();
    menu.addItem(1, "Remove");

    juce::Component::SafePointer<RemoteControlTile> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(event.eventComponent),
                       [safeThis] (int result)
                       {
                           if (safeThis == nullptr || result == 0)
                               return;

                           if (result == 1)
                           {
                               if (safeThis->onRemove)
                                   safeThis->onRemove(safeThis->controlMapping.parameterPath);
                               return;
                           }

                           if (result == 2)
                           {
                               safeThis->beginLabel2Edit();
                               return;
                           }

                           if (result == 3)
                           {
                               safeThis->setTileSelected(!safeThis->isTileSelected());
                               if (safeThis->onSelectionChange)
                                   safeThis->onSelectionChange(safeThis->controlMapping.parameterPath,
                                                               safeThis->isTileSelected());
                               return;
                           }

                           if (result >= 100 && result < 200)
                           {
                               const auto colour = colourForMenuId(result);
                               safeThis->setControlColour(colour);
                               if (safeThis->onColourChange)
                                   safeThis->onColourChange(safeThis->controlMapping.parameterPath, colour);
                           }
                       });
}

void RemoteControlTile::paintFrame(juce::Graphics& g, const ParameterInfo* info) const
{
    const auto online = info == nullptr || info->online;
    const bool dynamicStatus = info != nullptr && ControlAlertPolicy::hasThresholds(controlMapping)
                            && ControlAlertPolicy::supportsThresholds(*info);
    if ((controlMapping.colourArgb != 0 || dynamicStatus) && online)
    {
        const auto accent = accentColour();
        g.setColour(accent.darker(0.72f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(accent.withAlpha(info && info->writable ? 0.95f : 0.55f));
    }
    else
    {
        g.setColour(online ? Theme::panelRaised : Theme::panel);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(info && info->writable ? Theme::outline : Theme::outline);
    }
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);

    if (tileSelected)
    {
        g.setColour(Theme::accentBlue.brighter(0.25f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 7.0f, 2.5f);
    }
}

ButtonTile::ButtonTile(ControlMapping m) : RemoteControlTile(std::move(m))
{
    name.setText(controlMapping.label.isNotEmpty() ? controlMapping.label : controlMapping.parameterPath, juce::dontSendNotification);
    name.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    name.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(name);

    secondaryName.setText(controlMapping.label2, juce::dontSendNotification);
    secondaryName.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    secondaryName.setColour(juce::Label::textColourId, Theme::textBright);
    secondaryName.setJustificationType(juce::Justification::centred);
    secondaryName.setMinimumHorizontalScale(0.65f);
    secondaryName.setEditable(false, true, false);
    secondaryName.onTextChange = [this]
    {
        controlMapping.label2 = secondaryName.getText();
        if (onLabel2Change) onLabel2Change(controlMapping.parameterPath, controlMapping.label2);
    };
    addAndMakeVisible(secondaryName);

    addAndMakeVisible(button);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    button.selectionMouseDown = [this] (const juce::MouseEvent& event)
    {
        return !event.mods.isPopupMenu() && requestSelection(event.mods, false);
    };

    meta.setFont(juce::FontOptions(8.0f));
    meta.setColour(juce::Label::textColourId, juce::Colours::grey);
    meta.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(meta);

    watchForContextMenu(name);
    watchForContextMenu(secondaryName);
    watchForContextMenu(button);
    watchForContextMenu(meta);
    applyControlColour();
    applyPathTooltip();

    button.onClick = [this]
    {
        if (!lastInfo || !lastInfo->writable || !onWrite) return;
        const auto& p = *lastInfo;
        juce::var next;
        switch (p.kind)
        {
            case ParameterKind::booleanValue: next = !static_cast<bool>(p.value); break;
            case ParameterKind::trigger: next = true; break;
            case ParameterKind::enumeration:
            {
                if (p.enumEntries.isEmpty()) return;
                int current = (int) (juce::int64) p.value;
                int index = -1;
                for (int i = 0; i < p.enumEntries.size(); ++i)
                    if (p.enumEntries[i].upToFirstOccurrenceOf(":", false, false).getIntValue() == current) { index = i; break; }
                const auto nextEntry = p.enumEntries[(index + 1 + p.enumEntries.size()) % p.enumEntries.size()];
                next = nextEntry.upToFirstOccurrenceOf(":", false, false).getIntValue();
                break;
            }
            case ParameterKind::integer:
            case ParameterKind::real: next = p.value.toString().getDoubleValue() == 0.0 ? 1 : 0; break;
            default: return;
        }
        onWrite(controlMapping.parameterPath, next);
    };
}

void ButtonTile::applyControlColour()
{
    const bool dynamicStatus = lastInfo && ControlAlertPolicy::hasThresholds(controlMapping)
                            && ControlAlertPolicy::supportsThresholds(*lastInfo);
    if (controlMapping.colourArgb == 0 && !dynamicStatus)
    {
        button.removeColour(juce::TextButton::buttonColourId);
        button.removeColour(juce::TextButton::buttonOnColourId);
        return;
    }
    const auto accent = accentColour();
    button.setColour(juce::TextButton::buttonColourId, accent.darker(0.28f));
    button.setColour(juce::TextButton::buttonOnColourId, accent.brighter(0.12f));
}

void ButtonTile::beginLabel2Edit()
{
    secondaryName.showEditor();
}

void ButtonTile::setControlLabel2(const juce::String& label2)
{
    controlMapping.label2 = label2;
    secondaryName.setText(label2, juce::dontSendNotification);
}

void ButtonTile::paint(juce::Graphics& g) { paintFrame(g, lastInfo ? &*lastInfo : nullptr); }
void ButtonTile::resized()
{
    auto r = getLocalBounds().reduced(5);
    name.setBounds(r.removeFromTop(17));
    secondaryName.setBounds(r.removeFromTop(14));
    r.removeFromTop(1);
    meta.setBounds(r.removeFromBottom(12));
    button.setBounds(r.reduced(3, 2));
}

void ButtonTile::refresh(const ParameterInfo& p)
{
    lastInfo = p;
    const bool supported = p.kind == ParameterKind::booleanValue
                        || p.kind == ParameterKind::trigger
                        || p.kind == ParameterKind::enumeration
                        || p.kind == ParameterKind::integer
                        || p.kind == ParameterKind::real;
    button.setEnabled(p.online && supported);
    juce::String text = p.value.toString();
    if (p.kind == ParameterKind::booleanValue) text = static_cast<bool>(p.value) ? "ON" : "OFF";
    else if (p.kind == ParameterKind::trigger) text = "TRIGGER";
    else if (p.kind == ParameterKind::enumeration)
    {
        const auto current = (int) (juce::int64) p.value;
        for (auto entry : p.enumEntries)
            if (entry.upToFirstOccurrenceOf(":", false, false).getIntValue() == current)
                text = entry.fromFirstOccurrenceOf(":", false, false);
    }
    button.setButtonText(text.isNotEmpty() ? text : "-");
    button.setToggleState(p.kind == ParameterKind::booleanValue && static_cast<bool>(p.value), juce::dontSendNotification);
    meta.setText((p.localPath.isNotEmpty() ? p.localPath : p.path) + "  |  " + ParameterInfo::kindToString(p.kind), juce::dontSendNotification);
    applyControlColour();
    repaint();
}

ShowContentTile::ShowContentTile(ControlMapping m)
    : RemoteControlTile(std::move(m)),
      resizeCorner(this, &resizeConstrainer)
{
    resizeConstrainer.setSizeLimits(180, 110, 720, 420);
    name.setText(controlMapping.label.isNotEmpty() ? controlMapping.label : controlMapping.parameterPath, juce::dontSendNotification);
    name.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    name.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(name);

    secondaryName.setText(controlMapping.label2, juce::dontSendNotification);
    secondaryName.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    secondaryName.setColour(juce::Label::textColourId, Theme::textBright);
    secondaryName.setJustificationType(juce::Justification::centred);
    secondaryName.setMinimumHorizontalScale(0.65f);
    secondaryName.setEditable(false, true, false);
    secondaryName.onTextChange = [this]
    {
        controlMapping.label2 = secondaryName.getText();
        if (onLabel2Change) onLabel2Change(controlMapping.parameterPath, controlMapping.label2);
    };
    addAndMakeVisible(secondaryName);

    selectionToggle.setButtonText(juce::String());
    selectionToggle.onClick = [this]
    {
        const auto modifiers = juce::ModifierKeys::getCurrentModifiersRealtime();
        if ((modifiers.isCommandDown() || modifiers.isShiftDown())
            && requestSelection(modifiers, false))
            return;

        setTileSelected(selectionToggle.getToggleState());
        if (onSelectionChange)
            onSelectionChange(controlMapping.parameterPath, isTileSelected());
    };
    addAndMakeVisible(selectionToggle);

    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.selectionMouseDown = [this] (const juce::MouseEvent& event)
    {
        return !event.mods.isPopupMenu() && requestSelection(event.mods, false);
    };
    button.onClick = [this]
    {
        if (!lastInfo || !lastInfo->online || lastInfo->kind != ParameterKind::stringValue)
            return;
        const auto path = controlMapping.parameterPath;
        const bool writable = lastInfo->writable;
        const auto writer = onWrite;
        showStringContent(button,
                          controlMapping.label2.isNotEmpty() ? controlMapping.label2 : controlMapping.label,
                          lastInfo->value.toString(),
                          writable,
                          [writer, path] (const juce::String& text)
                          {
                              if (writer) writer(path, juce::var(text));
                          });
    };
    addAndMakeVisible(button);

    addAndMakeVisible(resizeCorner);
    resizeCorner.setAlwaysOnTop(true);

    meta.setFont(juce::FontOptions(8.0f));
    meta.setColour(juce::Label::textColourId, juce::Colours::grey);
    meta.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(meta);

    watchForContextMenu(name);
    watchForContextMenu(secondaryName);
    watchForContextMenu(button);
    watchForContextMenu(meta);
    applyControlColour();
    applyPathTooltip();
    selectionToggle.setTooltip("Select for group resize and colour changes");
}

void ShowContentTile::applyControlColour()
{
    if (controlMapping.colourArgb == 0)
    {
        button.removeColour(juce::TextButton::buttonColourId);
        button.removeColour(juce::TextButton::buttonOnColourId);
        return;
    }
    const auto accent = accentColour();
    button.setColour(juce::TextButton::buttonColourId, accent.darker(0.28f));
    button.setColour(juce::TextButton::buttonOnColourId, accent.brighter(0.12f));
}

void ShowContentTile::beginLabel2Edit()
{
    secondaryName.showEditor();
}

void ShowContentTile::setControlLabel2(const juce::String& label2)
{
    controlMapping.label2 = label2;
    secondaryName.setText(label2, juce::dontSendNotification);
}

void ShowContentTile::setTileSelected(bool selected)
{
    RemoteControlTile::setTileSelected(selected);
    selectionToggle.setToggleState(selected, juce::dontSendNotification);
}

void ShowContentTile::paint(juce::Graphics& g)
{
    paintFrame(g, lastInfo ? &*lastInfo : nullptr);
}

void ShowContentTile::resized()
{
    auto r = getLocalBounds().reduced(5);
    auto titleRow = r.removeFromTop(17);
    selectionToggle.setBounds(titleRow.removeFromRight(18));
    name.setBounds(titleRow);
    secondaryName.setBounds(r.removeFromTop(14));
    r.removeFromTop(1);
    meta.setBounds(r.removeFromBottom(12));
    button.setBounds(r.reduced(3, 2));
    resizeCorner.setBounds(getWidth() - 16, getHeight() - 16, 16, 16);
    resizeCorner.toFront(false);

    if (getWidth() > 0 && getHeight() > 0
        && (controlMapping.tileWidth != getWidth() || controlMapping.tileHeight != getHeight()))
    {
        const int widthDelta = controlMapping.tileWidth > 0 ? getWidth() - controlMapping.tileWidth : 0;
        const int heightDelta = controlMapping.tileHeight > 0 ? getHeight() - controlMapping.tileHeight : 0;
        controlMapping.tileWidth = getWidth();
        controlMapping.tileHeight = getHeight();
        if (onSizeChange)
            onSizeChange(controlMapping.parameterPath, getWidth(), getHeight(), widthDelta, heightDelta);
    }
}

void ShowContentTile::refresh(const ParameterInfo& p)
{
    lastInfo = p;
    const bool available = p.online && p.kind == ParameterKind::stringValue;
    button.setEnabled(available);
    const auto content = available ? p.value.toString() : juce::String();
    button.setContentText(available ? content : juce::String("Offline"));
    button.setTooltip(available ? "Click to open/edit full content" : "Content unavailable");
    meta.setText((p.localPath.isNotEmpty() ? p.localPath : p.path) + "  |  " + ParameterInfo::kindToString(p.kind),
                 juce::dontSendNotification);
    applyControlColour();
    repaint();
}

EncoderTile::EncoderTile(ControlMapping m) : RemoteControlTile(std::move(m))
{
    name.setText(controlMapping.label.isNotEmpty() ? controlMapping.label : controlMapping.parameterPath, juce::dontSendNotification);
    name.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    name.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(name);

    secondaryName.setText(controlMapping.label2, juce::dontSendNotification);
    secondaryName.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    secondaryName.setColour(juce::Label::textColourId, Theme::textBright);
    secondaryName.setJustificationType(juce::Justification::centred);
    secondaryName.setMinimumHorizontalScale(0.65f);
    secondaryName.setEditable(false, true, false);
    secondaryName.onTextChange = [this]
    {
        controlMapping.label2 = secondaryName.getText();
        if (onLabel2Change) onLabel2Change(controlMapping.parameterPath, controlMapping.label2);
    };
    addAndMakeVisible(secondaryName);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setScrollWheelEnabled(true);
    slider.selectionMouseDown = [this] (const juce::MouseEvent& event)
    {
        return !event.mods.isPopupMenu() && requestSelection(event.mods, false);
    };
    slider.onValueChange = [this]
    {
        if (updating || !lastInfo || !lastInfo->writable || !onWrite) return;
        const auto& p = *lastInfo;
        if (p.kind != ParameterKind::enumeration && !configuredHasMaximum)
            ensureOpenEndedRangeFor(slider.getValue());
        juce::var v;
        if (p.kind == ParameterKind::enumeration) v = enumValueForIndex(p, juce::roundToInt(slider.getValue()));
        else
        {
            const double factor = p.factor == 0.0 ? 1.0 : p.factor;
            const double raw = slider.getValue() * factor;
            if (p.kind == ParameterKind::integer) v = (juce::int64) std::llround(raw);
            else v = raw;
        }
        onWrite(controlMapping.parameterPath, v);
    };
    addAndMakeVisible(slider);

    value.setJustificationType(juce::Justification::centred);
    value.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    value.setColour(juce::Label::textColourId, Theme::textBright);
    addAndMakeVisible(value);
    meta.setJustificationType(juce::Justification::centred);
    meta.setFont(juce::FontOptions(8.0f));
    meta.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(meta);

    watchForContextMenu(name);
    watchForContextMenu(secondaryName);
    watchForContextMenu(slider);
    watchForContextMenu(value);
    watchForContextMenu(meta);
    applyControlColour();
    applyPathTooltip();
}

void EncoderTile::applyControlColour()
{
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    const bool dynamicStatus = lastInfo && ControlAlertPolicy::hasThresholds(controlMapping)
                            && ControlAlertPolicy::supportsThresholds(*lastInfo);
    if (controlMapping.colourArgb == 0 && !dynamicStatus)
    {
        slider.removeColour(juce::Slider::rotarySliderFillColourId);
        slider.removeColour(juce::Slider::rotarySliderOutlineColourId);
        return;
    }
    const auto accent = accentColour();
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, accent.darker(0.62f));
}

void EncoderTile::beginLabel2Edit()
{
    secondaryName.showEditor();
}

void EncoderTile::setControlLabel2(const juce::String& label2)
{
    controlMapping.label2 = label2;
    secondaryName.setText(label2, juce::dontSendNotification);
}

void EncoderTile::ensureOpenEndedRangeFor(double displayValue)
{
    if (configuredHasMaximum || configuredKind == ParameterKind::enumeration) return;
    const double quantum = configuredKind == ParameterKind::integer ? 10.0 : 1.0;
    const double nextUpper = ControlValuePolicy::extendOpenEndedUpperBound(dynamicUpperBound, displayValue, quantum);
    if (nextUpper <= dynamicUpperBound) return;

    dynamicUpperBound = nextUpper;
    const bool wasUpdating = updating;
    updating = true;
    slider.setRange(slider.getMinimum(), dynamicUpperBound, slider.getInterval());
    updating = wasUpdating;
}

void EncoderTile::paint(juce::Graphics& g) { paintFrame(g, lastInfo ? &*lastInfo : nullptr); }
void EncoderTile::resized()
{
    auto r = getLocalBounds().reduced(5);
    name.setBounds(r.removeFromTop(17));
    secondaryName.setBounds(r.removeFromTop(14));
    meta.setBounds(r.removeFromBottom(12));
    value.setBounds(r.removeFromBottom(18));
    const int knob = juce::jmax(30, juce::jmin(r.getWidth(), r.getHeight()));
    slider.setBounds(r.withSizeKeepingCentre(knob, knob));
}

int EncoderTile::enumIndexForValue(const ParameterInfo& p, int raw) const
{
    for (int i = 0; i < p.enumEntries.size(); ++i)
        if (p.enumEntries[i].upToFirstOccurrenceOf(":", false, false).getIntValue() == raw) return i;
    return 0;
}

int EncoderTile::enumValueForIndex(const ParameterInfo& p, int index) const
{
    if (p.enumEntries.isEmpty()) return index;
    index = juce::jlimit(0, p.enumEntries.size() - 1, index);
    return p.enumEntries[index].upToFirstOccurrenceOf(":", false, false).getIntValue();
}

void EncoderTile::configureFor(const ParameterInfo& p)
{
    const bool hasMin = !p.minimum.isVoid();
    const bool hasMax = !p.maximum.isVoid();
    const double factor = p.factor == 0.0 ? 1.0 : p.factor;

    const bool configurationChanged = configuredKind != p.kind
                                   || configuredEnumSize != p.enumEntries.size()
                                   || configuredFactor != factor
                                   || configuredHasMinimum != hasMin
                                   || configuredHasMaximum != hasMax;

    if (!configurationChanged) return;

    updating = true;
    if (p.kind == ParameterKind::enumeration && !p.enumEntries.isEmpty())
    {
        slider.setRange(0.0, (double) p.enumEntries.size() - 1.0, 1.0);
    }
    else
    {
        double lo = (hasMin ? p.minimum.toString().getDoubleValue() : 0.0) / factor;
        const double defaultUpper = p.kind == ParameterKind::integer ? 100.0 : 1.0;
        const double quantum = p.kind == ParameterKind::integer ? 10.0 : 1.0;
        double hi = defaultUpper;
        if (hasMax)
            hi = p.maximum.toString().getDoubleValue() / factor;
        else
        {
            dynamicUpperBound = defaultUpper;
            const double observed = p.value.toString().getDoubleValue() / factor;
            dynamicUpperBound = ControlValuePolicy::extendOpenEndedUpperBound(dynamicUpperBound, observed, quantum);
            hi = dynamicUpperBound;
        }
        if (hi <= lo) hi = lo + 1.0;
        const double step = p.kind == ParameterKind::integer ? (1.0 / std::abs(factor)) : 0.001;
        slider.setRange(lo, hi, step);
    }
    configuredKind = p.kind;
    configuredEnumSize = p.enumEntries.size();
    configuredFactor = factor;
    configuredHasMinimum = hasMin;
    configuredHasMaximum = hasMax;
    updating = false;
}

void EncoderTile::refresh(const ParameterInfo& p)
{
    lastInfo = p;
    configureFor(p);
    slider.setEnabled(p.writable && p.online && (p.kind == ParameterKind::integer || p.kind == ParameterKind::real || p.kind == ParameterKind::enumeration));
    updating = true;
    if (p.kind == ParameterKind::enumeration)
    {
        const int raw = (int) (juce::int64) p.value;
        slider.setValue(enumIndexForValue(p, raw), juce::dontSendNotification);
        juce::String display = juce::String(raw);
        const auto i = enumIndexForValue(p, raw);
        if (i >= 0 && i < p.enumEntries.size()) display = p.enumEntries[i].fromFirstOccurrenceOf(":", false, false);
        value.setText(display, juce::dontSendNotification);
    }
    else
    {
        const double factor = p.factor == 0.0 ? 1.0 : p.factor;
        const auto display = p.value.toString().getDoubleValue() / factor;
        ensureOpenEndedRangeFor(display);
        slider.setValue(display, juce::dontSendNotification);

        juce::String displayText;
        if (p.kind == ParameterKind::integer && factor == 1.0)
        {
            displayText = juce::String((juce::int64) std::llround(display));
        }
        else
        {
            displayText = juce::String(display, 3);
            if (displayText.containsChar('.'))
                displayText = displayText.trimCharactersAtEnd("0").trimCharactersAtEnd(".");
            if (displayText.isEmpty() || displayText == "-") displayText = "0";
        }
        value.setText(displayText, juce::dontSendNotification);
    }
    updating = false;
    meta.setText((p.localPath.isNotEmpty() ? p.localPath : p.path) + "  |  " + ParameterInfo::kindToString(p.kind), juce::dontSendNotification);
    applyControlColour();
    repaint();
}
}
