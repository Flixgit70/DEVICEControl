#include "ControlTreeModel.h"
#include <algorithm>

namespace er
{
namespace
{
juce::String matrixTypeName(int value)
{
    switch (value)
    {
        case 0: return "1:N";
        case 1: return "1:1";
        case 2: return "N:N";
        default: return "Type " + juce::String(value);
    }
}

juce::String addressingModeName(int value)
{
    switch (value)
    {
        case 0: return "linear";
        case 1: return "non-linear";
        default: return "addressing " + juce::String(value);
    }
}

juce::String joinNumbers(const std::vector<int>& values)
{
    juce::StringArray text;
    for (const auto value : values) text.add(juce::String(value));
    return text.joinIntoString(", ");
}

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

int MatrixInfo::activeCrosspointCount() const noexcept
{
    int count = 0;
    for (const auto& connection : connections) count += (int) connection.sources.size();
    return count;
}

juce::String MatrixInfo::summary() const
{
    juce::String text;
    if (type) text << matrixTypeName(*type) << " ";
    if (addressingMode) text << addressingModeName(*addressingMode) << " ";

    if (sourceCount || targetCount)
        text << (sourceCount ? juce::String(*sourceCount) : "?") << "x"
             << (targetCount ? juce::String(*targetCount) : "?") << " ";

    if (connectionsPresent)
        text << activeCrosspointCount() << " XP";

    return text.trim();
}

juce::String MatrixInfo::details() const
{
    juce::String text;
    text << "Type: " << (type ? matrixTypeName(*type) : juce::String("not reported")) << "\n";
    text << "Addressing: " << (addressingMode ? addressingModeName(*addressingMode) : juce::String("not reported")) << "\n";
    text << "Sources: " << (sourceCount ? juce::String(*sourceCount) : juce::String("not reported"));
    if (sourcesPresent) text << "  [" << joinNumbers(sources) << "]";
    text << "\nTargets: " << (targetCount ? juce::String(*targetCount) : juce::String("not reported"));
    if (targetsPresent) text << "  [" << joinNumbers(targets) << "]";
    text << "\nActive crosspoints: " << juce::String(activeCrosspointCount());

    if (maximumTotalConnects) text << "\nMaximum total connects: " << juce::String(*maximumTotalConnects);
    if (maximumConnectsPerTarget) text << "\nMaximum per target: " << juce::String(*maximumConnectsPerTarget);

    if (connectionsPresent)
    {
        text << "\n\nConnections:";
        if (connections.empty()) text << " none reported";
        for (const auto& connection : connections)
            text << "\nTarget " << juce::String(connection.target) << " <- "
                 << (connection.sources.empty() ? juce::String("none") : joinNumbers(connection.sources));
    }
    if (labelsPresent)
    {
        text << "\n\nLabel sets:";
        if (labels.empty()) text << " none reported";
        for (const auto& label : labels)
            text << "\n" << (label.description.isNotEmpty() ? label.description : juce::String("Label"))
                 << ": " << label.basePath;
    }
    return text;
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

bool ControlTreeModel::numericPathLess(const juce::String& a, const juce::String& b)
{
    auto localPath = [] (const juce::String& path)
    {
        if (!path.startsWithChar('@')) return path;
        const auto dot = path.indexOfChar('.');
        return dot < 0 ? juce::String() : path.substring(dot + 1);
    };

    juce::StringArray left, right;
    left.addTokens(localPath(a), ".", "");
    right.addTokens(localPath(b), ".", "");
    const auto count = juce::jmin(left.size(), right.size());
    for (int i = 0; i < count; ++i)
    {
        const auto& x = left[i];
        const auto& y = right[i];
        if (!x.containsOnly("0123456789") || !y.containsOnly("0123456789"))
            return a.compareNatural(b) < 0;

        // Compare decimal components without narrowing large Ember node numbers.
        const auto nx = x.trimCharactersAtStart("0");
        const auto ny = y.trimCharactersAtStart("0");
        if (nx.length() != ny.length()) return nx.length() < ny.length();
        const auto cmp = nx.compare(ny);
        if (cmp != 0) return cmp < 0;
    }
    if (left.size() != right.size()) return left.size() < right.size();
    return a.compareNatural(b) < 0;
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
        || (identifier.isNotEmpty() && e.identifier != identifier)
        || (description.isNotEmpty() && e.description != description);
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

void ControlTreeModel::upsertMatrix(const juce::String& path, const juce::String& identifier,
                                   const juce::String& description, const MatrixInfo& update)
{
    if (path.isEmpty()) return;
    const juce::ScopedLock sl(lock);
    auto& e = entries[path];
    bool changed = e.path != path || e.kind != TreeEntryKind::matrix;
    e.kind = TreeEntryKind::matrix;
    e.path = path;
    e.parentPath = parentOf(path);
    if (identifier.isNotEmpty() && e.identifier != identifier) { e.identifier = identifier; changed = true; }
    if (description.isNotEmpty() && e.description != description) { e.description = description; changed = true; }

    auto mergeScalar = [&changed] (auto& destination, const auto& source)
    {
        if (source && destination != source) { destination = source; changed = true; }
    };
    mergeScalar(e.matrix.type, update.type);
    mergeScalar(e.matrix.addressingMode, update.addressingMode);
    mergeScalar(e.matrix.targetCount, update.targetCount);
    mergeScalar(e.matrix.sourceCount, update.sourceCount);
    mergeScalar(e.matrix.maximumTotalConnects, update.maximumTotalConnects);
    mergeScalar(e.matrix.maximumConnectsPerTarget, update.maximumConnectsPerTarget);

    if (update.targetsPresent
        && (!e.matrix.targetsPresent || e.matrix.targets != update.targets))
    {
        e.matrix.targetsPresent = true;
        e.matrix.targets = update.targets;
        changed = true;
    }
    if (update.sourcesPresent
        && (!e.matrix.sourcesPresent || e.matrix.sources != update.sources))
    {
        e.matrix.sourcesPresent = true;
        e.matrix.sources = update.sources;
        changed = true;
    }
    if (update.connectionsPresent
        && !e.matrix.connectionsPresent)
    {
        e.matrix.connectionsPresent = true;
        e.matrix.connections = update.connections;
        changed = true;
    }
    else if (update.connectionsPresent && update.connections.empty() && !e.matrix.connections.empty())
    {
        e.matrix.connections.clear();
        changed = true;
    }
    else if (update.connectionsPresent)
    {
        // Connection notifications usually report one changed target. Merge
        // those tallies instead of discarding every other known crosspoint.
        for (const auto& connection : update.connections)
        {
            const auto found = std::find_if(e.matrix.connections.begin(), e.matrix.connections.end(),
                                            [&connection] (const auto& existing)
                                            {
                                                return existing.target == connection.target;
                                            });
            if (found == e.matrix.connections.end())
            {
                e.matrix.connections.push_back(connection);
                changed = true;
            }
            else if (!(*found == connection))
            {
                *found = connection;
                changed = true;
            }
        }
    }
    if (update.labelsPresent
        && (!e.matrix.labelsPresent || e.matrix.labels != update.labels))
    {
        e.matrix.labelsPresent = true;
        e.matrix.labels = update.labels;
        changed = true;
    }

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
    return it != entries.end() && it->second.kind != TreeEntryKind::parameter && it->second.directoryLoaded;
}

bool ControlTreeModel::isDirectoryLoading(const juce::String& path) const
{
    const juce::ScopedLock sl(lock);
    if (path.isEmpty()) return rootLoading;
    const auto it = entries.find(path);
    return it != entries.end() && it->second.kind != TreeEntryKind::parameter && it->second.directoryLoading;
}

bool ControlTreeModel::isMatrix(const juce::String& path) const
{
    const juce::ScopedLock sl(lock);
    const auto it = entries.find(path);
    return it != entries.end() && it->second.kind == TreeEntryKind::matrix;
}

std::optional<TreeEntry> ControlTreeModel::find(const juce::String& path) const
{
    const juce::ScopedLock sl(lock);
    const auto it = entries.find(path);
    if (it == entries.end()) return std::nullopt;
    return it->second;
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
        if (a.kind != b.kind)
        {
            if (a.kind == TreeEntryKind::node) return true;
            if (b.kind == TreeEntryKind::node) return false;
            return a.kind == TreeEntryKind::matrix;
        }
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
