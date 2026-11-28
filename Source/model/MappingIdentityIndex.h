#pragma once
#include "ParameterInfo.h"
#include "ControlTreeModel.h"
#include "ProviderConfig.h"
#include <map>
#include <set>

namespace er
{
// UI-thread index of identities, rebuilt only when names/topology change.
// Empty values deliberately represent ambiguous names and cannot resolve.
class MappingIdentityIndex
{
public:
    void rebuild(const std::vector<ParameterInfo>& parameters,
                 const std::vector<TreeEntry>& tree)
    {
        parameterPaths.clear();
        matrixPaths.clear();
        matrixNames.clear();
        for (const auto& parameter : parameters)
            add(parameterPaths, parameter.providerId,
                parameter.treePath.replace(" / ", "/"), parameter.path);

        std::map<juce::String, const TreeEntry*> entries;
        for (const auto& entry : tree) entries.emplace(entry.path, &entry);
        for (const auto& entry : tree)
        {
            if (entry.kind != TreeEntryKind::matrix) continue;
            juce::StringArray parts;
            std::set<juce::String> visited;
            const auto* current = &entry;
            bool complete = true;
            while (current->parentPath.isNotEmpty())
            {
                if (!visited.insert(current->path).second) { complete = false; break; }
                parts.insert(0, current->displayName());
                const auto parent = entries.find(current->parentPath);
                if (parent == entries.end()) { complete = false; break; }
                current = parent->second;
            }
            if (!complete || parts.isEmpty()) continue;
            const auto name = parts.joinIntoString("/");
            matrixNames.emplace(entry.path, name);
            add(matrixPaths, ProviderPath::providerId(entry.path), name, entry.path);
        }
    }

    juce::String parameterPath(const juce::String& providerId, const juce::String& name) const
    {
        return find(parameterPaths, providerId, name);
    }

    juce::String matrixPath(const juce::String& providerId, const juce::String& name) const
    {
        return find(matrixPaths, providerId, name);
    }

    juce::String matrixName(const juce::String& path) const
    {
        const auto found = matrixNames.find(path);
        return found != matrixNames.end() ? found->second : juce::String();
    }

private:
    using Key = std::pair<juce::String, juce::String>;
    using Paths = std::map<Key, juce::String>;
    static void add(Paths& paths, const juce::String& providerId,
                    const juce::String& name, const juce::String& path)
    {
        if (name.isEmpty()) return;
        const auto inserted = paths.emplace(Key { providerId, name }, path);
        if (!inserted.second && inserted.first->second != path)
            inserted.first->second.clear();
    }
    static juce::String find(const Paths& paths, const juce::String& providerId,
                             const juce::String& name)
    {
        const auto found = paths.find(Key { providerId, name });
        return found != paths.end() ? found->second : juce::String();
    }
    Paths parameterPaths, matrixPaths;
    std::map<juce::String, juce::String> matrixNames;
};
}
