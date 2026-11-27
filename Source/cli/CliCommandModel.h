#pragma once
#include <JuceHeader.h>
#include "../model/ControlTreeModel.h"
#include "../model/PageModel.h"
#include "../model/ParameterInfo.h"
#include "../model/ProviderConfig.h"

namespace er
{
struct CliSnapshot
{
    std::vector<ProviderConfig> providers;
    std::vector<TreeEntry> tree;
    std::vector<ParameterInfo> parameters;
    std::vector<PageModel> pages;
    int currentPage = 0;
    juce::String status;
    juce::String appVersion;
};

struct CliCandidate
{
    juce::String token;
    juce::String description;
};

struct CliCommandResult
{
    bool ok = true;
    juce::String text;
    juce::String completedLine;
    juce::String requestDirectory;
    std::vector<CliCandidate> candidates;
};

class CliCommandModel
{
public:
    enum class Mode { execute, help, complete };

    static CliCommandResult process(const juce::String& line, Mode mode,
                                    const CliSnapshot& snapshot);
};
}
