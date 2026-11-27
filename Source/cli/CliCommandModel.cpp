#include "CliCommandModel.h"
#include <algorithm>
#include <map>
#include <set>

namespace er
{
namespace
{
struct ParsedToken
{
    juce::String text;
    int start = 0;
    int end = 0;
};

std::vector<ParsedToken> tokenize(const juce::String& line)
{
    std::vector<ParsedToken> result;
    int position = 0;
    while (position < line.length())
    {
        while (position < line.length() && juce::CharacterFunctions::isWhitespace(line[position])) ++position;
        if (position >= line.length()) break;

        ParsedToken token;
        token.start = position;
        const auto quote = line[position] == '"' || line[position] == '\'' ? line[position++] : 0;
        while (position < line.length())
        {
            const auto character = line[position];
            if (quote != 0)
            {
                if (character == '\\' && position + 1 < line.length()
                    && line[position + 1] == quote)
                {
                    token.text += quote;
                    position += 2;
                    continue;
                }
                if (character == quote) { ++position; break; }
            }
            else if (juce::CharacterFunctions::isWhitespace(character))
            {
                break;
            }
            token.text += character;
            ++position;
        }
        token.end = position;
        result.push_back(std::move(token));
    }
    return result;
}

juce::String quoted(const juce::String& token)
{
    if (!token.containsAnyOf(" \t\"'")) return token;
    return "\"" + token.replace("\"", "\\\"") + "\"";
}

bool equalsIgnoreCase(const juce::String& left, const juce::String& right)
{
    return left.compareIgnoreCase(right) == 0;
}

bool startsIgnoreCase(const juce::String& value, const juce::String& prefix)
{
    return value.startsWithIgnoreCase(prefix);
}

juce::String kindDescription(TreeEntryKind kind)
{
    switch (kind)
    {
        case TreeEntryKind::node: return "parameter group";
        case TreeEntryKind::matrix: return "XY Matrix";
        case TreeEntryKind::parameter: return "parameter";
    }
    return {};
}

juce::String localPathComponent(const juce::String& qualifiedPath)
{
    const auto localPath = ProviderPath::localPath(qualifiedPath);
    const auto separator = localPath.lastIndexOfChar('.');
    return separator >= 0 ? localPath.substring(separator + 1) : localPath;
}

juce::String controlTypeName(ControlType type)
{
    switch (type)
    {
        case ControlType::button: return "Button";
        case ControlType::encoder: return "Encoder";
        case ControlType::showContent: return "Content";
        case ControlType::meter: return "Meter";
        case ControlType::matrix: return "Matrix";
    }
    return "Unknown";
}

juce::String candidateTable(const std::vector<CliCandidate>& candidates)
{
    if (candidates.empty()) return "  No further entries are currently loaded.";
    int width = 0;
    for (const auto& candidate : candidates) width = juce::jmax(width, candidate.token.length());
    width = juce::jlimit(12, 34, width + 3);

    juce::String result;
    for (const auto& candidate : candidates)
        result << "  " << candidate.token.paddedRight(' ', width) << candidate.description << "\n";
    return result.trimEnd();
}

template <typename GetName>
int uniqueMatch(const juce::String& token, int count, GetName getName)
{
    int exactMatch = -1;
    for (int i = 0; i < count; ++i)
        if (equalsIgnoreCase(getName(i), token))
        {
            if (exactMatch >= 0) return -2;
            exactMatch = i;
        }
    if (exactMatch >= 0) return exactMatch;

    int match = -1;
    for (int i = 0; i < count; ++i)
    {
        if (!startsIgnoreCase(getName(i), token)) continue;
        if (match >= 0) return -2;
        match = i;
    }
    return match;
}

std::vector<const TreeEntry*> childrenOf(const CliSnapshot& snapshot, const juce::String& parent)
{
    std::vector<const TreeEntry*> children;
    for (const auto& entry : snapshot.tree)
        if (entry.parentPath == parent) children.push_back(&entry);
    std::sort(children.begin(), children.end(), [] (const auto* left, const auto* right)
    {
        return left->displayName().compareNatural(right->displayName()) < 0;
    });
    return children;
}

const TreeEntry* findTreeEntry(const CliSnapshot& snapshot, const juce::String& path)
{
    for (const auto& entry : snapshot.tree)
        if (entry.path == path) return &entry;
    return nullptr;
}

const ParameterInfo* findParameter(const CliSnapshot& snapshot, const juce::String& path)
{
    for (const auto& parameter : snapshot.parameters)
        if (parameter.path == path) return &parameter;
    return nullptr;
}

std::vector<CliCandidate> childCandidates(const std::vector<const TreeEntry*>& children,
                                          const juce::String& prefix)
{
    std::vector<CliCandidate> result;
    for (const auto* child : children)
    {
        const auto name = child->displayName();
        const auto localNumber = localPathComponent(child->path);
        if (prefix.isNotEmpty() && !startsIgnoreCase(name, prefix)
            && !startsIgnoreCase(child->identifier, prefix)
            && !startsIgnoreCase(localNumber, prefix))
            continue;
        auto description = kindDescription(child->kind) + " [" + localNumber + "]";
        if (child->description.isNotEmpty() && child->description != name)
            description << " — " << child->description;
        result.push_back({ name, description });
    }
    return result;
}

int resolveChild(const std::vector<const TreeEntry*>& children, const juce::String& token)
{
    for (int i = 0; i < (int) children.size(); ++i)
    {
        const auto* child = children[(size_t) i];
        const auto localNumber = localPathComponent(child->path);
        if (equalsIgnoreCase(localNumber, token)) return i;
    }

    int exactMatch = -1;
    for (int i = 0; i < (int) children.size(); ++i)
    {
        const auto* child = children[(size_t) i];
        if (!equalsIgnoreCase(child->displayName(), token)
            && !equalsIgnoreCase(child->identifier, token))
            continue;
        if (exactMatch >= 0) return -2;
        exactMatch = i;
    }
    if (exactMatch >= 0) return exactMatch;

    int match = -1;
    for (int i = 0; i < (int) children.size(); ++i)
    {
        const auto* child = children[(size_t) i];
        if (!startsIgnoreCase(child->displayName(), token)
            && !startsIgnoreCase(child->identifier, token))
            continue;
        if (match >= 0) return -2;
        match = i;
    }
    return match;
}

juce::String parameterDetails(const TreeEntry& entry, const ParameterInfo* parameter)
{
    juce::String text;
    text << "Name:         " << entry.displayName() << "\n"
         << "Path:         " << entry.path << "\n";
    if (entry.description.isNotEmpty()) text << "Description:  " << entry.description << "\n";
    if (parameter == nullptr)
    {
        text << "Value:        -\nStatus:       not loaded";
        return text;
    }
    text << "Value:        " << parameter->formattedValue() << "\n"
         << "Type:         " << ParameterInfo::kindToString(parameter->kind) << "\n"
         << "Writable:     " << (parameter->writable ? "yes" : "no") << "\n"
         << "Online:       " << (parameter->online ? "yes" : "no");
    if (!parameter->minimum.isVoid()) text << "\nMinimum:      " << parameter->minimum.toString();
    if (!parameter->maximum.isVoid()) text << "\nMaximum:      " << parameter->maximum.toString();
    return text;
}

juce::String pageControlLabel(const ControlMapping& mapping, const CliSnapshot& snapshot)
{
    auto label = mapping.label2.isNotEmpty() ? mapping.label2 : mapping.label;
    if (label.isEmpty())
        if (const auto* parameter = findParameter(snapshot, mapping.parameterPath))
            label = parameter->displayName();
    return label.isNotEmpty() ? label : mapping.parameterPath;
}

juce::String printablePageControlLabel(const ControlMapping& mapping, const CliSnapshot& snapshot)
{
    return pageControlLabel(mapping, snapshot).replaceCharacters("\r\n\t", "   ").substring(0, 256);
}

std::vector<CliCandidate> pageRowCandidates(const PageModel& page, const CliSnapshot& snapshot,
                                            const juce::String& prefix)
{
    std::vector<CliCandidate> result;
    if (prefix.isEmpty() || startsIgnoreCase("all", prefix))
        result.push_back({ "all", "all controls" });

    if (prefix.containsOnly("0123456789") && prefix.isNotEmpty())
    {
        const auto exactRow = prefix.getIntValue();
        if (exactRow >= 1 && exactRow <= (int) page.controls.size())
        {
            const auto& mapping = page.controls[(size_t) exactRow - 1];
            return { { juce::String(exactRow), pageControlLabel(mapping, snapshot)
                                                + " — " + controlTypeName(mapping.type) } };
        }
    }

    for (int index = 0; index < (int) page.controls.size(); ++index)
    {
        const auto& mapping = page.controls[(size_t) index];
        const auto row = juce::String(index + 1);
        const auto label = printablePageControlLabel(mapping, snapshot);
        if (prefix.isNotEmpty() && !startsIgnoreCase(row, prefix)
            && !startsIgnoreCase(label, prefix))
            continue;
        result.push_back({ row, label + " — " + controlTypeName(mapping.type) });
    }
    return result;
}

bool parsePageRows(const juce::String& selector, const PageModel& page,
                   const CliSnapshot& snapshot, std::set<int>& rows, juce::String& error)
{
    if (selector.equalsIgnoreCase("all"))
    {
        for (int index = 0; index < (int) page.controls.size(); ++index) rows.insert(index);
        return true;
    }

    const auto compact = selector.removeCharacters(" \t");
    if (compact.isNotEmpty() && compact.containsOnly("0123456789,-"))
    {
        if (compact.startsWithChar(',') || compact.endsWithChar(',') || compact.contains(",,"))
        {
            error = "% Invalid row list. Example: 1-4,8-10,14,17";
            return false;
        }

        juce::StringArray parts;
        parts.addTokens(compact, ",", {});
        for (const auto& part : parts)
        {
            int first = 0;
            int last = 0;
            const auto separator = part.indexOfChar('-');
            if (separator >= 0)
            {
                if (separator == 0 || separator == part.length() - 1
                    || separator != part.lastIndexOfChar('-'))
                {
                    error = "% Invalid row range '" + part + "'. Example: 3-8";
                    return false;
                }
                first = part.substring(0, separator).getIntValue();
                last = part.substring(separator + 1).getIntValue();
            }
            else
            {
                first = last = part.getIntValue();
            }

            if (first < 1 || last < first || last > (int) page.controls.size())
            {
                error = "% Row selection '" + part + "' is outside 1-"
                      + juce::String((int) page.controls.size()) + ".";
                return false;
            }
            for (int row = first; row <= last; ++row) rows.insert(row - 1);
        }
        return !rows.empty();
    }

    const auto labelMatch = uniqueMatch(selector.trim(), (int) page.controls.size(), [&] (int index)
    {
        return pageControlLabel(page.controls[(size_t) index], snapshot);
    });
    if (labelMatch >= 0)
    {
        rows.insert(labelMatch);
        return true;
    }
    error = labelMatch == -2 ? "% Ambiguous control label. Use its row number."
                             : "% Control not found. Use show panel <panel> ?";
    return false;
}

juce::String pageDetails(const PageModel& page, int pageNumber, const CliSnapshot& snapshot,
                         const std::set<int>* selectedRows = nullptr)
{
    juce::String text;
    text << "PANEL " << juce::String(pageNumber) << " — " << page.name << "\n\n";
    if (page.controls.empty()) return text + "  No controls configured.";

    constexpr int rowWidth = 6;
    constexpr int typeWidth = 10;
    constexpr int valueWidth = 22;
    constexpr int statusWidth = 11;
    int labelWidth = juce::String("LABEL").length() + 2;
    for (int index = 0; index < (int) page.controls.size(); ++index)
    {
        if (selectedRows != nullptr && selectedRows->count(index) == 0) continue;
        labelWidth = juce::jmax(labelWidth,
                               printablePageControlLabel(page.controls[(size_t) index], snapshot).length() + 2);
    }
    labelWidth = juce::jlimit(7, 258, labelWidth);
    text << juce::String("ROW").paddedRight(' ', rowWidth)
         << juce::String("TYPE").paddedRight(' ', typeWidth)
         << juce::String("LABEL").paddedRight(' ', labelWidth)
         << juce::String("VALUE").paddedRight(' ', valueWidth)
         << juce::String("STATUS").paddedRight(' ', statusWidth)
         << "PATH\n";
    text << juce::String::repeatedString("-", labelWidth + 76) << "\n";

    for (int index = 0; index < (int) page.controls.size(); ++index)
    {
        if (selectedRows != nullptr && selectedRows->count(index) == 0) continue;
        const auto& mapping = page.controls[(size_t) index];
        const auto* parameter = findParameter(snapshot, mapping.parameterPath);
        const auto label = printablePageControlLabel(mapping, snapshot);
        auto value = parameter != nullptr ? parameter->formattedValue() : juce::String("-");
        if (mapping.type == ControlType::matrix)
            if (const auto* matrix = findTreeEntry(snapshot, mapping.parameterPath)) value = matrix->matrix.summary();
        value = value.replaceCharacters("\r\n\t", "   ").substring(0, valueWidth - 2);
        const auto* treeEntry = findTreeEntry(snapshot, mapping.parameterPath);
        const juce::String status = parameter != nullptr ? (parameter->online ? "online" : "offline")
                                                         : (treeEntry != nullptr ? "loaded" : "offline");
        text << juce::String(index + 1).paddedRight(' ', rowWidth)
             << controlTypeName(mapping.type).paddedRight(' ', typeWidth)
             << label.paddedRight(' ', labelWidth)
             << value.paddedRight(' ', valueWidth)
             << status.paddedRight(' ', statusWidth)
             << mapping.parameterPath << "\n";
    }
    return text.trimEnd();
}

std::vector<CliCandidate> topCandidates(const CliSnapshot& snapshot, const juce::String& prefix)
{
    std::vector<CliCandidate> result;
    for (const auto& fixed : std::vector<CliCandidate> { { "panel", "configured Pages/Panels" },
                                                         { "status", "provider connection overview" },
                                                         { "version", "GUI and CLI release version" } })
        if (prefix.isEmpty() || startsIgnoreCase(fixed.token, prefix)) result.push_back(fixed);
    for (const auto& provider : snapshot.providers)
        if (prefix.isEmpty() || startsIgnoreCase(provider.name, prefix)
            || startsIgnoreCase(provider.id, prefix))
            result.push_back({ provider.name, "Ember+ provider" });
    return result;
}

CliCommandResult completeWithCandidates(const juce::String& line, int replacementStart,
                                        std::vector<CliCandidate> candidates, bool complete)
{
    CliCommandResult result;
    result.candidates = std::move(candidates);
    result.text = candidateTable(result.candidates);
    if (complete && result.candidates.size() == 1)
        result.completedLine = line.substring(0, replacementStart)
                             + quoted(result.candidates.front().token) + " ";
    return result;
}
}

CliCommandResult CliCommandModel::process(const juce::String& originalLine, Mode mode,
                                          const CliSnapshot& snapshot)
{
    auto commandLine = originalLine;
    auto line = commandLine.trimEnd();
    auto tokens = tokenize(line);
    if (mode == Mode::execute && !tokens.empty() && tokens.back().text == "?")
    {
        commandLine = line.substring(0, tokens.back().start);
        line = commandLine.trimEnd();
        tokens.pop_back();
        mode = Mode::help;
    }

    const bool trailingSpace = commandLine.isNotEmpty()
                            && juce::CharacterFunctions::isWhitespace(commandLine.getLastCharacter());
    const auto replacementStart = trailingSpace || tokens.empty() ? commandLine.length()
                                                                   : tokens.back().start;

    if (tokens.empty())
        return completeWithCandidates(commandLine, replacementStart,
                                      { { "show", "display providers, tree data or panels" },
                                        { "help", "show CLI help" },
                                        { "exit", "close the CLI" } }, mode == Mode::complete);

    if (startsIgnoreCase("help", tokens[0].text))
        return { true,
                 "Commands:\n"
                 "  show ?                         show providers and top-level choices\n"
                 "  show <provider> ?              show the next tree level\n"
                 "  show <provider> <path>         show a group or parameter\n"
                 "  show panel ?                   list configured panels\n"
                 "  show panel <name|number>       show every panel control and live value\n"
                 "  show panel <panel> <rows>      show rows such as 1-4,8-10,14,17\n"
                 "  show status                    show provider connection status\n"
                 "  show version                   show GUI and CLI release version\n"
                 "\nPress ? at any position for choices. Press Tab to complete.", {}, {}, {} };

    if (!startsIgnoreCase("show", tokens[0].text))
    {
        CliCommandResult result;
        result.ok = false;
        result.text = "% Unknown command. Type help or show ?";
        return result;
    }

    if (tokens.size() == 1 && !trailingSpace)
        return completeWithCandidates(commandLine, replacementStart,
                                      { { "show", "display providers, tree data or panels" } },
                                      mode == Mode::complete);

    if (tokens.size() == 1 || (tokens.size() == 2 && !trailingSpace
                               && mode != Mode::execute))
    {
        const auto prefix = tokens.size() == 2 ? tokens[1].text : juce::String();
        return completeWithCandidates(commandLine, replacementStart, topCandidates(snapshot, prefix),
                                      mode == Mode::complete);
    }

    const auto second = tokens[1].text;
    if (startsIgnoreCase("version", second))
        return { true, "DEVICEControl " + (snapshot.appVersion.isNotEmpty() ? snapshot.appVersion
                                                                            : juce::String("unknown")),
                 {}, {}, {} };
    if (startsIgnoreCase("status", second))
        return { true, snapshot.status, {}, {}, {} };

    if (startsIgnoreCase("panel", second))
    {
        if (tokens.size() == 2 || (tokens.size() == 3 && !trailingSpace && mode != Mode::execute))
        {
            const auto prefix = tokens.size() == 3 ? tokens[2].text : juce::String();
            std::vector<CliCandidate> candidates;
            candidates.push_back({ "current", "currently visible panel" });
            for (int i = 0; i < (int) snapshot.pages.size(); ++i)
                candidates.push_back({ snapshot.pages[(size_t) i].name,
                                       "Panel " + juce::String(i + 1) });
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&] (const auto& candidate)
            {
                return prefix.isNotEmpty() && !startsIgnoreCase(candidate.token, prefix);
            }), candidates.end());
            if (mode == Mode::execute && tokens.size() == 2)
                return { true, candidateTable(candidates), {}, {}, candidates };
            return completeWithCandidates(commandLine, replacementStart, std::move(candidates),
                                          mode == Mode::complete);
        }

        const auto pageToken = tokens[2].text;
        int pageIndex = -1;
        if (equalsIgnoreCase(pageToken, "current")) pageIndex = snapshot.currentPage;
        else if (pageToken.containsOnly("0123456789")) pageIndex = pageToken.getIntValue() - 1;
        else pageIndex = uniqueMatch(pageToken, (int) snapshot.pages.size(), [&] (int index)
        {
            return snapshot.pages[(size_t) index].name;
        });

        if (pageIndex < 0 || pageIndex >= (int) snapshot.pages.size())
        {
            CliCommandResult result;
            result.ok = false;
            result.text = pageIndex == -2 ? "% Ambiguous panel name. Use show panel ?"
                                          : "% Panel not found. Use show panel ?";
            return result;
        }

        const auto& page = snapshot.pages[(size_t) pageIndex];
        if (mode != Mode::execute)
        {
            if (tokens.size() == 3 && trailingSpace)
                return completeWithCandidates(commandLine, replacementStart,
                                              pageRowCandidates(page, snapshot, {}),
                                              mode == Mode::complete);
            if (tokens.size() == 4 && !trailingSpace)
                return completeWithCandidates(commandLine, replacementStart,
                                              pageRowCandidates(page, snapshot, tokens[3].text),
                                              mode == Mode::complete);

            CliCommandResult result;
            result.text = "  Row selection is complete. Press Enter, or use a list such as 1-4,8-10,14,17.";
            return result;
        }

        if (tokens.size() == 3)
            return { true, pageDetails(page, pageIndex + 1, snapshot), {}, {}, {} };

        juce::StringArray selectorParts;
        for (int index = 3; index < (int) tokens.size(); ++index)
            selectorParts.add(tokens[(size_t) index].text);
        const auto selector = selectorParts.joinIntoString(" ");
        std::set<int> selectedRows;
        juce::String selectionError;
        if (!parsePageRows(selector, page, snapshot, selectedRows, selectionError))
        {
            CliCommandResult result;
            result.ok = false;
            result.text = selectionError;
            return result;
        }
        return { true, pageDetails(page, pageIndex + 1, snapshot, &selectedRows), {}, {}, {} };
    }

    int providerIndex = -1;
    for (int i = 0; i < (int) snapshot.providers.size(); ++i)
        if (equalsIgnoreCase(snapshot.providers[(size_t) i].name, second)
            || equalsIgnoreCase(snapshot.providers[(size_t) i].id, second)) providerIndex = i;
    if (providerIndex < 0)
        providerIndex = uniqueMatch(second, (int) snapshot.providers.size(), [&] (int index)
        {
            return snapshot.providers[(size_t) index].name;
        });
    if (providerIndex < 0)
    {
        CliCommandResult result;
        result.ok = false;
        result.text = providerIndex == -2 ? "% Ambiguous provider name. Use show ?"
                                          : "% Provider not found. Use show ?";
        return result;
    }

    const auto& provider = snapshot.providers[(size_t) providerIndex];
    auto currentPath = ProviderPath::root(provider.id);
    const TreeEntry* currentEntry = findTreeEntry(snapshot, currentPath);
    const int pathTokenCount = (int) tokens.size() - 2;
    const bool wantsSuggestions = mode != Mode::execute;
    const int tokensToResolve = wantsSuggestions && !trailingSpace ? juce::jmax(0, pathTokenCount - 1)
                                                                   : pathTokenCount;

    for (int index = 0; index < tokensToResolve; ++index)
    {
        const auto children = childrenOf(snapshot, currentPath);
        const auto match = resolveChild(children, tokens[(size_t) index + 2].text);
        if (match < 0)
        {
            CliCommandResult result;
            result.ok = false;
            result.text = match == -2 ? "% Ambiguous entry. Press ? for the available choices."
                                      : "% Tree entry not found. Press ? for the available choices.";
            return result;
        }
        currentEntry = children[(size_t) match];
        currentPath = currentEntry->path;
    }

    if (wantsSuggestions)
    {
        const auto prefix = !trailingSpace && pathTokenCount > 0 ? tokens.back().text : juce::String();
        auto children = childrenOf(snapshot, currentPath);
        auto result = completeWithCandidates(commandLine, replacementStart,
                                             childCandidates(children, prefix), mode == Mode::complete);
        if (currentEntry != nullptr && currentEntry->kind == TreeEntryKind::node
            && !currentEntry->directoryLoaded)
        {
            result.requestDirectory = currentEntry->path;
            if (children.empty()) result.text = "  Loading directory from provider; press ? or Tab again shortly.";
        }
        return result;
    }

    if (currentEntry == nullptr)
    {
        CliCommandResult result;
        result.ok = false;
        result.text = "% Provider tree is not loaded yet.";
        result.requestDirectory = currentPath;
        return result;
    }

    if (currentEntry->kind == TreeEntryKind::parameter)
        return { true, parameterDetails(*currentEntry, findParameter(snapshot, currentEntry->path)), {}, {}, {} };
    if (currentEntry->kind == TreeEntryKind::matrix)
        return { true, "Matrix:       " + currentEntry->displayName() + "\nPath:         "
                     + currentEntry->path + "\n" + currentEntry->matrix.details(), {}, {}, {} };

    const auto children = childrenOf(snapshot, currentPath);
    CliCommandResult result;
    result.candidates = childCandidates(children, {});
    result.text = candidateTable(result.candidates);
    if (!currentEntry->directoryLoaded)
    {
        result.requestDirectory = currentEntry->path;
        if (children.empty()) result.text = "  Loading directory from provider; repeat the command shortly.";
    }
    return result;
}
}
