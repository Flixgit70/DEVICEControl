#pragma once
#include <JuceHeader.h>
#include "../model/ParameterStore.h"
#include "../model/ControlTreeModel.h"
#include <vector>

namespace er
{
/** Low-level contract implemented by EmberClient.

    This interface is Ember-specific and intentionally sits below EmberProvider.
    Application/UI code must use IProviderBackend/ProviderManager instead.
*/
class IEmberClientBackend
{
public:
    virtual ~IEmberClientBackend() = default;

    virtual bool connectTo(const juce::String& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool requestDirectory(const juce::String& numericPath) = 0;
    virtual void startSubtreeScan(const std::vector<juce::String>& numericRoots) = 0;
    virtual bool setParameterValue(const juce::String& numericPath, const juce::var& newValue) = 0;

    virtual bool isConnected() const noexcept = 0;
    virtual bool isScanning() const noexcept = 0;
    virtual juce::String statusText() const = 0;
    virtual juce::String endpointText() const = 0;
    virtual int discoveredParameterCount() const = 0;
    virtual ParameterStore& parameters() noexcept = 0;
    virtual const ParameterStore& parameters() const noexcept = 0;
    virtual ControlTreeModel& tree() noexcept = 0;
    virtual const ControlTreeModel& tree() const noexcept = 0;
};
}
