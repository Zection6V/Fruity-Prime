#pragma once

#include <cstdint>
#include <span>

namespace fruityprime::testing::effects {

struct AngleLookup {
    std::int32_t index1 = 0;
    std::int32_t index2 = 0;
    std::int32_t index3 = 0;
    std::int32_t index4 = 0;
    float angle1 = 0.0F;
    float angle2 = 0.0F;
};

// Fixed-point helpers from Testing/TestEffects.cs.  They preserve the DS
// 12-bit fractional arithmetic instead of replacing it with floating point.
[[nodiscard]] std::int32_t fx_div(std::int32_t left,
                                  std::int32_t right);
[[nodiscard]] std::int32_t interpolate_fx41(
    std::span<const std::int32_t> parameters, std::int32_t percent);
[[nodiscard]] AngleLookup effect_angle_lookup(std::uint32_t random_value) noexcept;

} // namespace fruityprime::testing::effects
