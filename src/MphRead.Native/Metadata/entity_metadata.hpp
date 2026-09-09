#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace fruityprime::metadata {

// These tables are the native counterpart of Metadata.Objects, Platforms,
// Doors, and JumpPads.  Entity payloads carry numeric IDs; keeping the
// cartridge names and the animation/recolor choices here gives every native
// frontend the same model-resource contract.
struct ObjectInfo {
    std::uint16_t id = 0;
    std::string_view name;
    std::array<std::int8_t, 4> animation_ids{{-1, -1, -1, -1}};
    std::uint8_t recolor_id = 0;
    bool has_animation = false;
    bool lighting = false;
    bool ignore_animation = false;
};

constexpr std::size_t ObjectCount = 54;

[[nodiscard]] const std::array<ObjectInfo, ObjectCount>& objects() noexcept;
[[nodiscard]] const ObjectInfo* object_info(std::uint32_t id) noexcept;

struct PlatformInfo {
    std::uint16_t id = 0;
    std::string_view name;
    std::array<std::int8_t, 4> animation_ids{{-1, -1, -1, -1}};
    bool has_animation = false;
    bool lighting = false;
};

constexpr std::size_t PlatformCount = 45;

// C# GetPlatformById aliases ID 1 to ID 0 and returns null for ID 2.  The
// native API exposes both the raw table and the canonical model lookup so a
// caller cannot accidentally render the duplicate or the invisible platform.
[[nodiscard]] const std::array<PlatformInfo, PlatformCount>&
platforms() noexcept;
[[nodiscard]] const PlatformInfo* platform_info(std::uint32_t id) noexcept;
[[nodiscard]] const PlatformInfo* platform_model_info(
    std::uint32_t id) noexcept;

struct DoorInfo {
    std::uint8_t id = 0;
    std::string_view name;
    std::string_view lock_name;
    float lock_offset = 0.0F;
    float radius = 0.0F;
};

constexpr std::size_t DoorCount = 4;

[[nodiscard]] const std::array<DoorInfo, DoorCount>& doors() noexcept;
[[nodiscard]] const DoorInfo* door_info(std::uint32_t id) noexcept;

constexpr std::size_t FhDoorCount = 3;

[[nodiscard]] const std::array<std::string_view, FhDoorCount>&
fh_doors() noexcept;

constexpr std::array<std::uint8_t, 10> DoorPalettes{{
    0, 1, 2, 7, 6, 3, 4, 5, 0, 0
}};

constexpr std::size_t JumpPadCount = 6;

[[nodiscard]] const std::array<std::string_view, JumpPadCount>&
jump_pads() noexcept;
[[nodiscard]] std::string_view jump_pad_name(std::uint32_t id) noexcept;

} // namespace fruityprime::metadata
