#pragma once

#include <algorithm>
#include <utility>

namespace MphRead::Mods::Input
{
    struct StickCalibration
    {
        float CenterX = 0.0F;
        float CenterY = 0.0F;
        float MinX = 0.0F;
        float MaxX = 0.0F;
        float MinY = 0.0F;
        float MaxY = 0.0F;

        [[nodiscard]] static constexpr StickCalibration Default() noexcept
        {
            return {0, 0, -1, 1, -1, 1};
        }

        [[nodiscard]] std::pair<float, float> Normalize(float x, float y) const noexcept
        {
            return {Axis(x, CenterX, MinX, MaxX), Axis(y, CenterY, MinY, MaxY)};
        }

        friend constexpr bool operator==(const StickCalibration&, const StickCalibration&) noexcept = default;

    private:
        [[nodiscard]] static float Axis(float value, float center, float min, float max) noexcept
        {
            return std::clamp((value - center) / std::max(.1F, value >= center ? max - center : center - min), -1.0F, 1.0F);
        }
    };
}
