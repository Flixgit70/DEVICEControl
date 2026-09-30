#pragma once
#include <JuceHeader.h>
#include <map>
#include <vector>
#include <set>
#include <cstdint>
#include <optional>

namespace er
{
enum class TreeEntryKind { node, matrix, parameter };

struct MatrixConnectionInfo
{
    int target = 0;
    std::vector<int> sources;
    int operation = 0;
    int disposition = 0;

    bool operator==(const MatrixConnectionInfo& other) const noexcept
    {
        return target == other.target && sources == other.sources
            && operation == other.operation && disposition == other.disposition;
    }
};

struct MatrixLabelInfo
{
    juce::String basePath;
    juce::String description;

    bool operator==(const MatrixLabelInfo& other) const noexcept
    {
        return basePath == other.basePath && description == other.description;
    }
};

/** A partial Matrix update. Optional scalar properties and the collection
    presence flags are important because Ember+ providers may send metadata,
    signal lists and connection tallies in separate updates. */
struct MatrixInfo
{
    std::optional<int> type;
    std::optional<int> addressingMode;
    std::optional<int> targetCount;
    std::optional<int> sourceCount;
    std::optional<int> maximumTotalConnects;
    std::optional<int> maximumConnectsPerTarget;
    bool targetsPresent = false;
    bool sourcesPresent = false;
    bool connectionsPresent = false;
    bool labelsPresent = false;
    std::vector<int> targets;
    std::vector<int> sources;
    std::vector<MatrixConnectionInfo> connections;
    std::vector<MatrixLabelInfo> labels;

    int activeCrosspointCount() const noexcept;
    juce::String summary() const;
    juce::String details() const;
};


struct TreeFilterResult
{
    std::set<juce::String> matches;
    std::set<juce::String> visible; // matches + every parent path needed to display them
};

struct TreeEntry
{
    TreeEntryKind kind = TreeEntryKind::node;
    juce::String path;
    juce::String parentPath;
    juce::String identifier;
    juce::String description;
    bool writable = false;
    bool online = true;
    bool directoryLoaded = false;
    bool directoryLoading = false;
    MatrixInfo matrix;

    juce::String displayName() const
    {
        if (identifier.isNotEmpty()) return identifier;
        if (description.isNotEmpty()) return description;
        if (path.isEmpty()) return "Root";
        const auto dot = path.lastIndexOfChar('.');
        return dot >= 0 ? path.substring(dot + 1) : path;
    }
};

/** GUI-neutral, thread-safe snapshot of the currently browsed provider tree.
    This is intentionally not a full mirror of the provider. Directories only
    appear as the user opens them (or when a selected subtree is collected).
*/
class ControlTreeModel
{
public:
    void clear();
    void upsertNode(const juce::String& path, const juce::String& identifier,
                    const juce::String& description = {});
    void upsertParameter(const juce::String& path, const juce::String& identifier,
                         const juce::String& description, bool writable, bool online);
    void upsertMatrix(const juce::String& path, const juce::String& identifier,
                      const juce::String& description, const MatrixInfo& update);

    void markDirectoryLoading(const juce::String& path, bool loading);
    void markDirectoryLoaded(const juce::String& path, bool loaded = true);

    bool isDirectoryLoaded(const juce::String& path) const;
    bool isDirectoryLoading(const juce::String& path) const;
    bool isMatrix(const juce::String& path) const;
    std::optional<TreeEntry> find(const juce::String& path) const;
    std::vector<TreeEntry> childrenOf(const juce::String& parentPath) const;
    std::vector<TreeEntry> snapshot() const;
    std::uint64_t revision() const noexcept { return revisionCounter.load(); }

    static juce::String parentOf(const juce::String& path);
    // Compare Ember path components as numbers, including provider-qualified paths.
    static bool numericPathLess(const juce::String& a, const juce::String& b);
    static bool isSameOrDescendant(const juce::String& path, const juce::String& root);
    // Local-entry helper retained for simple metadata checks. The actual browser
    // filter below is path-aware: normal whitespace-separated tokens are ANDed
    // across the complete loaded ancestry of each candidate entry. Tokens
    // prefixed with '-' are exclusions and must not occur anywhere in that
    // candidate's complete loaded path/search metadata.
    static bool matchesFilter(const TreeEntry& entry, const juce::String& query);
    static TreeFilterResult buildFilter(const std::vector<TreeEntry>& entries, const juce::String& query);
    static std::vector<juce::String> parameterPathsMatchingFilter(const std::vector<TreeEntry>& entries,
                                                                   const juce::String& query);
    static std::set<juce::String> nodePathsUnderRoots(const std::vector<TreeEntry>& entries,
                                                       const std::vector<juce::String>& roots);

private:
    mutable juce::CriticalSection lock;
    std::map<juce::String, TreeEntry> entries;
    bool rootLoaded = false;
    bool rootLoading = false;
    std::atomic<std::uint64_t> revisionCounter { 0 };
};
}
