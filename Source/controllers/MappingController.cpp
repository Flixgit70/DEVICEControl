#include "MappingController.h"
#include "../model/ControlTreeModel.h"
#include <algorithm>

namespace er
{
int MappingController::addControlsToPage(const std::vector<juce::String>& paths, ControlType type, int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= (int) pages.pages().size()) return 0;
    auto& page = pages.pages()[(size_t) pageIndex];
    int added = 0;

    for (const auto& path : paths)
    {
        auto info = providers.findParameter(path);
        if (!info) continue;

        auto existing = std::find_if(page.controls.begin(), page.controls.end(), [&] (const ControlMapping& m)
        {
            return m.parameterPath == path;
        });

        ControlMapping mapping;
        mapping.parameterPath = path;
        mapping.type = type;
        if (existing != page.controls.end())
        {
            mapping.label = existing->label;
            mapping.label2 = existing->label2;
            mapping.colourArgb = existing->colourArgb;
            mapping.tileWidth = existing->tileWidth;
            mapping.tileHeight = existing->tileHeight;
            mapping.orangeAboveThresholdEnabled = existing->orangeAboveThresholdEnabled;
            mapping.orangeAboveThreshold = existing->orangeAboveThreshold;
            mapping.redAboveThresholdEnabled = existing->redAboveThresholdEnabled;
            mapping.redAboveThreshold = existing->redAboveThreshold;
            mapping.orangeBelowThresholdEnabled = existing->orangeBelowThresholdEnabled;
            mapping.orangeBelowThreshold = existing->orangeBelowThreshold;
            mapping.redBelowThresholdEnabled = existing->redBelowThresholdEnabled;
            mapping.redBelowThreshold = existing->redBelowThreshold;
        }
        else
        {
            mapping.label = info->fullTreeLabel();
            if (info->description.isNotEmpty() && info->description != mapping.label)
                mapping.label2 = info->description;
        }

        if (existing == page.controls.end()) page.controls.push_back(std::move(mapping));
        else *existing = std::move(mapping);
        ++added;
    }
    return added;
}

std::vector<ParameterInfo> MappingController::parametersUnderRoots(const std::vector<juce::String>& roots) const
{
    std::vector<ParameterInfo> out;
    for (const auto& parameter : providers.parameterSnapshot())
    {
        for (const auto& root : roots)
        {
            if (ControlTreeModel::isSameOrDescendant(parameter.path, root))
            {
                out.push_back(parameter);
                break;
            }
        }
    }

    std::sort(out.begin(), out.end(), [] (const ParameterInfo& a, const ParameterInfo& b)
    {
        const auto providerCmp = a.providerName.compareNatural(b.providerName);
        return providerCmp != 0 ? providerCmp < 0 : a.treePath.compareNatural(b.treePath) < 0;
    });
    return out;
}

bool MappingController::qualifyLegacyImportedMappings(PageSet& importedPages, juce::String& error) const
{
    bool haveLegacyPath = false;
    for (const auto& page : importedPages.pages())
        for (const auto& control : page.controls)
            if (!ProviderPath::isQualified(control.parameterPath)) haveLegacyPath = true;
    if (!haveLegacyPath) return true;

    const auto configs = providers.providerConfigs();
    if (configs.size() != 1)
    {
        error = "This CSV contains legacy paths without provider_id. With multiple providers configured, add the provider_id column or import it in a project with exactly one provider.";
        return false;
    }

    for (auto& page : importedPages.pages())
        for (auto& control : page.controls)
            if (!ProviderPath::isQualified(control.parameterPath))
                control.parameterPath = ProviderPath::qualify(configs.front().id, control.parameterPath);
    return true;
}
}
