#include <JuceHeader.h>
#include "model/PageModel.h"
#include "model/ParameterStore.h"
#include "model/ControlTreeModel.h"
#include "model/MappingDraft.h"
#include "model/ControlValuePolicy.h"
#include "model/ControlAlertPolicy.h"
#include "model/ProviderConfig.h"
#include "services/CsvService.h"
#include "services/ProjectService.h"
#include <iostream>

namespace
{
int failures = 0;

void expect(bool condition, const juce::String& message)
{
    if (condition)
        return;

    ++failures;
    std::cerr << "FAIL: " << message << std::endl;
}

juce::File tempFile(const juce::String& suffix)
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("EmberRemoteRegression", suffix, false);
}

void testPageSet()
{
    er::PageSet pages;
    expect(pages.pages().size() == 3, "PageSet must start with exactly three pages");
    expect(pages.pages()[0].name == "1", "First default page must be named 1");
    expect(pages.pages()[1].name == "2", "Second default page must be named 2");
    expect(pages.pages()[2].name == "3", "Third default page must be named 3");

    const auto index = pages.addPage();
    expect(index == 3, "Fourth page must be returned at zero-based index 3");
    expect(pages.pages().size() == 4, "addPage must append one page");
    expect(pages.pages()[3].name == "4", "Fourth page must be named 4");
}


void testPageControlRemoval()
{
    er::PageModel page;
    page.name = "1";
    er::ControlMapping mute;
    mute.parameterPath = "1.2.3";
    mute.label = "Mute";
    mute.type = er::ControlType::button;
    page.controls.push_back(mute);

    er::ControlMapping gain;
    gain.parameterPath = "1.2.4";
    gain.label = "Gain";
    gain.type = er::ControlType::encoder;
    page.controls.push_back(gain);

    expect(page.removeControl("1.2.3"), "Removing an existing page control must report success");
    expect(page.controls.size() == 1, "Removing one control must leave the other control intact");
    if (!page.controls.empty())
        expect(page.controls[0].parameterPath == "1.2.4", "Removal must target only the requested parameter path");

    expect(page.setControlType("1.2.4", er::ControlType::button),
           "Changing an existing control type must report success");
    if (!page.controls.empty())
        expect(page.controls[0].type == er::ControlType::button,
               "Changed control type must be stored in the page model");
    expect(!page.setControlType("9.9.9", er::ControlType::encoder),
           "Changing a missing control type must report no change");

    expect(page.setControlColour("1.2.4", 0xffff941f), "Changing an existing control colour must report success");
    if (!page.controls.empty())
        expect(page.controls[0].colourArgb == 0xffff941f, "Control colour must be stored in the page model");
    expect(!page.setControlColour("9.9.9", 0xff00ff00), "Changing a missing control colour must report no change");
    expect(page.setControlLabel2("1.2.4", "Main Fader"), "Editing Label 2 on an existing page control must report success");
    if (!page.controls.empty())
        expect(page.controls[0].label2 == "Main Fader", "Edited Label 2 must be stored in the page model");
    expect(!page.setControlLabel2("9.9.9", "Missing"), "Editing Label 2 on a missing control must report no change");
    expect(page.setControlThresholds("1.2.4", true, 80.0, true, 95.0, true, 20.0, true, 10.0),
           "Editing all four alert thresholds on an existing control must report success");
    if (!page.controls.empty())
        expect(page.controls[0].orangeAboveThresholdEnabled && page.controls[0].orangeAboveThreshold == 80.0
               && page.controls[0].redAboveThresholdEnabled && page.controls[0].redAboveThreshold == 95.0
               && page.controls[0].orangeBelowThresholdEnabled && page.controls[0].orangeBelowThreshold == 20.0
               && page.controls[0].redBelowThresholdEnabled && page.controls[0].redBelowThreshold == 10.0,
               "All four alert thresholds must be stored in the page model");

    expect(!page.removeControl("9.9.9"), "Removing a missing control must report no change");
    expect(page.controls.size() == 1, "Removing a missing control must not modify the page");
    page.clearControls();
    expect(page.controls.empty(), "Clear All must remove every control from a page");
}

void testParameterStoreRevisions()
{
    er::ParameterStore store;
    er::ParameterInfo p;
    p.path = "1.2.3";
    p.identifier = "Gain";
    p.kind = er::ParameterKind::real;
    p.value = 1.0;
    p.writable = true;

    store.upsert(p);
    const auto valueRevision1 = store.revision();
    const auto treeRevision1 = store.structureRevision();
    expect(store.size() == 1, "ParameterStore must contain inserted parameter");
    expect(valueRevision1 > 0, "Parameter insert must advance value revision");
    expect(treeRevision1 > 0, "Parameter insert must advance structure revision");

    auto valueOnly = p;
    valueOnly.value = 2.0;
    store.upsert(valueOnly);
    expect(store.revision() == valueRevision1 + 1, "Value update must advance value revision exactly once");
    expect(store.structureRevision() == treeRevision1, "Value-only update must not advance structure revision");

    auto structural = valueOnly;
    structural.description = "Main gain";
    store.upsert(structural);
    expect(store.structureRevision() == treeRevision1 + 1, "Structural metadata update must advance structure revision");

    const auto found = store.find("1.2.3");
    expect(found.has_value(), "ParameterStore::find must find existing path");
    expect(found && (double) found->value == 2.0, "ParameterStore must retain latest value");
}

void testCsvExportAndImport()
{
    const auto exportFile = tempFile(".csv");

    er::ParameterInfo p1;
    p1.providerId = "provider-a";
    p1.providerName = "Ravenna A";
    p1.localPath = "1.2.3";
    p1.path = er::ProviderPath::qualify(p1.providerId, p1.localPath);
    p1.treePath = "ravenna / Routing / Inputs";
    p1.identifier = "Inputs";
    p1.description = "Input Level";
    p1.kind = er::ParameterKind::real;
    p1.value = 0.5;
    p1.minimum = 0.0;
    p1.maximum = 1.0;
    p1.factor = 0.1;
    p1.writable = true;
    p1.enumEntries.add("A");
    p1.enumEntries.add("B");

    er::ParameterInfo p2;
    p2.path = "9.1";
    p2.treePath = "System / Online";
    p2.identifier = "Online";
    p2.description = "Online; \"State\"";
    p2.kind = er::ParameterKind::booleanValue;
    p2.value = true;
    p2.writable = false;

    juce::String error;
    expect(er::CsvService::exportParameters(exportFile, {p1, p2}, error), "CSV export must succeed: " + error);
    const auto exported = exportFile.loadFileAsString();
    expect(exported.contains("provider_id;provider;path;branch_1;branch_2;branch_3;identifier"),
           "CSV export must include provider identity before the dynamic branch columns");
    expect(exported.contains("provider-a;Ravenna A;1.2.3"),
           "CSV export must keep provider id/name separate while path stays local numeric Ember path without quotes");
    expect(!exported.contains("tree_path"), "New CSV export must not flatten the hierarchy into tree_path");
    expect(exported.contains("ravenna;Routing;Inputs"),
           "ravenna / Routing / Inputs must export into three separate unquoted CSV cells");
    expect(exported.contains("System;Online;"),
           "Shallower tree rows must be padded with an empty branch cell");
    expect(exported.contains(";label;label2;colour;orange_above_threshold;red_above_threshold;orange_below_threshold;red_below_threshold"),
           "CSV export must contain Label 1, Label 2 and all four fixed alert threshold columns");
    expect(exported.contains("Ravenna A/ravenna/Routing/Inputs"),
           "CSV Label 1 must include the provider root plus the complete slash-separated Ember tree path");
    expect(exported.contains("Online, \"State\""),
           "Embedded semicolons must be converted to commas while literal quote characters remain unwrapped");
    expect(!exported.contains("\"provider-a\""),
           "CSV export must not wrap fields in quotation marks");
    exportFile.deleteFile();

    const auto importFile = tempFile(".csv");
    const juce::String csv =
        "provider_id;provider;path;branch_1;branch_2;branch_3;enabled;control;page;label;Label 2;colour;orange_above_threshold;red_above_threshold;orange_below_threshold;red_below_threshold\n"
        "provider-a;Ravenna A;1.2.3;Mixer;Inputs;Gain;true;encoder;4;Gain Left;Main Fader;#FFFF941F;80;95;20;10\n"
        ";;9.9;Ignored;Disabled;;false;button;2;Ignored;;;;;;\n"
        ";;4.5;Mixer;Mute;;x;button;1;Mute A;Program;#FF2F8FD8;;;;\n"
        ";;7.8;Mixer;Aux;Enable;true;button;3;;Auxiliary;;;;;\n";
    expect(importFile.replaceWithText(csv), "Test CSV file must be writable");

    er::PageSet pages;
    error.clear();
    expect(er::CsvService::importMappings(importFile, pages, error), "CSV import must succeed: " + error);
    expect(pages.pages().size() == 4, "CSV page 4 mapping must create fourth page");
    expect(pages.pages()[3].controls.size() == 1, "Enabled page 4 row must create one control");
    if (!pages.pages()[3].controls.empty())
    {
        const auto& c = pages.pages()[3].controls[0];
        expect(c.parameterPath == er::ProviderPath::qualify("provider-a", "1.2.3"), "Provider-aware CSV import must qualify the local Ember path");
        expect(c.label == "Gain Left", "Plain semicolon CSV label must import without quotation marks");
        expect(c.label2 == "Main Fader", "Second CSV label must be imported even when Excel-style header spelling uses Label 2");
        expect(c.colourArgb == 0xffff941f, "CSV colour must be imported as ARGB");
        expect(c.type == er::ControlType::encoder, "encoder control type must be imported");
        expect(c.orangeAboveThresholdEnabled && c.orangeAboveThreshold == 80.0, "Upper orange threshold must import from CSV");
        expect(c.redAboveThresholdEnabled && c.redAboveThreshold == 95.0, "Upper red threshold must import from CSV");
        expect(c.orangeBelowThresholdEnabled && c.orangeBelowThreshold == 20.0, "Lower orange threshold must import from CSV");
        expect(c.redBelowThresholdEnabled && c.redBelowThreshold == 10.0, "Lower red threshold must import from CSV");
    }
    expect(pages.pages()[1].controls.empty(), "Disabled CSV row must not create a control");
    expect(pages.pages()[0].controls.size() == 1, "x must be accepted as enabled marker");
    if (!pages.pages()[0].controls.empty())
    {
        expect(pages.pages()[0].controls[0].label == "Mute A", "Plain unquoted CSV label must survive import");
        expect(pages.pages()[0].controls[0].label2 == "Program", "Second label must survive CSV import");
        expect(pages.pages()[0].controls[0].colourArgb == 0xff2f8fd8, "Second CSV colour must survive import");
    }
    expect(pages.pages()[2].controls.size() == 1, "CSV fallback-label row must create one page 3 control");
    if (!pages.pages()[2].controls.empty())
    {
        expect(pages.pages()[2].controls[0].label == "Mixer/Aux/Enable",
               "Blank CSV Label 1 must be reconstructed from the separate branch columns");
        expect(pages.pages()[2].controls[0].label2 == "Auxiliary",
               "CSV Label 2 must remain independent when Label 1 is reconstructed from branches");
    }
    importFile.deleteFile();

    const auto legacyFile = tempFile(".csv");
    const juce::String legacyCsv =
        "path;tree_path;enabled;control;page;label;label2;colour;orange_direction;orange_threshold;red_direction;red_threshold\n"
        "10.20;Legacy / Branch / Gain;true;encoder;1;Legacy Gain;Legacy Description;;below;20;above;90\n"
        "10.21;Legacy / Branch / Mute;true;button;1;;Legacy Mute;;;;;\n";
    expect(legacyFile.replaceWithText(legacyCsv), "Legacy CSV test file must be writable");
    er::PageSet legacyPages;
    error.clear();
    expect(er::CsvService::importMappings(legacyFile, legacyPages, error),
           "Legacy tree_path CSV import must remain supported: " + error);
    expect(legacyPages.pages()[0].controls.size() == 2, "Legacy CSV must create both mappings");
    if (!legacyPages.pages()[0].controls.empty())
    {
        expect(legacyPages.pages()[0].controls[0].label == "Legacy Gain",
               "Legacy direction columns must still import in plain semicolon format");
        expect(legacyPages.pages()[0].controls[0].label2 == "Legacy Description",
               "Legacy semicolon CSV must retain Label 2");
        expect(legacyPages.pages()[0].controls[0].orangeBelowThresholdEnabled
               && legacyPages.pages()[0].controls[0].orangeBelowThreshold == 20.0
               && legacyPages.pages()[0].controls[0].redAboveThresholdEnabled
               && legacyPages.pages()[0].controls[0].redAboveThreshold == 90.0,
               "Legacy 0.4.2 direction-based CSV thresholds must migrate into the fixed lower/upper fields");
    }
    if (legacyPages.pages()[0].controls.size() > 1)
        expect(legacyPages.pages()[0].controls[1].label == "Legacy/Branch/Mute",
               "Legacy tree_path must still provide the Label 1 fallback");
    legacyFile.deleteFile();
}

void testCsvRejectsMissingPathColumn()
{
    const auto file = tempFile(".csv");
    expect(file.replaceWithText("enabled;control\ntrue;button\n"), "Invalid test CSV must be writable");
    er::PageSet pages;
    juce::String error;
    expect(!er::CsvService::importMappings(file, pages, error), "CSV without path column must fail");
    expect(error.containsIgnoreCase("path"), "CSV missing-path error must mention path");
    file.deleteFile();
}

void testProjectRoundTrip()
{
    er::PageSet source;
    source.pages()[0].controls.push_back({"1.2.3", "Mute", er::ControlType::button, "Program", 0xffff941f});
    source.pages()[1].controls.push_back({"4.5.6", "Gain", er::ControlType::encoder, "Main Fader", 0xff2f8fd8});
    source.pages()[1].viewMode = er::PageViewMode::list;
    source.pages()[1].controls[0].orangeAboveThresholdEnabled = true;
    source.pages()[1].controls[0].orangeAboveThreshold = 70.0;
    source.pages()[1].controls[0].redAboveThresholdEnabled = true;
    source.pages()[1].controls[0].redAboveThreshold = 90.0;
    source.pages()[1].controls[0].orangeBelowThresholdEnabled = true;
    source.pages()[1].controls[0].orangeBelowThreshold = 20.0;
    source.pages()[1].controls[0].redBelowThresholdEnabled = true;
    source.pages()[1].controls[0].redBelowThreshold = 10.0;
    er::ControlMapping matrix;
    matrix.parameterPath = "@provider-a.5.1.1.1";
    matrix.label = "Routing Matrix";
    matrix.type = er::ControlType::matrix;
    auto targetFolder = er::MatrixAxisNode::createFolder("Main Outputs");
    targetFolder.expanded = false;
    targetFolder.children.push_back(er::MatrixAxisNode::signal(101, "Output 101"));
    matrix.matrix.targets.push_back(std::move(targetFolder));
    matrix.matrix.sources.push_back(er::MatrixAxisNode::signal(201, "Input 201"));
    source.pages()[2].controls.push_back(std::move(matrix));
    source.addPage();
    source.pages()[3].name = "Custom";

    const auto file = tempFile(".emberremote");
    juce::String error;
    expect(er::ProjectService::save(file, source, "192.168.10.20", 9000, error), "Project save must succeed: " + error);

    er::PageSet loaded;
    juce::String host;
    int port = 0;
    error.clear();
    expect(er::ProjectService::load(file, loaded, host, port, error), "Project load must succeed: " + error);
    expect(host == "192.168.10.20", "Project host must round-trip");
    expect(port == 9000, "Project port must round-trip");
    expect(loaded.pages().size() == 4, "Project page count must round-trip");
    expect(loaded.pages()[3].name == "Custom", "Custom page name must round-trip");
    expect(loaded.pages()[0].controls.size() == 1, "Button mapping must round-trip");
    expect(loaded.pages()[1].controls.size() == 1, "Encoder mapping must round-trip");
    expect(loaded.pages()[2].controls.size() == 1, "Matrix mapping must round-trip");
    if (!loaded.pages()[1].controls.empty())
    {
        expect(loaded.pages()[1].controls[0].type == er::ControlType::encoder, "Encoder type must round-trip");
        expect(loaded.pages()[1].controls[0].label2 == "Main Fader", "Second label must round-trip in project files");
        expect(loaded.pages()[1].controls[0].colourArgb == 0xff2f8fd8, "Control colour must round-trip in project files");
        expect(loaded.pages()[1].controls[0].orangeAboveThresholdEnabled && loaded.pages()[1].controls[0].orangeAboveThreshold == 70.0,
               "Upper orange threshold must round-trip in project files");
        expect(loaded.pages()[1].controls[0].redAboveThresholdEnabled && loaded.pages()[1].controls[0].redAboveThreshold == 90.0,
               "Upper red threshold must round-trip in project files");
        expect(loaded.pages()[1].controls[0].orangeBelowThresholdEnabled && loaded.pages()[1].controls[0].orangeBelowThreshold == 20.0,
               "Lower orange threshold must round-trip in project files");
        expect(loaded.pages()[1].controls[0].redBelowThresholdEnabled && loaded.pages()[1].controls[0].redBelowThreshold == 10.0,
               "Lower red threshold must round-trip in project files");
        expect(loaded.pages()[1].viewMode == er::PageViewMode::list, "Page list view mode must round-trip in project files");
    }
    if (!loaded.pages()[2].controls.empty())
    {
        const auto& loadedMatrix = loaded.pages()[2].controls[0].matrix;
        expect(loadedMatrix.targets.size() == 1 && loadedMatrix.targets[0].folder,
               "Matrix target folder must round-trip in project files");
        if (!loadedMatrix.targets.empty())
        {
            expect(!loadedMatrix.targets[0].expanded,
                   "Collapsed Matrix folder state must round-trip in project files");
            expect(loadedMatrix.targets[0].children.size() == 1
                   && loadedMatrix.targets[0].children[0].signalNumber == 101,
                   "Signals inside a collapsed Matrix folder must remain stored");
        }
    }

    file.deleteFile();
}


void testProviderPathAndMultiProviderProject()
{
    const auto a = er::ProviderPath::qualify("provider-a", "1.2.3");
    const auto b = er::ProviderPath::qualify("provider-b", "1.2.3");
    expect(a != b, "Identical Ember numeric paths on different providers must remain distinct");
    expect(er::ProviderPath::providerId(a) == "provider-a", "Qualified paths must retain provider id");
    expect(er::ProviderPath::localPath(a) == "1.2.3", "Qualified paths must retain local Ember path");
    expect(er::ControlTreeModel::isSameOrDescendant(er::ProviderPath::qualify("provider-a", "1.2.3"),
                                                   er::ProviderPath::root("provider-a")),
           "Provider root must be the parent boundary for every path on that provider");
    expect(!er::ControlTreeModel::isSameOrDescendant(er::ProviderPath::qualify("provider-b", "1.2.3"),
                                                    er::ProviderPath::root("provider-a")),
           "Provider subtree matching must never cross provider ids");

    er::ProviderConfig pa; pa.id = "provider-a"; pa.name = "Ravenna A"; pa.driver = "ember"; pa.host = "10.0.0.10"; pa.port = 9000; pa.enabled = true; pa.autoConnect = true;
    er::ProviderConfig pb; pb.id = "provider-b"; pb.name = "Lawo"; pb.driver = "ember"; pb.host = "10.0.0.20"; pb.port = 9001; pb.enabled = true; pb.autoConnect = false;
    er::PageSet pages;
    pages.pages()[0].controls.push_back({ a, "Ravenna A/Routing/Gain", er::ControlType::encoder, "Gain" });
    pages.pages()[0].controls.push_back({ b, "Lawo/Console/Gain", er::ControlType::encoder, "Gain" });

    const auto file = tempFile(".emberremote");
    juce::String error;
    expect(er::ProjectService::save(file, pages, { pa, pb }, error), "Multi-provider project save must succeed: " + error);
    er::PageSet loaded;
    std::vector<er::ProviderConfig> providers;
    expect(er::ProjectService::load(file, loaded, providers, error), "Multi-provider project load must succeed: " + error);
    expect(providers.size() == 2, "Project file must round-trip every configured provider");
    if (providers.size() == 2)
    {
        expect(providers[0].driver == "ember" && providers[1].driver == "ember",
               "Project file must round-trip provider driver ids");
    }
    expect(loaded.pages()[0].controls.size() == 2, "Project file must round-trip controls from both providers");
    if (loaded.pages()[0].controls.size() == 2)
    {
        expect(loaded.pages()[0].controls[0].parameterPath == a, "Provider A control path must round-trip");
        expect(loaded.pages()[0].controls[1].parameterPath == b, "Provider B control path must round-trip");
    }
    file.deleteFile();
}

void testLazyTreeModel()
{
    er::ControlTreeModel tree;
    tree.upsertNode("1", "Mixer");
    tree.upsertNode("1.2", "Input");
    tree.upsertParameter("1.2.3", "Gain", "", true, true);

    auto root = tree.childrenOf("");
    expect(root.size() == 1, "Tree root must contain one discovered top-level node");
    expect(!tree.isDirectoryLoaded("1"), "Discovered node must not be considered loaded before GetDirectory completes");

    tree.markDirectoryLoading("1", true);
    expect(tree.isDirectoryLoading("1"), "Tree must track lazy directory loading state");
    tree.markDirectoryLoaded("1", true);
    expect(tree.isDirectoryLoaded("1"), "Tree must mark requested directory as loaded");
    expect(!tree.isDirectoryLoading("1"), "Loaded directory must no longer be marked loading");

    auto children = tree.childrenOf("1.2");
    expect(children.size() == 1 && children[0].kind == er::TreeEntryKind::parameter,
           "Tree must keep parameter leaves under their numeric parent path");
    expect(er::ControlTreeModel::isSameOrDescendant("1.2.3", "1.2"), "Subtree prefix helper must match descendants");
    expect(!er::ControlTreeModel::isSameOrDescendant("1.20.3", "1.2"), "Subtree prefix helper must respect numeric path boundaries");

    tree.upsertNode("1.2.5", "Dynamics");
    tree.upsertNode("1.20", "Other Input");
    const auto expandable = er::ControlTreeModel::nodePathsUnderRoots(tree.snapshot(), { "1.2" });
    expect(expandable.count("1.2") == 1 && expandable.count("1.2.5") == 1,
           "Expand/collapse helper must include the selected node and all discovered node descendants");
    expect(expandable.count("1.20") == 0,
           "Expand/collapse helper must respect numeric path boundaries");
    expect(expandable.count("1.2.3") == 0,
           "Expand/collapse helper must not treat parameter leaves as directories");

    tree.upsertParameter("1.2.4", "Volume", "Main Fader Left", true, true);
    const auto snapshot = tree.snapshot();
    const auto filter = er::ControlTreeModel::buildFilter(snapshot, "fader left");
    expect(filter.matches.count("1.2.4") == 1, "Tree filter must match identifier/description text case-insensitively");
    expect(filter.visible.count("1") == 1 && filter.visible.count("1.2") == 1 && filter.visible.count("1.2.4") == 1,
           "Tree filter must retain the complete parent branch for every match");
    expect(filter.matches.count("1.2.3") == 0, "All filter tokens must match the same candidate full path");

    const auto identifierFilter = er::ControlTreeModel::buildFilter(snapshot, "gain");
    expect(identifierFilter.matches.count("1.2.3") == 1, "Tree filter must match Ember identifiers/labels");
    const auto pathFilter = er::ControlTreeModel::buildFilter(snapshot, "1.2.4");
    expect(pathFilter.matches.count("1.2.4") == 1, "Tree filter must also match the numeric Ember path");

    const auto parameterMatches = er::ControlTreeModel::parameterPathsMatchingFilter(snapshot, "fader left");
    expect(parameterMatches.size() == 1 && parameterMatches[0] == "1.2.4",
           "Filter bulk actions must return matching parameter leaves only");
    const auto nodeMatches = er::ControlTreeModel::parameterPathsMatchingFilter(snapshot, "Dynamics");
    expect(nodeMatches.empty(), "Filter bulk actions must never return matching branch nodes as controls");

    // Conjunctive full-path filtering: the terms may live on different levels
    // of the same Ember hierarchy, but every token must be present somewhere
    // on that candidate's complete path.
    tree.upsertNode("5", "Ravenna");
    tree.upsertNode("5.1", "Routing");
    tree.upsertNode("5.1.2", "Buffer");
    tree.upsertParameter("5.1.2.3", "RX", "Receive buffer", true, true);
    tree.upsertParameter("5.1.2.4", "TX", "Transmit buffer", true, true);
    const auto fullPathSnapshot = tree.snapshot();
    const auto fullPathFilter = er::ControlTreeModel::buildFilter(fullPathSnapshot, "Ravenna Buffer RX");
    expect(fullPathFilter.matches.count("5.1.2.3") == 1,
           "Full-path AND filter must match tokens distributed across ancestor levels and the parameter");
    expect(fullPathFilter.matches.count("5.1.2.4") == 0,
           "Full-path AND filter must require every token; RX must not match the TX parameter");
    expect(fullPathFilter.visible.count("5") == 1 && fullPathFilter.visible.count("5.1") == 1
           && fullPathFilter.visible.count("5.1.2") == 1 && fullPathFilter.visible.count("5.1.2.3") == 1,
           "Full-path filter must keep the complete Ravenna/Routing/Buffer/RX branch visible");
    const auto fullPathParameterMatches = er::ControlTreeModel::parameterPathsMatchingFilter(fullPathSnapshot, "Ravenna Buffer RX");
    expect(fullPathParameterMatches.size() == 1 && fullPathParameterMatches[0] == "5.1.2.3",
           "Select Matches / direct mapping must use the same path-aware AND semantics");

    // Exclusion tokens use a leading '-'. Positive terms remain conjunctive,
    // while an excluded token may occur anywhere on the candidate's complete
    // loaded ancestry/metadata and must remove that candidate from the result.
    tree.upsertNode("6", "Ravenna");
    tree.upsertNode("6.1", "Input");
    tree.upsertParameter("6.1.1", "Gain", "Main input gain", true, true);
    tree.upsertNode("6.1.2", "Buffer");
    tree.upsertParameter("6.1.2.1", "Gain", "Buffered input gain", true, true);
    const auto exclusionSnapshot = tree.snapshot();
    const auto exclusionFilter = er::ControlTreeModel::buildFilter(exclusionSnapshot, "Ravenna Input -Buffer");
    expect(exclusionFilter.matches.count("6.1.1") == 1,
           "Positive filter tokens must still be ANDed when exclusions are present");
    expect(exclusionFilter.matches.count("6.1.2.1") == 0,
           "A -token must exclude candidates when that token occurs anywhere in the full path");
    const auto exclusionParameterMatches = er::ControlTreeModel::parameterPathsMatchingFilter(exclusionSnapshot, "Ravenna Input -Buffer");
    expect(std::find(exclusionParameterMatches.begin(), exclusionParameterMatches.end(), juce::String("6.1.1")) != exclusionParameterMatches.end()
           && std::find(exclusionParameterMatches.begin(), exclusionParameterMatches.end(), juce::String("6.1.2.1")) == exclusionParameterMatches.end(),
           "Select Matches / match actions must use the same exclusion semantics as the visible filter");

    const auto exclusionOnly = er::ControlTreeModel::buildFilter(exclusionSnapshot, "-Buffer");
    expect(exclusionOnly.matches.count("6.1.1") == 1 && exclusionOnly.matches.count("6.1.2.1") == 0,
           "A pure exclusion filter must show loaded candidates except those containing the excluded token");
}

void testMappingDraft()
{
    er::ParameterInfo mute;
    mute.path = "1.1";
    mute.identifier = "Mute";
    mute.description = "Program Mute";
    mute.treePath = "Mixer / Inputs / Channel 1 / Mute";
    mute.kind = er::ParameterKind::booleanValue;
    mute.writable = true;
    mute.online = true;

    er::ParameterInfo gain;
    gain.path = "1.2";
    gain.identifier = "Gain";
    gain.description = "Main Fader";
    gain.treePath = "Mixer / Inputs / Channel 1 / Gain";
    gain.kind = er::ParameterKind::real;
    gain.writable = true;
    gain.online = true;

    er::MappingDraft draft;
    draft.addParameters({ mute, gain, gain }, 2);
    expect(draft.rows().size() == 2, "MappingDraft must de-duplicate parameter paths");
    expect(draft.rows()[0].type == er::ControlType::button, "Boolean parameter must default to Button");
    expect(draft.rows()[1].type == er::ControlType::encoder, "Numeric parameter must default to Encoder");
    expect(draft.rows()[0].label == "Mixer/Inputs/Channel 1/Mute"
           && draft.rows()[1].label == "Mixer/Inputs/Channel 1/Gain",
           "MappingDraft Label 1 must use the complete slash-separated Ember tree");
    expect(draft.rows()[0].label2 == "Program Mute" && draft.rows()[1].label2 == "Main Fader",
           "MappingDraft must prefill Label 2 from Ember descriptions when Label 1 uses the identifier");
    expect(draft.rows()[0].page == 2 && draft.rows()[1].page == 2, "Bulk mapping must use selected default page");

    draft.setAllEnabled(false);
    expect(!draft.rows()[0].enabled && !draft.rows()[1].enabled, "Select-all Use toggle must be able to disable every mapping row");
    draft.setEnabledForPaths({ "1.1", "1.2" }, true);
    expect(draft.allEnabled(), "Multi-row Use editing must enable every selected mapping path");
    expect(draft.setLabel2ForPath("1.2", "Edited Label 2"), "MappingDraft must allow Page Label 2 edits to stay synchronized");
    expect(draft.rows()[1].label2 == "Edited Label 2", "MappingDraft must retain synchronized Label 2 edits");
    expect(draft.setThresholdsForPath("1.2", true, 75.0, true, 90.0, true, 25.0, true, 10.0),
           "MappingDraft must synchronize all four list-view threshold edits");
    expect(draft.rows()[1].orangeAboveThresholdEnabled && draft.rows()[1].redAboveThresholdEnabled
           && draft.rows()[1].orangeBelowThresholdEnabled && draft.rows()[1].redBelowThresholdEnabled,
           "MappingDraft must retain all four synchronized threshold enable states");

    er::PageSet pages;
    draft.applyTo(pages);
    expect(pages.pages()[1].controls.size() == 2, "Applying mapping draft must create controls on selected page");
    expect(pages.pages()[1].controls[0].label2.isNotEmpty(), "Applied control must carry Label 2");
    pages.pages()[1].setControlColour("1.2", 0xff38a86b);
    draft.rows()[1].page = 3;
    draft.applyTo(pages);
    expect(pages.pages()[1].controls.size() == 1, "Re-applying after page change must move rather than duplicate mapping");
    expect(pages.pages()[2].controls.size() == 1, "Moved mapping must appear on its new page");
    if (!pages.pages()[2].controls.empty())
    {
        expect(pages.pages()[2].controls[0].colourArgb == 0xff38a86b, "Re-applying MappingDraft must preserve a right-click control colour");
        expect(pages.pages()[2].controls[0].orangeAboveThresholdEnabled && pages.pages()[2].controls[0].orangeAboveThreshold == 75.0,
               "MappingDraft must apply upper orange threshold to page controls");
        expect(pages.pages()[2].controls[0].redAboveThresholdEnabled && pages.pages()[2].controls[0].redAboveThreshold == 90.0,
               "MappingDraft must apply upper red threshold to page controls");
        expect(pages.pages()[2].controls[0].orangeBelowThresholdEnabled && pages.pages()[2].controls[0].orangeBelowThreshold == 25.0
               && pages.pages()[2].controls[0].redBelowThresholdEnabled && pages.pages()[2].controls[0].redBelowThreshold == 10.0,
               "MappingDraft must apply both lower thresholds to page controls");
    }

    draft.rows()[0].enabled = false;
    draft.applyTo(pages);
    expect(pages.pages()[1].controls.empty(), "Disabled mapping row must remove its previously applied control");

    // Apply-to-current-page behaviour: the UI no longer uses an independently
    // editable Page column. The active page is authoritative for every mapping
    // row and enabled controls move there without duplication.
    er::MappingDraft currentPageDraft;
    currentPageDraft.addParameters({ mute, gain }, 1);
    er::PageSet currentPagePages;
    currentPageDraft.applyToPage(currentPagePages, 4);
    expect(currentPagePages.pages().size() >= 4, "Apply to current page must create the selected target page when needed");
    expect(currentPagePages.pages()[3].controls.size() == 2, "All enabled Mapping Table rows must be assigned to the currently selected page");
    expect(currentPageDraft.rows()[0].page == 4 && currentPageDraft.rows()[1].page == 4,
           "Mapping Table Page display must update to the actual current-page assignment");
    currentPageDraft.applyToPage(currentPagePages, 2);
    expect(currentPagePages.pages()[3].controls.empty(), "Re-applying on another current page must move controls instead of duplicating them");
    expect(currentPagePages.pages()[1].controls.size() == 2, "Controls must follow the newly selected current page");

    er::PageSet importedPages;
    importedPages.pages()[0].controls.push_back({"8.8", "Imported/Control", er::ControlType::button, "Imported Label 2", 0xff9a6ad6});
    er::MappingDraft importedDraft;
    importedDraft.replaceFromPages(importedPages);
    expect(importedDraft.rows().size() == 1, "Imported Pages must rebuild one MappingDraft row per unique parameter");
    if (!importedDraft.rows().empty())
    {
        expect(importedDraft.rows()[0].label == "Imported/Control", "MappingDraft rebuild must preserve imported Label 1");
        expect(importedDraft.rows()[0].label2 == "Imported Label 2", "MappingDraft rebuild must preserve imported Label 2");
        expect(importedDraft.rows()[0].colourArgb == 0xff9a6ad6, "MappingDraft rebuild must preserve imported control colour");
    }
    importedDraft.removeRowsForPage(1);
    expect(importedDraft.rows().empty(), "Clear All must be able to remove Mapping Table rows assigned to the cleared page");

    if (!draft.rows().empty())
    {
        const auto path = draft.rows().front().path;
        const auto oldType = draft.rows().front().type;
        const auto newType = oldType == er::ControlType::button ? er::ControlType::encoder : er::ControlType::button;
        expect(draft.setTypeForPath(path, newType),
               "MappingDraft must support changing a control type by parameter path");
        expect(draft.rows().front().type == newType,
               "MappingDraft type change must persist in the draft row");
    }
}


void testShowContentControlType()
{
    er::ParameterInfo text;
    text.path = "2.3.4";
    text.identifier = "Message";
    text.description = "Current provider message";
    text.treePath = "System / Status / Message";
    text.kind = er::ParameterKind::stringValue;
    text.value = "Line one\nLine two";
    text.writable = false;
    text.online = true;

    er::MappingDraft draft;
    draft.addParameters({ text }, 1);
    expect(draft.rows().size() == 1, "String parameter must create one mapping row");
    if (!draft.rows().empty())
    {
        expect(draft.rows()[0].enabled, "Read-only online string parameter must be usable as Show Content");
        expect(draft.rows()[0].type == er::ControlType::showContent,
               "String parameter must default to Show Content");
    }

    er::PageSet pages;
    draft.applyTo(pages);
    expect(pages.pages()[0].controls.size() == 1, "Show Content mapping must apply to a page");
    if (!pages.pages()[0].controls.empty())
        expect(pages.pages()[0].controls[0].type == er::ControlType::showContent,
               "Applied string control must retain Show Content type");

    const auto projectFile = tempFile(".emberremote");
    juce::String error;
    expect(er::ProjectService::save(projectFile, pages, "127.0.0.1", 9000, error),
           "Show Content project save must succeed: " + error);
    er::PageSet loaded;
    juce::String host;
    int port = 0;
    expect(er::ProjectService::load(projectFile, loaded, host, port, error),
           "Show Content project load must succeed: " + error);
    expect(!loaded.pages()[0].controls.empty()
           && loaded.pages()[0].controls[0].type == er::ControlType::showContent,
           "Show Content control type must round-trip in project files");
    projectFile.deleteFile();

    const auto csvFile = tempFile(".csv");
    const juce::String csv =
        "path;enabled;control;page;label;label2\n"
        "2.3.4;true;show_content;1;System/Status/Message;Message\n";
    expect(csvFile.replaceWithText(csv), "Show Content CSV test file must be writable");
    er::PageSet imported;
    error.clear();
    expect(er::CsvService::importMappings(csvFile, imported, error),
           "Show Content CSV import must succeed: " + error);
    expect(!imported.pages()[0].controls.empty()
           && imported.pages()[0].controls[0].type == er::ControlType::showContent,
           "show_content CSV type must import as Show Content");
    csvFile.deleteFile();

    expect(text.formattedValue() == "Line one\nLine two",
           "String formattedValue must preserve complete content for Show Content");
}

void testOpenEndedEncoderRangePolicy()
{
    using er::ControlValuePolicy::extendOpenEndedUpperBound;
    expect(extendOpenEndedUpperBound(100.0, 99.0) == 100.0,
           "Open-ended encoder range must stay at 100 below its boundary");
    expect(extendOpenEndedUpperBound(100.0, 100.0) == 110.0,
           "Open-ended encoder range must extend by 10 when it reaches 100");
    expect(extendOpenEndedUpperBound(100.0, 101.0) == 110.0,
           "Returned values just above 100 must expand the encoder to 110");
    expect(extendOpenEndedUpperBound(100.0, 135.0) == 140.0,
           "High returned values must expand open-ended encoder ranges in 10-unit steps");
}


void testControlAlertPolicy()
{
    er::ControlMapping mapping;
    mapping.orangeAboveThresholdEnabled = true;
    mapping.orangeAboveThreshold = 80.0;
    mapping.redAboveThresholdEnabled = true;
    mapping.redAboveThreshold = 90.0;
    mapping.orangeBelowThresholdEnabled = true;
    mapping.orangeBelowThreshold = 20.0;
    mapping.redBelowThresholdEnabled = true;
    mapping.redBelowThreshold = 10.0;

    er::ParameterInfo info;
    info.kind = er::ParameterKind::real;
    info.factor = 1.0;
    info.value = 50.0;
    expect(er::ControlAlertPolicy::evaluate(mapping, info) == er::AlertState::normal,
           "Values inside the four-threshold window must remain green/normal");
    info.value = 85.0;
    expect(er::ControlAlertPolicy::evaluate(mapping, info) == er::AlertState::orange,
           "Values at or above upper orange but below upper red must enter orange state");
    info.value = 95.0;
    expect(er::ControlAlertPolicy::evaluate(mapping, info) == er::AlertState::red,
           "Values at or above upper red must enter red state");
    info.value = 15.0;
    expect(er::ControlAlertPolicy::evaluate(mapping, info) == er::AlertState::orange,
           "Values at or below lower orange but above lower red must enter orange state");
    info.value = 5.0;
    expect(er::ControlAlertPolicy::evaluate(mapping, info) == er::AlertState::red,
           "Values at or below lower red must enter red state");
}

void testParameterDisplayNameFallback()
{
    er::ParameterInfo p;
    p.path = "7.8.9";
    expect(p.displayName() == "7.8.9", "displayName must fall back to numeric path");
    p.description = "Description";
    expect(p.displayName() == "Description", "displayName must prefer description over path");
    p.identifier = "Identifier";
    expect(p.displayName() == "Identifier", "displayName must prefer identifier");
    p.treePath = "Mixer / Inputs / Channel 1 / Identifier";
    expect(p.fullTreeLabel() == "Mixer/Inputs/Channel 1/Identifier",
           "fullTreeLabel must compact the complete Ember tree with slash separators");
}

void testParameterValueFormatting()
{
    er::ParameterInfo p;
    p.kind = er::ParameterKind::integer;
    p.value = 42;
    expect(p.formattedValue() == "42", "Integer status values must be shown without decimals");

    p.kind = er::ParameterKind::real;
    p.value = 123;
    p.factor = 10.0;
    expect(p.formattedValue() == "12.3", "Real status values must respect the Ember factor");

    p.kind = er::ParameterKind::booleanValue;
    p.value = true;
    expect(p.formattedValue() == "true", "Boolean status values must be human-readable");

    p.kind = er::ParameterKind::enumeration;
    p.value = 2;
    p.enumEntries.clear();
    p.enumEntries.add("1:Off");
    p.enumEntries.add("2:On Air");
    expect(p.formattedValue() == "On Air", "Enum status values must show their Ember label when available");
}
}

int main()
{
    testPageSet();
    testPageControlRemoval();
    testParameterStoreRevisions();
    testCsvExportAndImport();
    testCsvRejectsMissingPathColumn();
    testProjectRoundTrip();
    testProviderPathAndMultiProviderProject();
    testLazyTreeModel();
    testMappingDraft();
    testShowContentControlType();
    testOpenEndedEncoderRangePolicy();
    testControlAlertPolicy();
    testParameterDisplayNameFallback();
    testParameterValueFormatting();

    if (failures == 0)
    {
        std::cout << "All EmberRemote regression tests passed." << std::endl;
        return 0;
    }

    std::cerr << failures << " EmberRemote regression test(s) failed." << std::endl;
    return 1;
}
