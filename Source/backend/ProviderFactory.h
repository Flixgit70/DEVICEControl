#pragma once
#include "IProviderBackend.h"
#include "../model/ProviderConfig.h"
#include <memory>

namespace er
{
struct ProviderFactory
{
    static std::unique_ptr<IProviderBackend> create(const ProviderConfig& config);
    static juce::String normaliseDriver(juce::String driver);
};
}
