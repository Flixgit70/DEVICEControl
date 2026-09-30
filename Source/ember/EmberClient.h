#pragma once
#include <JuceHeader.h>
#include "IEmberClientBackend.h"
#include <ember/Ember.hpp>
#include <s101/StreamDecoder.hpp>
#include <s101/StreamEncoder.hpp>
#include <s101/MessageType.hpp>
#include <s101/CommandType.hpp>
#include <s101/Dtd.hpp>
#include <s101/PackageFlag.hpp>
#include <set>
#include <deque>
#include <map>
#include <atomic>
#include <vector>

namespace er
{
class EmberClient final : public IEmberClientBackend, private juce::Thread
{
public:
    EmberClient();
    ~EmberClient() override;

    bool connectTo(const juce::String& host, int port) override;
    void disconnect() override;
    bool requestDirectory(const juce::String& numericPath) override;
    bool refreshDirectory(const juce::String& numericPath) override;
    void startSubtreeScan(const std::vector<juce::String>& numericRoots) override;
    bool setParameterValue(const juce::String& numericPath, const juce::var& newValue) override;
    bool setMatrixCrosspoint(const juce::String& numericPath, int target, int source,
                             bool connect) override;

    bool isConnected() const noexcept override { return connected.load(); }
    bool isScanning() const noexcept override { return scanning.load(); }
    juce::String statusText() const override;
    juce::String endpointText() const override;
    int discoveredParameterCount() const override { return store.size(); }
    ParameterStore& parameters() noexcept override { return store; }
    const ParameterStore& parameters() const noexcept override { return store; }
    ControlTreeModel& tree() noexcept override { return treeModel; }
    const ControlTreeModel& tree() const noexcept override { return treeModel; }

private:
    using Decoder = libs101::StreamDecoder<unsigned char>;
    using Path = std::vector<int>;

    void run() override;
    void setStatus(juce::String text);
    void handleS101Packet(Decoder::const_iterator first, Decoder::const_iterator last);
    static void dispatchS101(Decoder::const_iterator first, Decoder::const_iterator last, EmberClient* self);
    void handleDecodedRoot(const libember::dom::Node* root);
    void visitContainer(const libember::dom::Container& container, const Path& basePath);
    void visitNode(const libember::glow::GlowNodeBase& node, const Path& path);
    void visitMatrix(const libember::glow::GlowMatrixBase& matrix, const Path& path);
    void visitParameter(const libember::glow::GlowParameterBase& parameter, const Path& path);
    ParameterInfo makeParameterInfo(const libember::glow::GlowParameterBase& parameter, const Path& path) const;
    juce::String makeTreePath(const Path& parameterPath, const juce::String& parameterName) const;

    bool queueDirectory(const Path& path, bool refreshOnly = false);
    void requestNextDirectory();
    void finishRequest(const Path& path);
    bool sendDirectoryRequest(const Path& path);
    bool sendGlowTree(const libember::dom::Node& root);
    bool sendKeepAliveResponse(unsigned char slot);
    bool writeS101Bytes(const std::vector<unsigned char>& bytes);
    bool pathIsInsideRecursiveSelection(const Path& path) const;
    void seedRecursiveScan(const Path& root);

    static juce::String pathToString(const Path& path);
    static Path stringToPath(const juce::String& path);
    static Path oidToPath(const libember::ber::ObjectIdentifier& oid);
    static libember::ber::ObjectIdentifier pathToOid(const Path& path);
    static juce::var glowValueToVar(const libember::glow::Value& value);
    static ParameterKind toParameterKind(libember::glow::ParameterType type);

    std::unique_ptr<juce::StreamingSocket> socket;
    mutable juce::CriticalSection stateLock;
    juce::CriticalSection writeLock;
    mutable juce::CriticalSection scanLock;
    juce::String hostName;
    int tcpPort = 9000;
    juce::String status { "Disconnected" };
    std::atomic<bool> connected { false };
    std::atomic<bool> scanning { false };

    Decoder decoder;
    libember::dom::AsyncDomReader domReader;
    ParameterStore store;
    ControlTreeModel treeModel;

    std::deque<Path> pendingDirectories;
    std::deque<Path> pendingRefreshDirectories;
    std::set<juce::String> queuedDirectories;
    std::map<juce::String, juce::String> nodeIdentifiers;
    std::vector<Path> recursiveRoots;
    bool requestInFlight = false;
    Path inFlightPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmberClient)
};
}
