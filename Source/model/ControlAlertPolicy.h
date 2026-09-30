#pragma once
#include <JuceHeader.h>
#include "PageModel.h"
#include "ParameterInfo.h"
#include <optional>
#include <cmath>

namespace er
{
enum class AlertState { normal, orange, red };

namespace ControlAlertPolicy
{
inline bool supportsThresholds(const ParameterInfo& info)
{
    return info.kind == ParameterKind::integer || info.kind == ParameterKind::real;
}

inline std::optional<double> displayNumericValue(const ParameterInfo& info)
{
    if (!supportsThresholds(info) || info.value.isVoid())
        return std::nullopt;

    const double factor = info.factor == 0.0 ? 1.0 : info.factor;
    const double value = info.value.toString().getDoubleValue() / factor;
    if (!std::isfinite(value))
        return std::nullopt;
    return value;
}

inline bool hasThresholds(const ControlMapping& mapping)
{
    return mapping.orangeAboveThresholdEnabled || mapping.redAboveThresholdEnabled
        || mapping.orangeBelowThresholdEnabled || mapping.redBelowThresholdEnabled;
}

inline AlertState evaluate(const ControlMapping& mapping, const ParameterInfo& info)
{
    const auto value = displayNumericValue(info);
    if (!value)
        return AlertState::normal;

    const bool red = (mapping.redAboveThresholdEnabled && *value >= mapping.redAboveThreshold)
                  || (mapping.redBelowThresholdEnabled && *value <= mapping.redBelowThreshold);
    if (red)
        return AlertState::red;

    const bool orange = (mapping.orangeAboveThresholdEnabled && *value >= mapping.orangeAboveThreshold)
                     || (mapping.orangeBelowThresholdEnabled && *value <= mapping.orangeBelowThreshold);
    if (orange)
        return AlertState::orange;

    return AlertState::normal;
}

}
}
