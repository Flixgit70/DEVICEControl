#include "CsvService.h"
namespace er
{
namespace
{
juce::uint32 colourFromString(juce::String text)
{
    text = text.trim();
    if (text.startsWithChar('#')) text = text.substring(1);
    if (text.isEmpty()) return 0;
    if (text.length() == 6) text = "FF" + text;
    if (text.length() != 8) return 0;
    return (juce::uint32) text.getHexValue32();
}

juce::StringArray splitTreePath(juce::String treePath)
{
    juce::StringArray parts;
    parts.addTokens(treePath, "/", "\"");
    parts.trim();
    parts.removeEmptyStrings();
    return parts;
}

juce::String thresholdField(bool enabled, double value)
{
    if (!enabled) return {};
    return juce::String(value, 12).trimCharactersAtEnd("0").trimCharactersAtEnd(".");
}

juce::String controlTypeField(ControlType type)
{
    return type == ControlType::showContent ? "show_content"
         : type == ControlType::encoder ? "encoder"
         : type == ControlType::meter ? "meter"
         : type == ControlType::matrix ? "matrix" : "button";
}


juce::String normaliseColumnName(juce::String text)
{
    text = text.trim().toLowerCase();
    if (text.isNotEmpty() && text[0] == (juce::juce_wchar) 0xfeff)
        text = text.substring(1);
    return text.removeCharacters(" _-\t");
}
juce::String branchLabelFromColumns(const juce::StringArray& columns,
                                    const juce::Array<int>& branchIndices)
{
    juce::StringArray parts;
    for (const auto index : branchIndices)
        if (index >= 0 && index < columns.size())
        {
            const auto value = columns[index].trim();
            if (value.isNotEmpty())
                parts.add(value);
        }
    return parts.joinIntoString("/");
}
}

juce::String CsvService::plainField(juce::String s)
{
    // Excel-style semicolon CSV: no quoting. Keep the delimiter out of field data
    // and collapse line breaks so every record stays on exactly one line.
    return s.replace(";", ",")
            .replace("\r\n", " ")
            .replace("\n", " ")
            .replace("\r", " ");
}

bool CsvService::exportPageMappings(const juce::File& file, const PageModel& page, int pageNumber,
                                     std::function<std::optional<ParameterInfo>(const juce::String&)> resolver,
                                     juce::String& error)
{
    std::vector<ParameterInfo> params;
    params.reserve(page.controls.size());
    for (const auto& mapping : page.controls)
    {
        auto p = resolver ? resolver(mapping.parameterPath).value_or(ParameterInfo{}) : ParameterInfo{};
        if (p.path.isEmpty()) p.path = mapping.parameterPath;
        if (p.localPath.isEmpty()) p.localPath = ProviderPath::localPath(p.path);
        if (p.providerId.isEmpty()) p.providerId = ProviderPath::providerId(p.path);
        if (p.treePath.isEmpty()) p.treePath = mapping.label;
        params.push_back(std::move(p));
    }
    return exportParameters(file, params, error, &page.controls, pageNumber);
}

bool CsvService::exportParameters(const juce::File& file, const std::vector<ParameterInfo>& params,
                                   juce::String& error, const std::vector<ControlMapping>* mappings, int pageNumber)
{
    int maxBranchDepth = 0;
    std::vector<juce::StringArray> branches;
    branches.reserve(params.size());

    for (const auto& p : params)
    {
        const auto branchSource = p.treePath.isNotEmpty() ? p.treePath.replace(" / ", "/") : p.displayName();
        auto parts = splitTreePath(branchSource);
        maxBranchDepth = juce::jmax(maxBranchDepth, parts.size());
        branches.push_back(std::move(parts));
    }

    juce::StringArray header;
    header.add("provider_id");
    header.add("provider");
    header.add("path");
    for (int level = 1; level <= maxBranchDepth; ++level)
        header.add("branch_" + juce::String(level));
    header.add("identifier"); header.add("description"); header.add("type"); header.add("value");
    header.add("minimum"); header.add("maximum"); header.add("factor"); header.add("writable");
    header.add("online"); header.add("enum"); header.add("enabled"); header.add("control");
    header.add("page"); header.add("label"); header.add("label2"); header.add("colour");
    header.add("orange_above_threshold"); header.add("red_above_threshold");
    header.add("orange_below_threshold"); header.add("red_below_threshold");

    juce::StringArray lines;
    lines.add(header.joinIntoString(";"));

    for (size_t row = 0; row < params.size(); ++row)
    {
        const auto& p = params[row];
        const auto& branchParts = branches[row];
        juce::StringArray cols;
        cols.add(plainField(p.providerId));
        cols.add(plainField(p.providerName));
        const auto exportPath = p.localPath.isNotEmpty() ? p.localPath : ProviderPath::localPath(p.path);
        cols.add(plainField(exportPath));
        for (int level = 0; level < maxBranchDepth; ++level)
            cols.add(plainField(level < branchParts.size() ? branchParts[level] : juce::String()));

        cols.add(plainField(p.identifier)); cols.add(plainField(p.description));
        cols.add(plainField(ParameterInfo::kindToString(p.kind))); cols.add(plainField(p.value.toString()));
        cols.add(plainField(p.minimum.toString())); cols.add(plainField(p.maximum.toString()));
        cols.add(juce::String(p.factor)); cols.add(p.writable ? "true" : "false"); cols.add(p.online ? "true" : "false");
        cols.add(plainField(p.enumEntries.joinIntoString("|")));
        const auto* mapping = mappings && row < mappings->size() ? &(*mappings)[row] : nullptr;
        const auto label1 = mapping ? mapping->label : p.fullTreeLabel();
        const auto label2 = mapping ? mapping->label2
                                     : (p.description.isNotEmpty() && p.description != label1 ? p.description : juce::String());
        const auto defaultControl = p.kind == ParameterKind::stringValue ? juce::String("show_content")
                                  : (p.kind == ParameterKind::booleanValue || p.kind == ParameterKind::trigger)
                                        ? juce::String("button") : juce::String("encoder");
        cols.add(mapping ? "true" : "false");
        cols.add(mapping ? controlTypeField(mapping->type) : defaultControl);
        cols.add(juce::String(mapping ? pageNumber : 1));
        cols.add(plainField(label1)); cols.add(plainField(label2));
        cols.add(mapping && mapping->colourArgb != 0
                     ? "#" + juce::String::toHexString((juce::int64) mapping->colourArgb).paddedLeft('0', 8).toUpperCase()
                     : juce::String());
        cols.add(mapping ? thresholdField(mapping->orangeAboveThresholdEnabled, mapping->orangeAboveThreshold) : juce::String());
        cols.add(mapping ? thresholdField(mapping->redAboveThresholdEnabled, mapping->redAboveThreshold) : juce::String());
        cols.add(mapping ? thresholdField(mapping->orangeBelowThresholdEnabled, mapping->orangeBelowThreshold) : juce::String());
        cols.add(mapping ? thresholdField(mapping->redBelowThresholdEnabled, mapping->redBelowThreshold) : juce::String());
        lines.add(cols.joinIntoString(";"));
    }
    if (!file.replaceWithText(lines.joinIntoString("\n"))) { error = "Could not write " + file.getFullPathName(); return false; }
    return true;
}

static juce::StringArray parseSemicolonLine(const juce::String& line)
{
    juce::StringArray out;
    juce::String current;
    for (int i = 0; i < line.length(); ++i)
    {
        const auto c = line[i];
        if (c == ';')
        {
            out.add(current.trim());
            current.clear();
        }
        else
        {
            current << c;
        }
    }
    out.add(current.trim());
    return out;
}

bool CsvService::importMappings(const juce::File& file, PageSet& pages, juce::String& error)
{
    if (!file.existsAsFile()) { error = "File not found"; return false; }
    juce::StringArray lines; lines.addLines(file.loadFileAsString());
    if (lines.isEmpty()) { error = "CSV is empty"; return false; }
    auto header=parseSemicolonLine(lines[0]);
    auto idx=[&](const juce::String& name)
    {
        const auto wanted = normaliseColumnName(name);
        for (int i = 0; i < header.size(); ++i)
            if (normaliseColumnName(header[i]) == wanted)
                return i;
        return -1;
    };
    const int providerIdI=idx("provider_id"), providerNameI=idx("provider"), pathI=idx("path"), treePathI=idx("tree_path"), enabledI=idx("enabled"), controlI=idx("control"), pageI=idx("page"), labelI=idx("label");
    const int label2I=idx("label2"), descriptionI=idx("description"), colourI=idx("colour");
    const int orangeAboveI=idx("orange_above_threshold"), redAboveI=idx("red_above_threshold");
    const int orangeBelowI=idx("orange_below_threshold"), redBelowI=idx("red_below_threshold");
    const int legacyOrangeDirectionI=idx("orange_direction"), legacyOrangeThresholdI=idx("orange_threshold");
    const int legacyRedDirectionI=idx("red_direction"), legacyRedThresholdI=idx("red_threshold");
    if (pathI<0) { error = "CSV needs a 'path' column"; return false; }
    juce::ignoreUnused(providerNameI);

    juce::Array<int> branchIndices;
    for (int level = 1;; ++level)
    {
        const auto branchIndex = idx("branch_" + juce::String(level));
        if (branchIndex < 0)
            break;
        branchIndices.add(branchIndex);
    }

    for (int lineNo=1; lineNo<lines.size(); ++lineNo)
    {
        if (lines[lineNo].trim().isEmpty()) continue;
        auto c=parseSemicolonLine(lines[lineNo]);
        auto get=[&](int i)->juce::String { return i>=0 && i<c.size() ? c[i] : juce::String(); };
        auto enabled=get(enabledI); if (enabledI>=0 && !( enabled.equalsIgnoreCase("true") || enabled=="1" || enabled.equalsIgnoreCase("yes") || enabled.equalsIgnoreCase("x"))) continue;
        auto path=get(pathI).trim(); if (path.isEmpty()) continue;
        const auto providerId = get(providerIdI).trim();
        if (!ProviderPath::isQualified(path) && providerId.isNotEmpty())
            path = ProviderPath::qualify(providerId, path);
        int page=juce::jmax(1, get(pageI).getIntValue());
        while ((int)pages.pages().size()<page) pages.addPage();
        ControlMapping m;
        m.parameterPath=path;
        m.label=get(labelI).trim();
        if (m.label.isEmpty())
        {
            if (treePathI >= 0)
                m.label=get(treePathI).trim().replace(" / ", "/");
            if (m.label.isEmpty())
                m.label=branchLabelFromColumns(c, branchIndices);
        }
        if (m.label.isEmpty())
            m.label=path;
        // Label 2 is independent from Label 1. Accept common Excel header spellings
        // such as "Label 2", "label_2" and "label2" via normalised headers.
        // Only fall back to the Ember description when the CSV has no Label-2
        // column at all; an intentionally blank Label 2 must stay blank.
        m.label2 = label2I >= 0 ? get(label2I).trim() : get(descriptionI).trim();
        m.colourArgb=colourFromString(get(colourI));
        auto applyThresholdText = [&] (int column, bool& enabled, double& value)
        {
            const auto text = get(column).trim();
            enabled = text.isNotEmpty();
            value = enabled ? text.getDoubleValue() : 0.0;
        };
        applyThresholdText(orangeAboveI, m.orangeAboveThresholdEnabled, m.orangeAboveThreshold);
        applyThresholdText(redAboveI, m.redAboveThresholdEnabled, m.redAboveThreshold);
        applyThresholdText(orangeBelowI, m.orangeBelowThresholdEnabled, m.orangeBelowThreshold);
        applyThresholdText(redBelowI, m.redBelowThresholdEnabled, m.redBelowThreshold);

        // Backward compatibility with 0.4.2 CSVs that used one threshold plus a direction per colour.
        if (orangeAboveI < 0 && orangeBelowI < 0)
        {
            const auto text = get(legacyOrangeThresholdI).trim();
            if (text.isNotEmpty())
            {
                const bool below = get(legacyOrangeDirectionI).trim().equalsIgnoreCase("below")
                                || get(legacyOrangeDirectionI).trim() == "<="
                                || get(legacyOrangeDirectionI).trim() == "<";
                if (below) { m.orangeBelowThresholdEnabled = true; m.orangeBelowThreshold = text.getDoubleValue(); }
                else       { m.orangeAboveThresholdEnabled = true; m.orangeAboveThreshold = text.getDoubleValue(); }
            }
        }
        if (redAboveI < 0 && redBelowI < 0)
        {
            const auto text = get(legacyRedThresholdI).trim();
            if (text.isNotEmpty())
            {
                const bool below = get(legacyRedDirectionI).trim().equalsIgnoreCase("below")
                                || get(legacyRedDirectionI).trim() == "<="
                                || get(legacyRedDirectionI).trim() == "<";
                if (below) { m.redBelowThresholdEnabled = true; m.redBelowThreshold = text.getDoubleValue(); }
                else       { m.redAboveThresholdEnabled = true; m.redAboveThreshold = text.getDoubleValue(); }
            }
        }
        const auto controlText = get(controlI).trim();
        if (controlText.equalsIgnoreCase("encoder")) m.type = ControlType::encoder;
        else if (controlText.equalsIgnoreCase("show_content") || controlText.equalsIgnoreCase("show content")
                 || controlText.equalsIgnoreCase("content")) m.type = ControlType::showContent;
        else if (controlText.equalsIgnoreCase("meter") || controlText.equalsIgnoreCase("ppm"))
            m.type = ControlType::meter;
        else if (controlText.equalsIgnoreCase("matrix"))
            m.type = ControlType::matrix;
        else m.type = ControlType::button;
        pages.pages()[(size_t)(page-1)].controls.push_back(m);
    }
    return true;
}
}
