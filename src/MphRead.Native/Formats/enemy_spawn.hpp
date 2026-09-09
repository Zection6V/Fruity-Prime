#pragma once

#include "Formats/raw_formats.hpp"
#include "Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace fruityprime::enemy_spawn {

// The managed editor groups the MPH enemy-spawn union into thirteen layouts.
// Keep the grouping explicit in native code so callers can inspect the same
// fields without depending on compiler-specific union layout.
enum class SpawnerType : std::uint8_t {
    Common = 0,
    WarWasp = 1,
    Shriekbat = 2,
    Temroid = 3,
    Petrasyl = 4,
    Cretaphid = 5,
    Turret = 6,
    CarnivorousPlant = 7,
    BarbedWarWasp = 8,
    Hunter = 9,
    SlenchTurret = 10,
    Gorea1A = 11,
    Gorea2 = 12,
    Unknown = 0xff
};

[[nodiscard]] SpawnerType spawner_type(formats::EnemyType type) noexcept;

struct MessageLink {
    std::int16_t target_id = -1;
    formats::Message message = formats::Message::None;
};

struct WarWaspFields {
    std::array<raw::RawCollisionVolume, 3> volumes{};
    std::array<formats::Vector3Fx, 16> movement_vectors{};
    std::uint8_t position_count = 0;
    std::uint8_t padding_1a9 = 0;
    std::uint16_t padding_1aa = 0;
    std::uint32_t movement_type = 0;
};

struct Fields {
    SpawnerType spawner_type = SpawnerType::Unknown;
    std::uint32_t enemy_subtype = 0;
    std::uint32_t enemy_version = 0;
    std::array<raw::RawCollisionVolume, 4> volumes{};

    formats::Vector3Fx path_vector{};
    formats::Vector3Fx enemy_facing{};
    formats::Vector3Fx enemy_position{};
    formats::Vector3Fx idle_range{};
    std::array<std::uint32_t, 7> unused{};
    std::int32_t weave_offset = 0;
    std::int32_t field_88 = 0;

    std::uint16_t enemy_health = 0;
    std::uint16_t enemy_damage = 0;
    WarWaspFields war_wasp;

    std::uint32_t hunter_id = 0;
    std::uint32_t encounter_type = 0;
    std::uint32_t hunter_weapon = 0;
    std::uint16_t hunter_health = 0;
    std::uint16_t hunter_health_max = 0;
    std::uint16_t hunter_health_threshold = 0;
    std::uint8_t hunter_color = 0;
    std::uint8_t hunter_chance = 0;

    std::int32_t index = 0;
    formats::Vector3Fx sphere1_position{};
    formats::Fixed sphere1_radius{};
    formats::Vector3Fx sphere2_position{};
    formats::Fixed sphere2_radius{};
    formats::Vector3Fx field_28{};
    formats::Fixed field_34{};
    formats::Fixed field_38{};
};

struct Data {
    static constexpr std::size_t Size = 512;

    formats::EnemyType enemy_type = formats::EnemyType::WarWasp;
    Fields fields;
    std::int16_t linked_entity_id = -1;
    std::uint8_t spawn_total = 0;
    std::uint8_t spawn_limit = 0;
    std::uint8_t spawn_count = 0;
    bool active = false;
    bool always_active = false;
    std::uint8_t item_chance = 0;
    std::uint16_t spawner_health = 0;
    std::uint16_t cooldown_time = 0;
    std::uint16_t initial_cooldown = 0;
    formats::Fixed active_distance{};
    formats::Fixed enemy_active_distance{};
    std::string node_name;
    std::array<MessageLink, 3> messages{};
    formats::ItemType item_type = formats::ItemType::None;
};

struct FirstHuntData {
    static constexpr std::size_t Size = 268;

    raw::FhRawCollisionVolume box{};
    raw::FhRawCollisionVolume cylinder{};
    raw::FhRawCollisionVolume sphere{};
    formats::FhEnemyType enemy_type = formats::FhEnemyType::WarWasp;
    std::uint8_t spawn_total = 0;
    std::uint8_t spawn_limit = 0;
    std::uint8_t spawn_count = 0;
    std::uint8_t padding_eb = 0;
    std::uint16_t cooldown = 0;
    std::uint16_t start_frame = 0;
    std::string node_name;
    std::int16_t parent_id = -1;
    std::uint16_t padding_102 = 0;
    formats::FhMessage empty_message = formats::FhMessage::None;
};

// Decode the complete fixed payload, including the 40-byte EntityDataHeader
// at the beginning. The returned values use the same fixed-point/raw-volume
// representations as Formats/EntityEnemy.cs.
[[nodiscard]] Data decode(std::span<const std::uint8_t> payload);
[[nodiscard]] FirstHuntData decode_first_hunt(
    std::span<const std::uint8_t> payload);

} // namespace fruityprime::enemy_spawn
