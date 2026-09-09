// Native counterpart of src/MphRead/Entities/Players/DynamicLightEntity.cs.
#include "DynamicLightEntity.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::players {
namespace {

[[nodiscard]] net::Vec3 normalized_or(net::Vec3 value,
                                      net::Vec3 fallback) noexcept {
    const float length_squared = value.x * value.x + value.y * value.y
        + value.z * value.z;
    if (length_squared <= 0.0000001F) {
        return fallback;
    }
    const float inverse = 1.0F / std::sqrt(length_squared);
    return {value.x * inverse, value.y * inverse, value.z * inverse};
}

[[nodiscard]] float update_channel(float current, float source,
                                   float frames) noexcept {
    constexpr float color_step = 8.0F / 255.0F;
    const float difference = source - current;
    if (std::abs(difference) < color_step) {
        return source;
    }
    if (current > source) {
        const int factor = static_cast<int>(std::trunc(
            (difference + color_step) / (8.0F * color_step)));
        if (factor <= -1) {
            return current + static_cast<float>(factor - 1)
                * color_step * frames;
        }
        return current - color_step * frames;
    }
    const int factor = static_cast<int>(std::trunc(
        difference / (8.0F * color_step)));
    if (factor >= 1) {
        return current + static_cast<float>(factor) * color_step * frames;
    }
    return current + color_step * frames;
}

void approach_vector(net::Vec3& current, net::Vec3 source,
                     float frames) noexcept {
    const float factor = frames / 8.0F;
    current.x += (source.x - current.x) * factor;
    current.y += (source.y - current.y) * factor;
    current.z += (source.z - current.z) * factor;
}

void approach_color(net::Vec3& current, net::Vec3 source,
                    float frames) noexcept {
    current.x = update_channel(current.x, source.x, frames);
    current.y = update_channel(current.y, source.y, frames);
    current.z = update_channel(current.z, source.z, frames);
}

} // namespace

void DynamicLightEntityBase::reset_lights(
    net::Vec3 light1_vector, net::Vec3 light1_color,
    net::Vec3 light2_vector, net::Vec3 light2_color) noexcept {
    light1_vector_ = normalized_or(light1_vector, {0.0F, 1.0F, 0.0F});
    light1_color_ = light1_color;
    light2_vector_ = normalized_or(light2_vector, {0.0F, 1.0F, 0.0F});
    light2_color_ = light2_color;
}

void DynamicLightEntityBase::update_light_sources(
    net::Vec3 position, std::span<const LightSource> sources,
    net::Vec3 room_light1_vector, net::Vec3 room_light1_color,
    net::Vec3 room_light2_vector, net::Vec3 room_light2_color,
    float frame_seconds) noexcept {
    const float frames = std::max(0.0F, frame_seconds * 30.0F);
    bool has_light1 = false;
    bool has_light2 = false;
    for (const auto& source : sources) {
        if (!source.volume.contains({position.x, position.y, position.z})) {
            continue;
        }
        if (source.light1_enabled) {
            has_light1 = true;
            approach_vector(light1_vector_, source.light1_vector, frames);
            approach_color(light1_color_, source.light1_color, frames);
        }
        if (source.light2_enabled) {
            has_light2 = true;
            approach_vector(light2_vector_, source.light2_vector, frames);
            approach_color(light2_color_, source.light2_color, frames);
        }
    }
    if (!has_light1) {
        approach_vector(light1_vector_, room_light1_vector, frames);
        approach_color(light1_color_, room_light1_color, frames);
    }
    if (!has_light2) {
        approach_vector(light2_vector_, room_light2_vector, frames);
        approach_color(light2_color_, room_light2_color, frames);
    }
    light1_vector_ = normalized_or(light1_vector_, {0.0F, 1.0F, 0.0F});
    light2_vector_ = normalized_or(light2_vector_, {0.0F, 1.0F, 0.0F});
}

} // namespace fruityprime::players
