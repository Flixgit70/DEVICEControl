#pragma once
#include <JuceHeader.h>

namespace er
{
struct ProviderConfig
{
    juce::String id;
    juce::String name;
    juce::String driver { "ember" };
    juce::String host;
    int port = 9000;
    bool enabled = true;
    bool autoConnect = false;

    static ProviderConfig createDefault(const juce::String& displayName = "Provider 1",
                                        const juce::String& initialHost = "127.0.0.1",
                                        int initialPort = 9000)
    {
        ProviderConfig c;
        c.id = juce::Uuid().toString();
        c.name = displayName;
        c.driver = "ember";
        c.host = initialHost;
        c.port = initialPort > 0 ? initialPort : 9000;
        return c;
    }
};

/** Internal provider-aware tree/parameter path helper.

    The GUI and page mappings use a single qualified path string so all existing
    tree/filter/page code can stay simple while several providers may expose the
    same provider-local path. A provider UUID never contains '.', so the local
    dot hierarchy remains usable below the synthetic provider root.

      @<provider-id>          provider root
      @<provider-id>.1.2.3    provider-local path 1.2.3
*/
struct ProviderPath
{
    static juce::String root(const juce::String& providerId)
    {
        return providerId.isEmpty() ? juce::String() : "@" + providerId;
    }

    static juce::String qualify(const juce::String& providerId, const juce::String& localPath)
    {
        const auto r = root(providerId);
        if (r.isEmpty()) return localPath;
        const auto local = localPath.trimCharactersAtStart(".");
        return local.isEmpty() ? r : r + "." + local;
    }

    static bool isQualified(const juce::String& path)
    {
        return path.startsWithChar('@');
    }

    static juce::String providerId(const juce::String& qualifiedPath)
    {
        if (!isQualified(qualifiedPath)) return {};
        const auto dot = qualifiedPath.indexOfChar('.');
        return dot < 0 ? qualifiedPath.substring(1) : qualifiedPath.substring(1, dot);
    }

    static juce::String localPath(const juce::String& qualifiedPath)
    {
        if (!isQualified(qualifiedPath)) return qualifiedPath;
        const auto dot = qualifiedPath.indexOfChar('.');
        return dot < 0 ? juce::String() : qualifiedPath.substring(dot + 1);
    }

    static bool isProviderRoot(const juce::String& path)
    {
        return isQualified(path) && path.indexOfChar('.') < 0;
    }
};
}
