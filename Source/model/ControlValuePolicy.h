#pragma once
#include <algorithm>
#include <cmath>

namespace er
{
namespace ControlValuePolicy
{
inline double extendOpenEndedUpperBound(double currentUpper, double value, double quantum = 10.0)
{
    quantum = std::max(0.001, std::abs(quantum));
    if (!std::isfinite(currentUpper) || currentUpper <= 0.0)
        currentUpper = 100.0;
    if (!std::isfinite(value) || value < currentUpper)
        return currentUpper;

    const auto steps = std::floor((value - currentUpper) / quantum) + 1.0;
    return currentUpper + steps * quantum;
}
}
}
