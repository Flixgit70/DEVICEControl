#pragma once
#include "../backend/ProviderManager.h"
#include "../model/PageModel.h"

namespace er
{
/** Coordinates provider parameters with page mappings without depending on UI widgets. */
class MappingController
{
public:
    MappingController(ProviderManager& providersToUse, PageSet& pagesToUse)
        : providers(providersToUse), pages(pagesToUse) {}

    int addControlsToPage(const std::vector<juce::String>& paths, ControlType type, int pageIndex);
    std::vector<ParameterInfo> parametersUnderRoots(const std::vector<juce::String>& roots) const;
    bool qualifyLegacyImportedMappings(PageSet& importedPages, juce::String& error) const;

private:
    ProviderManager& providers;
    PageSet& pages;
};
}
