#pragma once

#include "enemy_catalog.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::enemy::detail {

[[nodiscard]] inline std::uint32_t read_u32(
    std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    if (offset + 4 > bytes.size()) {
        return 0;
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

[[nodiscard]] inline std::uint16_t read_u16(
    std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    if (offset + 2 > bytes.size()) {
        return 0;
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

[[nodiscard]] inline float read_fixed(
    std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<float>(static_cast<std::int32_t>(
        read_u32(bytes, offset))) / 4096.0F;
}

[[nodiscard]] inline net::Vec3 read_vector(
    std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return {read_fixed(bytes, offset), read_fixed(bytes, offset + 4),
            read_fixed(bytes, offset + 8)};
}

[[nodiscard]] inline net::Vec3 add(net::Vec3 left, net::Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline scene::VolumePoint to_volume_point(net::Vec3 value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] inline scene::EntityVolume read_volume(
    std::span<const std::uint8_t> bytes, std::size_t offset,
    net::Vec3 origin) noexcept {
    scene::EntityVolume volume;
    if (offset + 64 > bytes.size()) {
        return volume;
    }
    switch (read_u32(bytes, offset)) {
    case static_cast<std::uint32_t>(formats::VolumeType::Box):
        volume.kind = scene::VolumeKind::Box;
        volume.box_vector1 = to_volume_point(read_vector(bytes, offset + 4));
        volume.box_vector2 = to_volume_point(read_vector(bytes, offset + 16));
        volume.box_vector3 = to_volume_point(read_vector(bytes, offset + 28));
        volume.box_position = to_volume_point(add(
            read_vector(bytes, offset + 40), origin));
        volume.box_dot1 = read_fixed(bytes, offset + 52);
        volume.box_dot2 = read_fixed(bytes, offset + 56);
        volume.box_dot3 = read_fixed(bytes, offset + 60);
        break;
    case static_cast<std::uint32_t>(formats::VolumeType::Cylinder):
        volume.kind = scene::VolumeKind::Cylinder;
        volume.cylinder_vector = to_volume_point(
            read_vector(bytes, offset + 4));
        volume.cylinder_position = to_volume_point(add(
            read_vector(bytes, offset + 16), origin));
        volume.cylinder_radius = read_fixed(bytes, offset + 28);
        volume.cylinder_dot = read_fixed(bytes, offset + 32);
        break;
    case static_cast<std::uint32_t>(formats::VolumeType::Sphere):
        volume.kind = scene::VolumeKind::Sphere;
        volume.sphere_position = to_volume_point(add(
            read_vector(bytes, offset + 4), origin));
        volume.sphere_radius = read_fixed(bytes, offset + 16);
        break;
    default:
        break;
    }
    return volume;
}

template <typename T>
[[nodiscard]] inline constexpr std::array<T, 12> fill_cretaphid_array(T value) {
    std::array<T, 12> result{};
    result.fill(value);
    return result;
}

} // namespace fruityprime::enemy::detail
