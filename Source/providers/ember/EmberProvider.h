#pragma once
#include "../../backend/IProviderBackend.h"
#include "../../ember/EmberClient.h"

namespace er
{
/** IProviderBackend adapter for the existing, stable Ember+ client. */
class EmberProvider final : public IProviderBackend
{
public:
    juce::String driverId() const override { return "ember"; }
    bool connectTo(const juce::String& host, int port) override { return client.connectTo(host, port); }
    void disconnect() override { client.disconnect(); }

    bool requestDirectory(const juce::String& localPath) override { return client.requestDirectory(localPath); }
    void startSubtreeScan(const std::vector<juce::String>& localRoots) override { client.startSubtreeScan(localRoots); }
    bool setParameterValue(const juce::String& localPath, const juce::var& value) override
    {
        return client.setParameterValue(localPath, value);
    }

    bool isConnected() const noexcept override { return client.isConnected(); }
    bool isScanning() const noexcept override { return client.isScanning(); }
    juce::String statusText() const override { return client.statusText(); }
    int discoveredParameterCount() const override { return client.discoveredParameterCount(); }

    std::vector<TreeEntry> treeSnapshot() const override { return client.tree().snapshot(); }
    bool isDirectoryLoaded(const juce::String& localPath) const override { return client.tree().isDirectoryLoaded(localPath); }
    bool isDirectoryLoading(const juce::String& localPath) const override { return client.tree().isDirectoryLoading(localPath); }
    std::uint64_t treeRevision() const override { return client.tree().revision(); }

    std::vector<ParameterInfo> parameterSnapshot() const override { return client.parameters().snapshot(); }
    std::optional<ParameterInfo> findParameter(const juce::String& localPath) const override
    {
        return client.parameters().find(localPath);
    }
    std::uint64_t parameterRevision() const override { return client.parameters().revision(); }
    std::uint64_t parameterStructureRevision() const override { return client.parameters().structureRevision(); }

private:
    EmberClient client;
};
}
