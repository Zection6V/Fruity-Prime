/*
 * Native counterpart of MphRead/Formats/EntityClass.cs.
 *
 * The managed file is an editor projection over the decoded entity records:
 * its constructors copy fields, convert fixed-point vectors and expose a
 * field-by-field CompareTo operation.  The native scene decoder already owns
 * those decoded records, so this translation unit keeps the same projection
 * without introducing a second parser or a generated god object.
 */
#include "Formats/entity_editors.hpp"
#include "Formats/editor_entity.hpp"

#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <type_traits>
#include <utility>

namespace fruityprime::editor {
namespace {

using FieldList = std::vector<FieldValue>;

template <typename T>
[[nodiscard]] std::string scalar(T value) {
    std::ostringstream output;
    if constexpr (std::is_same_v<T, bool>) {
        output << (value ? "true" : "false");
    } else if constexpr (std::is_enum_v<T>) {
        using Underlying = std::underlying_type_t<T>;
        output << static_cast<std::conditional_t<std::is_signed_v<Underlying>,
                                                  std::int64_t,
                                                  std::uint64_t>>(
            static_cast<Underlying>(value));
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            output << static_cast<std::int64_t>(value);
        } else {
            output << static_cast<std::uint64_t>(value);
        }
    } else {
        output << std::setprecision(9) << value;
    }
    return output.str();
}

void add(FieldList& fields, std::string_view name, std::string value) {
    fields.push_back({std::string(name), std::move(value)});
}

[[maybe_unused]] void add(FieldList& fields, std::string_view name,
                          std::string_view value) {
    fields.push_back({std::string(name), std::string(value)});
}

template <typename T>
void add(FieldList& fields, std::string_view name, T value) {
    fields.push_back({std::string(name), scalar(value)});
}

[[nodiscard]] formats::Vector3 to_float(formats::Vector3Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] formats::Vector3 to_float(scene::VolumePoint value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] formats::Vector4 to_float(scene::RotationPoint value) noexcept {
    return {value.x, value.y, value.z, value.w};
}

[[nodiscard]] std::string vector_value(formats::Vector3 value) {
    return scalar(value.x) + "," + scalar(value.y) + "," + scalar(value.z);
}

[[nodiscard]] std::string vector_value(formats::Vector4 value) {
    return scalar(value.x) + "," + scalar(value.y) + "," + scalar(value.z)
        + "," + scalar(value.w);
}

[[nodiscard]] std::string vector_value(formats::Vector3Fx value) {
    return vector_value(to_float(value));
}

[[nodiscard]] std::string vector_value(scene::VolumePoint value) {
    return vector_value(to_float(value));
}

template <typename T, std::size_t N, typename Formatter>
[[nodiscard]] std::string array_value(const std::array<T, N>& values,
                                      Formatter formatter) {
    std::ostringstream output;
    output << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            output << ';';
        }
        output << formatter(values[i]);
    }
    output << ']';
    return output.str();
}

[[nodiscard]] std::string hex_value(const std::array<std::uint8_t, 400>& bytes) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint8_t byte : bytes) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

[[nodiscard]] std::string volume_value(const scene::EntityVolume& volume) {
    std::ostringstream output;
    output << scalar(volume.kind) << ':';
    switch (volume.kind) {
    case scene::VolumeKind::Box:
        output << vector_value(volume.box_vector1) << '|'
               << vector_value(volume.box_vector2) << '|'
               << vector_value(volume.box_vector3) << '|'
               << vector_value(volume.box_position) << '|'
               << scalar(volume.box_dot1) << '|'
               << scalar(volume.box_dot2) << '|'
               << scalar(volume.box_dot3);
        break;
    case scene::VolumeKind::Cylinder:
        output << vector_value(volume.cylinder_vector) << '|'
               << vector_value(volume.cylinder_position) << '|'
               << scalar(volume.cylinder_radius) << '|'
               << scalar(volume.cylinder_dot);
        break;
    case scene::VolumeKind::Sphere:
        output << vector_value(volume.sphere_position) << '|'
               << scalar(volume.sphere_radius);
        break;
    default:
        break;
    }
    return output.str();
}

[[nodiscard]] std::string volume_value(const raw::FhRawCollisionVolume& volume) {
    std::ostringstream output;
    output << scalar(volume.type) << ':';
    switch (volume.type) {
    case formats::FhVolumeType::Sphere:
        output << vector_value(volume.data.sphere.position) << '|'
               << scalar(volume.data.sphere.radius.to_float());
        break;
    case formats::FhVolumeType::Box:
        output << vector_value(volume.data.box.position) << '|'
               << vector_value(volume.data.box.vector1) << '|'
               << vector_value(volume.data.box.vector2) << '|'
               << vector_value(volume.data.box.vector3) << '|'
               << scalar(volume.data.box.dot1.to_float()) << '|'
               << scalar(volume.data.box.dot2.to_float()) << '|'
               << scalar(volume.data.box.dot3.to_float());
        break;
    case formats::FhVolumeType::Cylinder:
        output << vector_value(volume.data.cylinder.position) << '|'
               << vector_value(volume.data.cylinder.vector) << '|'
               << scalar(volume.data.cylinder.dot.to_float()) << '|'
               << scalar(volume.data.cylinder.radius.to_float());
        break;
    }
    return output.str();
}

[[maybe_unused]] [[nodiscard]] std::string volume_value(
    const raw::RawCollisionVolume& volume) {
    std::ostringstream output;
    output << scalar(volume.type) << ':';
    switch (volume.type) {
    case formats::VolumeType::Box:
        output << vector_value(volume.data.box.position) << '|'
               << vector_value(volume.data.box.vector1) << '|'
               << vector_value(volume.data.box.vector2) << '|'
               << vector_value(volume.data.box.vector3) << '|'
               << scalar(volume.data.box.dot1.to_float()) << '|'
               << scalar(volume.data.box.dot2.to_float()) << '|'
               << scalar(volume.data.box.dot3.to_float());
        break;
    case formats::VolumeType::Cylinder:
        output << vector_value(volume.data.cylinder.position) << '|'
               << vector_value(volume.data.cylinder.vector) << '|'
               << scalar(volume.data.cylinder.radius.to_float()) << '|'
               << scalar(volume.data.cylinder.dot.to_float());
        break;
    case formats::VolumeType::Sphere:
        output << vector_value(volume.data.sphere.position) << '|'
               << scalar(volume.data.sphere.radius.to_float());
        break;
    }
    return output.str();
}

void add_link(FieldList& fields, std::string_view target_name,
              std::string_view message_name, std::string_view parameter1_name,
              std::string_view parameter2_name,
              const scene::MessageLinkData& link) {
    add(fields, target_name, link.target);
    add(fields, message_name, link.message);
    add(fields, parameter1_name, link.parameter1);
    add(fields, parameter2_name, link.parameter2);
}

void add_link_without_parameter2(FieldList& fields, std::string_view target_name,
                                 std::string_view message_name,
                                 std::string_view parameter1_name,
                                 const scene::MessageLinkData& link) {
    add(fields, target_name, link.target);
    add(fields, message_name, link.message);
    add(fields, parameter1_name, link.parameter1);
}

void add_platform_fields(FieldList& fields, const scene::PlatformData& data) {
    add(fields, "NoPort", data.no_port);
    add(fields, "ModelId", data.model_id);
    add(fields, "ParentId", data.parent_id);
    add(fields, "Active", data.active);
    add(fields, "Delay", data.delay);
    add(fields, "ScanData1", data.scan_data1);
    add(fields, "ScanMsgTarget", data.scan_message_target);
    add(fields, "ScanMessage", data.scan_message);
    add(fields, "ScanData2", data.scan_data2);
    add(fields, "PositionCount", data.position_count);
    add(fields, "Positions", array_value(data.positions, [](scene::VolumePoint value) {
        return vector_value(value);
    }));
    add(fields, "Rotations", array_value(data.rotations, [](scene::RotationPoint value) {
        return vector_value(to_float(value));
    }));
    add(fields, "PositionOffset", vector_value(data.position_offset));
    add(fields, "ForwardSpeed", data.forward_speed);
    add(fields, "BackwardSpeed", data.backward_speed);
    add(fields, "PortalName", data.portal_name);
    add(fields, "MovementType", data.movement_type);
    add(fields, "ForCutscene", data.for_cutscene != 0);
    add(fields, "ReverseType", data.reverse_type);
    add(fields, "Flags", data.flags);
    add(fields, "ContactDamage", data.contact_damage);
    add(fields, "BeamSpawnDir", vector_value(data.beam_spawn_direction));
    add(fields, "BeamSpawnPos", vector_value(data.beam_spawn_position));
    add(fields, "BeamId", data.beam_id);
    add(fields, "BeamInterval", data.beam_interval);
    add(fields, "BeamOnIntervals", data.beam_on_intervals);
    add(fields, "ResistEffectId", data.resist_effect_id);
    add(fields, "Health", data.health);
    add(fields, "Effectiveness", data.effectiveness);
    add(fields, "DamageEffectId", data.damage_effect_id);
    add(fields, "DeadEffectId", data.dead_effect_id);
    add(fields, "ItemChance", data.item_chance);
    add(fields, "ItemType", data.item_type);
    add(fields, "Unused1D0", data.unused_1d0);
    add(fields, "Unused1D4", data.unused_1d4);
    add_link(fields, "BeamHitMsgTarget", "BeamHitMessage", "BeamHitMsgParam1",
             "BeamHitMsgParam2", data.beam_hit_message);
    add_link(fields, "PlayerColMsgTarget", "PlayerColMessage",
             "PlayerColMsgParam1", "PlayerColMsgParam2",
             data.player_collision_message);
    add_link(fields, "DeadMsgTarget", "DeadMessage", "DeadMsgParam1",
             "DeadMsgParam2", data.dead_message);
    for (std::size_t i = 0; i < data.lifetime_messages.size(); ++i) {
        const std::string prefix = "LifetimeMsg" + std::to_string(i + 1);
        const std::string index_name = prefix + "Index";
        const std::string target_name = prefix + "Target";
        const std::string message_name = "LifetimeMessage" + std::to_string(i + 1);
        const std::string parameter1_name = prefix + "Param1";
        const std::string parameter2_name = prefix + "Param2";
        add(fields, index_name, data.lifetime_message_indices[i]);
        add_link(fields, target_name, message_name, parameter1_name,
                 parameter2_name, data.lifetime_messages[i]);
    }
}

void add_fields(FieldList& fields, const std::monostate&) {
    add(fields, "Data", "none");
}

void add_fields(FieldList& fields, const scene::PlatformData& data) {
    add_platform_fields(fields, data);
}

void add_fields(FieldList& fields, const scene::ObjectData& data) {
    add(fields, "Flags", data.flags);
    add(fields, "EffectFlags", data.effect_flags);
    add(fields, "ModelId", data.model_id);
    add(fields, "LinkedEntity", data.linked_entity);
    add(fields, "ScanId", data.scan_id);
    add(fields, "ScanMsgTarget", data.scan_message_target);
    add(fields, "ScanMessage", data.scan_message);
    add(fields, "EffectId", data.effect_id);
    add(fields, "EffectInterval", data.effect_interval);
    add(fields, "EffectOnIntervals", data.effect_on_intervals);
    add(fields, "EffectPositionOffset", vector_value(data.effect_position_offset));
    add(fields, "Volume", volume_value(data.volume));
}

void add_fields(FieldList& fields, const scene::EnemySpawnData& data) {
    add(fields, "EnemyType", data.enemy_type);
    add(fields, "Fields", hex_value(data.fields));
    add(fields, "LinkedEntityId", data.linked_entity_id);
    add(fields, "SpawnTotal", data.spawn_total);
    add(fields, "SpawnLimit", data.spawn_limit);
    add(fields, "SpawnCount", data.spawn_count);
    add(fields, "Active", data.active);
    add(fields, "AlwaysActive", data.always_active);
    add(fields, "ItemChance", data.item_chance);
    add(fields, "SpawnerHealth", data.spawner_health);
    add(fields, "CooldownTime", data.cooldown_time);
    add(fields, "InitialCooldown", data.initial_cooldown);
    add(fields, "ActiveDistance", data.active_distance);
    add(fields, "EnemyActiveDistance", data.enemy_active_distance);
    add(fields, "NodeName", data.node_name);
    add_link_without_parameter2(fields, "Message1Target", "Message1", "Message1Param1",
                                data.message1);
    add_link_without_parameter2(fields, "Message2Target", "Message2", "Message2Param1",
                                data.message2);
    add_link_without_parameter2(fields, "Message3Target", "Message3", "Message3Param1",
                                data.message3);
    add(fields, "ItemType", data.item_type);
}

void add_fields(FieldList& fields, const scene::PlayerSpawnData& data) {
    add(fields, "Availability", data.availability);
    add(fields, "Active", data.active);
    add(fields, "TeamIndex", data.team_index);
}

void add_fields(FieldList& fields, const scene::DoorData& data) {
    add(fields, "DoorNodeName", data.node_name);
    add(fields, "PaletteId", data.palette_id);
    add(fields, "DoorType", data.door_type);
    add(fields, "ConnectorId", data.connector_id);
    add(fields, "TargetLayerId", data.target_layer_id);
    add(fields, "Locked", data.locked);
    add(fields, "Field42", data.out_connector_id);
    add(fields, "Field43", data.out_loader_id);
    add(fields, "EntityFilename", data.entity_filename);
    add(fields, "RoomName", data.room_name);
}

void add_fields(FieldList& fields, const scene::ItemSpawnData& data) {
    add(fields, "ParentId", data.parent_id);
    add(fields, "ItemType", data.item_type);
    add(fields, "Enabled", data.enabled);
    add(fields, "HasBase", data.has_base);
    add(fields, "AlwaysActive", data.always_active);
    add(fields, "MaxSpawnCount", data.max_spawn_count);
    add(fields, "SpawnInterval", data.spawn_interval);
    add(fields, "SpawnDelay", data.spawn_delay);
    add(fields, "NotifyEntityId", data.notify_entity_id);
    add(fields, "CollectedMessage", data.collected_message);
    add(fields, "CollectedMsgParam1", data.collected_parameter1);
    add(fields, "CollectedMsgParam2", data.collected_parameter2);
}

void add_fields(FieldList& fields, const scene::TriggerVolumeData& data) {
    add(fields, "Subtype", data.subtype);
    add(fields, "Volume", volume_value(data.volume));
    add(fields, "Active", data.active);
    add(fields, "AlwaysActive", data.always_active);
    add(fields, "DeactivateAfterUse", data.deactivate_after_use);
    add(fields, "RepeatDelay", data.repeat_delay);
    add(fields, "CheckDelay", data.check_delay);
    add(fields, "RequiredStateBit", data.required_state_bit);
    add(fields, "TriggerFlags", data.trigger_flags);
    add(fields, "TriggerThreshold", data.trigger_threshold);
    add_link(fields, "ParentId", "ParentMessage", "ParentMsgParam1",
             "ParentMsgParam2", data.parent_message);
    add_link(fields, "ChildId", "ChildMessage", "ChildMsgParam1",
             "ChildMsgParam2", data.child_message);
}

void add_fields(FieldList& fields, const scene::AreaVolumeData& data) {
    add(fields, "Volume", volume_value(data.volume));
    add(fields, "Active", data.active);
    add(fields, "AlwaysActive", data.always_active);
    add(fields, "AllowMultiple", data.allow_multiple);
    add(fields, "MessageDelay", data.message_delay);
    add(fields, "Unused6A", data.unused_6a);
    add(fields, "InsideMessage", data.inside_message);
    add(fields, "InsideMsgParam1", data.inside_parameter1);
    add(fields, "InsideMsgParam2", data.inside_parameter2);
    add(fields, "ParentId", data.parent_id);
    add(fields, "ExitMessage", data.exit_message);
    add(fields, "ExitMsgParam1", data.exit_parameter1);
    add(fields, "ExitMsgParam2", data.exit_parameter2);
    add(fields, "ChildId", data.child_id);
    add(fields, "Cooldown", data.cooldown);
    add(fields, "Priority", data.priority);
    add(fields, "TriggerFlags", data.trigger_flags);
}

void add_fields(FieldList& fields, const scene::JumpPadData& data) {
    add(fields, "ParentId", data.parent_id);
    add(fields, "Unused28", data.unused_28);
    add(fields, "Volume", volume_value(data.volume));
    add(fields, "BeamVector", vector_value(data.beam_vector));
    add(fields, "Speed", data.speed);
    add(fields, "ControlLockTime", data.control_lock_time);
    add(fields, "CooldownTime", data.cooldown_time);
    add(fields, "Active", data.active);
    add(fields, "ModelId", data.model_id);
    add(fields, "BeamType", data.beam_type);
    add(fields, "TriggerFlags", data.trigger_flags);
}

void add_fields(FieldList& fields, const scene::PointModuleData& data) {
    add(fields, "NextId", data.next_id);
    add(fields, "PrevId", data.previous_id);
    add(fields, "Active", data.active);
}

void add_fields(FieldList& fields, const scene::MorphCameraData& data) {
    add(fields, "Volume", volume_value(data.volume));
}

void add_fields(FieldList& fields, const scene::OctolithFlagData& data) {
    add(fields, "TeamId", data.team_id);
}

void add_fields(FieldList& fields, const scene::FlagBaseData& data) {
    add(fields, "TeamId", data.team_id);
    add(fields, "Volume", volume_value(data.volume));
}

void add_fields(FieldList& fields, const scene::TeleporterData& data) {
    add(fields, "LoadIndex", data.load_index);
    add(fields, "TargetIndex", data.target_index);
    add(fields, "ArtifactId", data.artifact_id);
    add(fields, "Active", data.active);
    add(fields, "Invisible", data.invisible);
    add(fields, "TargetRoom", data.entity_filename);
    add(fields, "TargetPosition", vector_value(data.target_position));
    add(fields, "TeleporterNodeName", data.node_name);
}

void add_fields(FieldList& fields, const scene::NodeDefenseData& data) {
    add(fields, "Volume", volume_value(data.volume));
}

void add_fields(FieldList& fields, const scene::LightSourceData& data) {
    add(fields, "Volume", volume_value(data.volume));
    add(fields, "Light1Enabled", data.light1_enabled);
    add(fields, "Light1Color", array_value(data.light1_color, [](std::uint8_t value) {
        return scalar(value);
    }));
    add(fields, "Light1Vector", vector_value(data.light1_vector));
    add(fields, "Light2Enabled", data.light2_enabled);
    add(fields, "Light2Color", array_value(data.light2_color, [](std::uint8_t value) {
        return scalar(value);
    }));
    add(fields, "Light2Vector", vector_value(data.light2_vector));
}

void add_fields(FieldList& fields, const scene::ArtifactData& data) {
    add(fields, "ModelId", data.model_id);
    add(fields, "ArtifactId", data.artifact_id);
    add(fields, "Active", data.active);
    add(fields, "HasBase", data.has_base);
    add_link_without_parameter2(fields, "Message1Target", "Message1", "Message1Param1",
                                data.message1);
    add_link_without_parameter2(fields, "Message2Target", "Message2", "Message2Param1",
                                data.message2);
    add_link_without_parameter2(fields, "Message3Target", "Message3", "Message3Param1",
                                data.message3);
    add(fields, "LinkedEntityId", data.linked_entity_id);
}

void add_fields(FieldList& fields, const scene::CameraSequenceData& data) {
    add(fields, "SequenceId", data.sequence_id);
    add(fields, "Handoff", data.handoff);
    add(fields, "Loop", data.loop);
    add(fields, "BlockInput", data.block_input);
    add(fields, "ForceAltForm", data.force_alt_form);
    add(fields, "ForceBipedForm", data.force_biped_form);
    add(fields, "DelayFrames", data.delay_frames);
    add(fields, "PlayerId1", data.player_id1);
    add(fields, "PlayerId2", data.player_id2);
    add(fields, "Entity1", data.entity1);
    add(fields, "Entity2", data.entity2);
    add(fields, "EndMessageTargetId", data.end_message_target_id);
    add(fields, "EndMessage", data.end_message);
    add(fields, "EndMessageParam", data.end_message_parameter);
}

void add_fields(FieldList& fields, const scene::ForceFieldData& data) {
    add(fields, "ForceFieldType", data.type);
    add(fields, "Width", data.width);
    add(fields, "Height", data.height);
    add(fields, "Active", data.active);
}

void add_fields(FieldList& fields, const enemy_spawn::FirstHuntData& data) {
    add(fields, "Box", volume_value(data.box));
    add(fields, "Cylinder", volume_value(data.cylinder));
    add(fields, "Sphere", volume_value(data.sphere));
    add(fields, "EnemyType", data.enemy_type);
    add(fields, "SpawnTotal", data.spawn_total);
    add(fields, "SpawnLimit", data.spawn_limit);
    add(fields, "SpawnCount", data.spawn_count);
    add(fields, "Cooldown", data.cooldown);
    add(fields, "StartFrame", data.start_frame);
    add(fields, "NodeName", data.node_name);
    add(fields, "ParentId", data.parent_id);
    add(fields, "EmptyMessage", data.empty_message);
}

void add_fields(FieldList& fields, const scene::FhDoorData& data) {
    add(fields, "RoomName", data.room_name);
    add(fields, "Locked", data.locked != 0);
    add(fields, "ModelId", data.model_id);
}

void add_fields(FieldList& fields, const scene::FhItemSpawnData& data) {
    add(fields, "ItemType", data.item_type);
    add(fields, "SpawnLimit", data.spawn_limit);
    add(fields, "CooldownTime", data.cooldown_time);
    add(fields, "Unused2C", data.unused_2c);
}

void add_fields(FieldList& fields, const scene::FhTriggerVolumeData& data) {
    add(fields, "Subtype", data.subtype);
    add(fields, "Box", volume_value(data.box));
    add(fields, "Sphere", volume_value(data.sphere));
    add(fields, "Cylinder", volume_value(data.cylinder));
    add(fields, "OneUse", data.one_use);
    add(fields, "Cooldown", data.cooldown);
    add(fields, "TriggerFlags", data.trigger_flags);
    add(fields, "Threshold", data.threshold);
    add(fields, "ParentId", data.parent_id);
    add(fields, "ParentMessage", data.parent_message);
    add(fields, "ParentMsgParam1", data.parent_parameter1);
    add(fields, "ChildId", data.child_id);
    add(fields, "ChildMessage", data.child_message);
    add(fields, "ChildMsgParam1", data.child_parameter1);
}

void add_fields(FieldList& fields, const scene::FhAreaVolumeData& data) {
    add(fields, "Subtype", data.subtype);
    add(fields, "Box", volume_value(data.box));
    add(fields, "Sphere", volume_value(data.sphere));
    add(fields, "Cylinder", volume_value(data.cylinder));
    add(fields, "InsideMessage", data.inside_message);
    add(fields, "InsideMsgParam1", data.inside_parameter1);
    add(fields, "ExitMessage", data.exit_message);
    add(fields, "ExitMsgParam1", data.exit_parameter1);
    add(fields, "Cooldown", data.cooldown);
    add(fields, "TriggerFlags", data.trigger_flags);
}

void add_fields(FieldList& fields, const scene::FhPlatformData& data) {
    add(fields, "NoPortal", data.no_portal);
    add(fields, "GroupId", data.group_id);
    add(fields, "Unused2C", data.unused_2c);
    add(fields, "Delay", data.delay);
    add(fields, "PositionCount", data.position_count);
    add(fields, "Volume", volume_value(data.volume));
    add(fields, "Positions", array_value(data.positions, [](scene::VolumePoint value) {
        return vector_value(value);
    }));
    add(fields, "Speed", data.speed);
    add(fields, "PortalName", data.portal_name);
}

void add_fields(FieldList& fields, const scene::FhJumpPadData& data) {
    add(fields, "VolumeType", data.volume_type);
    add(fields, "Box", volume_value(data.box));
    add(fields, "Sphere", volume_value(data.sphere));
    add(fields, "Cylinder", volume_value(data.cylinder));
    add(fields, "CooldownTime", data.cooldown_time);
    add(fields, "BeamVector", vector_value(data.beam_vector));
    add(fields, "Speed", data.speed);
    add(fields, "ControlLockTime", data.control_lock_time);
    add(fields, "ModelId", data.model_id);
    add(fields, "BeamType", data.beam_type);
    add(fields, "TriggerFlags", data.trigger_flags);
}

void add_fields(FieldList& fields, const scene::FhMorphCameraData& data) {
    add(fields, "Volume", volume_value(data.volume));
}

[[nodiscard]] bool is_first_hunt_data(const scene::TypedEntityData& data) {
    return std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        return std::is_same_v<T, enemy_spawn::FirstHuntData>
            || std::is_same_v<T, scene::FhDoorData>
            || std::is_same_v<T, scene::FhItemSpawnData>
            || std::is_same_v<T, scene::FhTriggerVolumeData>
            || std::is_same_v<T, scene::FhAreaVolumeData>
            || std::is_same_v<T, scene::FhPlatformData>
            || std::is_same_v<T, scene::FhJumpPadData>
            || std::is_same_v<T, scene::FhMorphCameraData>;
    }, data);
}

} // namespace

EntityEditor make_entity_editor(const scene::EntityInstance& entity) {
    EntityEditor editor;
    editor.base.type = entity.kind;
    editor.base.raw_type = entity.type;
    editor.base.id = entity.entity_id;
    editor.base.layer_mask = entity.layer_mask;
    editor.base.position = to_float(entity.position);
    editor.base.up = to_float(entity.up_vector);
    editor.base.facing = to_float(entity.facing_vector);
    editor.base.node_name = entity.node_name;
    editor.data = entity.typed_data;
    editor.first_hunt = entity.type >= 100 || is_first_hunt_data(editor.data);
    return editor;
}

std::string_view entity_type_name(scene::EntityKind type) noexcept {
    switch (type) {
    case scene::EntityKind::Platform: return "Platform";
    case scene::EntityKind::Object: return "Object";
    case scene::EntityKind::PlayerSpawn: return "PlayerSpawn";
    case scene::EntityKind::Door: return "Door";
    case scene::EntityKind::ItemSpawn: return "ItemSpawn";
    case scene::EntityKind::ItemInstance: return "ItemInstance";
    case scene::EntityKind::EnemySpawn: return "EnemySpawn";
    case scene::EntityKind::TriggerVolume: return "TriggerVolume";
    case scene::EntityKind::AreaVolume: return "AreaVolume";
    case scene::EntityKind::JumpPad: return "JumpPad";
    case scene::EntityKind::PointModule: return "PointModule";
    case scene::EntityKind::MorphCamera: return "MorphCamera";
    case scene::EntityKind::OctolithFlag: return "OctolithFlag";
    case scene::EntityKind::FlagBase: return "FlagBase";
    case scene::EntityKind::Teleporter: return "Teleporter";
    case scene::EntityKind::NodeDefense: return "NodeDefense";
    case scene::EntityKind::LightSource: return "LightSource";
    case scene::EntityKind::Artifact: return "Artifact";
    case scene::EntityKind::CameraSequence: return "CameraSequence";
    case scene::EntityKind::ForceField: return "ForceField";
    case scene::EntityKind::BeamEffect: return "BeamEffect";
    case scene::EntityKind::Bomb: return "Bomb";
    case scene::EntityKind::EnemyInstance: return "EnemyInstance";
    case scene::EntityKind::Halfturret: return "Halfturret";
    case scene::EntityKind::Player: return "Player";
    case scene::EntityKind::BeamProjectile: return "BeamProjectile";
    default: return "Unknown";
    }
}

std::string entity_type_name(const EntityEditor& entity) {
    const std::string_view base_name = entity_type_name(entity.base.type);
    if (!entity.first_hunt) {
        return std::string(base_name);
    }
    return "Fh" + std::string(base_name);
}

std::vector<FieldValue> field_values(const EntityEditor& entity) {
    FieldList fields;
    add(fields, "Type", entity_type_name(entity));
    add(fields, "RawType", entity.base.raw_type);
    add(fields, "Id", entity.base.id);
    add(fields, "LayerMask", entity.base.layer_mask);
    add(fields, "Position", vector_value(entity.base.position));
    add(fields, "Up", vector_value(entity.base.up));
    add(fields, "Facing", vector_value(entity.base.facing));
    add(fields, "NodeName", entity.base.node_name);
    std::visit([&fields](const auto& value) { add_fields(fields, value); },
               entity.data);
    return fields;
}

std::vector<Difference> compare(const EntityEditor& left,
                                const EntityEditor& right) {
    const auto left_fields = field_values(left);
    const auto right_fields = field_values(right);
    std::map<std::string, std::string> left_map;
    std::map<std::string, std::string> right_map;
    for (const auto& field : left_fields) {
        left_map[field.name] = field.value;
    }
    for (const auto& field : right_fields) {
        right_map[field.name] = field.value;
    }

    std::set<std::string> names;
    for (const auto& [name, unused] : left_map) {
        static_cast<void>(unused);
        names.insert(name);
    }
    for (const auto& [name, unused] : right_map) {
        static_cast<void>(unused);
        names.insert(name);
    }

    std::vector<Difference> differences;
    for (const std::string& name : names) {
        const auto left_it = left_map.find(name);
        const auto right_it = right_map.find(name);
        const std::string left_value = left_it == left_map.end()
            ? "N/A" : left_it->second;
        const std::string right_value = right_it == right_map.end()
            ? "N/A" : right_it->second;
        if (left_value != right_value) {
            differences.push_back({name, left_value, right_value});
        }
    }
    return differences;
}

std::string describe(const EntityEditor& entity) {
    std::ostringstream output;
    output << entity_type_name(entity) << " id=" << entity.base.id << '\n';
    for (const auto& field : field_values(entity)) {
        output << field.name << ": " << field.value << '\n';
    }
    return output.str();
}

} // namespace fruityprime::editor
