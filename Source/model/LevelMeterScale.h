#pragma once

#include <algorithm>
#include <cmath>

namespace er
{
struct LevelMeterScale
{
    static constexpr double minimum = -80.0;
    static constexpr double initialMaximum = 5.0;
    static constexpr double extensionStep = 5.0;

    static double extendMaximum(double currentMaximum, double value) noexcept
    {
        if (!std::isfinite(value) || value <= currentMaximum)
            return currentMaximum;

        return std::max(initialMaximum,
                        std::ceil(value / extensionStep) * extensionStep);
    }

    static double normalised(double value, double maximum) noexcept
    {
        if (!std::isfinite(value) || maximum <= minimum)
            return 0.0;

        return std::clamp((value - minimum) / (maximum - minimum), 0.0, 1.0);
    }
};
}
