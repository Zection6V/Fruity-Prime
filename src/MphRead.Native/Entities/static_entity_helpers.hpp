#pragma once

#include "Entities/static_entities.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruityprime::entities::static_entities::detail {

[[nodiscard]] inline float valid_seconds(float seconds) noexcept {
    return std::isfinite(seconds) && seconds > 0.0F ? seconds : 0.0F;
}

[[nodiscard]] inline scene::VolumePoint subtract(
    scene::VolumePoint left, scene::VolumePoint right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline scene::VolumePoint add(
    scene::VolumePoint left, scene::VolumePoint right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline scene::VolumePoint multiply(
    scene::VolumePoint value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline float length(scene::VolumePoint value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

inline void translate_volume(scene::EntityVolume& volume,
                             scene::VolumePoint delta) noexcept {
    volume.box_position = add(volume.box_position, delta);
    volume.cylinder_position = add(volume.cylinder_position, delta);
    volume.sphere_position = add(volume.sphere_position, delta);
}

[[nodiscard]] inline scene::EntityVolume first_hunt_volume(
    const raw::FhRawCollisionVolume& source,
    scene::VolumePoint entity_position) noexcept {
    scene::EntityVolume result;
    const auto point = [](const formats::Vector3Fx& value) {
        return scene::VolumePoint{value.x.to_float(), value.y.to_float(),
                                  value.z.to_float()};
    };
    switch (source.type) {
    case formats::FhVolumeType::Sphere:
        result.kind = scene::VolumeKind::Sphere;
        result.sphere_position = add(point(source.data.sphere.position),
                                     entity_position);
        result.sphere_radius = source.data.sphere.radius.to_float();
        break;
    case formats::FhVolumeType::Box:
        result.kind = scene::VolumeKind::Box;
        result.box_position = add(point(source.data.box.position),
                                  entity_position);
        result.box_vector1 = point(source.data.box.vector1);
        result.box_vector2 = point(source.data.box.vector2);
        result.box_vector3 = point(source.data.box.vector3);
        result.box_dot1 = source.data.box.dot1.to_float();
        result.box_dot2 = source.data.box.dot2.to_float();
        result.box_dot3 = source.data.box.dot3.to_float();
        break;
    case formats::FhVolumeType::Cylinder:
        result.kind = scene::VolumeKind::Cylinder;
        result.cylinder_position = add(point(source.data.cylinder.position),
                                       entity_position);
        result.cylinder_vector = point(source.data.cylinder.vector);
        result.cylinder_dot = source.data.cylinder.dot.to_float();
        result.cylinder_radius = source.data.cylinder.radius.to_float();
        break;
    }
    return result;
}

[[nodiscard]] inline const raw::FhRawCollisionVolume& first_hunt_volume_for(
    formats::FhTriggerType type, const raw::FhRawCollisionVolume& box,
    const raw::FhRawCollisionVolume& sphere,
    const raw::FhRawCollisionVolume& cylinder) noexcept {
    if (type == formats::FhTriggerType::Box) {
        return box;
    }
    if (type == formats::FhTriggerType::Cylinder) {
        return cylinder;
    }
    return sphere;
}

[[nodiscard]] inline bool cartridge_message(
    const messaging::MessageInfo& info, std::uint32_t value) noexcept {
    return info.cartridge_message == value;
}

[[nodiscard]] inline bool activation_message(
    const messaging::MessageInfo& info) noexcept {
    return cartridge_message(info, 18) || cartridge_message(info, 44)
        || info.message == messaging::Message::Activate
        || info.message == messaging::Message::Open
        || info.message == messaging::Message::Start;
}

[[nodiscard]] inline bool deactivation_message(
    const messaging::MessageInfo& info) noexcept {
    return cartridge_message(info, 6) || cartridge_message(info, 45)
        || info.message == messaging::Message::Deactivate
        || info.message == messaging::Message::Close
        || info.message == messaging::Message::Stop;
}

template <typename T>
[[nodiscard]] inline const T& data_or_default(
    const scene::EntityInstance& source) {
    if (const auto* data = std::get_if<T>(&source.typed_data);
        data != nullptr) {
        return *data;
    }
    static const T empty{};
    return empty;
}

class BasicEntity final : public Entity {
public:
    explicit BasicEntity(const scene::EntityInstance& source) noexcept
        : Entity(source) {}
};

} // namespace fruityprime::entities::static_entities::detail
