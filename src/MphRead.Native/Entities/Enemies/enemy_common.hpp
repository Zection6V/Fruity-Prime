#pragma once

#include "Entities/gameplay.hpp"
#include "Metadata/metadata.hpp"
#include "Entities/room_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::gameplay {
namespace {

[[nodiscard]] bool has_button(net::IntentButtons buttons,
                               net::IntentButtons button) {
    return (static_cast<std::uint32_t>(buttons)
            & static_cast<std::uint32_t>(button)) != 0;
}

[[nodiscard]] net::Vec3 add(net::Vec3 a, net::Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] net::Vec3 subtract(net::Vec3 a, net::Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] net::Vec3 multiply(net::Vec3 value, float factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] net::Vec3 cross(net::Vec3 a, net::Vec3 b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

[[nodiscard]] float dot(net::Vec3 a, net::Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] float length_squared(net::Vec3 value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] net::Vec3 normalized_or(net::Vec3 value, net::Vec3 fallback) {
    const float squared = length_squared(value);
    if (squared <= std::numeric_limits<float>::epsilon()) {
        return fallback;
    }
    const float inverse = 1.0F / std::sqrt(squared);
    return multiply(value, inverse);
}

[[nodiscard]] net::Vec3 rotate_about_axis(net::Vec3 value, net::Vec3 axis,
                                           float degrees) {
    constexpr float Pi = 3.14159265358979323846F;
    const net::Vec3 unit_axis = normalized_or(axis, {0.0F, 1.0F, 0.0F});
    const float radians = degrees * Pi / 180.0F;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    return add(add(multiply(value, cosine),
                   multiply(cross(unit_axis, value), sine)),
               multiply(unit_axis,
                        dot(unit_axis, value) * (1.0F - cosine)));
}

[[nodiscard]] collision::Vec3 to_collision(net::Vec3 value) {
    return {value.x, value.y, value.z};
}

[[nodiscard]] float distance_squared(net::Vec3 a, net::Vec3 b) {
    return length_squared(subtract(a, b));
}

[[nodiscard]] char lower_ascii(char value) {
    return static_cast<char>(std::tolower(
        static_cast<unsigned char>(value)));
}

[[nodiscard]] bool ascii_prefix_equal(std::string_view left,
                                       std::string_view right) {
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

[[nodiscard]] std::string_view path_filename(std::string_view path) {
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string_view::npos
        ? path : path.substr(separator + 1);
}

[[nodiscard]] std::string room_name_for_entity_filename(
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

[[nodiscard]] net::Vec3 to_float(formats::Vector3Fx value) {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] const metadata::WeaponInfo& weapon_profile(
    std::uint8_t weapon) noexcept {
    return metadata::weapon_info(weapon);
}

[[nodiscard]] net::Vec3 color_from_rgb555(std::uint16_t value) noexcept {
    return {
        static_cast<float>((value >> 0) & 0x1f) / 31.0F,
        static_cast<float>((value >> 5) & 0x1f) / 31.0F,
        static_cast<float>((value >> 10) & 0x1f) / 31.0F
    };
}

void increment_saturating(std::uint16_t& value) noexcept {
    if (value != std::numeric_limits<std::uint16_t>::max()) {
        ++value;
    }
}

void increment_points(std::int16_t& value) noexcept {
    if (value != std::numeric_limits<std::int16_t>::max()) {
        ++value;
    }
}

[[nodiscard]] bool objective_player(const net::PlayerState& player) noexcept {
    return player.health > 0
        && (player.flags & net::PlayerState::FlagActive) != 0
        && (player.flags & net::PlayerState::FlagSpectating) == 0;
}

[[nodiscard]] scene::VolumePoint to_volume_point(net::Vec3 value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] scene::VolumePoint to_volume_point(
    formats::Vector3Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] net::Vec3 to_net(scene::VolumePoint value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] ItemType item_type_from_record(std::int32_t value) noexcept {
    if (value < static_cast<std::int32_t>(ItemType::None)
        || value > static_cast<std::int32_t>(ItemType::PickWpnMissile)) {
        return ItemType::None;
    }
    return static_cast<ItemType>(value);
}

[[nodiscard]] bool is_health_item(ItemType type) noexcept {
    return type == ItemType::HealthMedium || type == ItemType::HealthSmall
        || type == ItemType::HealthBig;
}

[[nodiscard]] bool is_ammo_item(ItemType type) noexcept {
    return type == ItemType::UASmall || type == ItemType::UABig
        || type == ItemType::MissileSmall || type == ItemType::MissileBig;
}

[[nodiscard]] bool is_weapon_item(ItemType type) noexcept {
    return type == ItemType::VoltDriver || type == ItemType::Battlehammer
        || type == ItemType::Imperialist || type == ItemType::Judicator
        || type == ItemType::Magmaul || type == ItemType::ShockCoil
        || type == ItemType::OmegaCannon
        || type == ItemType::AffinityWeapon
        || type == ItemType::PickWpnMissile;
}

[[nodiscard]] std::uint8_t weapon_for_item(ItemType type,
                                            std::uint8_t hunter) noexcept {
    if (type == ItemType::AffinityWeapon) {
        // Formats.Weapons.AffinityWeapons, in the same hunter order used by
        // the managed PlayerValues table.
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

[[nodiscard]] std::uint32_t read_enemy_field_u32(
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

[[nodiscard]] float read_enemy_field_fixed(
    const std::array<std::uint8_t, 400>& fields,
    std::size_t offset) noexcept {
    const auto raw = static_cast<std::int32_t>(
        read_enemy_field_u32(fields, offset));
    return static_cast<float>(raw) / 4096.0F;
}

[[nodiscard]] net::Vec3 read_enemy_field_vec3(
    const std::array<std::uint8_t, 400>& fields,
    std::size_t offset) noexcept {
    return {
        read_enemy_field_fixed(fields, offset),
        read_enemy_field_fixed(fields, offset + 4),
        read_enemy_field_fixed(fields, offset + 8)
    };
}

[[nodiscard]] bool update_authored_motion(EnemyState& agent,
                                          float seconds) noexcept {
    const auto& motion = agent.authored_motion;
    if (!motion.supported) {
        return false;
    }
    if (!std::isfinite(seconds) || seconds <= 0.0F) {
        return true;
    }

    // The managed engine advances these enemy state machines at 60 calls per
    // second, while their authored step values are expressed as the half-step
    // used by the original fixed-point routine. Keep the conversion here so
    // callers with a different fixed timestep still get the same path speed.
    const float frames = seconds * 60.0F;
    const net::Vec3 old_position = agent.position;
    if (motion.movement_type == 0) {
        agent.circle_angle += 0.75F * frames;
        while (agent.circle_angle >= 360.0F) {
            agent.circle_angle -= 360.0F;
        }
        while (agent.circle_angle < 0.0F) {
            agent.circle_angle += 360.0F;
        }
        constexpr float Pi = 3.14159265358979323846F;
        const float angle = agent.circle_angle * Pi / 180.0F;
        agent.position = {
            motion.initial_position.x + std::sin(angle) * motion.circle_radius,
            motion.initial_position.y,
            motion.initial_position.z + std::cos(angle) * motion.circle_radius};
    } else if (motion.path_count > 0) {
        const float step = std::max(0.0F,
                                    motion.step_distance * 0.5F * frames);
        float remaining = step;
        const std::size_t path_count = motion.path_count;
        std::size_t transitions = 0;
        if (agent.motion_index >= path_count) {
            agent.motion_index = 0;
        }
        while (remaining > 0.0F && transitions <= path_count) {
            const net::Vec3 target = motion.path_positions[agent.motion_index];
            const net::Vec3 delta = subtract(target, agent.position);
            const float distance = std::sqrt(std::max(0.0F,
                                                       length_squared(delta)));
            if (distance <= 0.0001F) {
                agent.position = target;
                if (static_cast<std::size_t>(agent.motion_index) + 1
                    >= path_count) {
                    agent.motion_index = 0;
                    // WarWasp movement type 3 is a finite patrol. The C#
                    // state machine marks the instance dead when it wraps.
                    if (motion.movement_type == 3) {
                        agent.health = 0;
                        break;
                    }
                } else {
                    ++agent.motion_index;
                }
                ++transitions;
                continue;
            }
            const float amount = std::min(distance, remaining);
            agent.position = add(agent.position,
                                 multiply(delta, amount / distance));
            remaining -= amount;
            if (amount < distance) {
                break;
            }
        }
    }

    const net::Vec3 displacement = subtract(agent.position, old_position);
    if (length_squared(displacement) > 0.000001F) {
        agent.velocity = multiply(displacement, 1.0F / seconds);
        agent.facing = normalized_or(displacement,
                                     {0.0F, 0.0F, 1.0F});
    } else {
        agent.velocity = {};
    }
    return true;
}

void add_capped(std::uint16_t& value, std::uint32_t amount,
                std::uint16_t maximum) noexcept {
    value = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        static_cast<std::uint32_t>(value) + amount, maximum));
}

// Values are the managed Formats.Message and TriggerFlags enum values. Keep
// them explicit here until the native scene/message catalogue is complete.
constexpr std::uint32_t MessageSetActive = 5;
constexpr std::uint32_t MessageDamage = 7;
constexpr std::uint32_t MessageTrigger = 9;
constexpr std::uint32_t MessageGravity = 15;
constexpr std::uint32_t MessageUnlock = 16;
constexpr std::uint32_t MessageLock = 17;
constexpr std::uint32_t MessageActivate = 18;
constexpr std::uint32_t MessageDeath = 21;
constexpr std::uint32_t MessagePreventFormSwitch = 35;
constexpr std::uint32_t MessageDripMoatPlatform = 46;
constexpr std::uint32_t TriggerPlayerBiped = 1u << 9;
constexpr std::uint32_t TriggerPlayerAlt = 1u << 10;

constexpr std::uint32_t TriggerTypeVolume = 0;
constexpr std::uint32_t TriggerTypeThreshold = 1;
constexpr std::uint32_t TriggerTypeRelay = 2;
constexpr std::uint32_t TriggerTypeAutomatic = 3;
constexpr std::uint32_t TriggerTypeStateBits = 4;
} // namespace
} // namespace fruityprime::gameplay
