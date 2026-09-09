#pragma once

#include "Entities/gameplay.hpp"
#include "Metadata/metadata.hpp"
#include "Entities/room_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <string_view>

// Shared implementation details for the split gameplay translation units.
// Keeping these small, pure helpers here lets the public Session declaration
// stay focused on the managed boundary while avoiding another monolithic
// gameplay.cpp.
namespace fruityprime::gameplay::detail {

[[nodiscard]] inline bool has_button(net::IntentButtons buttons,
                                     net::IntentButtons button) noexcept {
    return (static_cast<std::uint32_t>(buttons)
            & static_cast<std::uint32_t>(button)) != 0;
}

[[nodiscard]] inline net::Vec3 add(net::Vec3 a, net::Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] inline net::Vec3 subtract(net::Vec3 a, net::Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] inline net::Vec3 multiply(net::Vec3 value,
                                        float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] inline net::Vec3 cross(net::Vec3 a, net::Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

[[nodiscard]] inline float dot(net::Vec3 a, net::Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] inline float length_squared(net::Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] inline net::Vec3 normalized_or(net::Vec3 value,
                                             net::Vec3 fallback) noexcept {
    const float squared = length_squared(value);
    if (squared <= std::numeric_limits<float>::epsilon()) {
        return fallback;
    }
    return multiply(value, 1.0F / std::sqrt(squared));
}

[[nodiscard]] inline collision::Vec3 to_collision(net::Vec3 value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] inline float distance_squared(net::Vec3 a,
                                            net::Vec3 b) noexcept {
    return length_squared(subtract(a, b));
}

[[nodiscard]] inline char lower_ascii(char value) noexcept {
    return static_cast<char>(std::tolower(
        static_cast<unsigned char>(value)));
}

[[nodiscard]] inline bool ascii_prefix_equal(std::string_view left,
                                             std::string_view right) noexcept {
    if (left.empty() || right.empty()) {
        return false;
    }
    const std::size_t length = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < length; ++index) {
        if (lower_ascii(left[index]) != lower_ascii(right[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline std::string_view path_filename(
    std::string_view path) noexcept {
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string_view::npos
        ? path : path.substr(separator + 1);
}

[[nodiscard]] inline std::string room_name_for_entity_filename(
    std::string_view entity_filename) {
    if (entity_filename.empty()) {
        return {};
    }
    const auto matches = [entity_filename](
                             const std::vector<scene::RoomCatalogEntry>& rooms)
        -> std::string {
        for (const auto& entry : rooms) {
            const std::string_view catalog_filename = path_filename(
                entry.definition.entity_path);
            if (ascii_prefix_equal(entity_filename, catalog_filename)) {
                return entry.name;
            }
        }
        return {};
    };
    if (std::string room = matches(scene::story_rooms()); !room.empty()) {
        return room;
    }
    return matches(scene::multiplayer_rooms());
}

[[nodiscard]] inline net::Vec3 to_float(formats::Vector3Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] inline const metadata::WeaponInfo& weapon_profile(
    std::uint8_t weapon) noexcept {
    return metadata::weapon_info(weapon);
}

[[nodiscard]] inline net::Vec3 color_from_rgb555(
    std::uint16_t value) noexcept {
    return {
        static_cast<float>((value >> 0) & 0x1f) / 31.0F,
        static_cast<float>((value >> 5) & 0x1f) / 31.0F,
        static_cast<float>((value >> 10) & 0x1f) / 31.0F
    };
}

inline void increment_saturating(std::uint16_t& value) noexcept {
    if (value != std::numeric_limits<std::uint16_t>::max()) {
        ++value;
    }
}

inline void increment_points(std::int16_t& value) noexcept {
    if (value != std::numeric_limits<std::int16_t>::max()) {
        ++value;
    }
}

[[nodiscard]] inline bool objective_player(
    const net::PlayerState& player) noexcept {
    return player.health > 0
        && (player.flags & net::PlayerState::FlagActive) != 0
        && (player.flags & net::PlayerState::FlagSpectating) == 0;
}

[[nodiscard]] inline scene::VolumePoint to_volume_point(
    net::Vec3 value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] inline scene::VolumePoint to_volume_point(
    formats::Vector3Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] inline net::Vec3 to_net(scene::VolumePoint value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] inline ItemType item_type_from_record(
    std::int32_t value) noexcept {
    if (value < static_cast<std::int32_t>(ItemType::None)
        || value > static_cast<std::int32_t>(ItemType::PickWpnMissile)) {
        return ItemType::None;
    }
    return static_cast<ItemType>(value);
}

[[nodiscard]] inline bool is_health_item(ItemType type) noexcept {
    return type == ItemType::HealthMedium || type == ItemType::HealthSmall
        || type == ItemType::HealthBig;
}

[[nodiscard]] inline bool is_ammo_item(ItemType type) noexcept {
    return type == ItemType::UASmall || type == ItemType::UABig
        || type == ItemType::MissileSmall || type == ItemType::MissileBig;
}

[[nodiscard]] inline bool is_weapon_item(ItemType type) noexcept {
    return type == ItemType::VoltDriver || type == ItemType::Battlehammer
        || type == ItemType::Imperialist || type == ItemType::Judicator
        || type == ItemType::Magmaul || type == ItemType::ShockCoil
        || type == ItemType::OmegaCannon
        || type == ItemType::AffinityWeapon
        || type == ItemType::PickWpnMissile;
}

[[nodiscard]] inline std::uint8_t weapon_for_item(
    ItemType type, std::uint8_t hunter) noexcept {
    if (type == ItemType::AffinityWeapon) {
        static constexpr std::array<std::uint8_t, 8> affinity{
            1, 2, 4, 7, 5, 6, 3, 0
        };
        return hunter < affinity.size() ? affinity[hunter] : 0xff;
    }
    switch (type) {
    case ItemType::VoltDriver: return 2;
    case ItemType::Battlehammer: return 3;
    case ItemType::Imperialist: return 4;
    case ItemType::Judicator: return 5;
    case ItemType::Magmaul: return 6;
    case ItemType::ShockCoil: return 7;
    case ItemType::OmegaCannon: return 8;
    default: return 0xff;
    }
}

[[nodiscard]] inline std::uint32_t read_enemy_field_u32(
    const std::array<std::uint8_t, 400>& fields,
    std::size_t offset) noexcept {
    if (offset + 4 > fields.size()) {
        return 0;
    }
    return static_cast<std::uint32_t>(fields[offset])
        | (static_cast<std::uint32_t>(fields[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(fields[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(fields[offset + 3]) << 24);
}

inline void add_capped(std::uint16_t& value, std::uint32_t amount,
                       std::uint16_t maximum) noexcept {
    value = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        static_cast<std::uint32_t>(value) + amount, maximum));
}

// Values are the managed Formats.Message and TriggerFlags enum values.
inline constexpr std::uint32_t MessageSetActive = 5;
inline constexpr std::uint32_t MessageDamage = 7;
inline constexpr std::uint32_t MessageTrigger = 9;
inline constexpr std::uint32_t MessageGravity = 15;
inline constexpr std::uint32_t MessageUnlock = 16;
inline constexpr std::uint32_t MessageLock = 17;
inline constexpr std::uint32_t MessageActivate = 18;
inline constexpr std::uint32_t MessageDeath = 21;
inline constexpr std::uint32_t MessagePreventFormSwitch = 35;
inline constexpr std::uint32_t MessageDripMoatPlatform = 46;
inline constexpr std::uint32_t TriggerPlayerBiped = 1u << 9;
inline constexpr std::uint32_t TriggerPlayerAlt = 1u << 10;

inline constexpr std::uint32_t TriggerTypeVolume = 0;
inline constexpr std::uint32_t TriggerTypeThreshold = 1;
inline constexpr std::uint32_t TriggerTypeRelay = 2;
inline constexpr std::uint32_t TriggerTypeAutomatic = 3;
inline constexpr std::uint32_t TriggerTypeStateBits = 4;

} // namespace fruityprime::gameplay::detail
