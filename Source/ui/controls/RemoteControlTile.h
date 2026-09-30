#pragma once
#include <JuceHeader.h>
#include <optional>
#include "../../model/PageModel.h"
#include "../../model/ParameterInfo.h"

namespace er
{
class RemoteControlTile : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    explicit RemoteControlTile(ControlMapping mappingToUse);
    const ControlMapping& mapping() const noexcept { return controlMapping; }
    virtual void refresh(const ParameterInfo&) = 0;
    std::function<void(const juce::String&, const juce::var&)> onWrite;
    std::function<void(const juce::String&)> onRemove;
    std::function<void(const juce::String&, juce::uint32)> onColourChange;
    std::function<void(const juce::String&, const juce::String&)> onLabel2Change;

    void mouseDown(const juce::MouseEvent&) override;

protected:
    void watchForContextMenu(juce::Component&);
    void paintFrame(juce::Graphics&, const ParameterInfo* info) const;
    juce::Colour accentColour() const;
    virtual void applyControlColour() = 0;
    virtual void beginLabel2Edit() = 0;
    void applyPathTooltip();

    ControlMapping controlMapping;
    std::optional<ParameterInfo> lastInfo;
};

class ButtonTile final : public RemoteControlTile
{
public:
    explicit ButtonTile(ControlMapping mappingToUse);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh(const ParameterInfo&) override;
private:
    void applyControlColour() override;
    void beginLabel2Edit() override;

    juce::Label name;
    juce::Label secondaryName;
    juce::TextButton button { "-" };
    juce::Label meta;
};

class ShowContentTile final : public RemoteControlTile
{
public:
    explicit ShowContentTile(ControlMapping mappingToUse);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh(const ParameterInfo&) override;
private:
    void applyControlColour() override;
    void beginLabel2Edit() override;

    juce::Label name;
    juce::Label secondaryName;
    juce::TextButton button { "Show Content" };
    juce::Label meta;
};

class EncoderTile final : public RemoteControlTile
{
public:
    explicit EncoderTile(ControlMapping mappingToUse);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh(const ParameterInfo&) override;
private:
    int enumIndexForValue(const ParameterInfo&, int value) const;
    int enumValueForIndex(const ParameterInfo&, int index) const;
    void configureFor(const ParameterInfo&);
    void applyControlColour() override;
    void beginLabel2Edit() override;
    void ensureOpenEndedRangeFor(double displayValue);

    juce::Label name;
    juce::Label secondaryName;
    juce::Slider slider;
    juce::Label value;
    juce::Label meta;
    bool updating = false;
    ParameterKind configuredKind = ParameterKind::unknown;
    int configuredEnumSize = -1;
    double configuredFactor = 1.0;
    bool configuredHasMinimum = false;
    bool configuredHasMaximum = false;
    double dynamicUpperBound = 100.0;
};
}
