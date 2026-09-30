#pragma once
#include "ParameterInfo.h"
#include <map>
#include <atomic>
#include <optional>
#include <cstdint>

namespace er
{
class ParameterStore
{
public:
    void clear();
    void upsert(const ParameterInfo& info);
    std::vector<ParameterInfo> snapshot() const;
    std::optional<ParameterInfo> find(const juce::String& path) const;
    int size() const;
    std::uint64_t revision() const noexcept { return changeRevision.load(); }
    std::uint64_t structureRevision() const noexcept { return treeRevision.load(); }

private:
    mutable juce::CriticalSection lock;
    std::map<juce::String, ParameterInfo> parameters;
    std::atomic<std::uint64_t> changeRevision { 0 };
    std::atomic<std::uint64_t> treeRevision { 0 };
};
}
