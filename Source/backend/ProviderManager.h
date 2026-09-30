#pragma once
#include <JuceHeader.h>
#include "IProviderBackend.h"
#include "../model/ProviderConfig.h"
#include <memory>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace er
{
/** Protocol-neutral owner/router for all configured providers.

    Each session owns exactly one IProviderBackend implementation. The UI only
    deals with provider-qualified paths and never needs to know whether the
    underlying provider uses Ember+, HTTP/JSON, NMOS or another protocol.
*/
class ProviderManager
{
public:
    ProviderManager() = default;
    ~ProviderManager();

    void setProviders(const std::vector<ProviderConfig>& configs);
    std::vector<ProviderConfig> providerConfigs() const;
    ProviderConfig addProvider();
    bool updateProvider(const ProviderConfig& config);
    bool removeProvider(const juce::String& providerId);

    bool connectProvider(const juce::String& providerId);
    void disconnectProvider(const juce::String& providerId);
    int connectEnabled(bool autoOnly = false);
    void disconnectAll();

    bool requestDirectory(const juce::String& qualifiedPath);
    bool refreshDirectory(const juce::String& qualifiedPath);
    void startSubtreeScan(const std::vector<juce::String>& qualifiedRoots);
    bool setParameterValue(const juce::String& qualifiedPath, const juce::var& value);
    bool setMatrixCrosspoint(const juce::String& qualifiedPath, int target, int source,
                             bool connect);

    bool anyConnected() const;
    bool isProviderConnected(const juce::String& providerId) const;
    bool isScanning() const;
    int connectedCount() const;
    int enabledCount() const;
    int discoveredParameterCount() const;
    juce::String statusText() const;
    juce::String providerStatus(const juce::String& providerId) const;

    std::vector<TreeEntry> treeSnapshot() const;
    std::optional<TreeEntry> findTreeEntry(const juce::String& qualifiedPath) const;
    std::vector<ParameterInfo> parameterSnapshot() const;
    std::optional<ParameterInfo> findParameter(const juce::String& qualifiedPath) const;

    std::uint64_t treeRevision() const;
    std::uint64_t parameterRevision() const;
    std::uint64_t parameterStructureRevision() const;

    juce::String firstEnabledProviderId() const;
    juce::String resolveProviderIdByName(const juce::String& name) const;

private:
    struct Session
    {
        ProviderConfig config;
        std::unique_ptr<IProviderBackend> backend;
    };

    Session* findSession(const juce::String& providerId);
    const Session* findSession(const juce::String& providerId) const;
    static ParameterInfo qualifyParameter(const Session&, ParameterInfo);
    static std::uint64_t combine(std::uint64_t seed, std::uint64_t value);

    std::vector<std::unique_ptr<Session>> sessions;
    std::uint64_t configRevision = 1;
};
}
