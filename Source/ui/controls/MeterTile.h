#pragma once

#include "RemoteControlTile.h"
#include "../../model/LevelMeterScale.h"

namespace er
{
class LevelMeterComponent final : public juce::Component
{
public:
    void setLevel(double newLevel, bool isAvailable);
    void setAccentColour(juce::Colour newColour);
    double maximum() const noexcept { return upperBound; }
    void paint(juce::Graphics&) override;

private:
    float yForLevel(double value, juce::Rectangle<float> bounds) const noexcept;

    double level = LevelMeterScale::minimum;
    double upperBound = LevelMeterScale::initialMaximum;
    bool available = false;
    juce::Colour accent = juce::Colour(0xff00beff);
};

class MeterTile final : public RemoteControlTile
{
public:
    explicit MeterTile(ControlMapping mappingToUse);
    void setControlLabel2(const juce::String& label2) override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh(const ParameterInfo&) override;

private:
    void applyControlColour() override;
    void beginLabel2Edit() override;

    juce::Label name;
    juce::Label secondaryName;
    LevelMeterComponent meter;
    juce::Label value;
    juce::Label meta;
};
}
