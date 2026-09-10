#include "Entities/scene.hpp"

#include <cmath>
#include <span>
#include <stdexcept>
#include <utility>

namespace fruityprime::scene {
namespace {

[[nodiscard]] std::vector<std::uint8_t> archive_entry(
    const assets::Store& assets, std::string_view archive_path,
    std::string_view entry_name) {
    const auto archive = assets.archive(archive_path);
    for (std::size_t i = 0; i < archive.entries().size(); ++i) {
        if (archive.entries()[i].filename == entry_name) {
            return archive.file(i);
        }
    }
    throw std::out_of_range("room archive entry was not found: "
                            + std::string(entry_name));
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes,
                       std::size_t offset) {
    if (offset > bytes.size() || 2 > bytes.size() - offset) {
        throw std::runtime_error("typed entity payload is truncated");
    }
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
                       std::size_t offset) {
    if (offset > bytes.size() || 4 > bytes.size() - offset) {
        throw std::runtime_error("typed entity payload is truncated");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::int16_t read_i16(std::span<const std::uint8_t> bytes,
                      std::size_t offset) {
    return static_cast<std::int16_t>(read_u16(bytes, offset));
}

std::int32_t read_i32(std::span<const std::uint8_t> bytes,
                      std::size_t offset) {
    return static_cast<std::int32_t>(read_u32(bytes, offset));
}

float read_fixed(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<float>(read_i32(bytes, offset)) / 4096.0F;
}

std::string read_fixed_string(std::span<const std::uint8_t> bytes,
                              std::size_t offset, std::size_t length) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw std::runtime_error("typed entity payload is truncated");
    }
    std::size_t end = 0;
    while (end < length && bytes[offset + end] != 0) {
        ++end;
    }
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset),
                       end);
}

VolumePoint read_volume_point(std::span<const std::uint8_t> bytes,
                              std::size_t offset) {
    return {read_fixed(bytes, offset), read_fixed(bytes, offset + 4),
            read_fixed(bytes, offset + 8)};
}

formats::Vector3Fx read_vector3_fx(std::span<const std::uint8_t> bytes,
                                   std::size_t offset) {
    return {read_i32(bytes, offset), read_i32(bytes, offset + 4),
            read_i32(bytes, offset + 8)};
}

RotationPoint read_rotation_point(std::span<const std::uint8_t> bytes,
                                  std::size_t offset) {
    return {read_fixed(bytes, offset), read_fixed(bytes, offset + 4),
            read_fixed(bytes, offset + 8), read_fixed(bytes, offset + 12)};
}

MessageLinkData read_message_link32(std::span<const std::uint8_t> bytes,
                                    std::size_t offset) {
    return {read_i32(bytes, offset), read_u32(bytes, offset + 4),
            read_i32(bytes, offset + 8), read_i32(bytes, offset + 12)};
}

MessageLinkData read_message_link16(std::span<const std::uint8_t> bytes,
                                    std::size_t offset) {
    return {read_i16(bytes, offset), read_u32(bytes, offset + 4),
            read_i32(bytes, offset + 8), read_i32(bytes, offset + 12)};
}

void require_payload(std::span<const std::uint8_t> payload, std::size_t size,
                     std::string_view name) {
    if (payload.size() < size) {
        throw std::runtime_error(std::string(name)
                                 + " entity payload is truncated");
    }
}

EntityVolume read_volume(std::span<const std::uint8_t> bytes,
                         std::size_t offset, VolumePoint entity_position) {
    constexpr std::size_t size = 64;
    if (offset > bytes.size() || size > bytes.size() - offset) {
        throw std::runtime_error("entity collision volume is truncated");
    }

    EntityVolume volume;
    switch (read_u32(bytes, offset)) {
    case 0: // VolumeType.Box
        volume.kind = VolumeKind::Box;
        volume.box_vector1 = read_volume_point(bytes, offset + 4);
        volume.box_vector2 = read_volume_point(bytes, offset + 16);
        volume.box_vector3 = read_volume_point(bytes, offset + 28);
        volume.box_position = read_volume_point(bytes, offset + 40);
        volume.box_dot1 = read_fixed(bytes, offset + 52);
        volume.box_dot2 = read_fixed(bytes, offset + 56);
        volume.box_dot3 = read_fixed(bytes, offset + 60);
        volume.box_position.x += entity_position.x;
        volume.box_position.y += entity_position.y;
        volume.box_position.z += entity_position.z;
        break;
    case 1: // VolumeType.Cylinder
        volume.kind = VolumeKind::Cylinder;
        volume.cylinder_vector = read_volume_point(bytes, offset + 4);
        volume.cylinder_position = read_volume_point(bytes, offset + 16);
        volume.cylinder_radius = read_fixed(bytes, offset + 28);
        volume.cylinder_dot = read_fixed(bytes, offset + 32);
        volume.cylinder_position.x += entity_position.x;
        volume.cylinder_position.y += entity_position.y;
        volume.cylinder_position.z += entity_position.z;
        break;
    case 2: // VolumeType.Sphere
        volume.kind = VolumeKind::Sphere;
        volume.sphere_position = read_volume_point(bytes, offset + 4);
        volume.sphere_radius = read_fixed(bytes, offset + 16);
        volume.sphere_position.x += entity_position.x;
        volume.sphere_position.y += entity_position.y;
        volume.sphere_position.z += entity_position.z;
        break;
    default:
        throw std::runtime_error("entity collision volume has an invalid type");
    }
    return volume;
}

raw::FhRawCollisionVolume read_first_hunt_volume(
    std::span<const std::uint8_t> bytes, std::size_t offset) {
    constexpr std::size_t size = 64;
    if (offset > bytes.size() || size > bytes.size() - offset) {
        throw std::runtime_error("First Hunt collision volume is truncated");
    }

    raw::FhRawCollisionVolume volume{};
    const auto type = read_u32(bytes, offset);
    if (type > static_cast<std::uint32_t>(formats::FhVolumeType::Cylinder)) {
        throw std::runtime_error(
            "First Hunt collision volume has an invalid type");
    }
    volume.type = static_cast<formats::FhVolumeType>(type);
    switch (volume.type) {
    case formats::FhVolumeType::Sphere:
        volume.data.sphere.position = read_vector3_fx(bytes, offset + 4);
        volume.data.sphere.radius = formats::Fixed{
            read_i32(bytes, offset + 16)};
        break;
    case formats::FhVolumeType::Box:
        volume.data.box.position = read_vector3_fx(bytes, offset + 4);
        volume.data.box.vector1 = read_vector3_fx(bytes, offset + 16);
        volume.data.box.vector2 = read_vector3_fx(bytes, offset + 28);
        volume.data.box.vector3 = read_vector3_fx(bytes, offset + 40);
        volume.data.box.dot1 = formats::Fixed{
            read_i32(bytes, offset + 52)};
        volume.data.box.dot2 = formats::Fixed{
            read_i32(bytes, offset + 56)};
        volume.data.box.dot3 = formats::Fixed{
            read_i32(bytes, offset + 60)};
        break;
    case formats::FhVolumeType::Cylinder:
        volume.data.cylinder.position = read_vector3_fx(bytes, offset + 4);
        volume.data.cylinder.vector = read_vector3_fx(bytes, offset + 16);
        volume.data.cylinder.dot = formats::Fixed{
            read_i32(bytes, offset + 28)};
        volume.data.cylinder.radius = formats::Fixed{
            read_i32(bytes, offset + 32)};
        break;
    }
    return volume;
}

TypedEntityData decode_typed_entity_data(
    std::span<const std::uint8_t> payload, EntityKind kind,
    VolumePoint entity_position) {
    switch (kind) {
    case EntityKind::Platform: {
        require_payload(payload, 588, "platform");
        PlatformData data;
        data.no_port = read_u32(payload, 40);
        data.model_id = read_u32(payload, 44);
        data.parent_id = read_i16(payload, 48);
        data.active = payload[50] != 0;
        data.delay = payload[51];
        data.scan_data1 = read_u16(payload, 52);
        data.scan_message_target = read_i16(payload, 54);
        data.scan_message = read_u32(payload, 56);
        data.scan_data2 = read_u16(payload, 60);
        data.position_count = read_u16(payload, 62);
        for (std::size_t i = 0; i < data.positions.size(); ++i) {
            data.positions[i] = read_volume_point(payload, 64 + i * 12);
        }
        for (std::size_t i = 0; i < data.rotations.size(); ++i) {
            data.rotations[i] = read_rotation_point(payload, 184 + i * 16);
        }
        data.position_offset = read_volume_point(payload, 344);
        data.forward_speed = read_fixed(payload, 356);
        data.backward_speed = read_fixed(payload, 360);
        data.portal_name = read_fixed_string(payload, 364, 16);
        data.movement_type = read_u32(payload, 380);
        data.for_cutscene = read_u32(payload, 384);
        data.reverse_type = read_u32(payload, 388);
        data.flags = read_u32(payload, 392);
        data.contact_damage = read_u32(payload, 396);
        data.beam_spawn_direction = read_volume_point(payload, 400);
        data.beam_spawn_position = read_volume_point(payload, 412);
        data.beam_id = read_i32(payload, 424);
        data.beam_interval = read_u32(payload, 428);
        data.beam_on_intervals = read_u32(payload, 432);
        data.unused_1d0 = read_u32(payload, 468);
        data.unused_1d4 = read_u32(payload, 472);
        data.resist_effect_id = read_i32(payload, 440);
        data.health = read_u32(payload, 444);
        data.effectiveness = read_u32(payload, 448);
        data.damage_effect_id = read_i32(payload, 452);
        data.dead_effect_id = read_i32(payload, 456);
        data.item_chance = payload[460];
        data.item_type = read_i32(payload, 464);
        data.beam_hit_message = read_message_link32(payload, 476);
        data.player_collision_message = read_message_link32(payload, 492);
        data.dead_message = read_message_link32(payload, 508);
        for (std::size_t i = 0; i < data.lifetime_messages.size(); ++i) {
            const std::size_t offset = 524 + i * 16;
            data.lifetime_message_indices[i] = read_u16(payload, offset);
            data.lifetime_messages[i] = {
                read_i16(payload, offset + 2), read_u32(payload, offset + 4),
                read_i32(payload, offset + 8), read_i32(payload, offset + 12)};
        }
        return data;
    }
    case EntityKind::Object: {
        require_payload(payload, 152, "object");
        ObjectData data;
        data.flags = payload[40];
        data.effect_flags = read_u32(payload, 44);
        data.model_id = read_i32(payload, 48);
        data.linked_entity = read_i16(payload, 52);
        data.scan_id = read_u16(payload, 54);
        data.scan_message_target = read_i16(payload, 56);
        data.scan_message = read_u32(payload, 60);
        data.effect_id = read_i32(payload, 64);
        data.effect_interval = read_u32(payload, 68);
        data.effect_on_intervals = read_u32(payload, 72);
        data.effect_position_offset = read_volume_point(payload, 76);
        data.volume = read_volume(payload, 88, entity_position);
        return data;
    }
    case EntityKind::EnemySpawn: {
        require_payload(payload, 512, "enemy spawn");
        EnemySpawnData data;
        data.enemy_type = payload[40];
        for (std::size_t i = 0; i < data.fields.size(); ++i) {
            data.fields[i] = payload[44 + i];
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
        data.node_name = read_fixed_string(payload, 468, 16);
        data.message1 = {read_i16(payload, 484), read_u32(payload, 488), 0, 0};
        data.message2 = {read_i16(payload, 492), read_u32(payload, 496), 0, 0};
        data.message3 = {read_i16(payload, 500), read_u32(payload, 504), 0, 0};
        data.item_type = read_i32(payload, 508);
        return data;
    }
    case EntityKind::PlayerSpawn:
        if (payload.size() < 43) {
            throw std::runtime_error("player spawn entity payload is truncated");
        }
        return PlayerSpawnData{
            payload[40], payload[41] != 0,
            static_cast<std::int8_t>(payload[42])};
    case EntityKind::Door: {
        require_payload(payload, 104, "door");
        return DoorData{
            read_fixed_string(payload, 40, 16), read_u32(payload, 56),
            read_u32(payload, 60),
            read_u32(payload, 64), payload[68], payload[69] != 0, payload[70],
            payload[71], read_fixed_string(payload, 72, 16),
            read_fixed_string(payload, 88, 16)};
    }
    case EntityKind::ItemSpawn:
        if (payload.size() < 72) {
            throw std::runtime_error("item spawn entity payload is truncated");
        }
        return ItemSpawnData{
            read_i32(payload, 40), read_i32(payload, 44),
            payload[48] != 0, payload[49] != 0, payload[50] != 0,
            read_u16(payload, 52), read_u16(payload, 54),
            read_u16(payload, 56), read_i16(payload, 58),
            read_u32(payload, 60), read_i32(payload, 64),
            read_i32(payload, 68)};
    case EntityKind::TriggerVolume: {
        require_payload(payload, 160, "trigger volume");
        TriggerVolumeData data;
        data.subtype = read_u32(payload, 40);
        data.volume = read_volume(payload, 44, entity_position);
        data.active = payload[110] != 0;
        data.always_active = payload[111] != 0;
        data.deactivate_after_use = payload[112] != 0;
        data.repeat_delay = read_u16(payload, 114);
        data.check_delay = read_u16(payload, 116);
        data.required_state_bit = read_u16(payload, 118);
        data.trigger_flags = read_u32(payload, 120);
        data.trigger_threshold = read_u32(payload, 124);
        data.parent_id = read_i16(payload, 128);
        data.parent_message = read_message_link16(payload, 128);
        data.child_id = read_i16(payload, 144);
        data.child_message = read_message_link16(payload, 144);
        return data;
    }
    case EntityKind::AreaVolume: {
        require_payload(payload, 152, "area volume");
        AreaVolumeData data;
        data.volume = read_volume(payload, 40, entity_position);
        data.active = payload[106] != 0;
        data.always_active = payload[107] != 0;
        data.allow_multiple = payload[108] != 0;
        data.message_delay = payload[109];
        data.unused_6a = read_u16(payload, 110);
        data.inside_message = read_u32(payload, 112);
        data.inside_parameter1 = read_i32(payload, 116);
        data.inside_parameter2 = read_i32(payload, 120);
        data.parent_id = read_i16(payload, 124);
        data.exit_message = read_u32(payload, 128);
        data.exit_parameter1 = read_i32(payload, 132);
        data.exit_parameter2 = read_i32(payload, 136);
        data.child_id = read_i16(payload, 140);
        data.cooldown = read_u16(payload, 142);
        data.priority = read_u32(payload, 144);
        data.trigger_flags = read_u32(payload, 148);
        return data;
    }
    case EntityKind::JumpPad: {
        require_payload(payload, 148, "jump pad");
        return JumpPadData{
            read_i32(payload, 40), read_u32(payload, 44),
            read_volume(payload, 48, entity_position),
            read_volume_point(payload, 112), read_fixed(payload, 124),
            read_u16(payload, 128), read_u16(payload, 130),
            payload[132] != 0, read_u32(payload, 136), read_u32(payload, 140),
            read_u32(payload, 144)};
    }
    case EntityKind::PointModule:
        if (payload.size() < 45) {
            throw std::runtime_error("point module entity payload is truncated");
        }
        return PointModuleData{
            read_i16(payload, 40), read_i16(payload, 42),
            payload[44] != 0};
    case EntityKind::MorphCamera: {
        require_payload(payload, 104, "morph camera");
        return MorphCameraData{read_volume(payload, 40, entity_position)};
    }
    case EntityKind::OctolithFlag:
        if (payload.size() < 41) {
            throw std::runtime_error("octolith flag entity payload is truncated");
        }
        return OctolithFlagData{payload[40]};
    case EntityKind::FlagBase:
        if (payload.size() < 108) {
            throw std::runtime_error("flag base entity payload is truncated");
        }
        return FlagBaseData{read_u32(payload, 40),
                            read_volume(payload, 44, entity_position)};
    case EntityKind::Teleporter: {
        require_payload(payload, 92, "teleporter");
        return TeleporterData{
            payload[40], payload[41], payload[42], payload[43] != 0,
            payload[44] != 0, read_fixed_string(payload, 45, 15),
            read_volume_point(payload, 64), read_fixed_string(payload, 76, 16)};
    }
    case EntityKind::NodeDefense:
        if (payload.size() < 104) {
            throw std::runtime_error(
                "node defense entity payload is truncated");
        }
        return NodeDefenseData{read_volume(payload, 40, entity_position)};
    case EntityKind::LightSource: {
        require_payload(payload, 136, "light source");
        LightSourceData data;
        data.volume = read_volume(payload, 40, entity_position);
        data.light1_enabled = payload[104] != 0;
        data.light1_color = {payload[105], payload[106], payload[107]};
        data.light1_vector = read_volume_point(payload, 108);
        data.light2_enabled = payload[120] != 0;
        data.light2_color = {payload[121], payload[122], payload[123]};
        data.light2_vector = read_volume_point(payload, 124);
        return data;
    }
    case EntityKind::Artifact: {
        require_payload(payload, 70, "artifact");
        ArtifactData data;
        data.model_id = payload[40];
        data.artifact_id = payload[41];
        data.active = payload[42] != 0;
        data.has_base = payload[43] != 0;
        data.message1 = {read_i16(payload, 44), read_u32(payload, 48), 0, 0};
        data.message2 = {read_i16(payload, 52), read_u32(payload, 56), 0, 0};
        data.message3 = {read_i16(payload, 60), read_u32(payload, 64), 0, 0};
        data.linked_entity_id = read_i16(payload, 68);
        return data;
    }
    case EntityKind::CameraSequence: {
        require_payload(payload, 64, "camera sequence");
        return CameraSequenceData{
            payload[40], payload[41] != 0, payload[42] != 0,
            payload[43] != 0, payload[44] != 0, payload[45] != 0,
            read_u16(payload, 46), payload[48], payload[49], read_i16(payload, 50),
            read_i16(payload, 52), read_i16(payload, 54), read_u32(payload, 56),
            read_i32(payload, 60)};
    }
    case EntityKind::ForceField: {
        require_payload(payload, 53, "force field");
        return ForceFieldData{read_u32(payload, 40), read_fixed(payload, 44),
                              read_fixed(payload, 48), payload[52] != 0};
    }
    default:
        return {};
    }
}

TypedEntityData decode_first_hunt_typed_entity_data(
    std::span<const std::uint8_t> payload, EntityKind kind) {
    switch (kind) {
    case EntityKind::PlayerSpawn:
        require_payload(payload, 43, "First Hunt player spawn");
        return PlayerSpawnData{payload[40], payload[41] != 0,
                               static_cast<std::int8_t>(payload[42])};
    case EntityKind::Door: {
        require_payload(payload, 64, "First Hunt door");
        return FhDoorData{read_fixed_string(payload, 40, 16),
                          read_u32(payload, 56), read_u32(payload, 60)};
    }
    case EntityKind::ItemSpawn: {
        require_payload(payload, 50, "First Hunt item spawn");
        return FhItemSpawnData{
            static_cast<formats::FhItemType>(read_i32(payload, 40)),
            read_u16(payload, 44), read_u16(payload, 46), read_u16(payload, 48)};
    }
    case EntityKind::EnemySpawn:
        return enemy_spawn::decode_first_hunt(payload);
    case EntityKind::TriggerVolume: {
        require_payload(payload, 272, "First Hunt trigger volume");
        FhTriggerVolumeData data;
        data.subtype = static_cast<formats::FhTriggerType>(
            read_u32(payload, 40));
        data.box = read_first_hunt_volume(payload, 44);
        data.sphere = read_first_hunt_volume(payload, 108);
        data.cylinder = read_first_hunt_volume(payload, 172);
        data.one_use = read_u16(payload, 236);
        data.cooldown = read_u16(payload, 238);
        data.trigger_flags = read_u32(payload, 240);
        data.threshold = read_u32(payload, 244);
        data.parent_id = read_i16(payload, 248);
        data.parent_message = static_cast<formats::FhMessage>(
            read_u32(payload, 252));
        data.parent_parameter1 = read_i32(payload, 256);
        data.child_id = read_i16(payload, 260);
        data.child_message = static_cast<formats::FhMessage>(
            read_u32(payload, 264));
        data.child_parameter1 = read_i32(payload, 268);
        return data;
    }
    case EntityKind::AreaVolume: {
        require_payload(payload, 260, "First Hunt area volume");
        FhAreaVolumeData data;
        data.subtype = static_cast<formats::FhTriggerType>(
            read_u32(payload, 40));
        data.box = read_first_hunt_volume(payload, 44);
        data.sphere = read_first_hunt_volume(payload, 108);
        data.cylinder = read_first_hunt_volume(payload, 172);
        data.inside_message = static_cast<formats::FhMessage>(
            read_u32(payload, 236));
        data.inside_parameter1 = read_i32(payload, 240);
        data.exit_message = static_cast<formats::FhMessage>(
            read_u32(payload, 244));
        data.exit_parameter1 = read_i32(payload, 248);
        data.cooldown = read_u16(payload, 252);
        data.trigger_flags = read_u32(payload, 256);
        return data;
    }
    case EntityKind::Platform: {
        require_payload(payload, 236, "First Hunt platform");
        FhPlatformData data;
        data.no_portal = read_u32(payload, 40);
        data.group_id = read_u32(payload, 44);
        data.unused_2c = read_u32(payload, 48);
        data.delay = payload[52];
        data.position_count = payload[53];
        data.volume = read_first_hunt_volume(payload, 56);
        for (std::size_t i = 0; i < data.positions.size(); ++i) {
            data.positions[i] = read_volume_point(payload, 120 + i * 12);
        }
        data.speed = read_fixed(payload, 216);
        data.portal_name = read_fixed_string(payload, 220, 16);
        return data;
    }
    case EntityKind::JumpPad: {
        require_payload(payload, 272, "First Hunt jump pad");
        FhJumpPadData data;
        data.volume_type = static_cast<formats::FhTriggerType>(
            read_u32(payload, 40));
        data.box = read_first_hunt_volume(payload, 44);
        data.sphere = read_first_hunt_volume(payload, 108);
        data.cylinder = read_first_hunt_volume(payload, 172);
        data.cooldown_time = read_u32(payload, 236);
        data.beam_vector = read_volume_point(payload, 240);
        data.speed = read_fixed(payload, 252);
        data.control_lock_time = read_u32(payload, 256);
        data.model_id = read_u32(payload, 260);
        data.beam_type = read_u32(payload, 264);
        data.trigger_flags = read_u32(payload, 268);
        return data;
    }
    case EntityKind::PointModule:
        require_payload(payload, 45, "First Hunt point module");
        return PointModuleData{read_i16(payload, 40), read_i16(payload, 42),
                               payload[44] != 0};
    case EntityKind::MorphCamera: {
        require_payload(payload, 104, "First Hunt morph camera");
        return FhMorphCameraData{read_first_hunt_volume(payload, 40)};
    }
    default:
        return {};
    }
}

[[nodiscard]] VolumePoint add(VolumePoint left, VolumePoint right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] VolumePoint subtract(VolumePoint left,
                                   VolumePoint right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] VolumePoint multiply(VolumePoint value, float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] float dot(VolumePoint left, VolumePoint right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] VolumePoint cross(VolumePoint left,
                                VolumePoint right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

[[nodiscard]] float length_squared(VolumePoint value) noexcept {
    return dot(value, value);
}

} // namespace

bool EntityVolume::contains(VolumePoint point) const noexcept {
    constexpr float epsilon = 0.0001F;
    switch (kind) {
    case VolumeKind::Box: {
        const VolumePoint difference = subtract(point, box_position);
        const float value1 = dot(box_vector1, difference);
        const float value2 = dot(box_vector2, difference);
        const float value3 = dot(box_vector3, difference);
        return value1 >= -epsilon && value1 <= box_dot1 + epsilon
            && value2 >= -epsilon && value2 <= box_dot2 + epsilon
            && value3 >= -epsilon && value3 <= box_dot3 + epsilon;
    }
    case VolumeKind::Cylinder: {
        const VolumePoint axis = multiply(cylinder_vector, cylinder_dot);
        const float axis_length_squared = length_squared(axis);
        if (axis_length_squared <= epsilon) {
            return false;
        }
        const VolumePoint to_bottom = subtract(point, cylinder_position);
        const VolumePoint to_top = subtract(
            point, add(cylinder_position, axis));
        const float radial_squared = length_squared(cross(to_bottom, axis));
        return dot(to_bottom, axis) >= -epsilon
            && dot(to_top, axis) <= epsilon
            && radial_squared <= cylinder_radius * cylinder_radius
                * axis_length_squared + epsilon;
    }
    case VolumeKind::Sphere:
        return length_squared(subtract(point, sphere_position))
            <= sphere_radius * sphere_radius + epsilon;
    default:
        return false;
    }
}

EntityVolume EntityVolume::moved(
    const VolumePoint offset) const noexcept {
    EntityVolume moved = *this;
    switch (kind) {
    case VolumeKind::Box:
        moved.box_position = {box_position.x + offset.x,
                              box_position.y + offset.y,
                              box_position.z + offset.z};
        break;
    case VolumeKind::Cylinder:
        moved.cylinder_position = {cylinder_position.x + offset.x,
                                   cylinder_position.y + offset.y,
                                   cylinder_position.z + offset.z};
        break;
    case VolumeKind::Sphere:
        moved.sphere_position = {sphere_position.x + offset.x,
                                 sphere_position.y + offset.y,
                                 sphere_position.z + offset.z};
        break;
    case VolumeKind::Invalid:
        break;
    }
    return moved;
}

VolumePoint EntityVolume::center() const noexcept {
    switch (kind) {
    case VolumeKind::Box:
        return add(box_position,
                   add(multiply(box_vector1, box_dot1 * 0.5F),
                       add(multiply(box_vector2, box_dot2 * 0.5F),
                           multiply(box_vector3, box_dot3 * 0.5F))));
    case VolumeKind::Cylinder:
        return add(cylinder_position,
                   multiply(cylinder_vector, cylinder_dot * 0.5F));
    case VolumeKind::Sphere:
        return sphere_position;
    default:
        return {};
    }
}

EntityKind classify_entity_type(std::uint16_t type, bool first_hunt) noexcept {
    if (first_hunt) {
        if (type > 17) {
            return EntityKind::Unknown;
        }
        type = static_cast<std::uint16_t>(type + 100);
    } else if (type >= 100) {
        return EntityKind::Unknown;
    }
    switch (type) {
    case 0: return EntityKind::Platform;
    case 1: return EntityKind::Object;
    case 2: return EntityKind::PlayerSpawn;
    case 3: return EntityKind::Door;
    case 4: return EntityKind::ItemSpawn;
    case 5: return EntityKind::ItemInstance;
    case 6: return EntityKind::EnemySpawn;
    case 7: return EntityKind::TriggerVolume;
    case 8: return EntityKind::AreaVolume;
    case 9: return EntityKind::JumpPad;
    case 10: return EntityKind::PointModule;
    case 11: return EntityKind::MorphCamera;
    case 12: return EntityKind::OctolithFlag;
    case 13: return EntityKind::FlagBase;
    case 14: return EntityKind::Teleporter;
    case 15: return EntityKind::NodeDefense;
    case 16: return EntityKind::LightSource;
    case 17: return EntityKind::Artifact;
    case 18: return EntityKind::CameraSequence;
    case 19: return EntityKind::ForceField;
    case 21: return EntityKind::BeamEffect;
    case 22: return EntityKind::Bomb;
    case 23: return EntityKind::EnemyInstance;
    case 24: return EntityKind::Halfturret;
    case 25: return EntityKind::Player;
    case 26: return EntityKind::BeamProjectile;
    case 101: return EntityKind::PlayerSpawn;
    case 103: return EntityKind::Door;
    case 104: return EntityKind::ItemSpawn;
    case 105: return EntityKind::ItemInstance;
    case 106: return EntityKind::EnemySpawn;
    case 109: return EntityKind::TriggerVolume;
    case 110: return EntityKind::AreaVolume;
    case 111: return EntityKind::Platform;
    case 112: return EntityKind::JumpPad;
    case 113: return EntityKind::PointModule;
    case 114: return EntityKind::MorphCamera;
    case 115: return EntityKind::EnemyInstance;
    case 116: return EntityKind::Player;
    case 117: return EntityKind::BeamProjectile;
    default: return EntityKind::Unknown;
    }
}

Room Room::load(const assets::Store& assets,
                const RoomDefinition& definition) {
    if (definition.name.empty()
        || (definition.model_archive.empty() && definition.external_root.empty())
        || definition.model_entry.empty() || definition.collision_entry.empty()) {
        throw std::invalid_argument("room definition is incomplete");
    }

    Room room;
    room.definition_ = definition;
    std::optional<assets::Store> external_assets;
    if (!definition.external_root.empty()) {
        external_assets.emplace(assets::Store::from_directory(
            definition.external_root));
    }
    const auto read_direct = [&assets, &external_assets](std::string_view path) {
        return external_assets.has_value()
            ? external_assets->bytes(path) : assets.bytes(path);
    };
    auto model_bytes = external_assets.has_value()
        ? external_assets->bytes(definition.model_entry)
        : archive_entry(assets, definition.model_archive, definition.model_entry);
    auto texture_bytes = definition.texture_path.empty()
        ? std::vector<std::uint8_t>{}
        : read_direct(definition.texture_path);
    room.model_ = model::File::from_resources(
        std::move(model_bytes), std::move(texture_bytes));
    room.collision_ = collision::File::from_bytes(
        external_assets.has_value()
            ? external_assets->bytes(definition.collision_entry)
            : archive_entry(assets, definition.model_archive,
                            definition.collision_entry));

    if (!definition.animation_entry.empty()) {
        room.animation_bytes_ = external_assets.has_value()
            ? external_assets->bytes(definition.animation_entry)
            : archive_entry(assets, definition.model_archive,
                            definition.animation_entry);
        room.model_->load_animations(room.animation_bytes_,
                                     definition.model_entry);
    }

    if (!definition.node_path.empty()) {
        room.node_data_ = node::NodeData::from_bytes(read_direct(
            definition.node_path));
    }

    if (!definition.entity_path.empty()) {
        room.entities_file_ = entity::File::from_bytes(read_direct(
            definition.entity_path));
        if (room.entities_file_->is_first_hunt()) {
            room.entities_.reserve(room.entities_file_->first_hunt_records().size());
            for (const auto& record : room.entities_file_->first_hunt_records()) {
                const auto kind = classify_entity_type(record.header.type, true);
                TypedEntityData typed_data =
                    decode_first_hunt_typed_entity_data(record.payload, kind);
                room.entities_.push_back(EntityInstance{
                    record.header.type,
                    kind,
                    record.header.entity_id,
                    0,
                    record.entry.node_name,
                    record.header.position,
                    record.header.up_vector,
                    record.header.facing_vector,
                    std::move(typed_data),
                    record.payload
                });
            }
        } else {
            room.entities_.reserve(room.entities_file_->records().size());
            for (const auto& record : room.entities_file_->records()) {
                room.entities_.push_back(EntityInstance{
                    record.header.type,
                    classify_entity_type(record.header.type),
                    record.header.entity_id,
                    record.entry.layer_mask,
                    record.entry.node_name,
                    record.header.position,
                    record.header.up_vector,
                    record.header.facing_vector,
                    decode_typed_entity_data(
                        record.payload,
                        classify_entity_type(record.header.type),
                        {record.header.position.x.to_float(),
                         record.header.position.y.to_float(),
                         record.header.position.z.to_float()}),
                    record.payload
                });
            }
        }
    }
    return room;
}

const model::File& Room::model() const {
    if (!model_.has_value()) {
        throw std::logic_error("room model was not loaded");
    }
    return *model_;
}

model::File& Room::mutable_model() {
    if (!model_.has_value()) {
        throw std::logic_error("room model was not loaded");
    }
    return *model_;
}

const collision::File& Room::collision() const {
    if (!collision_.has_value()) {
        throw std::logic_error("room collision was not loaded");
    }
    return *collision_;
}

} // namespace fruityprime::scene
