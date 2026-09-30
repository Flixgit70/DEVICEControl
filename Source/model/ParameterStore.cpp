#include "ParameterStore.h"
namespace er
{
void ParameterStore::clear()
{
    const juce::ScopedLock sl(lock);
    parameters.clear();
    ++changeRevision;
    ++treeRevision;
}
void ParameterStore::upsert(const ParameterInfo& info)
{
    const juce::ScopedLock sl(lock);
    bool structureChanged = false;
    if (auto it = parameters.find(info.path); it == parameters.end())
        structureChanged = true;
    else
    {
        const auto& old = it->second;
        structureChanged = old.identifier != info.identifier
                        || old.description != info.description
                        || old.treePath != info.treePath
                        || old.kind != info.kind
                        || old.writable != info.writable
                        || old.enumEntries.joinIntoString("\n") != info.enumEntries.joinIntoString("\n");
    }
    parameters[info.path] = info;
    ++changeRevision;
    if (structureChanged) ++treeRevision;
}
std::vector<ParameterInfo> ParameterStore::snapshot() const
{
    const juce::ScopedLock sl(lock);
    std::vector<ParameterInfo> out; out.reserve(parameters.size());
    for (const auto& [_, p] : parameters) out.push_back(p);
    return out;
}
std::optional<ParameterInfo> ParameterStore::find(const juce::String& path) const
{
    const juce::ScopedLock sl(lock);
    if (auto it = parameters.find(path); it != parameters.end()) return it->second;
    return std::nullopt;
}
int ParameterStore::size() const { const juce::ScopedLock sl(lock); return (int) parameters.size(); }
}
