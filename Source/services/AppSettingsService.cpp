#include "AppSettingsService.h"
#include <algorithm>

namespace er
{
AppSettingsService::AppSettingsService()
{
    juce::PropertiesFile::Options options;
    // Keep the legacy settings key so upgrading to DEVICEControl preserves
    // existing providers, recent hosts and user state.
    options.applicationName = "EmberRemote";
    options.filenameSuffix = "settings";
    options.folderName = "EmberRemote";
    options.osxLibrarySubFolder = "Application Support";
    properties.setStorageParameters(options);
    loadHostHistory();
}

void AppSettingsService::loadHostHistory()
{
    hosts.clear();
    if (auto* settings = properties.getUserSettings())
    {
        juce::StringArray stored;
        stored.addLines(settings->getValue("recentHosts"));
        stored.trim();
        stored.removeEmptyStrings();
        for (const auto& host : stored)
            if (std::find(hosts.begin(), hosts.end(), host) == hosts.end())
                hosts.push_back(host);
    }
}

void AppSettingsService::rememberHost(const juce::String& hostToRemember)
{
    const auto host = hostToRemember.trim();
    if (host.isEmpty()) return;

    hosts.erase(std::remove(hosts.begin(), hosts.end(), host), hosts.end());
    hosts.insert(hosts.begin(), host);
    constexpr size_t maxRecentHosts = 20;
    if (hosts.size() > maxRecentHosts) hosts.resize(maxRecentHosts);

    if (auto* settings = properties.getUserSettings())
    {
        juce::StringArray lines;
        for (const auto& recent : hosts) lines.add(recent);
        settings->setValue("recentHosts", lines.joinIntoString("\n"));
        settings->saveIfNeeded();
    }
}

std::vector<ProviderConfig> AppSettingsService::loadProviderConfigs()
{
    std::vector<ProviderConfig> configs;
    if (auto* settings = properties.getUserSettings())
    {
        const auto value = juce::JSON::parse(settings->getValue("providersJson"));
        if (value.isArray())
        {
            for (const auto& item : *value.getArray())
            {
                auto* o = item.getDynamicObject();
                if (!o) continue;

                ProviderConfig c;
                c.id = o->getProperty("id").toString();
                if (c.id.isEmpty()) c.id = juce::Uuid().toString();
                c.name = o->getProperty("name").toString();
                c.driver = o->hasProperty("driver") ? o->getProperty("driver").toString() : juce::String("ember");
                if (c.driver.isEmpty()) c.driver = "ember";
                c.host = o->getProperty("host").toString();
                c.port = (int) o->getProperty("port");
                if (c.port <= 0) c.port = 9000;
                c.enabled = o->hasProperty("enabled") ? (bool) o->getProperty("enabled") : true;
                c.autoConnect = o->hasProperty("autoConnect") ? (bool) o->getProperty("autoConnect") : false;
                configs.push_back(std::move(c));
            }
        }
    }

    if (configs.empty())
    {
        const auto host = hosts.empty() ? juce::String("127.0.0.1") : hosts.front();
        configs.push_back(ProviderConfig::createDefault("Provider 1", host, 9000));
    }
    return configs;
}

void AppSettingsService::saveProviderConfigs(const std::vector<ProviderConfig>& configs)
{
    if (auto* settings = properties.getUserSettings())
    {
        juce::Array<juce::var> array;
        for (const auto& cfg : configs)
        {
            auto o = std::make_unique<juce::DynamicObject>();
            o->setProperty("id", cfg.id);
            o->setProperty("name", cfg.name);
            o->setProperty("driver", cfg.driver.isEmpty() ? juce::String("ember") : cfg.driver);
            o->setProperty("host", cfg.host);
            o->setProperty("port", cfg.port);
            o->setProperty("enabled", cfg.enabled);
            o->setProperty("autoConnect", cfg.autoConnect);
            array.add(juce::var(o.release()));
        }
        settings->setValue("providersJson", juce::JSON::toString(juce::var(array), false));
        settings->saveIfNeeded();
    }
}
}
