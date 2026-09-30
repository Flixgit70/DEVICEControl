#include "ProviderConfigPanel.h"
#include "../app/Theme.h"

namespace er
{
namespace
{
class ToggleEditor final : public juce::ToggleButton
{
public:
    std::function<void(bool)> changed;
    ToggleEditor() { onClick = [this] { if (changed) changed(getToggleState()); }; }
};

class TextEditorCell final : public juce::TextEditor
{
public:
    std::function<void(const juce::String&)> committed;
    TextEditorCell()
    {
        setSelectAllWhenFocused(true);
        onReturnKey = [this] { commit(); };
        onFocusLost = [this] { commit(); };
    }
    void commit() { if (committed) committed(getText().trim()); }
};

class HostEditorCell final : public juce::ComboBox
{
public:
    std::function<void(const juce::String&)> committed;
    HostEditorCell()
    {
        setEditableText(true);
        onChange = [this] { if (committed) committed(getText().trim()); };
    }
};

class ActionButton final : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
};
}

ProviderConfigPanel::ProviderConfigPanel(ProviderManager& managerToUse) : manager(managerToUse)
{
    title.setText("PROVIDERS", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    addAndMakeVisible(title);

    info.setColour(juce::Label::textColourId, Theme::text);
    info.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(info);

    auto& h = table.getHeader();
    h.setStretchToFitActive(false);
    h.addColumn("Use", enabledColumn, 52, 44, 80, juce::TableHeaderComponent::notSortable);
    h.addColumn("Auto", autoColumn, 56, 46, 80, juce::TableHeaderComponent::notSortable);
    h.addColumn("Name", nameColumn, 170, 90, -1, juce::TableHeaderComponent::notSortable);
    h.addColumn("Host / IP", hostColumn, 220, 120, -1, juce::TableHeaderComponent::notSortable);
    h.addColumn("Port", portColumn, 76, 60, 120, juce::TableHeaderComponent::notSortable);
    h.addColumn("Status", statusColumn, 220, 120, -1, juce::TableHeaderComponent::notSortable);
    h.addColumn("Connection", actionColumn, 105, 90, 140, juce::TableHeaderComponent::notSortable);
    h.addColumn("", removeColumn, 74, 64, 90, juce::TableHeaderComponent::notSortable);
    table.setRowHeight(32);
    table.setColour(juce::ListBox::backgroundColourId, Theme::background);
    if (auto* viewport = table.getViewport()) viewport->setScrollBarsShown(true, true);
    addAndMakeVisible(table);

    addButton.onClick = [this]
    {
        manager.addProvider();
        refresh();
        if (onConfigurationChanged) onConfigurationChanged();
    };
    connectEnabledButton.onClick = [this]
    {
        manager.connectEnabled(false);
        refresh();
        if (onConfigurationChanged) onConfigurationChanged();
    };
    disconnectAllButton.onClick = [this]
    {
        manager.disconnectAll();
        refresh();
    };
    addAndMakeVisible(addButton);
    addAndMakeVisible(connectEnabledButton);
    addAndMakeVisible(disconnectAllButton);
    refresh();
}

void ProviderConfigPanel::setRecentHosts(std::vector<juce::String> hosts)
{
    recentHosts = std::move(hosts);
    table.updateContent();
}

void ProviderConfigPanel::refresh()
{
    rows = manager.providerConfigs();
    table.updateContent();
    table.repaint();
    info.setText(juce::String(manager.connectedCount()) + "/" + juce::String((int) rows.size())
                     + " connected  |  enabled providers run in parallel and appear as separate roots in the device tree",
                 juce::dontSendNotification);
}

void ProviderConfigPanel::paint(juce::Graphics& g)
{
    g.setColour(Theme::panelRaised);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
}

void ProviderConfigPanel::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto top = r.removeFromTop(30);
    title.setBounds(top.removeFromLeft(180));
    disconnectAllButton.setBounds(top.removeFromRight(110).reduced(0, 1));
    top.removeFromRight(6);
    connectEnabledButton.setBounds(top.removeFromRight(128).reduced(0, 1));
    top.removeFromRight(6);
    addButton.setBounds(top.removeFromRight(96).reduced(0, 1));
    r.removeFromTop(4);
    info.setBounds(r.removeFromBottom(24));
    r.removeFromBottom(4);
    table.setBounds(r);
}

int ProviderConfigPanel::getNumRows() { return (int) rows.size(); }
ProviderConfig* ProviderConfigPanel::rowAt(int rowNumber)
{
    return rowNumber >= 0 && rowNumber < (int) rows.size() ? &rows[(size_t) rowNumber] : nullptr;
}
const ProviderConfig* ProviderConfigPanel::rowAt(int rowNumber) const
{
    return rowNumber >= 0 && rowNumber < (int) rows.size() ? &rows[(size_t) rowNumber] : nullptr;
}

void ProviderConfigPanel::paintRowBackground(juce::Graphics& g, int, int width, int height, bool selected)
{
    if (selected)
    {
        g.setColour(Theme::accentDark);
        g.fillRect(0, 0, width, height);
    }
}

void ProviderConfigPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool)
{
    const auto* row = rowAt(rowNumber);
    if (!row || columnId != statusColumn) return;
    g.setColour(manager.isProviderConnected(row->id) ? Theme::green : Theme::textDim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(manager.providerStatus(row->id), 5, 0, width - 10, height, juce::Justification::centredLeft, true);
}

void ProviderConfigPanel::commitConfig(const ProviderConfig& config)
{
    manager.updateProvider(config);
    rows = manager.providerConfigs();
    table.updateContent();
    table.repaint();
    info.setText(juce::String(manager.connectedCount()) + "/" + juce::String((int) rows.size())
                     + " connected  |  enabled providers run in parallel and appear as separate roots in the device tree",
                 juce::dontSendNotification);
    if (onConfigurationChanged) onConfigurationChanged();
}

juce::Component* ProviderConfigPanel::refreshComponentForCell(int rowNumber, int columnId, bool,
                                                               juce::Component* existing)
{
    auto* row = rowAt(rowNumber);
    if (!row) { delete existing; return nullptr; }
    const auto id = row->id;

    if (columnId == enabledColumn || columnId == autoColumn)
    {
        auto* c = dynamic_cast<ToggleEditor*>(existing);
        if (!c) { delete existing; c = new ToggleEditor(); }
        c->setToggleState(columnId == enabledColumn ? row->enabled : row->autoConnect, juce::dontSendNotification);
        c->changed = [this, id, columnId] (bool value)
        {
            auto currentRows = manager.providerConfigs();
            for (auto& cfg : currentRows) if (cfg.id == id)
            {
                if (columnId == enabledColumn) cfg.enabled = value;
                else cfg.autoConnect = value;
                commitConfig(cfg);
                break;
            }
        };
        return c;
    }

    if (columnId == nameColumn || columnId == portColumn)
    {
        auto* c = dynamic_cast<TextEditorCell*>(existing);
        if (!c) { delete existing; c = new TextEditorCell(); }
        if (columnId == portColumn) c->setInputRestrictions(5, "0123456789");
        else c->setInputRestrictions(0);
        const auto text = columnId == nameColumn ? row->name : juce::String(row->port);
        if (c->getText() != text) c->setText(text, false);
        c->committed = [this, id, columnId] (const juce::String& value)
        {
            auto currentRows = manager.providerConfigs();
            for (auto& cfg : currentRows) if (cfg.id == id)
            {
                if (columnId == nameColumn) cfg.name = value.isNotEmpty() ? value : "Provider";
                else cfg.port = juce::jmax(1, value.getIntValue());
                commitConfig(cfg);
                break;
            }
        };
        return c;
    }

    if (columnId == hostColumn)
    {
        auto* c = dynamic_cast<HostEditorCell*>(existing);
        if (!c) { delete existing; c = new HostEditorCell(); }
        c->clear(juce::dontSendNotification);
        int itemId = 1;
        for (const auto& host : recentHosts) c->addItem(host, itemId++);
        c->setText(row->host, juce::dontSendNotification);
        c->committed = [this, id] (const juce::String& value)
        {
            auto currentRows = manager.providerConfigs();
            for (auto& cfg : currentRows) if (cfg.id == id)
            {
                cfg.host = value;
                commitConfig(cfg);
                break;
            }
        };
        return c;
    }

    if (columnId == actionColumn)
    {
        auto* c = dynamic_cast<ActionButton*>(existing);
        if (!c) { delete existing; c = new ActionButton(); }
        const bool connected = manager.isProviderConnected(id);
        c->setButtonText(connected ? "Disconnect" : "Connect");
        c->onClick = [this, id]
        {
            if (manager.isProviderConnected(id)) manager.disconnectProvider(id);
            else if (manager.connectProvider(id))
            {
                const auto configs = manager.providerConfigs();
                for (const auto& cfg : configs) if (cfg.id == id && onSuccessfulHost) onSuccessfulHost(cfg.host);
            }
            refresh();
        };
        return c;
    }

    if (columnId == removeColumn)
    {
        auto* c = dynamic_cast<ActionButton*>(existing);
        if (!c) { delete existing; c = new ActionButton(); }
        c->setButtonText("Remove");
        c->onClick = [this, id]
        {
            manager.removeProvider(id);
            refresh();
            if (onConfigurationChanged) onConfigurationChanged();
        };
        return c;
    }

    delete existing;
    return nullptr;
}
}
