#pragma once
#include "../model/ParameterInfo.h"
#include "../model/PageModel.h"
#include "../model/ProviderConfig.h"

namespace er
{
class CsvService
{
public:
    static bool exportParameters(const juce::File& file, const std::vector<ParameterInfo>& params, juce::String& error);
    static bool importMappings(const juce::File& file, PageSet& pages, juce::String& error);
private:
    static juce::String plainField(juce::String s);
};
}
