#include "Testing/test_effects.hpp"

#include <limits>
#include <stdexcept>

namespace fruityprime::testing::effects {

std::int32_t fx_div(std::int32_t left, std::int32_t right) {
    if (right == 0) {
        throw std::invalid_argument("fixed-point division by zero");
    }
    const std::int64_t numerator = static_cast<std::int64_t>(left) << 12;
    const std::int64_t quotient = numerator / right;
    if (quotient < std::numeric_limits<std::int32_t>::min()
        || quotient > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error("fixed-point division overflow");
    }
    return static_cast<std::int32_t>(quotient);
}

std::int32_t interpolate_fx41(std::span<const std::int32_t> parameters,
                              std::int32_t percent) {
    if (parameters.size() < 2 || (parameters.size() & 1U) != 0) {
        throw std::invalid_argument("Fx41 parameters must be value pairs");
    }
    std::int32_t index1 = -1;
    std::size_t index2 = 0;
    if (percent < parameters[0]) {
        return parameters[1];
    }
    if (parameters[0] != std::numeric_limits<std::int32_t>::min()) {
        for (;;) {
            if (parameters[index2] > percent) {
                break;
            }
            index1 = static_cast<std::int32_t>(index2);
            if (index2 + 2 >= parameters.size()) {
                throw std::invalid_argument(
                    "Fx41 parameters need a terminal Int32.MinValue");
            }
            const std::int32_t next = parameters[index2 + 2];
            index2 += 2;
            if (next == std::numeric_limits<std::int32_t>::min()) {
                break;
            }
        }
    }
    if (index1 < 0) {
        return 0;
    }

    const auto start = static_cast<std::size_t>(index1);
    const std::int32_t next_percent = parameters[start + 2];
    if (next_percent == std::numeric_limits<std::int32_t>::min()) {
        return parameters[start + 1];
    }
    const std::int32_t denominator = next_percent - parameters[start];
    const std::int32_t local_percent = fx_div(
        percent - parameters[start], denominator);
    const std::int64_t difference = static_cast<std::int64_t>(
        parameters[start + 3] - parameters[start + 1]);
    const std::int64_t product = difference * local_percent + 2048;
    const std::int64_t delta = product >> 12;
    const std::int64_t result = parameters[start + 1] + delta;
    if (result < std::numeric_limits<std::int32_t>::min()
        || result > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error("Fx41 interpolation overflow");
    }
    return static_cast<std::int32_t>(result);
}

AngleLookup effect_angle_lookup(std::uint32_t random_value) noexcept {
    constexpr std::uint64_t kAngleScale = 0xB60B60B60BULL;
    const std::uint64_t quotient = (kAngleScale * random_value) >> 32;
    const std::int32_t index1 = static_cast<std::int32_t>(
        2 * ((16 * (quotient + 2048U)) >> 20));
    const std::int32_t index2 = index1 + 1;
    const std::int32_t index3 = static_cast<std::int32_t>(
        2 * (((((quotient + 2048U) >> 12) + 0x4000U) >> 4)));
    const std::int32_t index4 = index3 + 1;
    return {
        index1, index2, index3, index4,
        static_cast<float>(index1 / 2) * (360.0F / 4096.0F),
        static_cast<float>(index3 / 2) * (360.0F / 4096.0F)};
}

} // namespace fruityprime::testing::effects
