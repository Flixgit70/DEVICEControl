#pragma once
#include <JuceHeader.h>
#include "../model/ParameterInfo.h"
#include "../model/ControlTreeModel.h"
#include <optional>
#include <vector>
#include <cstdint>

namespace er
{
/** Protocol-neutral contract for one remote control provider.

    Paths passed to this interface are provider-local paths. ProviderManager is
    responsible for adding/removing stable provider IDs at the application
    boundary. Implementations may use Ember+, HTTP/JSON, NMOS or another
    transport without leaking protocol details into the UI.
*/
class IProviderBackend
{
public:
    virtual ~IProviderBackend() = default;

    virtual juce::String driverId() const = 0;
    virtual bool connectTo(const juce::String& host, int port) = 0;
    virtual void disconnect() = 0;

    virtual bool requestDirectory(const juce::String& localPath) = 0;
    virtual bool refreshDirectory(const juce::String& localPath) = 0;
    virtual void startSubtreeScan(const std::vector<juce::String>& localRoots) = 0;
    virtual bool setParameterValue(const juce::String& localPath, const juce::var& value) = 0;
    virtual bool setMatrixCrosspoint(const juce::String& localPath, int target, int source,
                                     bool connect) = 0;

    virtual bool isConnected() const noexcept = 0;
    virtual bool isScanning() const noexcept = 0;
    virtual juce::String statusText() const = 0;
    virtual int discoveredParameterCount() const = 0;

    virtual std::vector<TreeEntry> treeSnapshot() const = 0;
    virtual std::optional<TreeEntry> findTreeEntry(const juce::String& localPath) const = 0;
    virtual bool isDirectoryLoaded(const juce::String& localPath) const = 0;
    virtual bool isDirectoryLoading(const juce::String& localPath) const = 0;
    virtual std::uint64_t treeRevision() const = 0;

    virtual std::vector<ParameterInfo> parameterSnapshot() const = 0;
    virtual std::optional<ParameterInfo> findParameter(const juce::String& localPath) const = 0;
    virtual std::uint64_t parameterRevision() const = 0;
    virtual std::uint64_t parameterStructureRevision() const = 0;
};
}
