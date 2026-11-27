#include "CliTransport.h"
#include <cstring>

namespace er
{
namespace
{
constexpr int maximumMessageBytes = 4 * 1024 * 1024;

bool readExact(juce::StreamingSocket& socket, void* destination, int bytes, int timeoutMs)
{
    auto* output = static_cast<char*>(destination);
    int received = 0;
    while (received < bytes)
    {
        if (socket.waitUntilReady(true, timeoutMs) <= 0) return false;
        const auto count = socket.read(output + received, bytes - received, false);
        if (count <= 0) return false;
        received += count;
    }
    return true;
}

bool writeExact(juce::StreamingSocket& socket, const void* source, int bytes)
{
    const auto* input = static_cast<const char*>(source);
    int written = 0;
    while (written < bytes)
    {
        const auto count = socket.write(input + written, bytes - written);
        if (count <= 0) return false;
        written += count;
    }
    return true;
}

bool receiveString(juce::StreamingSocket& socket, juce::String& result, int timeoutMs)
{
    juce::uint32 networkLength = 0;
    if (!readExact(socket, &networkLength, (int) sizeof(networkLength), timeoutMs)) return false;
    const auto length = (int) juce::ByteOrder::swapIfLittleEndian(networkLength);
    if (length < 0 || length > maximumMessageBytes) return false;
    juce::MemoryBlock bytes((size_t) length + 1, true);
    if (length > 0 && !readExact(socket, bytes.getData(), length, timeoutMs)) return false;
    result = juce::String::fromUTF8(static_cast<const char*>(bytes.getData()), length);
    return true;
}

bool sendString(juce::StreamingSocket& socket, const juce::String& text)
{
    const auto utf8 = text.toRawUTF8();
    const auto length = (int) std::strlen(utf8);
    const auto networkLength = juce::ByteOrder::swapIfLittleEndian((juce::uint32) length);
    return writeExact(socket, &networkLength, (int) sizeof(networkLength))
        && writeExact(socket, utf8, length);
}

juce::String modeName(CliCommandModel::Mode mode)
{
    switch (mode)
    {
        case CliCommandModel::Mode::execute: return "execute";
        case CliCommandModel::Mode::help: return "help";
        case CliCommandModel::Mode::complete: return "complete";
    }
    return "execute";
}

CliCommandModel::Mode modeFromName(const juce::String& mode)
{
    if (mode == "help") return CliCommandModel::Mode::help;
    if (mode == "complete") return CliCommandModel::Mode::complete;
    return CliCommandModel::Mode::execute;
}

juce::String serialiseResult(const CliCommandResult& result)
{
    auto object = new juce::DynamicObject();
    object->setProperty("ok", result.ok);
    object->setProperty("text", result.text);
    object->setProperty("completedLine", result.completedLine);
    juce::Array<juce::var> candidates;
    for (const auto& candidate : result.candidates)
    {
        auto item = new juce::DynamicObject();
        item->setProperty("token", candidate.token);
        item->setProperty("description", candidate.description);
        candidates.add(juce::var(item));
    }
    object->setProperty("candidates", candidates);
    return juce::JSON::toString(juce::var(object), true);
}

juce::String errorResponse(const juce::String& text)
{
    CliCommandResult result;
    result.ok = false;
    result.text = text;
    return serialiseResult(result);
}
}

CliServer::CliServer(Handler handlerToUse)
    : juce::Thread("DEVICEControl CLI server"), handler(std::move(handlerToUse))
{
}

CliServer::~CliServer() { stop(); }

bool CliServer::start()
{
    stop();
    listener = std::make_unique<juce::StreamingSocket>();
    if (!listener->createListener(deviceControlCliPort, "127.0.0.1"))
    {
        listener.reset();
        return false;
    }
    startThread();
    return true;
}

void CliServer::stop()
{
    signalThreadShouldExit();
    if (listener) listener->close();
    stopThread(3000);
    listener.reset();
}

void CliServer::run()
{
    while (!threadShouldExit() && listener)
    {
        std::unique_ptr<juce::StreamingSocket> connection(listener->waitForNextConnection());
        if (!connection || threadShouldExit()) continue;

        juce::String requestText;
        if (!receiveString(*connection, requestText, 3000)) continue;
        const auto request = juce::JSON::parse(requestText);
        if (!request.isObject())
        {
            sendString(*connection, errorResponse("Invalid CLI request."));
            continue;
        }

        const auto line = request.getProperty("line", {}).toString();
        const auto mode = modeFromName(request.getProperty("mode", "execute").toString());
        struct Pending
        {
            juce::WaitableEvent completed;
            juce::String response;
        };
        const auto pending = std::make_shared<Pending>();
        const auto callback = handler;
        juce::MessageManager::callAsync([pending, callback, line, mode]
        {
            pending->response = callback ? serialiseResult(callback(line, mode))
                                         : errorResponse("CLI service is unavailable.");
            pending->completed.signal();
        });

        bool completed = false;
        for (int waited = 0; waited < 5000 && !threadShouldExit(); waited += 50)
            if (pending->completed.wait(50))
            {
                completed = true;
                break;
            }
        if (threadShouldExit()) continue;
        if (!completed)
            pending->response = errorResponse("DEVICEControl did not answer within five seconds.");
        sendString(*connection, pending->response);
    }
}

CliClient::Response CliClient::request(const juce::String& line, CliCommandModel::Mode mode,
                                       int timeoutMs) const
{
    Response response;
    juce::StreamingSocket socket;
    if (!socket.connect("127.0.0.1", deviceControlCliPort, timeoutMs))
    {
        response.text = "Could not connect to DEVICEControl. Start the GUI application first.";
        return response;
    }

    auto request = new juce::DynamicObject();
    request->setProperty("line", line);
    request->setProperty("mode", modeName(mode));
    if (!sendString(socket, juce::JSON::toString(juce::var(request))))
    {
        response.text = "Could not send the CLI request.";
        return response;
    }

    juce::String responseText;
    if (!receiveString(socket, responseText, timeoutMs))
    {
        response.text = "DEVICEControl did not return a valid response.";
        return response;
    }

    const auto parsed = juce::JSON::parse(responseText);
    if (!parsed.isObject())
    {
        response.text = "DEVICEControl returned invalid JSON.";
        return response;
    }
    response.transportOk = true;
    response.commandOk = static_cast<bool>(parsed.getProperty("ok", false));
    response.text = parsed.getProperty("text", {}).toString();
    response.completedLine = parsed.getProperty("completedLine", {}).toString();
    if (const auto* candidates = parsed.getProperty("candidates", {}).getArray())
        for (const auto& candidate : *candidates)
            response.candidates.push_back({ candidate.getProperty("token", {}).toString(),
                                            candidate.getProperty("description", {}).toString() });
    return response;
}
}
