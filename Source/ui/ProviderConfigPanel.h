#pragma once
#include <JuceHeader.h>
#include "../backend/ProviderManager.h"

namespace er
{
class ProviderConfigPanel final : public juce::Component,
                                  private juce::TableListBoxModel
{
public:
    explicit ProviderConfigPanel(ProviderManager& managerToUse);

    void refresh();
    void setRecentHosts(std::vector<juce::String> hosts);

    std::function<void()> onConfigurationChanged;
    std::function<void(const juce::String&)> onSuccessfulHost;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum ColumnIds { enabledColumn = 1, autoColumn, nameColumn, hostColumn, portColumn, statusColumn, actionColumn, removeColumn };

    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;

    ProviderConfig* rowAt(int rowNumber);
    const ProviderConfig* rowAt(int rowNumber) const;
    void commitConfig(const ProviderConfig& config);

    ProviderManager& manager;
    std::vector<ProviderConfig> rows;
    std::vector<juce::String> recentHosts;

    juce::Label title;
    juce::Label info;
    juce::TableListBox table { "ProviderTable", this };
    juce::TextButton addButton { "+ Provider" };
    juce::TextButton connectEnabledButton { "Connect Enabled" };
    juce::TextButton disconnectAllButton { "Disconnect All" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProviderConfigPanel)
};
}
