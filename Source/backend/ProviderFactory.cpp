#include "ProviderFactory.h"
#include "../providers/ember/EmberProvider.h"

namespace er
{
juce::String ProviderFactory::normaliseDriver(juce::String driver)
{
    driver = driver.trim().toLowerCase();
    return driver.isEmpty() ? juce::String("ember") : driver;
}

std::unique_ptr<IProviderBackend> ProviderFactory::create(const ProviderConfig& config)
{
    const auto driver = normaliseDriver(config.driver);
    if (driver == "ember" || driver == "ember+" || driver == "emberplus")
        return std::make_unique<EmberProvider>();
    return {};
}
}
