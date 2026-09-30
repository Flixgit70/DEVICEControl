#pragma once
#include <JuceHeader.h>
#include "../model/ProviderConfig.h"
#include <vector>

namespace er
{
/** Persistent application settings kept outside MainComponent.

    This service owns host history and provider configuration persistence. It is
    intentionally protocol-neutral; provider-specific settings belong to the
    corresponding provider implementation when needed.
*/
class AppSettingsService
{
public:
    AppSettingsService();

    const std::vector<juce::String>& recentHosts() const noexcept { return hosts; }
    void rememberHost(const juce::String& host);

    std::vector<ProviderConfig> loadProviderConfigs();
    void saveProviderConfigs(const std::vector<ProviderConfig>& configs);

private:
    void loadHostHistory();

    juce::ApplicationProperties properties;
    std::vector<juce::String> hosts;
};
}
