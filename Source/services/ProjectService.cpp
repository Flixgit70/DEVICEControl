#include "ProjectService.h"

namespace er
{
namespace
{
juce::String colourToString(juce::uint32 colourArgb)
{
    if (colourArgb == 0) return {};
    return "#" + juce::String::toHexString((juce::int64) colourArgb).paddedLeft('0', 8).toUpperCase();
}

juce::uint32 colourFromVar(const juce::var& value)
{
    auto text = value.toString().trim();
    if (text.startsWithChar('#')) text = text.substring(1);
    if (text.isEmpty()) return 0;
    if (text.length() == 6) text = "FF" + text;
    if (text.length() != 8) return 0;
    return (juce::uint32) text.getHexValue32();
}


juce::String controlTypeToString(ControlType type)
{
    switch (type)
    {
        case ControlType::encoder: return "encoder";
        case ControlType::showContent: return "show_content";
        case ControlType::meter: return "meter";
        case ControlType::matrix: return "matrix";
        case ControlType::button: return "button";
    }
    return "button";
}

ControlType controlTypeFromString(juce::String text)
{
    text = text.trim();
    if (text.equalsIgnoreCase("encoder")) return ControlType::encoder;
    if (text.equalsIgnoreCase("show_content") || text.equalsIgnoreCase("show content")
        || text.equalsIgnoreCase("content")) return ControlType::showContent;
    if (text.equalsIgnoreCase("meter") || text.equalsIgnoreCase("ppm"))
        return ControlType::meter;
    if (text.equalsIgnoreCase("matrix")) return ControlType::matrix;
    return ControlType::button;
}

juce::var matrixAxisNodeToVar(const MatrixAxisNode& node)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("id", node.id);
    object->setProperty("name", node.name);
    object->setProperty("folder", node.folder);
    object->setProperty("signal", node.signalNumber);
    object->setProperty("expanded", node.expanded);
    if (node.folder)
    {
        juce::Array<juce::var> children;
        for (const auto& child : node.children) children.add(matrixAxisNodeToVar(child));
        object->setProperty("children", children);
    }
    return juce::var(object.release());
}

MatrixAxisNode matrixAxisNodeFromVar(const juce::var& value)
{
    MatrixAxisNode node;
    auto* object = value.getDynamicObject();
    if (!object) return node;
    node.id = object->getProperty("id").toString();
    node.name = object->getProperty("name").toString();
    node.folder = (bool) object->getProperty("folder");
    node.signalNumber = object->hasProperty("signal") ? (int) object->getProperty("signal") : -1;
    node.expanded = object->hasProperty("expanded") ? (bool) object->getProperty("expanded") : true;
    if (node.id.isEmpty())
        node.id = node.folder ? "folder:" + juce::Uuid().toString()
                              : "signal:" + juce::String(node.signalNumber);
    if (auto children = object->getProperty("children"); children.isArray())
        for (const auto& child : *children.getArray())
            node.children.push_back(matrixAxisNodeFromVar(child));
    return node;
}

ProviderConfig legacyConfig(const juce::String& host, int port)
{
    ProviderConfig c;
    c.id = "legacy-provider";
    c.name = "Provider 1";
    c.driver = "ember";
    c.host = host.isNotEmpty() ? host : "127.0.0.1";
    c.port = port > 0 ? port : 9000;
    c.enabled = true;
    c.autoConnect = false;
    return c;
}
}

bool ProjectService::save(const juce::File& file, const PageSet& pages,
                          const std::vector<ProviderConfig>& providers, juce::String& error)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("formatVersion", 9);

    juce::Array<juce::var> providerVars;
    for (const auto& provider : providers)
    {
        auto po = std::make_unique<juce::DynamicObject>();
        po->setProperty("id", provider.id);
        po->setProperty("name", provider.name);
        po->setProperty("driver", provider.driver.isEmpty() ? juce::String("ember") : provider.driver);
        po->setProperty("host", provider.host);
        po->setProperty("port", provider.port);
        po->setProperty("enabled", provider.enabled);
        po->setProperty("autoConnect", provider.autoConnect);
        providerVars.add(juce::var(po.release()));
    }
    root->setProperty("providers", providerVars);

    // Keep host/port mirrors for older human-readable tooling.
    if (!providers.empty())
    {
        root->setProperty("host", providers.front().host);
        root->setProperty("port", providers.front().port);
    }

    juce::Array<juce::var> pageVars;
    for (const auto& p : pages.pages())
    {
        auto po = std::make_unique<juce::DynamicObject>();
        po->setProperty("name", p.name);
        po->setProperty("viewMode", p.viewMode == PageViewMode::list ? "list" : "tiles");
        juce::Array<juce::var> controls;
        for (const auto& c : p.controls)
        {
            auto co = std::make_unique<juce::DynamicObject>();
            const auto providerId = ProviderPath::providerId(c.parameterPath);
            const auto localPath = ProviderPath::localPath(c.parameterPath);
            co->setProperty("providerId", providerId);
            co->setProperty("path", localPath);
            co->setProperty("qualifiedPath", c.parameterPath);
            co->setProperty("label", c.label);
            co->setProperty("label2", c.label2);
            co->setProperty("type", controlTypeToString(c.type));
            co->setProperty("colour", colourToString(c.colourArgb));
            co->setProperty("tileWidth", c.tileWidth);
            co->setProperty("tileHeight", c.tileHeight);
            co->setProperty("orangeAboveThresholdEnabled", c.orangeAboveThresholdEnabled);
            co->setProperty("orangeAboveThreshold", c.orangeAboveThreshold);
            co->setProperty("redAboveThresholdEnabled", c.redAboveThresholdEnabled);
            co->setProperty("redAboveThreshold", c.redAboveThreshold);
            co->setProperty("orangeBelowThresholdEnabled", c.orangeBelowThresholdEnabled);
            co->setProperty("orangeBelowThreshold", c.orangeBelowThreshold);
            co->setProperty("redBelowThresholdEnabled", c.redBelowThresholdEnabled);
            co->setProperty("redBelowThreshold", c.redBelowThreshold);
            if (c.type == ControlType::matrix)
            {
                juce::Array<juce::var> targets;
                juce::Array<juce::var> sources;
                for (const auto& node : c.matrix.targets) targets.add(matrixAxisNodeToVar(node));
                for (const auto& node : c.matrix.sources) sources.add(matrixAxisNodeToVar(node));
                co->setProperty("matrixTargets", targets);
                co->setProperty("matrixSources", sources);
            }
            controls.add(juce::var(co.release()));
        }
        po->setProperty("controls", controls);
        pageVars.add(juce::var(po.release()));
    }
    root->setProperty("pages", pageVars);

    if (!file.replaceWithText(juce::JSON::toString(juce::var(root.release()), true)))
    {
        error = "Could not save project";
        return false;
    }
    return true;
}

bool ProjectService::load(const juce::File& file, PageSet& pages,
                          std::vector<ProviderConfig>& providers, juce::String& error)
{
    auto v = juce::JSON::parse(file.loadFileAsString());
    if (!v.isObject()) { error = "Invalid project JSON"; return false; }
    auto* o = v.getDynamicObject();

    providers.clear();
    if (auto providerValue = o->getProperty("providers"); providerValue.isArray())
    {
        for (const auto& pvar : *providerValue.getArray())
        {
            auto* po = pvar.getDynamicObject();
            if (!po) continue;
            ProviderConfig c;
            c.id = po->getProperty("id").toString();
            if (c.id.isEmpty()) c.id = juce::Uuid().toString();
            c.name = po->getProperty("name").toString();
            if (c.name.isEmpty()) c.name = "Provider " + juce::String((int) providers.size() + 1);
            c.driver = po->hasProperty("driver") ? po->getProperty("driver").toString() : juce::String("ember");
            if (c.driver.isEmpty()) c.driver = "ember";
            c.host = po->getProperty("host").toString();
            c.port = (int) po->getProperty("port");
            if (c.port <= 0) c.port = 9000;
            c.enabled = po->hasProperty("enabled") ? (bool) po->getProperty("enabled") : true;
            c.autoConnect = po->hasProperty("autoConnect") ? (bool) po->getProperty("autoConnect") : false;
            providers.push_back(std::move(c));
        }
    }

    if (providers.empty())
        providers.push_back(legacyConfig(o->getProperty("host").toString(), (int) o->getProperty("port")));

    auto pv = o->getProperty("pages");
    if (!pv.isArray()) { error = "Project has no pages"; return false; }
    pages.pages().clear();
    for (const auto& pvar : *pv.getArray())
    {
        auto* po = pvar.getDynamicObject(); if (!po) continue;
        PageModel p;
        p.name = po->getProperty("name").toString();
        p.viewMode = po->getProperty("viewMode").toString().equalsIgnoreCase("list") ? PageViewMode::list : PageViewMode::tiles;
        if (auto cv = po->getProperty("controls"); cv.isArray())
        {
            for (const auto& cvar : *cv.getArray())
            {
                auto* co = cvar.getDynamicObject(); if (!co) continue;
                ControlMapping c;
                auto qualified = co->getProperty("qualifiedPath").toString();
                const auto providerId = co->getProperty("providerId").toString();
                const auto localPath = co->getProperty("path").toString();
                if (qualified.isEmpty())
                {
                    const auto effectiveProvider = providerId.isNotEmpty() ? providerId
                                                                          : (providers.size() == 1 ? providers.front().id : juce::String());
                    qualified = effectiveProvider.isNotEmpty() ? ProviderPath::qualify(effectiveProvider, localPath)
                                                               : localPath;
                }
                c.parameterPath = qualified;
                c.label = co->getProperty("label").toString();
                c.label2 = co->getProperty("label2").toString();
                c.type = controlTypeFromString(co->getProperty("type").toString());
                c.colourArgb = colourFromVar(co->getProperty("colour"));
                c.tileWidth = co->hasProperty("tileWidth") ? juce::jmax(0, (int) co->getProperty("tileWidth")) : 0;
                c.tileHeight = co->hasProperty("tileHeight") ? juce::jmax(0, (int) co->getProperty("tileHeight")) : 0;
                if (c.type == ControlType::matrix)
                {
                    if (auto targets = co->getProperty("matrixTargets"); targets.isArray())
                        for (const auto& node : *targets.getArray())
                            c.matrix.targets.push_back(matrixAxisNodeFromVar(node));
                    if (auto sources = co->getProperty("matrixSources"); sources.isArray())
                        for (const auto& node : *sources.getArray())
                            c.matrix.sources.push_back(matrixAxisNodeFromVar(node));
                }
                const bool hasNewThresholds = co->hasProperty("orangeAboveThresholdEnabled")
                                           || co->hasProperty("redAboveThresholdEnabled")
                                           || co->hasProperty("orangeBelowThresholdEnabled")
                                           || co->hasProperty("redBelowThresholdEnabled");
                if (hasNewThresholds)
                {
                    c.orangeAboveThresholdEnabled = co->hasProperty("orangeAboveThresholdEnabled") ? (bool) co->getProperty("orangeAboveThresholdEnabled") : false;
                    c.orangeAboveThreshold = co->hasProperty("orangeAboveThreshold") ? (double) co->getProperty("orangeAboveThreshold") : 0.0;
                    c.redAboveThresholdEnabled = co->hasProperty("redAboveThresholdEnabled") ? (bool) co->getProperty("redAboveThresholdEnabled") : false;
                    c.redAboveThreshold = co->hasProperty("redAboveThreshold") ? (double) co->getProperty("redAboveThreshold") : 0.0;
                    c.orangeBelowThresholdEnabled = co->hasProperty("orangeBelowThresholdEnabled") ? (bool) co->getProperty("orangeBelowThresholdEnabled") : false;
                    c.orangeBelowThreshold = co->hasProperty("orangeBelowThreshold") ? (double) co->getProperty("orangeBelowThreshold") : 0.0;
                    c.redBelowThresholdEnabled = co->hasProperty("redBelowThresholdEnabled") ? (bool) co->getProperty("redBelowThresholdEnabled") : false;
                    c.redBelowThreshold = co->hasProperty("redBelowThreshold") ? (double) co->getProperty("redBelowThreshold") : 0.0;
                }
                else
                {
                    const bool orangeEnabled = co->hasProperty("orangeThresholdEnabled") ? (bool) co->getProperty("orangeThresholdEnabled") : false;
                    const double orangeValue = co->hasProperty("orangeThreshold") ? (double) co->getProperty("orangeThreshold") : 0.0;
                    const bool orangeBelow = co->getProperty("orangeThresholdDirection").toString().equalsIgnoreCase("below");
                    if (orangeBelow) { c.orangeBelowThresholdEnabled = orangeEnabled; c.orangeBelowThreshold = orangeValue; }
                    else             { c.orangeAboveThresholdEnabled = orangeEnabled; c.orangeAboveThreshold = orangeValue; }

                    const bool redEnabled = co->hasProperty("redThresholdEnabled") ? (bool) co->getProperty("redThresholdEnabled") : false;
                    const double redValue = co->hasProperty("redThreshold") ? (double) co->getProperty("redThreshold") : 0.0;
                    const bool redBelow = co->getProperty("redThresholdDirection").toString().equalsIgnoreCase("below");
                    if (redBelow) { c.redBelowThresholdEnabled = redEnabled; c.redBelowThreshold = redValue; }
                    else          { c.redAboveThresholdEnabled = redEnabled; c.redAboveThreshold = redValue; }
                }
                p.controls.push_back(std::move(c));
            }
        }
        pages.pages().push_back(std::move(p));
    }
    if (pages.pages().empty()) pages.addPage();
    return true;
}

bool ProjectService::save(const juce::File& file, const PageSet& pages,
                          const juce::String& host, int port, juce::String& error)
{
    return save(file, pages, { legacyConfig(host, port) }, error);
}

bool ProjectService::load(const juce::File& file, PageSet& pages,
                          juce::String& host, int& port, juce::String& error)
{
    std::vector<ProviderConfig> providers;
    if (!load(file, pages, providers, error)) return false;
    if (!providers.empty())
    {
        host = providers.front().host;
        port = providers.front().port;
    }
    return true;
}
}
