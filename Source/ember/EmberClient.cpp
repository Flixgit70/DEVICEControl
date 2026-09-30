#include "EmberClient.h"
#include <array>
#include <algorithm>
#include <iterator>

namespace er
{
using namespace libember;

EmberClient::EmberClient()
    : juce::Thread("Ember+ network"), domReader(glow::GlowNodeFactory::getFactory())
{
}

EmberClient::~EmberClient() { disconnect(); }

bool EmberClient::connectTo(const juce::String& newHost, int newPort)
{
    disconnect();
    hostName = newHost.trim();
    tcpPort = newPort > 0 ? newPort : 9000;
    setStatus("Connecting to " + hostName + ":" + juce::String(tcpPort) + " ...");

    socket = std::make_unique<juce::StreamingSocket>();
    if (!socket->connect(hostName, tcpPort, 2500))
    {
        socket.reset();
        connected = false;
        setStatus("Connection failed");
        return false;
    }

    store.clear();
    treeModel.clear();
    {
        const juce::ScopedLock sl(scanLock);
        pendingDirectories.clear();
        pendingRefreshDirectories.clear();
        queuedDirectories.clear();
        nodeIdentifiers.clear();
        recursiveRoots.clear();
        requestInFlight = false;
        inFlightPath.clear();
    }

    connected = true;
    scanning = false;
    setStatus("Connected - loading root ...");
    startThread();
    requestDirectory(juce::String());
    return true;
}

void EmberClient::disconnect()
{
    signalThreadShouldExit();
    if (socket) socket->close();
    stopThread(2000);
    socket.reset();
    connected = false;
    scanning = false;
    {
        const juce::ScopedLock sl(scanLock);
        requestInFlight = false;
        pendingDirectories.clear();
        pendingRefreshDirectories.clear();
        queuedDirectories.clear();
        nodeIdentifiers.clear();
        recursiveRoots.clear();
        inFlightPath.clear();
    }
    domReader.reset();
    setStatus("Disconnected");
}

void EmberClient::run()
{
    std::array<unsigned char, 16384> buffer {};
    while (!threadShouldExit() && socket && connected.load())
    {
        const auto ready = socket->waitUntilReady(true, 100);
        if (ready < 0) break;
        if (ready == 0) continue;
        const int count = socket->read(buffer.data(), (int) buffer.size(), false);
        if (count <= 0) break;

        try { decoder.read(buffer.begin(), buffer.begin() + count, &EmberClient::dispatchS101, this); }
        catch (const std::exception& e) { setStatus("Protocol error: " + juce::String(e.what())); }
    }

    if (!threadShouldExit())
    {
        connected = false;
        scanning = false;
        setStatus("Connection closed by provider");
    }
}

void EmberClient::dispatchS101(Decoder::const_iterator first, Decoder::const_iterator last, EmberClient* self)
{
    if (self) self->handleS101Packet(first, last);
}

void EmberClient::handleS101Packet(Decoder::const_iterator first, Decoder::const_iterator last)
{
    const auto length = std::distance(first, last);
    if (length < 4) return;

    const unsigned char slot = *first;
    const unsigned char messageType = *(first + 1);
    const unsigned char command = *(first + 2);
    if (messageType != libs101::MessageType(libs101::MessageType::EmBER).value()) return;

    if (command == libs101::CommandType(libs101::CommandType::KeepAliveRequest).value())
    {
        sendKeepAliveResponse(slot);
        return;
    }
    if (command != libs101::CommandType(libs101::CommandType::EmBER).value() || length < 7) return;

    const unsigned char flags = *(first + 4);
    const int appBytes = *(first + 6);
    const auto headerBytes = 7 + appBytes;
    if (length < headerBytes) return;

    const bool isLastPackage = (flags & libs101::PackageFlag::LastPackage) != 0;

    try
    {
        // Keep one DOM reader alive for the complete S101 package sequence.
        // A large GetDirectory response may span several S101 packets; marking
        // the directory complete after the first decoded root makes a deep scan
        // stop before later siblings/descendants have even been discovered.
        // Lawo's TinyEmber v1.8.2 consumer likewise resets only after LastPackage.
        domReader.read(first + headerBytes, last);
        if (domReader.isRootReady())
        {
            std::unique_ptr<dom::Node> root(domReader.detachRoot());
            if (root) handleDecodedRoot(root.get());
        }

        if (isLastPackage)
        {
            domReader.reset();

            Path completedPath;
            bool completesDirectoryRequest = false;
            {
                const juce::ScopedLock sl(scanLock);
                if (requestInFlight)
                {
                    completedPath = inFlightPath;
                    completesDirectoryRequest = true;
                }
            }

            // Only advance the recursive GetDirectory queue when the complete
            // S101 package sequence has arrived. Any newly discovered child
            // nodes have already been queued by visitNode() at this point.
            if (completesDirectoryRequest) finishRequest(completedPath);
        }
    }
    catch (const std::exception& e)
    {
        domReader.reset();
        setStatus("Glow decode error: " + juce::String(e.what()));
    }
}

void EmberClient::handleDecodedRoot(const dom::Node* root)
{
    if (auto* container = dynamic_cast<const dom::Container*>(root)) visitContainer(*container, {});
}

void EmberClient::visitContainer(const dom::Container& container, const Path& basePath)
{
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        const auto& item = *it;

        if (auto* qn = dynamic_cast<const glow::GlowQualifiedNode*>(&item))
        {
            visitNode(*qn, oidToPath(qn->path()));
            continue;
        }
        if (auto* n = dynamic_cast<const glow::GlowNode*>(&item))
        {
            auto path = basePath;
            path.push_back(n->number());
            visitNode(*n, path);
            continue;
        }
        if (auto* qm = dynamic_cast<const glow::GlowQualifiedMatrix*>(&item))
        {
            visitMatrix(*qm, oidToPath(qm->path()));
            continue;
        }
        if (auto* m = dynamic_cast<const glow::GlowMatrix*>(&item))
        {
            auto path = basePath;
            path.push_back(m->number());
            visitMatrix(*m, path);
            continue;
        }
        if (auto* qp = dynamic_cast<const glow::GlowQualifiedParameter*>(&item))
        {
            visitParameter(*qp, oidToPath(qp->path()));
            continue;
        }
        if (auto* p = dynamic_cast<const glow::GlowParameter*>(&item))
        {
            auto path = basePath;
            path.push_back(p->number());
            visitParameter(*p, path);
            continue;
        }
        if (auto* nested = dynamic_cast<const dom::Container*>(&item)) visitContainer(*nested, basePath);
    }
}

void EmberClient::visitMatrix(const glow::GlowMatrixBase& matrix, const Path& path)
{
    const auto key = pathToString(path);
    juce::String identifier;
    if (matrix.contains(glow::MatrixProperty::Identifier)) identifier = juce::String(matrix.identifier());
    juce::String description;
    if (matrix.contains(glow::MatrixProperty::Description)) description = juce::String(matrix.description());

    MatrixInfo update;
    if (matrix.contains(glow::MatrixProperty::Type))
        update.type = matrix.type().value();
    if (matrix.contains(glow::MatrixProperty::AddressingMode))
        update.addressingMode = matrix.addressingMode().value();
    if (matrix.contains(glow::MatrixProperty::TargetCount))
        update.targetCount = matrix.targetCount();
    if (matrix.contains(glow::MatrixProperty::SourceCount))
        update.sourceCount = matrix.sourceCount();
    if (matrix.contains(glow::MatrixProperty::MaximumTotalConnects))
        update.maximumTotalConnects = matrix.maximumTotalConnects();
    if (matrix.contains(glow::MatrixProperty::MaximumConnectsPerTarget))
        update.maximumConnectsPerTarget = matrix.maximumConnectsPerTarget();

    if (matrix.targets() != nullptr)
    {
        update.targetsPresent = true;
        std::vector<const glow::GlowTarget*> targets;
        matrix.typedTargets(std::back_inserter(targets));
        for (const auto* target : targets)
            if (target != nullptr) update.targets.push_back(target->number());
    }

    if (matrix.sources() != nullptr)
    {
        update.sourcesPresent = true;
        std::vector<const glow::GlowSource*> sources;
        matrix.typedSources(std::back_inserter(sources));
        for (const auto* source : sources)
            if (source != nullptr) update.sources.push_back(source->number());
    }

    if (matrix.connections() != nullptr)
    {
        update.connectionsPresent = true;
        std::vector<const glow::GlowConnection*> connections;
        matrix.typedConnections(std::back_inserter(connections));
        for (const auto* connection : connections)
        {
            if (connection == nullptr) continue;
            MatrixConnectionInfo info;
            info.target = connection->target();
            const auto sourceIds = connection->sources();
            info.sources.assign(sourceIds.begin(), sourceIds.end());
            info.operation = connection->operation().value();
            info.disposition = connection->disposition().value();
            update.connections.push_back(std::move(info));
        }
    }

    if (matrix.labels() != nullptr)
    {
        update.labelsPresent = true;
        std::vector<const glow::GlowLabel*> labels;
        matrix.typedLabels(std::back_inserter(labels));
        for (const auto* label : labels)
        {
            if (label == nullptr) continue;
            MatrixLabelInfo info;
            info.basePath = pathToString(oidToPath(label->basePath()));
            info.description = juce::String(label->description());
            update.labels.push_back(std::move(info));
        }
    }

    treeModel.upsertMatrix(key, identifier, description, update);
    if (auto* children = matrix.children()) visitContainer(*children, path);
}

void EmberClient::visitNode(const glow::GlowNodeBase& node, const Path& path)
{
    const auto key = pathToString(path);
    juce::String identifier;
    if (node.contains(glow::NodeProperty::Identifier)) identifier = juce::String(node.identifier());
    juce::String description;
    if (node.contains(glow::NodeProperty::Description)) description = juce::String(node.description());

    {
        const juce::ScopedLock sl(scanLock);
        if (identifier.isNotEmpty()) nodeIdentifiers[key] = identifier;
    }
    treeModel.upsertNode(key, identifier, description);

    if (auto* children = node.children()) visitContainer(*children, path);

    if (scanning.load() && pathIsInsideRecursiveSelection(path))
        queueDirectory(path);
}

void EmberClient::visitParameter(const glow::GlowParameterBase& parameter, const Path& path)
{
    auto info = makeParameterInfo(parameter, path);
    store.upsert(info);
    treeModel.upsertParameter(info.path, info.identifier, info.description, info.writable, info.online);
}

ParameterInfo EmberClient::makeParameterInfo(const glow::GlowParameterBase& p, const Path& path) const
{
    const auto key = pathToString(path);
    ParameterInfo info;
    if (auto old = store.find(key)) info = *old;
    info.path = key;

    if (p.contains(glow::ParameterProperty::Identifier)) info.identifier = p.identifier();
    if (p.contains(glow::ParameterProperty::Description)) info.description = p.description();
    info.treePath = makeTreePath(path, info.displayName());
    if (p.contains(glow::ParameterProperty::Type) || info.kind == ParameterKind::unknown) info.kind = toParameterKind(p.effectiveType());
    if (p.contains(glow::ParameterProperty::Value)) info.value = glowValueToVar(p.value());
    if (p.contains(glow::ParameterProperty::Minimum))
    {
        auto v = p.minimum();
        info.minimum = v.type().value() == glow::ParameterType::Real ? juce::var(v.toReal()) : juce::var((juce::int64) v.toInteger());
    }
    if (p.contains(glow::ParameterProperty::Maximum))
    {
        auto v = p.maximum();
        info.maximum = v.type().value() == glow::ParameterType::Real ? juce::var(v.toReal()) : juce::var((juce::int64) v.toInteger());
    }
    if (p.contains(glow::ParameterProperty::Factor)) info.factor = p.factor();
    if (p.contains(glow::ParameterProperty::Access)) info.writable = p.isWriteable();
    if (p.contains(glow::ParameterProperty::IsOnline)) info.online = p.isOnline();

    if (p.contains(glow::ParameterProperty::Enumeration) || p.contains(glow::ParameterProperty::EnumMap))
    {
        info.enumEntries.clear();
        auto enumeration = p.enumerationMap();
        if (enumeration.size() == 0) enumeration = p.enumeration();
        for (auto it = enumeration.begin(); it != enumeration.end(); ++it)
            info.enumEntries.add(juce::String(it->second) + ":" + juce::String(it->first));
    }
    return info;
}

bool EmberClient::requestDirectory(const juce::String& numericPath)
{
    if (!connected.load()) return false;
    const auto path = stringToPath(numericPath);
    if (treeModel.isDirectoryLoaded(numericPath) || treeModel.isDirectoryLoading(numericPath)) return true;
    const bool queued = queueDirectory(path);
    requestNextDirectory();
    return queued || treeModel.isDirectoryLoading(numericPath) || treeModel.isDirectoryLoaded(numericPath);
}

bool EmberClient::refreshDirectory(const juce::String& numericPath)
{
    if (!connected.load()) return false;
    const auto path = stringToPath(numericPath);
    const bool queued = queueDirectory(path, true);
    requestNextDirectory();
    return queued;
}

void EmberClient::startSubtreeScan(const std::vector<juce::String>& numericRoots)
{
    if (!connected.load() || numericRoots.empty()) return;

    std::vector<Path> roots;
    for (const auto& r : numericRoots)
    {
        const auto path = stringToPath(r);
        if (r.isEmpty() || !path.empty()) roots.push_back(path);
    }
    if (roots.empty()) return;

    {
        const juce::ScopedLock sl(scanLock);
        recursiveRoots = roots;
    }
    scanning = true;
    setStatus("Collecting selected Ember+ branch" + juce::String(roots.size() == 1 ? " ..." : "es ..."));

    for (const auto& root : roots) seedRecursiveScan(root);
    requestNextDirectory();
}

void EmberClient::seedRecursiveScan(const Path& root)
{
    const auto key = pathToString(root);
    if (store.find(key).has_value()) return; // a selected leaf parameter needs no GetDirectory

    if (!treeModel.isDirectoryLoaded(key))
    {
        queueDirectory(root);
        return;
    }

    for (const auto& child : treeModel.childrenOf(key))
        if (child.kind == TreeEntryKind::node) seedRecursiveScan(stringToPath(child.path));
}

bool EmberClient::pathIsInsideRecursiveSelection(const Path& path) const
{
    std::vector<Path> roots;
    {
        const juce::ScopedLock sl(scanLock);
        roots = recursiveRoots;
    }
    for (const auto& root : roots)
    {
        if (path.size() < root.size()) continue;
        if (std::equal(root.begin(), root.end(), path.begin())) return true;
    }
    return false;
}

bool EmberClient::queueDirectory(const Path& path, bool refreshOnly)
{
    const auto key = pathToString(path);
    if (!refreshOnly && (treeModel.isDirectoryLoaded(key) || treeModel.isDirectoryLoading(key))) return false;

    {
        const juce::ScopedLock sl(scanLock);
        if (requestInFlight && pathToString(inFlightPath) == key) return false;
        if (!queuedDirectories.insert(key).second) return false;
        if (refreshOnly) pendingRefreshDirectories.push_back(path);
        else pendingDirectories.push_back(path);
    }
    if (!refreshOnly) treeModel.markDirectoryLoading(key, true);
    return true;
}

void EmberClient::requestNextDirectory()
{
    if (!connected.load()) return;
    Path next;
    bool haveNext = false;
    bool refreshOnly = false;
    bool shouldFinishScan = false;
    {
        const juce::ScopedLock sl(scanLock);
        if (requestInFlight) return;
        if (!pendingDirectories.empty())
        {
            next = pendingDirectories.front();
            pendingDirectories.pop_front();
            haveNext = true;
        }
        else if (scanning.load())
        {
            shouldFinishScan = true;
        }
        else if (!pendingRefreshDirectories.empty())
        {
            next = pendingRefreshDirectories.front();
            pendingRefreshDirectories.pop_front();
            refreshOnly = true;
            haveNext = true;
        }

        if (haveNext)
        {
            queuedDirectories.erase(pathToString(next));
            inFlightPath = next;
            requestInFlight = true;
        }
    }

    if (!haveNext)
    {
        if (shouldFinishScan)
        {
            scanning = false;
            {
                const juce::ScopedLock sl(scanLock);
                recursiveRoots.clear();
            }
            setStatus("Selected branches ready - " + juce::String(store.size()) + " loaded parameters");
        }
        else if (connected.load())
        {
            setStatus("Connected - browse the Ember+ tree");
        }
        return;
    }

    const auto key = pathToString(next);
    if (sendDirectoryRequest(next))
    {
        if (!refreshOnly)
            setStatus((scanning.load() ? "Collecting " : "Loading ") + (key.isEmpty() ? juce::String("root") : key)
                      + " - " + juce::String(store.size()) + " parameters loaded");
    }
    else
    {
        {
            const juce::ScopedLock sl(scanLock);
            requestInFlight = false;
            inFlightPath.clear();
        }
        treeModel.markDirectoryLoading(key, false);
        scanning = false;
        setStatus("GetDirectory send failed: " + (key.isEmpty() ? juce::String("root") : key));
    }
}

void EmberClient::finishRequest(const Path& path)
{
    const auto key = pathToString(path);
    treeModel.markDirectoryLoaded(key, true);
    {
        const juce::ScopedLock sl(scanLock);
        requestInFlight = false;
        inFlightPath.clear();
    }
    requestNextDirectory();
}

bool EmberClient::sendDirectoryRequest(const Path& path)
{
    auto root = std::make_unique<glow::GlowRootElementCollection>();
    if (path.empty()) new glow::GlowCommand(root.get(), glow::CommandType::GetDirectory, glow::DirFieldMask::All);
    else if (treeModel.isMatrix(pathToString(path)))
    {
        // A matrix GetDirectory is encoded as a command inside the qualified
        // matrix's Children collection. It is not a QualifiedNode request.
        auto* matrix = new glow::GlowQualifiedMatrix(root.get(), pathToOid(path));
        auto* children = matrix->children();
        children->insert(children->end(), new glow::GlowCommand(glow::CommandType::GetDirectory,
                                                                 glow::DirFieldMask::All));
    }
    else
    {
        auto* node = new glow::GlowQualifiedNode(root.get(), pathToOid(path));
        new glow::GlowCommand(node, glow::CommandType::GetDirectory, glow::DirFieldMask::All);
    }
    return sendGlowTree(*root);
}

bool EmberClient::sendGlowTree(const dom::Node& root)
{
    if (!socket || !connected.load()) return false;
    util::OctetStream payload;
    root.encode(payload);

    libs101::StreamEncoder<unsigned char> encoder;
    encoder.encode(0x00);
    encoder.encode(libs101::MessageType(libs101::MessageType::EmBER).value());
    encoder.encode(libs101::CommandType(libs101::CommandType::EmBER).value());
    encoder.encode(0x01);
    encoder.encode(0xC0);
    encoder.encode(libs101::Dtd(libs101::Dtd::Glow).value());
    encoder.encode(0x02);
    const auto version = glow::GlowDtd::version();
    encoder.encode((unsigned char) (version & 0xff));
    encoder.encode((unsigned char) ((version >> 8) & 0xff));
    encoder.encode(payload.begin(), payload.end());
    encoder.finish();

    std::vector<unsigned char> bytes(encoder.begin(), encoder.end());
    return writeS101Bytes(bytes);
}

bool EmberClient::sendKeepAliveResponse(unsigned char slot)
{
    if (!socket) return false;
    libs101::StreamEncoder<unsigned char> encoder;
    encoder.encode(slot);
    encoder.encode(libs101::MessageType(libs101::MessageType::EmBER).value());
    encoder.encode(libs101::CommandType(libs101::CommandType::KeepAliveResponse).value());
    encoder.encode(0x01);
    encoder.finish();
    std::vector<unsigned char> bytes(encoder.begin(), encoder.end());
    return writeS101Bytes(bytes);
}

bool EmberClient::writeS101Bytes(const std::vector<unsigned char>& bytes)
{
    if (!socket || bytes.empty()) return false;
    const juce::ScopedLock sl(writeLock);
    int offset = 0;
    while (offset < (int) bytes.size())
    {
        const int written = socket->write(bytes.data() + offset, (int) bytes.size() - offset);
        if (written <= 0) return false;
        offset += written;
    }
    return true;
}

bool EmberClient::setParameterValue(const juce::String& numericPath, const juce::var& newValue)
{
    if (!connected.load()) return false;
    const auto path = stringToPath(numericPath);
    const auto info = store.find(numericPath);
    if (path.empty() || !info) return false;

    auto root = std::make_unique<glow::GlowRootElementCollection>();
    auto* parameter = new glow::GlowQualifiedParameter(root.get(), pathToOid(path));
    switch (info->kind)
    {
        case ParameterKind::booleanValue: parameter->setValue((bool) newValue); break;
        case ParameterKind::real: parameter->setValue((double) newValue); break;
        case ParameterKind::stringValue: parameter->setValue(newValue.toString().toStdString()); break;
        case ParameterKind::trigger: parameter->setValue(true); break;
        case ParameterKind::enumeration:
        case ParameterKind::integer: parameter->setValue((long) (juce::int64) newValue); break;
        default: return false;
    }
    return sendGlowTree(*root);
}

bool EmberClient::setMatrixCrosspoint(const juce::String& numericPath, int target, int source,
                                      bool connect)
{
    if (!connected.load()) return false;
    const auto path = stringToPath(numericPath);
    if (path.empty() || target < 0 || source < 0 || !treeModel.isMatrix(numericPath)) return false;

    auto root = std::make_unique<glow::GlowRootElementCollection>();
    auto* matrix = new glow::GlowQualifiedMatrix(root.get(), pathToOid(path));
    auto* connections = matrix->connections();
    auto* connection = new glow::GlowConnection(target);
    const Path sources { source };
    connection->setSources(pathToOid(sources));
    if (!connect)
    {
        connection->setOperation(glow::ConnectionOperation::Disconnect);
    }
    else
    {
        // 1:N and 1:1 targets accept one source. Absolute also replaces a
        // previous source in one atomic Ember+ update when another XY cell is
        // clicked. N:N matrices keep the additive Connect operation.
        const auto matrixEntry = treeModel.find(numericPath);
        const bool singleSourcePerTarget = matrixEntry && matrixEntry->matrix.type
                                        && (*matrixEntry->matrix.type == 0 || *matrixEntry->matrix.type == 1);
        connection->setOperation(singleSourcePerTarget ? glow::ConnectionOperation::Absolute
                                                       : glow::ConnectionOperation::Connect);
    }
    connections->insert(connections->end(), connection);
    return sendGlowTree(*root);
}

juce::String EmberClient::makeTreePath(const Path& parameterPath, const juce::String& parameterName) const
{
    juce::StringArray names;
    Path prefix;
    const juce::ScopedLock sl(scanLock);
    if (!parameterPath.empty())
    {
        for (size_t i = 0; i + 1 < parameterPath.size(); ++i)
        {
            prefix.push_back(parameterPath[i]);
            const auto key = pathToString(prefix);
            if (auto it = nodeIdentifiers.find(key); it != nodeIdentifiers.end() && it->second.isNotEmpty()) names.add(it->second);
            else names.add(juce::String(parameterPath[i]));
        }
    }
    names.add(parameterName.isNotEmpty() ? parameterName : pathToString(parameterPath));
    return names.joinIntoString(" / ");
}

juce::String EmberClient::statusText() const
{
    const juce::ScopedLock sl(stateLock);
    return status;
}

juce::String EmberClient::endpointText() const { return hostName + ":" + juce::String(tcpPort); }
void EmberClient::setStatus(juce::String text) { const juce::ScopedLock sl(stateLock); status = std::move(text); }

juce::String EmberClient::pathToString(const Path& path)
{
    juce::StringArray s;
    for (auto n : path) s.add(juce::String(n));
    return s.joinIntoString(".");
}

EmberClient::Path EmberClient::stringToPath(const juce::String& path)
{
    Path out;
    juce::StringArray s;
    s.addTokens(path, ".", "");
    for (auto& item : s) if (item.trim().isNotEmpty()) out.push_back(item.getIntValue());
    return out;
}

EmberClient::Path EmberClient::oidToPath(const ber::ObjectIdentifier& oid)
{
    Path out;
    for (auto it = oid.begin(); it != oid.end(); ++it) out.push_back(*it);
    return out;
}

ber::ObjectIdentifier EmberClient::pathToOid(const Path& path)
{
    return ber::ObjectIdentifier(path.begin(), path.end());
}

juce::var EmberClient::glowValueToVar(const glow::Value& value)
{
    switch (value.type().value())
    {
        case glow::ParameterType::Integer: return juce::var((juce::int64) value.toInteger());
        case glow::ParameterType::Real: return juce::var(value.toReal());
        case glow::ParameterType::String: return juce::var(juce::String(value.toString()));
        case glow::ParameterType::Boolean: return juce::var(value.toBoolean());
        case glow::ParameterType::Enum: return juce::var((juce::int64) value.toInteger());
        default: return {};
    }
}

ParameterKind EmberClient::toParameterKind(glow::ParameterType type)
{
    switch (type.value())
    {
        case glow::ParameterType::Integer: return ParameterKind::integer;
        case glow::ParameterType::Real: return ParameterKind::real;
        case glow::ParameterType::String: return ParameterKind::stringValue;
        case glow::ParameterType::Boolean: return ParameterKind::booleanValue;
        case glow::ParameterType::Trigger: return ParameterKind::trigger;
        case glow::ParameterType::Enum: return ParameterKind::enumeration;
        case glow::ParameterType::Octets: return ParameterKind::octets;
        default: return ParameterKind::unknown;
    }
}
}
