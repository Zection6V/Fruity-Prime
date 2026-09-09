#include "Formats/enemy_spawn_editors.hpp"
#include "Formats/enemy_spawn_layouts.hpp"
#include "Formats/enemy_spawn.hpp"

#include <stdexcept>

namespace fruityprime::enemy_spawn {
namespace {

constexpr std::size_t kFieldsOffset = 44;
constexpr std::size_t kVolumeSize = 64;

void require_bytes(std::span<const std::uint8_t> bytes, std::size_t offset,
                   std::size_t size, const char* what) {
    if (offset > bytes.size() || size > bytes.size() - offset) {
        throw std::runtime_error(std::string("enemy spawn ") + what
                                 + " is outside the payload");
    }
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
                       std::size_t offset) {
    require_bytes(bytes, offset, 2, "u16");
    return static_cast<std::uint16_t>(bytes[offset])
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::int16_t read_i16(std::span<const std::uint8_t> bytes,
                      std::size_t offset) {
    return static_cast<std::int16_t>(read_u16(bytes, offset));
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
                       std::size_t offset) {
    require_bytes(bytes, offset, 4, "u32");
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::int32_t read_i32(std::span<const std::uint8_t> bytes,
                      std::size_t offset) {
    return static_cast<std::int32_t>(read_u32(bytes, offset));
}

formats::Fixed read_fixed(std::span<const std::uint8_t> bytes,
                          std::size_t offset) {
    return formats::Fixed{read_i32(bytes, offset)};
}

formats::Vector3Fx read_vector3(std::span<const std::uint8_t> bytes,
                                std::size_t offset) {
    return formats::Vector3Fx{read_i32(bytes, offset),
                              read_i32(bytes, offset + 4),
                              read_i32(bytes, offset + 8)};
}

std::string read_string(std::span<const std::uint8_t> bytes,
                        std::size_t offset, std::size_t width) {
    require_bytes(bytes, offset, width, "fixed string");
    std::size_t length = 0;
    while (length < width && bytes[offset + length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset),
                       length);
}

raw::RawCollisionVolume read_volume(std::span<const std::uint8_t> bytes,
                                    std::size_t offset) {
    require_bytes(bytes, offset, kVolumeSize, "MPH collision volume");
    raw::RawCollisionVolume volume{};
    const auto type = read_u32(bytes, offset);
    if (type > static_cast<std::uint32_t>(formats::VolumeType::Sphere)) {
        throw std::runtime_error("enemy spawn collision volume has an invalid type");
    }
    volume.type = static_cast<formats::VolumeType>(type);
    switch (volume.type) {
    case formats::VolumeType::Box:
        volume.data.box.vector1 = read_vector3(bytes, offset + 4);
        volume.data.box.vector2 = read_vector3(bytes, offset + 16);
        volume.data.box.vector3 = read_vector3(bytes, offset + 28);
        volume.data.box.position = read_vector3(bytes, offset + 40);
        volume.data.box.dot1 = read_fixed(bytes, offset + 52);
        volume.data.box.dot2 = read_fixed(bytes, offset + 56);
        volume.data.box.dot3 = read_fixed(bytes, offset + 60);
        break;
    case formats::VolumeType::Cylinder:
        volume.data.cylinder.vector = read_vector3(bytes, offset + 4);
        volume.data.cylinder.position = read_vector3(bytes, offset + 16);
        volume.data.cylinder.radius = read_fixed(bytes, offset + 28);
        volume.data.cylinder.dot = read_fixed(bytes, offset + 32);
        break;
    case formats::VolumeType::Sphere:
        volume.data.sphere.position = read_vector3(bytes, offset + 4);
        volume.data.sphere.radius = read_fixed(bytes, offset + 16);
        break;
    }
    return volume;
}

raw::FhRawCollisionVolume read_first_hunt_volume(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    require_bytes(bytes, offset, kVolumeSize, "First Hunt collision volume");
    raw::FhRawCollisionVolume volume{};
    const auto type = read_u32(bytes, offset);
    if (type > static_cast<std::uint32_t>(formats::FhVolumeType::Cylinder)) {
        throw std::runtime_error("enemy spawn First Hunt volume has an invalid type");
    }
    volume.type = static_cast<formats::FhVolumeType>(type);
    switch (volume.type) {
    case formats::FhVolumeType::Sphere:
        volume.data.sphere.position = read_vector3(bytes, offset + 4);
        volume.data.sphere.radius = read_fixed(bytes, offset + 16);
        break;
    case formats::FhVolumeType::Box:
        volume.data.box.position = read_vector3(bytes, offset + 4);
        volume.data.box.vector1 = read_vector3(bytes, offset + 16);
        volume.data.box.vector2 = read_vector3(bytes, offset + 28);
        volume.data.box.vector3 = read_vector3(bytes, offset + 40);
        volume.data.box.dot1 = read_fixed(bytes, offset + 52);
        volume.data.box.dot2 = read_fixed(bytes, offset + 56);
        volume.data.box.dot3 = read_fixed(bytes, offset + 60);
        break;
    case formats::FhVolumeType::Cylinder:
        volume.data.cylinder.position = read_vector3(bytes, offset + 4);
        volume.data.cylinder.vector = read_vector3(bytes, offset + 16);
        volume.data.cylinder.dot = read_fixed(bytes, offset + 28);
        volume.data.cylinder.radius = read_fixed(bytes, offset + 32);
        break;
    }
    return volume;
}

void read_war_wasp(std::span<const std::uint8_t> bytes, std::size_t offset,
                   WarWaspFields& fields) {
    for (std::size_t i = 0; i < fields.volumes.size(); ++i) {
        fields.volumes[i] = read_volume(bytes, offset + i * kVolumeSize);
    }
    constexpr std::size_t movement_offset = 3 * kVolumeSize;
    for (std::size_t i = 0; i < fields.movement_vectors.size(); ++i) {
        fields.movement_vectors[i] = read_vector3(
            bytes, offset + movement_offset + i * sizeof(formats::Vector3Fx));
    }
    fields.position_count = bytes[offset + 384];
    fields.padding_1a9 = bytes[offset + 385];
    fields.padding_1aa = read_u16(bytes, offset + 386);
    fields.movement_type = read_u32(bytes, offset + 388);
}

} // namespace

SpawnerType spawner_type(formats::EnemyType type) noexcept {
    switch (type) {
    case formats::EnemyType::Zoomer:
    case formats::EnemyType::Geemer:
    case formats::EnemyType::Blastcap:
    case formats::EnemyType::Voldrum2:
    case formats::EnemyType::Quadtroid:
    case formats::EnemyType::CrashPillar:
    case formats::EnemyType::Slench:
    case formats::EnemyType::LesserIthrak:
    case formats::EnemyType::Trocra:
        return SpawnerType::Common;
    case formats::EnemyType::WarWasp:
        return SpawnerType::WarWasp;
    case formats::EnemyType::Shriekbat:
        return SpawnerType::Shriekbat;
    case formats::EnemyType::Temroid:
    case formats::EnemyType::Petrasyl1:
        return SpawnerType::Temroid;
    case formats::EnemyType::Petrasyl2:
    case formats::EnemyType::Petrasyl3:
    case formats::EnemyType::Petrasyl4:
        return SpawnerType::Petrasyl;
    case formats::EnemyType::Cretaphid:
    case formats::EnemyType::GreaterIthrak:
        return SpawnerType::Cretaphid;
    case formats::EnemyType::AlimbicTurret:
    case formats::EnemyType::PsychoBit1:
    case formats::EnemyType::Voldrum1:
    case formats::EnemyType::FireSpawn:
        return SpawnerType::Turret;
    case formats::EnemyType::CarnivorousPlant:
        return SpawnerType::CarnivorousPlant;
    case formats::EnemyType::BarbedWarWasp:
        return SpawnerType::BarbedWarWasp;
    case formats::EnemyType::Hunter:
        return SpawnerType::Hunter;
    case formats::EnemyType::SlenchTurret:
        return SpawnerType::SlenchTurret;
    case formats::EnemyType::Gorea1A:
        return SpawnerType::Gorea1A;
    case formats::EnemyType::Gorea2:
        return SpawnerType::Gorea2;
    default:
        return SpawnerType::Unknown;
    }
}

Data decode(std::span<const std::uint8_t> payload) {
    require_bytes(payload, 0, Data::Size, "MPH data");
    Data data;
    data.enemy_type = static_cast<formats::EnemyType>(payload[40]);
    data.fields.spawner_type = spawner_type(data.enemy_type);
    const auto fields = payload.subspan(kFieldsOffset, 400);

    switch (data.fields.spawner_type) {
    case SpawnerType::Common:
        for (std::size_t i = 0; i < data.fields.volumes.size(); ++i) {
            data.fields.volumes[i] = read_volume(fields, i * kVolumeSize);
        }
        break;
    case SpawnerType::WarWasp:
        read_war_wasp(fields, 0, data.fields.war_wasp);
        break;
    case SpawnerType::Shriekbat:
        data.fields.volumes[0] = read_volume(fields, 0);
        data.fields.path_vector = read_vector3(fields, 64);
        data.fields.volumes[1] = read_volume(fields, 76);
        data.fields.volumes[2] = read_volume(fields, 140);
        break;
    case SpawnerType::Temroid:
        data.fields.volumes[0] = read_volume(fields, 0);
        for (std::size_t i = 0; i < data.fields.unused.size(); ++i) {
            data.fields.unused[i] = read_u32(fields, 64 + i * 4);
        }
        data.fields.enemy_facing = read_vector3(fields, 92);
        data.fields.enemy_position = read_vector3(fields, 104);
        data.fields.idle_range = read_vector3(fields, 116);
        break;
    case SpawnerType::Petrasyl:
        data.fields.volumes[0] = read_volume(fields, 0);
        for (std::size_t i = 0; i < 4; ++i) {
            data.fields.unused[i] = read_u32(fields, 64 + i * 4);
        }
        data.fields.enemy_position = read_vector3(fields, 80);
        data.fields.weave_offset = read_i32(fields, 92);
        data.fields.field_88 = read_i32(fields, 96);
        break;
    case SpawnerType::Cretaphid:
        data.fields.enemy_subtype = read_u32(fields, 0);
        for (std::size_t i = 0; i < data.fields.volumes.size(); ++i) {
            data.fields.volumes[i] = read_volume(fields, 4 + i * kVolumeSize);
        }
        break;
    case SpawnerType::Turret:
        data.fields.enemy_subtype = read_u32(fields, 0);
        data.fields.enemy_version = read_u32(fields, 4);
        for (std::size_t i = 0; i < data.fields.volumes.size(); ++i) {
            data.fields.volumes[i] = read_volume(fields, 8 + i * kVolumeSize);
        }
        break;
    case SpawnerType::CarnivorousPlant:
        data.fields.enemy_health = read_u16(fields, 0);
        data.fields.enemy_damage = read_u16(fields, 2);
        data.fields.enemy_subtype = read_u32(fields, 4);
        data.fields.volumes[0] = read_volume(fields, 8);
        break;
    case SpawnerType::BarbedWarWasp:
        data.fields.enemy_subtype = read_u32(fields, 0);
        data.fields.enemy_version = read_u32(fields, 4);
        read_war_wasp(fields, 8, data.fields.war_wasp);
        break;
    case SpawnerType::Hunter:
        data.fields.hunter_id = read_u32(fields, 0);
        data.fields.encounter_type = read_u32(fields, 4);
        data.fields.hunter_weapon = read_u32(fields, 8);
        data.fields.hunter_health = read_u16(fields, 12);
        data.fields.hunter_health_max = read_u16(fields, 14);
        data.fields.hunter_health_threshold = read_u16(fields, 16);
        data.fields.hunter_color = fields[18];
        data.fields.hunter_chance = fields[19];
        break;
    case SpawnerType::SlenchTurret:
        data.fields.enemy_subtype = read_u32(fields, 0);
        data.fields.enemy_version = read_u32(fields, 4);
        data.fields.volumes[0] = read_volume(fields, 8);
        data.fields.volumes[1] = read_volume(fields, 72);
        data.fields.index = read_i32(fields, 136);
        break;
    case SpawnerType::Gorea1A:
        data.fields.sphere1_position = read_vector3(fields, 0);
        data.fields.sphere1_radius = read_fixed(fields, 12);
        data.fields.sphere2_position = read_vector3(fields, 16);
        data.fields.sphere2_radius = read_fixed(fields, 28);
        break;
    case SpawnerType::Gorea2:
        data.fields.field_28 = read_vector3(fields, 0);
        data.fields.field_34 = read_fixed(fields, 12);
        data.fields.field_38 = read_fixed(fields, 16);
        break;
    case SpawnerType::Unknown:
        break;
    }

    data.linked_entity_id = read_i16(payload, 444);
    data.spawn_total = payload[446];
    data.spawn_limit = payload[447];
    data.spawn_count = payload[448];
    data.active = payload[449] != 0;
    data.always_active = payload[450] != 0;
    data.item_chance = payload[451];
    data.spawner_health = read_u16(payload, 452);
    data.cooldown_time = read_u16(payload, 454);
    data.initial_cooldown = read_u16(payload, 456);
    data.active_distance = read_fixed(payload, 460);
    data.enemy_active_distance = read_fixed(payload, 464);
    data.node_name = read_string(payload, 468, 16);
    for (std::size_t i = 0; i < data.messages.size(); ++i) {
        const auto offset = 484 + i * 8;
        data.messages[i].target_id = read_i16(payload, offset);
        data.messages[i].message = static_cast<formats::Message>(
            read_u32(payload, offset + 4));
    }
    data.item_type = static_cast<formats::ItemType>(read_i32(payload, 508));
    return data;
}

FirstHuntData decode_first_hunt(std::span<const std::uint8_t> payload) {
    require_bytes(payload, 0, FirstHuntData::Size, "First Hunt data");
    FirstHuntData data;
    data.box = read_first_hunt_volume(payload, 40);
    data.cylinder = read_first_hunt_volume(payload, 104);
    data.sphere = read_first_hunt_volume(payload, 168);
    data.enemy_type = static_cast<formats::FhEnemyType>(read_u32(payload, 232));
    data.spawn_total = payload[236];
    data.spawn_limit = payload[237];
    data.spawn_count = payload[238];
    data.padding_eb = payload[239];
    data.cooldown = read_u16(payload, 240);
    data.start_frame = read_u16(payload, 242);
    data.node_name = read_string(payload, 244, 16);
    data.parent_id = read_i16(payload, 260);
    data.padding_102 = read_u16(payload, 262);
    data.empty_message = static_cast<formats::FhMessage>(read_u32(payload, 264));
    return data;
}

} // namespace fruityprime::enemy_spawn
