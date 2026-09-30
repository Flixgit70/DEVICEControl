#include "MappingDraft.h"
#include "ParameterStore.h"
#include <algorithm>
#include <map>

namespace er
{
void MappingDraft::clear() { rowList.clear(); }

void MappingDraft::addParameters(const std::vector<ParameterInfo>& params, int defaultPage)
{
    for (const auto& p : params)
    {
        if (p.path.isEmpty()) continue;
        const auto found = std::find_if(rowList.begin(), rowList.end(), [&] (const auto& r) { return r.path == p.path; });
        if (found != rowList.end()) continue;

        MappingDraftRow r;
        r.path = p.path;
        r.provider = p.providerName;
        r.label = p.fullTreeLabel();
        if (p.description.isNotEmpty() && p.description != r.label)
            r.label2 = p.description;
        r.kind = p.kind;
        r.writable = p.writable;
        r.page = juce::jmax(1, defaultPage);
        if (p.kind == ParameterKind::stringValue)
        {
            // String parameters are useful as read-only status/content fields,
            // so they may be placed on pages even when the provider does not
            // allow writes.
            r.enabled = p.online;
            r.type = ControlType::showContent;
        }
        else
        {
            r.enabled = p.writable && p.online && p.kind != ParameterKind::unknown;
            r.type = (p.kind == ParameterKind::booleanValue || p.kind == ParameterKind::trigger)
                ? ControlType::button : ControlType::encoder;
        }
        rowList.push_back(std::move(r));
    }
}

void MappingDraft::replaceFromPages(const PageSet& pages, const ParameterStore* store)
{
    rowList.clear();
    for (size_t pageIndex = 0; pageIndex < pages.pages().size(); ++pageIndex)
    {
        for (const auto& control : pages.pages()[pageIndex].controls)
        {
            const auto existing = std::find_if(rowList.begin(), rowList.end(), [&] (const MappingDraftRow& row)
            {
                return row.path == control.parameterPath;
            });
            if (existing != rowList.end())
                continue;

            MappingDraftRow row;
            row.enabled = true;
            row.path = control.parameterPath;
            row.provider = ProviderPath::providerId(control.parameterPath);
            row.label = control.label;
            row.label2 = control.label2;
            row.type = control.type;
            row.page = (int) pageIndex + 1;
            row.colourArgb = control.colourArgb;
            row.orangeAboveThresholdEnabled = control.orangeAboveThresholdEnabled;
            row.orangeAboveThreshold = control.orangeAboveThreshold;
            row.redAboveThresholdEnabled = control.redAboveThresholdEnabled;
            row.redAboveThreshold = control.redAboveThreshold;
            row.orangeBelowThresholdEnabled = control.orangeBelowThresholdEnabled;
            row.orangeBelowThreshold = control.orangeBelowThreshold;
            row.redBelowThresholdEnabled = control.redBelowThresholdEnabled;
            row.redBelowThreshold = control.redBelowThreshold;
            if (store != nullptr)
            {
                if (const auto info = store->find(control.parameterPath))
                {
                    row.kind = info->kind;
                    row.writable = info->writable;
                }
            }
            rowList.push_back(std::move(row));
        }
    }
}

void MappingDraft::replaceFromPages(const PageSet& pages, std::function<std::optional<ParameterInfo>(const juce::String&)> resolver)
{
    rowList.clear();
    for (size_t pageIndex = 0; pageIndex < pages.pages().size(); ++pageIndex)
    {
        for (const auto& control : pages.pages()[pageIndex].controls)
        {
            const auto existing = std::find_if(rowList.begin(), rowList.end(), [&] (const MappingDraftRow& row)
            {
                return row.path == control.parameterPath;
            });
            if (existing != rowList.end()) continue;

            MappingDraftRow row;
            row.enabled = true;
            row.path = control.parameterPath;
            row.provider = ProviderPath::providerId(control.parameterPath);
            row.label = control.label;
            row.label2 = control.label2;
            row.type = control.type;
            row.page = (int) pageIndex + 1;
            row.colourArgb = control.colourArgb;
            row.orangeAboveThresholdEnabled = control.orangeAboveThresholdEnabled;
            row.orangeAboveThreshold = control.orangeAboveThreshold;
            row.redAboveThresholdEnabled = control.redAboveThresholdEnabled;
            row.redAboveThreshold = control.redAboveThreshold;
            row.orangeBelowThresholdEnabled = control.orangeBelowThresholdEnabled;
            row.orangeBelowThreshold = control.orangeBelowThreshold;
            row.redBelowThresholdEnabled = control.redBelowThresholdEnabled;
            row.redBelowThreshold = control.redBelowThreshold;
            if (resolver)
            {
                if (const auto info = resolver(control.parameterPath))
                {
                    row.provider = info->providerName;
                    row.kind = info->kind;
                    row.writable = info->writable;
                }
            }
            rowList.push_back(std::move(row));
        }
    }
}

void MappingDraft::setAllEnabled(bool enabled)
{
    for (auto& row : rowList)
        row.enabled = enabled;
}

void MappingDraft::setEnabledForPaths(const std::vector<juce::String>& paths, bool enabled)
{
    for (auto& row : rowList)
        if (std::find(paths.begin(), paths.end(), row.path) != paths.end())
            row.enabled = enabled;
}

bool MappingDraft::allEnabled() const
{
    return !rowList.empty() && std::all_of(rowList.begin(), rowList.end(), [] (const MappingDraftRow& row)
    {
        return row.enabled;
    });
}

void MappingDraft::removeRowsForPage(int page)
{
    rowList.erase(std::remove_if(rowList.begin(), rowList.end(), [page] (const MappingDraftRow& row)
    {
        return row.page == page;
    }), rowList.end());
}

bool MappingDraft::setLabel2ForPath(const juce::String& path, const juce::String& label2)
{
    const auto it = std::find_if(rowList.begin(), rowList.end(), [&] (const MappingDraftRow& row)
    {
        return row.path == path;
    });
    if (it == rowList.end() || it->label2 == label2)
        return false;
    it->label2 = label2;
    return true;
}

bool MappingDraft::setTypeForPath(const juce::String& path, ControlType type)
{
    const auto it = std::find_if(rowList.begin(), rowList.end(), [&] (const MappingDraftRow& row)
    {
        return row.path == path;
    });
    if (it == rowList.end() || it->type == type)
        return false;
    it->type = type;
    it->enabled = true;
    return true;
}

bool MappingDraft::setThresholdsForPath(const juce::String& path,
                                        bool orangeAboveEnabled, double orangeAboveValue,
                                        bool redAboveEnabled, double redAboveValue,
                                        bool orangeBelowEnabled, double orangeBelowValue,
                                        bool redBelowEnabled, double redBelowValue)
{
    const auto it = std::find_if(rowList.begin(), rowList.end(), [&] (const MappingDraftRow& row)
    {
        return row.path == path;
    });
    if (it == rowList.end())
        return false;

    const bool changed = it->orangeAboveThresholdEnabled != orangeAboveEnabled
                      || it->orangeAboveThreshold != orangeAboveValue
                      || it->redAboveThresholdEnabled != redAboveEnabled
                      || it->redAboveThreshold != redAboveValue
                      || it->orangeBelowThresholdEnabled != orangeBelowEnabled
                      || it->orangeBelowThreshold != orangeBelowValue
                      || it->redBelowThresholdEnabled != redBelowEnabled
                      || it->redBelowThreshold != redBelowValue;
    if (!changed)
        return false;

    it->orangeAboveThresholdEnabled = orangeAboveEnabled;
    it->orangeAboveThreshold = orangeAboveValue;
    it->redAboveThresholdEnabled = redAboveEnabled;
    it->redAboveThreshold = redAboveValue;
    it->orangeBelowThresholdEnabled = orangeBelowEnabled;
    it->orangeBelowThreshold = orangeBelowValue;
    it->redBelowThresholdEnabled = redBelowEnabled;
    it->redBelowThreshold = redBelowValue;
    return true;
}

void MappingDraft::applyTo(PageSet& pages) const
{
    // Preserve colours chosen directly on page controls when the Mapping Table
    // is re-applied. Labels/type/page are authoritative in the draft, but a
    // right-click colour choice should not be lost simply by pressing Apply.
    std::map<juce::String, juce::uint32> existingColours;
    for (const auto& page : pages.pages())
        for (const auto& control : page.controls)
            if (control.colourArgb != 0)
                existingColours[control.parameterPath] = control.colourArgb;

    // Rows are authoritative for their own parameter path: re-applying after
    // changing a page must move the control rather than duplicate it.
    for (const auto& row : rowList)
    {
        if (row.path.isEmpty()) continue;
        for (auto& page : pages.pages())
        {
            page.controls.erase(std::remove_if(page.controls.begin(), page.controls.end(), [&] (const ControlMapping& m)
            {
                return m.parameterPath == row.path;
            }), page.controls.end());
        }
    }

    for (const auto& row : rowList)
    {
        if (!row.enabled || row.path.isEmpty()) continue;
        const int pageIndex = juce::jmax(0, row.page - 1);
        while ((int) pages.pages().size() <= pageIndex) pages.addPage();

        ControlMapping mapping;
        mapping.parameterPath = row.path;
        mapping.label = row.label;
        mapping.type = row.type;
        mapping.label2 = row.label2;
        mapping.colourArgb = row.colourArgb;
        mapping.orangeAboveThresholdEnabled = row.orangeAboveThresholdEnabled;
        mapping.orangeAboveThreshold = row.orangeAboveThreshold;
        mapping.redAboveThresholdEnabled = row.redAboveThresholdEnabled;
        mapping.redAboveThreshold = row.redAboveThreshold;
        mapping.orangeBelowThresholdEnabled = row.orangeBelowThresholdEnabled;
        mapping.orangeBelowThreshold = row.orangeBelowThreshold;
        mapping.redBelowThresholdEnabled = row.redBelowThresholdEnabled;
        mapping.redBelowThreshold = row.redBelowThreshold;
        if (mapping.colourArgb == 0)
        {
            const auto it = existingColours.find(row.path);
            if (it != existingColours.end()) mapping.colourArgb = it->second;
        }
        pages.pages()[(size_t) pageIndex].controls.push_back(std::move(mapping));
    }
}

void MappingDraft::applyToPage(PageSet& pages, int pageNumber)
{
    const int targetPage = juce::jmax(1, pageNumber);

    // "Apply to Page N" is intentionally authoritative: every row in the
    // Mapping Table now targets the currently selected page. Disabled rows are
    // removed, enabled rows are recreated on that page. This avoids a hidden
    // second source of truth in the old editable Page column.
    for (auto& row : rowList)
        row.page = targetPage;

    applyTo(pages);
}

}
