#include "ControlTreeModel.h"
#include <algorithm>

namespace er
{
namespace
{
struct FilterTerms
{
    juce::StringArray include;
    juce::StringArray exclude;

    bool empty() const noexcept { return include.isEmpty() && exclude.isEmpty(); }
};

FilterTerms parseFilterTerms(const juce::String& query)
{
    FilterTerms terms;
    juce::StringArray tokens;
    tokens.addTokens(query.trim().toLowerCase(), " \t", "");
    tokens.removeEmptyStrings();

    for (const auto& token : tokens)
    {
        if (token.startsWithChar('-') && token.length() > 1)
            terms.exclude.add(token.substring(1));
        else if (token != "-")
            terms.include.add(token);
    }

    return terms;
}

bool matchesTerms(const juce::String& haystack, const FilterTerms& terms)
{
    if (terms.empty()) return false;

    const auto lower = haystack.toLowerCase();

    for (const auto& token : terms.include)
        if (!lower.contains(token))
            return false;

    for (const auto& token : terms.exclude)
        if (lower.contains(token))
            return false;

    return true;
}

std::map<juce::String, const TreeEntry*> buildEntryIndex(const std::vector<TreeEntry>& entries)
{
    std::map<juce::String, const TreeEntry*> index;
    for (const auto& entry : entries)
        index[entry.path] = &entry;
    return index;
}

juce::String fullPathSearchText(const TreeEntry& entry,
                                const std::map<juce::String, const TreeEntry*>& index)
{
    std::vector<const TreeEntry*> chain;
    std::set<juce::String> visited;
    const TreeEntry* current = &entry;

    while (current != nullptr && visited.insert(current->path).second)
    {
        chain.push_back(current);
        if (current->parentPath.isEmpty()) break;
        const auto it = index.find(current->parentPath);
        current = it != index.end() ? it->second : nullptr;
    }

    std::reverse(chain.begin(), chain.end());
    juce::String text;
    for (const auto* part : chain)
    {
        text << part->identifier << " "
             << part->description << " "
             << part->displayName() << " "
             << part->path << " ";
    }
    return text;
}
}
void ControlTreeModel::clear()
{
    const juce::ScopedLock sl(lock);
    entries.clear();
    rootLoaded = false;
    rootLoading = false;
    ++revisionCounter;
}

juce::String ControlTreeModel::parentOf(const juce::String& path)
{
    const auto dot = path.lastIndexOfChar('.');
    return dot >= 0 ? path.substring(0, dot) : juce::String();
}

bool ControlTreeModel::isSameOrDescendant(const juce::String& path, const juce::String& root)
{
    if (root.isEmpty()) return true;
    return path == root || path.startsWith(root + ".");
}

bool ControlTreeModel::matchesFilter(const TreeEntry& entry, const juce::String& query)
{
    const auto terms = parseFilterTerms(query);
    const auto localText = entry.identifier + " " + entry.description + " " + entry.displayName() + " " + entry.path;
    return matchesTerms(localText, terms);
}

TreeFilterResult ControlTreeModel::buildFilter(const std::vector<TreeEntry>& sourceEntries, const juce::String& query)
{
    TreeFilterResult result;
    const auto terms = parseFilterTerms(query);
    if (terms.empty()) return result;

    const auto index = buildEntryIndex(sourceEntries);
    for (const auto& entry : sourceEntries)
    {
        // Full-path conjunctive search: every query token must occur somewhere
        // along this entry's ancestry or on the entry itself. This lets a query
        // such as "Ravenna Buffer RX" match a parameter even when the three
        // terms live on different levels of the Ember tree.
        if (!matchesTerms(fullPathSearchText(entry, index), terms)) continue;
        result.matches.insert(entry.path);
        auto path = entry.path;
        while (path.isNotEmpty())
        {
            result.visible.insert(path);
            path = parentOf(path);
        }
    }
    return result;
}

std::vector<juce::String> ControlTreeModel::parameterPathsMatchingFilter(const std::vector<TreeEntry>& sourceEntries,
                                                                           const juce::String& query)
{
    std::vector<juce::String> result;
    const auto terms = parseFilterTerms(query);
    if (terms.empty()) return result;

    const auto index = buildEntryIndex(sourceEntries);
    for (const auto& entry : sourceEntries)
        if (entry.kind == TreeEntryKind::parameter
            && matchesTerms(fullPathSearchText(entry, index), terms))
            result.push_back(entry.path);

    std::sort(result.begin(), result.end(), [] (const juce::String& a, const juce::String& b)
    {
        return a.compareNatural(b) < 0;
    });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::set<juce::String> ControlTreeModel::nodePathsUnderRoots(const std::vector<TreeEntry>& sourceEntries,
                                                           const std::vector<juce::String>& roots)
{
    std::set<juce::String> result;
    if (roots.empty()) return result;

    for (const auto& entry : sourceEntries)
    {
        if (entry.kind != TreeEntryKind::node) continue;
        for (const auto& root : roots)
        {
            if (isSameOrDescendant(entry.path, root))
            {
                result.insert(entry.path);
                break;
            }
        }
    }
    return result;
}

void ControlTreeModel::upsertNode(const juce::String& path, const juce::String& identifier,
                                const juce::String& description)
{
    if (path.isEmpty()) return;
    const juce::ScopedLock sl(lock);
    auto& e = entries[path];
    const bool changed = e.path != path || e.kind != TreeEntryKind::node
        || e.identifier != identifier || e.description != description;
    e.kind = TreeEntryKind::node;
    e.path = path;
    e.parentPath = parentOf(path);
    if (identifier.isNotEmpty()) e.identifier = identifier;
    if (description.isNotEmpty()) e.description = description;
    if (changed) ++revisionCounter;
}

void ControlTreeModel::upsertParameter(const juce::String& path, const juce::String& identifier,
                                     const juce::String& description, bool writable, bool online)
{
    if (path.isEmpty()) return;
    const juce::ScopedLock sl(lock);
    auto& e = entries[path];
    const bool changed = e.path != path || e.kind != TreeEntryKind::parameter
        || e.identifier != identifier || e.description != description
        || e.writable != writable || e.online != online;
    e.kind = TreeEntryKind::parameter;
    e.path = path;
    e.parentPath = parentOf(path);
    e.identifier = identifier;
    e.description = description;
    e.writable = writable;
    e.online = online;
    if (changed) ++revisionCounter;
}

void ControlTreeModel::markDirectoryLoading(const juce::String& path, bool loading)
{
    const juce::ScopedLock sl(lock);
    if (path.isEmpty())
    {
        if (rootLoading != loading) { rootLoading = loading; ++revisionCounter; }
        return;
    }
    auto& e = entries[path];
    if (e.path.isEmpty())
    {
        e.kind = TreeEntryKind::node;
        e.path = path;
        e.parentPath = parentOf(path);
    }
    if (e.directoryLoading != loading) { e.directoryLoading = loading; ++revisionCounter; }
}

void ControlTreeModel::markDirectoryLoaded(const juce::String& path, bool loaded)
{
    const juce::ScopedLock sl(lock);
    if (path.isEmpty())
    {
        if (rootLoaded != loaded || rootLoading)
        {
            rootLoaded = loaded;
            rootLoading = false;
            ++revisionCounter;
        }
        return;
    }
    auto& e = entries[path];
    if (e.path.isEmpty())
    {
        e.kind = TreeEntryKind::node;
        e.path = path;
        e.parentPath = parentOf(path);
    }
    if (e.directoryLoaded != loaded || e.directoryLoading)
    {
        e.directoryLoaded = loaded;
        e.directoryLoading = false;
        ++revisionCounter;
    }
}

bool ControlTreeModel::isDirectoryLoaded(const juce::String& path) const
{
    const juce::ScopedLock sl(lock);
    if (path.isEmpty()) return rootLoaded;
    const auto it = entries.find(path);
    return it != entries.end() && it->second.kind == TreeEntryKind::node && it->second.directoryLoaded;
}

bool ControlTreeModel::isDirectoryLoading(const juce::String& path) const
{
    const juce::ScopedLock sl(lock);
    if (path.isEmpty()) return rootLoading;
    const auto it = entries.find(path);
    return it != entries.end() && it->second.kind == TreeEntryKind::node && it->second.directoryLoading;
}

std::vector<TreeEntry> ControlTreeModel::childrenOf(const juce::String& parentPath) const
{
    std::vector<TreeEntry> out;
    {
        const juce::ScopedLock sl(lock);
        for (const auto& [key, value] : entries)
            if (value.parentPath == parentPath) out.push_back(value);
    }
    std::sort(out.begin(), out.end(), [] (const TreeEntry& a, const TreeEntry& b)
    {
        if (a.kind != b.kind) return a.kind == TreeEntryKind::node;
        return a.displayName().compareNatural(b.displayName()) < 0;
    });
    return out;
}

std::vector<TreeEntry> ControlTreeModel::snapshot() const
{
    std::vector<TreeEntry> out;
    const juce::ScopedLock sl(lock);
    out.reserve(entries.size());
    for (const auto& [key, value] : entries) out.push_back(value);
    return out;
}
}
