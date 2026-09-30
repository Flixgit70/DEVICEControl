#include "ProviderManager.h"
#include "ProviderFactory.h"
#include <algorithm>
#include <thread>
#include <atomic>

namespace er
{

ProviderManager::~ProviderManager()
{
    disconnectAll();
}

ProviderManager::Session* ProviderManager::findSession(const juce::String& providerId)
{
    for (auto& s : sessions)
        if (s->config.id == providerId) return s.get();
    return nullptr;
}

const ProviderManager::Session* ProviderManager::findSession(const juce::String& providerId) const
{
    for (const auto& s : sessions)
        if (s->config.id == providerId) return s.get();
    return nullptr;
}

void ProviderManager::setProviders(const std::vector<ProviderConfig>& configs)
{
    disconnectAll();
    sessions.clear();
    sessions.reserve(configs.size());
    for (auto config : configs)
    {
        if (config.id.isEmpty()) config.id = juce::Uuid().toString();
        if (config.name.isEmpty()) config.name = "Provider " + juce::String((int) sessions.size() + 1);
        config.driver = ProviderFactory::normaliseDriver(config.driver);
        if (config.port <= 0) config.port = 9000;

        auto s = std::make_unique<Session>();
        s->config = std::move(config);
        s->backend = ProviderFactory::create(s->config);
        sessions.push_back(std::move(s));
    }
    ++configRevision;
}

std::vector<ProviderConfig> ProviderManager::providerConfigs() const
{
    std::vector<ProviderConfig> out;
    out.reserve(sessions.size());
    for (const auto& s : sessions) out.push_back(s->config);
    return out;
}

ProviderConfig ProviderManager::addProvider()
{
    auto config = ProviderConfig::createDefault("Provider " + juce::String((int) sessions.size() + 1));
    auto s = std::make_unique<Session>();
    s->config = config;
    s->backend = ProviderFactory::create(s->config);
    sessions.push_back(std::move(s));
    ++configRevision;
    return config;
}

bool ProviderManager::updateProvider(const ProviderConfig& incoming)
{
    auto* s = findSession(incoming.id);
    if (!s) return false;

    auto updated = incoming;
    if (updated.name.isEmpty()) updated.name = "Provider";
    updated.driver = ProviderFactory::normaliseDriver(updated.driver);
    if (updated.port <= 0) updated.port = 9000;

    const bool endpointChanged = s->config.host != updated.host || s->config.port != updated.port;
    const bool enabledChanged = s->config.enabled != updated.enabled;
    const bool driverChanged = ProviderFactory::normaliseDriver(s->config.driver) != updated.driver;

    if (s->backend && (endpointChanged || driverChanged || (enabledChanged && !updated.enabled)) && s->backend->isConnected())
        s->backend->disconnect();

    s->config = std::move(updated);
    if (driverChanged || !s->backend)
        s->backend = ProviderFactory::create(s->config);

    ++configRevision;
    return true;
}

bool ProviderManager::removeProvider(const juce::String& providerId)
{
    const auto it = std::find_if(sessions.begin(), sessions.end(), [&] (const auto& s)
    {
        return s->config.id == providerId;
    });
    if (it == sessions.end()) return false;
    if ((*it)->backend) (*it)->backend->disconnect();
    sessions.erase(it);
    ++configRevision;
    return true;
}

bool ProviderManager::connectProvider(const juce::String& providerId)
{
    auto* s = findSession(providerId);
    if (!s || !s->backend || !s->config.enabled || s->config.host.trim().isEmpty()) return false;
    if (s->backend->isConnected()) return true;
    return s->backend->connectTo(s->config.host.trim(), s->config.port > 0 ? s->config.port : 9000);
}

void ProviderManager::disconnectProvider(const juce::String& providerId)
{
    if (auto* s = findSession(providerId); s && s->backend) s->backend->disconnect();
}

int ProviderManager::connectEnabled(bool autoOnly)
{
    std::vector<Session*> targets;
    int alreadyConnected = 0;
    for (auto& s : sessions)
    {
        if (!s->backend || !s->config.enabled || (autoOnly && !s->config.autoConnect)) continue;
        if (s->backend->isConnected()) ++alreadyConnected;
        else if (s->config.host.trim().isNotEmpty()) targets.push_back(s.get());
    }

    std::atomic<int> newlyConnected { 0 };
    std::vector<std::thread> workers;
    workers.reserve(targets.size());
    for (auto* session : targets)
    {
        workers.emplace_back([session, &newlyConnected]
        {
            if (session->backend->connectTo(session->config.host.trim(),
                                            session->config.port > 0 ? session->config.port : 9000))
                ++newlyConnected;
        });
    }
    for (auto& worker : workers) worker.join();
    return alreadyConnected + newlyConnected.load();
}

void ProviderManager::disconnectAll()
{
    for (auto& s : sessions)
        if (s->backend) s->backend->disconnect();
}

bool ProviderManager::requestDirectory(const juce::String& qualifiedPath)
{
    auto* s = findSession(ProviderPath::providerId(qualifiedPath));
    if (!s || !s->backend || !s->backend->isConnected()) return false;
    return s->backend->requestDirectory(ProviderPath::localPath(qualifiedPath));
}

void ProviderManager::startSubtreeScan(const std::vector<juce::String>& qualifiedRoots)
{
    std::map<juce::String, std::vector<juce::String>> byProvider;
    for (const auto& root : qualifiedRoots)
    {
        const auto id = ProviderPath::providerId(root);
        if (id.isNotEmpty()) byProvider[id].push_back(ProviderPath::localPath(root));
    }

    for (const auto& [id, roots] : byProvider)
        if (auto* s = findSession(id); s && s->backend && s->backend->isConnected())
            s->backend->startSubtreeScan(roots);
}

bool ProviderManager::setParameterValue(const juce::String& qualifiedPath, const juce::var& value)
{
    auto* s = findSession(ProviderPath::providerId(qualifiedPath));
    return s != nullptr && s->backend != nullptr
        && s->backend->setParameterValue(ProviderPath::localPath(qualifiedPath), value);
}

bool ProviderManager::anyConnected() const { return connectedCount() > 0; }

bool ProviderManager::isProviderConnected(const juce::String& providerId) const
{
    const auto* s = findSession(providerId);
    return s && s->backend && s->backend->isConnected();
}

bool ProviderManager::isScanning() const
{
    for (const auto& s : sessions)
        if (s->backend && s->backend->isScanning()) return true;
    return false;
}

int ProviderManager::connectedCount() const
{
    int count = 0;
    for (const auto& s : sessions)
        if (s->backend && s->backend->isConnected()) ++count;
    return count;
}

int ProviderManager::enabledCount() const
{
    int count = 0;
    for (const auto& s : sessions) if (s->config.enabled) ++count;
    return count;
}

int ProviderManager::discoveredParameterCount() const
{
    int count = 0;
    for (const auto& s : sessions)
        if (s->backend) count += s->backend->discoveredParameterCount();
    return count;
}

juce::String ProviderManager::statusText() const
{
    const auto total = (int) sessions.size();
    const auto connected = connectedCount();
    juce::String text = juce::String(connected) + "/" + juce::String(total) + " providers connected";
    int scanningCount = 0;
    for (const auto& s : sessions)
        if (s->backend && s->backend->isScanning()) ++scanningCount;
    if (scanningCount > 0) text << "  |  " << juce::String(scanningCount) << " scanning";
    return text;
}

juce::String ProviderManager::providerStatus(const juce::String& providerId) const
{
    const auto* s = findSession(providerId);
    if (!s) return "Missing";
    if (!s->backend) return "Unsupported driver: " + s->config.driver;
    return s->backend->statusText();
}

ParameterInfo ProviderManager::qualifyParameter(const Session& s, ParameterInfo p)
{
    const auto local = p.path;
    p.providerId = s.config.id;
    p.providerName = s.config.name;
    p.localPath = local;
    p.path = ProviderPath::qualify(s.config.id, local);
    if (!s.backend || !s.backend->isConnected()) p.online = false;
    return p;
}

std::vector<ParameterInfo> ProviderManager::parameterSnapshot() const
{
    std::vector<ParameterInfo> out;
    for (const auto& s : sessions)
    {
        if (!s->backend) continue;
        auto local = s->backend->parameterSnapshot();
        out.reserve(out.size() + local.size());
        for (auto& p : local) out.push_back(qualifyParameter(*s, std::move(p)));
    }
    return out;
}

std::optional<ParameterInfo> ProviderManager::findParameter(const juce::String& qualifiedPath) const
{
    const auto id = ProviderPath::providerId(qualifiedPath);
    const auto* s = findSession(id);
    if (!s) return std::nullopt;
    const auto localPath = ProviderPath::localPath(qualifiedPath);
    if (s->backend)
    {
        auto p = s->backend->findParameter(localPath);
        if (p) return qualifyParameter(*s, std::move(*p));
    }

    ParameterInfo stub;
    stub.path = qualifiedPath;
    stub.providerId = s->config.id;
    stub.providerName = s->config.name;
    stub.localPath = localPath;
    stub.identifier = localPath;
    stub.treePath = localPath;
    stub.online = false;
    return stub;
}

std::vector<TreeEntry> ProviderManager::treeSnapshot() const
{
    std::vector<TreeEntry> out;
    for (const auto& s : sessions)
    {
        TreeEntry providerRoot;
        providerRoot.kind = TreeEntryKind::node;
        providerRoot.path = ProviderPath::root(s->config.id);
        providerRoot.parentPath = {};
        providerRoot.identifier = s->config.name;
        providerRoot.description = s->config.host + ":" + juce::String(s->config.port);
        providerRoot.online = s->backend && s->backend->isConnected();
        providerRoot.directoryLoaded = s->backend && s->backend->isDirectoryLoaded(juce::String());
        providerRoot.directoryLoading = s->backend && s->backend->isDirectoryLoading(juce::String());
        out.push_back(providerRoot);

        if (!s->backend) continue;
        for (auto e : s->backend->treeSnapshot())
        {
            const auto localPath = e.path;
            const auto localParent = e.parentPath;
            e.path = ProviderPath::qualify(s->config.id, localPath);
            e.parentPath = localParent.isEmpty() ? providerRoot.path
                                                  : ProviderPath::qualify(s->config.id, localParent);
            if (!s->backend->isConnected()) e.online = false;
            out.push_back(std::move(e));
        }
    }
    return out;
}

std::uint64_t ProviderManager::combine(std::uint64_t seed, std::uint64_t value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

std::uint64_t ProviderManager::treeRevision() const
{
    auto rev = configRevision;
    for (const auto& s : sessions)
    {
        rev = combine(rev, s->backend ? s->backend->treeRevision() : 0u);
        rev = combine(rev, s->backend && s->backend->isConnected() ? 1u : 0u);
    }
    return rev;
}

std::uint64_t ProviderManager::parameterRevision() const
{
    auto rev = configRevision;
    for (const auto& s : sessions)
    {
        rev = combine(rev, s->backend ? s->backend->parameterRevision() : 0u);
        rev = combine(rev, s->backend && s->backend->isConnected() ? 1u : 0u);
    }
    return rev;
}

std::uint64_t ProviderManager::parameterStructureRevision() const
{
    auto rev = configRevision;
    for (const auto& s : sessions)
    {
        rev = combine(rev, s->backend ? s->backend->parameterStructureRevision() : 0u);
        rev = combine(rev, s->backend && s->backend->isConnected() ? 1u : 0u);
    }
    return rev;
}

juce::String ProviderManager::firstEnabledProviderId() const
{
    for (const auto& s : sessions) if (s->config.enabled) return s->config.id;
    return sessions.empty() ? juce::String() : sessions.front()->config.id;
}

juce::String ProviderManager::resolveProviderIdByName(const juce::String& name) const
{
    for (const auto& s : sessions)
        if (s->config.name.equalsIgnoreCase(name)) return s->config.id;
    return {};
}
}
