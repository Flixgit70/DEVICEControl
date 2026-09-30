#include "MeterTile.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../app/Theme.h"

namespace er
{
namespace
{
juce::String formatScaleValue(double value)
{
    const auto rounded = juce::roundToInt(value);
    return value > 0.0 ? "+" + juce::String(rounded) : juce::String(rounded);
}
}

void LevelMeterComponent::setLevel(double newLevel, bool isAvailable)
{
    available = isAvailable && std::isfinite(newLevel);
    if (available)
    {
        level = newLevel;
        upperBound = LevelMeterScale::extendMaximum(upperBound, level);
    }
    repaint();
}

void LevelMeterComponent::setAccentColour(juce::Colour newColour)
{
    accent = newColour;
    repaint();
}

float LevelMeterComponent::yForLevel(double value, juce::Rectangle<float> bounds) const noexcept
{
    return bounds.getBottom()
         - (float) LevelMeterScale::normalised(value, upperBound) * bounds.getHeight();
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    auto scaleArea = bounds.removeFromRight(30.0f);
    auto bar = bounds.reduced(5.0f, 2.0f);

    g.setColour(Theme::background.darker(0.25f));
    g.fillRoundedRectangle(bar, 3.0f);

    if (available)
    {
        const auto fillTop = yForLevel(level, bar);
        g.saveState();
        g.reduceClipRegion(bar.withTop(juce::jlimit(bar.getY(), bar.getBottom(), fillTop)).toNearestInt());

        // PPMs deliberately use one fixed colour across the complete scale.
        // The control/tile colour must not alter the meter indication.
        g.setColour(Theme::yellow);
        g.fillRect(bar);
        g.restoreState();
    }

    g.setColour(Theme::outline);
    g.drawRoundedRectangle(bar, 3.0f, 1.0f);

    std::vector<double> ticks { LevelMeterScale::minimum, -60.0, -40.0, -20.0, -10.0, 0.0, 5.0, upperBound };
    std::sort(ticks.begin(), ticks.end());
    ticks.erase(std::unique(ticks.begin(), ticks.end()), ticks.end());

    g.setFont(juce::FontOptions(8.5f));
    for (const auto tick : ticks)
    {
        if (tick < LevelMeterScale::minimum || tick > upperBound)
            continue;

        const auto y = yForLevel(tick, bar);
        const int lineY = juce::jlimit(juce::roundToInt(bar.getY()),
                                       juce::roundToInt(bar.getBottom()) - 1,
                                       juce::roundToInt(y));
        const int labelY = juce::jlimit(juce::roundToInt(scaleArea.getY()),
                                        juce::roundToInt(scaleArea.getBottom()) - 12,
                                        lineY - 6);
        g.setColour(Theme::outline.withAlpha(0.8f));
        g.drawHorizontalLine(lineY, bar.getX(), bar.getRight());
        g.setColour(Theme::textDim);
        g.drawText(formatScaleValue(tick), scaleArea.getX(), labelY,
                   scaleArea.getWidth(), 12, juce::Justification::centredRight, false);
    }

    if (!available)
    {
        g.setColour(Theme::textDim);
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawText("N/A", bar, juce::Justification::centred);
    }
}

MeterTile::MeterTile(ControlMapping mappingToUse)
    : RemoteControlTile(std::move(mappingToUse))
{
    name.setText(controlMapping.label.isNotEmpty() ? controlMapping.label : controlMapping.parameterPath,
                 juce::dontSendNotification);
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
        if (onLabel2Change)
            onLabel2Change(controlMapping.parameterPath, controlMapping.label2);
    };
    addAndMakeVisible(secondaryName);

    addAndMakeVisible(meter);

    value.setJustificationType(juce::Justification::centred);
    value.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    value.setColour(juce::Label::textColourId, Theme::textBright);
    addAndMakeVisible(value);

    meta.setJustificationType(juce::Justification::centred);
    meta.setFont(juce::FontOptions(8.0f));
    meta.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(meta);

    watchForContextMenu(name);
    watchForContextMenu(secondaryName);
    watchForContextMenu(meter);
    watchForContextMenu(value);
    watchForContextMenu(meta);
    applyControlColour();
    applyPathTooltip();
}

void MeterTile::applyControlColour()
{
    meter.setAccentColour(accentColour());
}

void MeterTile::beginLabel2Edit()
{
    secondaryName.showEditor();
}

void MeterTile::setControlLabel2(const juce::String& label2)
{
    controlMapping.label2 = label2;
    secondaryName.setText(label2, juce::dontSendNotification);
}

void MeterTile::paint(juce::Graphics& g)
{
    paintFrame(g, lastInfo ? &*lastInfo : nullptr);
}

void MeterTile::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    name.setBounds(bounds.removeFromTop(17));
    secondaryName.setBounds(bounds.removeFromTop(14));
    meta.setBounds(bounds.removeFromBottom(12));
    value.setBounds(bounds.removeFromBottom(18));
    meter.setBounds(bounds.reduced(4, 2));
}

void MeterTile::refresh(const ParameterInfo& info)
{
    lastInfo = info;
    const bool numeric = info.kind == ParameterKind::integer || info.kind == ParameterKind::real;
    const double factor = info.factor == 0.0 ? 1.0 : info.factor;
    const double displayValue = numeric ? info.value.toString().getDoubleValue() / factor
                                        : LevelMeterScale::minimum;
    const bool available = info.online && numeric && !info.value.isVoid() && std::isfinite(displayValue);

    meter.setLevel(displayValue, available);
    value.setText(available ? info.formattedValue() + " dB" : juce::String("-"),
                  juce::dontSendNotification);
    meta.setText((info.localPath.isNotEmpty() ? info.localPath : info.path)
                     + "  |  " + ParameterInfo::kindToString(info.kind),
                 juce::dontSendNotification);
    applyControlColour();
    repaint();
}
}
