#pragma once

#include "Entities/runtime_entities.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::runtime::detail {

inline constexpr float kMinimumStep = 1.0e-6F;

[[nodiscard]] inline float finite_seconds(float seconds) noexcept {
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        return 0.0F;
    }
    return seconds;
}

[[nodiscard]] inline net::Vec3 add(net::Vec3 left, net::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline net::Vec3 multiply(net::Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline float length_squared(net::Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] inline net::Vec3 safe_velocity(
    const BeamProjectile& projectile) noexcept {
    if (length_squared(projectile.velocity)
        > kMinimumStep * kMinimumStep) {
        return projectile.velocity;
    }
    return multiply(projectile.direction, projectile.speed);
}

[[nodiscard]] inline bool cartridge_message(
    const messaging::MessageInfo& info, std::uint32_t value) noexcept {
    return info.cartridge_message == value;
}

[[nodiscard]] inline bool internal_message(
    const messaging::MessageInfo& info, messaging::Message value) noexcept {
    return info.cartridge_message == 0 && info.message == value;
}

} // namespace fruityprime::runtime::detail

