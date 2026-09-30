#pragma once
#include <JuceHeader.h>
#include <optional>
#include "../../model/PageModel.h"
#include "../../model/ParameterInfo.h"
#include "ContentTextButton.h"

namespace er
{
enum class TileSelectionGesture
{
    replace,
    toggle,
    range,
    addRange
};

class SelectionTextButton final : public juce::TextButton
{
public:
    explicit SelectionTextButton(const juce::String& text = {}) : juce::TextButton(text) {}
    std::function<bool(const juce::MouseEvent&)> selectionMouseDown;

    void mouseDown(const juce::MouseEvent& event) override
    {
        selectionGesture = selectionMouseDown && selectionMouseDown(event);
        if (!selectionGesture)
            juce::TextButton::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!selectionGesture)
            juce::TextButton::mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (selectionGesture)
        {
            selectionGesture = false;
            return;
        }
        juce::TextButton::mouseUp(event);
    }

private:
    bool selectionGesture = false;
};

class SelectionSlider final : public juce::Slider
{
public:
    std::function<bool(const juce::MouseEvent&)> selectionMouseDown;

    void mouseDown(const juce::MouseEvent& event) override
    {
        selectionGesture = selectionMouseDown && selectionMouseDown(event);
        if (!selectionGesture)
            juce::Slider::mouseDown(event);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!selectionGesture)
            juce::Slider::mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (selectionGesture)
        {
            selectionGesture = false;
            return;
        }
        juce::Slider::mouseUp(event);
    }

private:
    bool selectionGesture = false;
};

class RemoteControlTile : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    explicit RemoteControlTile(ControlMapping mappingToUse);
    const ControlMapping& mapping() const noexcept { return controlMapping; }
    virtual void refresh(const ParameterInfo&) = 0;
    virtual void setControlLabel2(const juce::String& label2) = 0;
    virtual void setTileSelected(bool selected);
    void setControlColour(juce::uint32 colourArgb);
    bool isTileSelected() const noexcept { return tileSelected; }
    std::function<void(const juce::String&, const juce::var&)> onWrite;
    std::function<void(const juce::String&)> onRemove;
    std::function<void(const juce::String&, juce::uint32)> onColourChange;
    std::function<void(const juce::String&, const juce::String&)> onLabel2Change;
    std::function<void(const juce::String&, int, int, int, int)> onSizeChange;
    std::function<void(const juce::String&, bool)> onSelectionChange;
    std::function<void(const juce::String&, TileSelectionGesture)> onSelectionGesture;

    void mouseDown(const juce::MouseEvent&) override;

protected:
    void watchForContextMenu(juce::Component&);
    void paintFrame(juce::Graphics&, const ParameterInfo* info) const;
    juce::Colour accentColour() const;
    virtual void applyControlColour() = 0;
    virtual void beginLabel2Edit() = 0;
    void applyPathTooltip();
    bool requestSelection(const juce::ModifierKeys&, bool allowPlainSelection);

    ControlMapping controlMapping;
    std::optional<ParameterInfo> lastInfo;
    bool tileSelected = false;
};

class ButtonTile final : public RemoteControlTile
{
public:
    explicit ButtonTile(ControlMapping mappingToUse);
    void setControlLabel2(const juce::String& label2) override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh(const ParameterInfo&) override;
private:
    void applyControlColour() override;
    void beginLabel2Edit() override;

    juce::Label name;
    juce::Label secondaryName;
    SelectionTextButton button { "-" };
    juce::Label meta;
};

class ShowContentTile final : public RemoteControlTile
{
public:
    explicit ShowContentTile(ControlMapping mappingToUse);
    void setControlLabel2(const juce::String& label2) override;
    void setTileSelected(bool selected) override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh(const ParameterInfo&) override;
private:
    void applyControlColour() override;
    void beginLabel2Edit() override;

    juce::Label name;
    juce::Label secondaryName;
    juce::ToggleButton selectionToggle;
    ContentTextButton button;
    juce::Label meta;
    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::ResizableCornerComponent resizeCorner;
};

class EncoderTile final : public RemoteControlTile
{
public:
    explicit EncoderTile(ControlMapping mappingToUse);
    void setControlLabel2(const juce::String& label2) override;
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
    SelectionSlider slider;
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
