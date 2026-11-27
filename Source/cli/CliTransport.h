#pragma once
#include <JuceHeader.h>
#include "CliCommandModel.h"

namespace er
{
constexpr int deviceControlCliPort = 37913;

class CliServer final : private juce::Thread
{
public:
    using Handler = std::function<CliCommandResult(const juce::String&, CliCommandModel::Mode)>;

    explicit CliServer(Handler handlerToUse);
    ~CliServer() override;

    bool start();
    void stop();

private:
    void run() override;
    Handler handler;
    std::unique_ptr<juce::StreamingSocket> listener;
};

class CliClient
{
public:
    struct Response
    {
        bool transportOk = false;
        bool commandOk = false;
        juce::String text;
        juce::String completedLine;
        std::vector<CliCandidate> candidates;
    };

    Response request(const juce::String& line, CliCommandModel::Mode mode,
                     int timeoutMs = 3000) const;
};
}
