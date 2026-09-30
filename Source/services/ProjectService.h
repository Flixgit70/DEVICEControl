#pragma once
#include "../model/PageModel.h"
#include "../model/ProviderConfig.h"
#include <vector>

namespace er
{
class ProjectService
{
public:
    static bool save(const juce::File&, const PageSet&, const std::vector<ProviderConfig>& providers, juce::String& error);
    static bool load(const juce::File&, PageSet&, std::vector<ProviderConfig>& providers, juce::String& error);

    // Legacy single-provider overloads retained for source/test compatibility.
    static bool save(const juce::File&, const PageSet&, const juce::String& host, int port, juce::String& error);
    static bool load(const juce::File&, PageSet&, juce::String& host, int& port, juce::String& error);
};
}
