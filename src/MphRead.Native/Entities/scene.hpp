#pragma once

#include "Formats/collision_format.hpp"
#include "Formats/enemy_spawn.hpp"
#include "Formats/entity_format.hpp"
#include "Assets/game_assets.hpp"
#include "Formats/model_format.hpp"
#include "Formats/node_data.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace fruityprime::scene {

struct RoomDefinition {
    std::string name;
    std::string model_archive;
    std::string model_entry;
    std::string texture_path;
    std::string collision_entry;
    std::string entity_path;
    // These resources are catalogued even though the first native scene
    // boundary does not yet apply model animation tracks or build the
    // navigation graph.
    std::string animation_entry;
    std::string node_path;
    // Non-cartridge custom rooms are written as individual files. When this
    // is set, model/collision/entity/animation/node entries are read directly
    // below this directory instead of through model_archive.
    std::filesystem::path external_root;
};

// EntityType values are part of the cartridge format.  Keep the native room
// boundary typed even when a particular simulation layer does not yet consume
// every payload.  First Hunt stores the same types as a zero-based table and
// the parser exposes those raw values separately, so classification accepts
// the format version as an argument.
enum class EntityKind : std::uint8_t {
    Unknown,
    Platform,
    Object,
    PlayerSpawn,
    Door,
    ItemSpawn,
    ItemInstance,
    EnemySpawn,
    TriggerVolume,
    AreaVolume,
    JumpPad,
    PointModule,
    MorphCamera,
    OctolithFlag,
    FlagBase,
    Teleporter,
    NodeDefense,
    LightSource,
    Artifact,
    CameraSequence,
    ForceField,
    BeamEffect,
    Bomb,
    EnemyInstance,
    Halfturret,
    Player,
    BeamProjectile
};

[[nodiscard]] EntityKind classify_entity_type(
    std::uint16_t type, bool first_hunt = false) noexcept;

struct VolumePoint {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

enum class VolumeKind : std::uint8_t {
    Invalid,
    Box,
    Cylinder,
    Sphere
};

// C# RawCollisionVolume/CollisionVolume equivalent. The cartridge stores
// these in entity payloads as fixed-point values; Room::load converts them to
// world-space floats and moves their local position by the entity header.
struct EntityVolume {
    VolumeKind kind = VolumeKind::Invalid;
    VolumePoint box_vector1;
    VolumePoint box_vector2;
    VolumePoint box_vector3;
    VolumePoint box_position;
    float box_dot1 = 0.0F;
    float box_dot2 = 0.0F;
    float box_dot3 = 0.0F;
    VolumePoint cylinder_vector;
    VolumePoint cylinder_position;
    float cylinder_radius = 0.0F;
    float cylinder_dot = 0.0F;
    VolumePoint sphere_position;
    float sphere_radius = 0.0F;

    [[nodiscard]] bool contains(VolumePoint point) const noexcept;
    [[nodiscard]] VolumePoint center() const noexcept;
};

struct RotationPoint {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 0.0F;
};

struct MessageLinkData {
    std::int32_t target = -1;
    std::uint32_t message = 0;
    std::int32_t parameter1 = 0;
    std::int32_t parameter2 = 0;
};

struct PlatformData {
    std::uint32_t no_port = 0;
    std::uint32_t model_id = 0;
    std::int16_t parent_id = -1;
    bool active = false;
    std::uint8_t delay = 0;
    std::uint16_t scan_data1 = 0;
    std::int16_t scan_message_target = -1;
    std::uint32_t scan_message = 0;
    std::uint16_t scan_data2 = 0;
    std::uint16_t position_count = 0;
    std::array<VolumePoint, 10> positions{};
    std::array<RotationPoint, 10> rotations{};
    VolumePoint position_offset;
    float forward_speed = 0.0F;
    float backward_speed = 0.0F;
    std::string portal_name;
    std::uint32_t movement_type = 0;
    std::uint32_t for_cutscene = 0;
    std::uint32_t reverse_type = 0;
    std::uint32_t flags = 0;
    std::uint32_t contact_damage = 0;
    VolumePoint beam_spawn_direction;
    VolumePoint beam_spawn_position;
    std::int32_t beam_id = -1;
    std::uint32_t beam_interval = 0;
    std::uint32_t beam_on_intervals = 0;
    std::uint32_t unused_1d0 = 0;
    std::uint32_t unused_1d4 = 0;
    std::int32_t resist_effect_id = -1;
    std::uint32_t health = 0;
    std::uint32_t effectiveness = 0;
    std::int32_t damage_effect_id = -1;
    std::int32_t dead_effect_id = -1;
    std::uint8_t item_chance = 0;
    std::int32_t item_type = -1;
    MessageLinkData beam_hit_message;
    MessageLinkData player_collision_message;
    MessageLinkData dead_message;
    std::array<MessageLinkData, 4> lifetime_messages{};
    std::array<std::uint16_t, 4> lifetime_message_indices{};
};

struct ObjectData {
    std::uint8_t flags = 0;
    std::uint32_t effect_flags = 0;
    std::int32_t model_id = -1;
    std::int16_t linked_entity = -1;
    std::uint16_t scan_id = 0;
    std::int16_t scan_message_target = -1;
    std::uint32_t scan_message = 0;
    std::int32_t effect_id = -1;
    std::uint32_t effect_interval = 0;
    std::uint32_t effect_on_intervals = 0;
    VolumePoint effect_position_offset;
    EntityVolume volume;
};

struct EnemySpawnData {
    std::uint8_t enemy_type = 0;
    // EnemySpawnFields is a 400-byte type-specific union in the cartridge
    // format. Keep every byte available to a future per-enemy decoder while
    // exposing the common lifecycle fields below as native values.
    std::array<std::uint8_t, 400> fields{};
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
    float active_distance = 0.0F;
    float enemy_active_distance = 0.0F;
    std::string node_name;
    MessageLinkData message1;
    MessageLinkData message2;
    MessageLinkData message3;
    std::int32_t item_type = -1;
};

// First Hunt has a separate, smaller entity table.  These decoded records
// mirror the fixed structs in Formats/Entity.cs rather than forcing callers
// to reinterpret the raw payload.  The collision volumes remain raw because
// their FH field order differs from the MPH volume contract.
struct FhDoorData {
    std::string room_name;
    std::uint32_t locked = 0;
    std::uint32_t model_id = 0;
};

struct FhItemSpawnData {
    formats::FhItemType item_type = formats::FhItemType::None;
    std::uint16_t spawn_limit = 0;
    std::uint16_t cooldown_time = 0;
    std::uint16_t unused_2c = 0;
};

struct FhTriggerVolumeData {
    formats::FhTriggerType subtype = formats::FhTriggerType::Sphere;
    raw::FhRawCollisionVolume box{};
    raw::FhRawCollisionVolume sphere{};
    raw::FhRawCollisionVolume cylinder{};
    std::uint16_t one_use = 0;
    std::uint16_t cooldown = 0;
    std::uint32_t trigger_flags = 0;
    std::uint32_t threshold = 0;
    std::int16_t parent_id = -1;
    formats::FhMessage parent_message = formats::FhMessage::None;
    std::int32_t parent_parameter1 = 0;
    std::int16_t child_id = -1;
    formats::FhMessage child_message = formats::FhMessage::None;
    std::int32_t child_parameter1 = 0;
};

struct FhAreaVolumeData {
    formats::FhTriggerType subtype = formats::FhTriggerType::Sphere;
    raw::FhRawCollisionVolume box{};
    raw::FhRawCollisionVolume sphere{};
    raw::FhRawCollisionVolume cylinder{};
    formats::FhMessage inside_message = formats::FhMessage::None;
    std::int32_t inside_parameter1 = 0;
    formats::FhMessage exit_message = formats::FhMessage::None;
    std::int32_t exit_parameter1 = 0;
    std::uint16_t cooldown = 0;
    std::uint32_t trigger_flags = 0;
};

struct FhPlatformData {
    std::uint32_t no_portal = 0;
    std::uint32_t group_id = 0;
    std::uint32_t unused_2c = 0;
    std::uint8_t delay = 0;
    std::uint8_t position_count = 0;
    raw::FhRawCollisionVolume volume{};
    std::array<VolumePoint, 8> positions{};
    float speed = 0.0F;
    std::string portal_name;
};

struct FhJumpPadData {
    formats::FhTriggerType volume_type = formats::FhTriggerType::Sphere;
    raw::FhRawCollisionVolume box{};
    raw::FhRawCollisionVolume sphere{};
    raw::FhRawCollisionVolume cylinder{};
    std::uint32_t cooldown_time = 0;
    VolumePoint beam_vector;
    float speed = 0.0F;
    std::uint32_t control_lock_time = 0;
    std::uint32_t model_id = 0;
    std::uint32_t beam_type = 0;
    std::uint32_t trigger_flags = 0;
};

struct FhMorphCameraData {
    raw::FhRawCollisionVolume volume{};
};

struct PlayerSpawnData {
    std::uint8_t availability = 0;
    bool active = false;
    std::int8_t team_index = -1;
};

struct ItemSpawnData {
    std::int32_t parent_id = -1;
    std::int32_t item_type = -1;
    bool enabled = false;
    bool has_base = false;
    bool always_active = false;
    std::uint16_t max_spawn_count = 0;
    std::uint16_t spawn_interval = 0;
    std::uint16_t spawn_delay = 0;
    std::int16_t notify_entity_id = -1;
    std::uint32_t collected_message = 0;
    std::int32_t collected_parameter1 = 0;
    std::int32_t collected_parameter2 = 0;
};

struct PointModuleData {
    std::int16_t next_id = -1;
    std::int16_t previous_id = -1;
    bool active = false;
};

struct OctolithFlagData {
    std::uint8_t team_id = 0;
};

struct FlagBaseData {
    std::uint32_t team_id = 0;
    EntityVolume volume;
};

struct NodeDefenseData {
    EntityVolume volume;
};

struct DoorData {
    std::string node_name;
    std::uint32_t palette_id = 0;
    std::uint32_t door_type = 0;
    std::uint32_t connector_id = 0;
    std::uint8_t target_layer_id = 0;
    bool locked = false;
    std::uint8_t out_connector_id = 0;
    std::uint8_t out_loader_id = 0;
    std::string entity_filename;
    std::string room_name;
};

struct TriggerVolumeData {
    std::uint32_t subtype = 0;
    EntityVolume volume;
    bool active = false;
    bool always_active = false;
    bool deactivate_after_use = false;
    std::uint16_t repeat_delay = 0;
    std::uint16_t check_delay = 0;
    std::uint16_t required_state_bit = 0;
    std::uint32_t trigger_flags = 0;
    std::uint32_t trigger_threshold = 0;
    std::int16_t parent_id = -1;
    MessageLinkData parent_message;
    std::int16_t child_id = -1;
    MessageLinkData child_message;
};

struct AreaVolumeData {
    EntityVolume volume;
    bool active = false;
    bool always_active = false;
    bool allow_multiple = false;
    std::uint8_t message_delay = 0;
    std::uint16_t unused_6a = 0;
    std::uint32_t inside_message = 0;
    std::int32_t inside_parameter1 = 0;
    std::int32_t inside_parameter2 = 0;
    std::int16_t parent_id = -1;
    std::uint32_t exit_message = 0;
    std::int32_t exit_parameter1 = 0;
    std::int32_t exit_parameter2 = 0;
    std::int16_t child_id = -1;
    std::uint16_t cooldown = 0;
    std::uint32_t priority = 0;
    std::uint32_t trigger_flags = 0;
};

struct JumpPadData {
    std::int32_t parent_id = -1;
    std::uint32_t unused_28 = 0;
    EntityVolume volume;
    VolumePoint beam_vector;
    float speed = 0.0F;
    std::uint16_t control_lock_time = 0;
    std::uint16_t cooldown_time = 0;
    bool active = false;
    std::uint32_t model_id = 0;
    std::uint32_t beam_type = 0;
    std::uint32_t trigger_flags = 0;
};

struct MorphCameraData {
    EntityVolume volume;
};

struct TeleporterData {
    std::uint8_t load_index = 0;
    std::uint8_t target_index = 0;
    std::uint8_t artifact_id = 0;
    bool active = false;
    bool invisible = false;
    std::string entity_filename;
    VolumePoint target_position;
    std::string node_name;
};

struct LightSourceData {
    EntityVolume volume;
    bool light1_enabled = false;
    std::array<std::uint8_t, 3> light1_color{};
    VolumePoint light1_vector;
    bool light2_enabled = false;
    std::array<std::uint8_t, 3> light2_color{};
    VolumePoint light2_vector;
};

struct ArtifactData {
    std::uint8_t model_id = 0;
    std::uint8_t artifact_id = 0;
    bool active = false;
    bool has_base = false;
    MessageLinkData message1;
    MessageLinkData message2;
    MessageLinkData message3;
    std::int16_t linked_entity_id = -1;
};

struct CameraSequenceData {
    std::uint8_t sequence_id = 0;
    bool handoff = false;
    bool loop = false;
    bool block_input = false;
    bool force_alt_form = false;
    bool force_biped_form = false;
    std::uint16_t delay_frames = 0;
    std::uint8_t player_id1 = 0;
    std::uint8_t player_id2 = 0;
    std::int16_t entity1 = -1;
    std::int16_t entity2 = -1;
    std::int16_t end_message_target_id = -1;
    std::uint32_t end_message = 0;
    std::int32_t end_message_parameter = 0;
};

struct ForceFieldData {
    std::uint32_t type = 0;
    float width = 0.0F;
    float height = 0.0F;
    bool active = false;
};

using TypedEntityData = std::variant<
    std::monostate, PlatformData, ObjectData, EnemySpawnData, PlayerSpawnData,
    DoorData, ItemSpawnData, TriggerVolumeData, AreaVolumeData, JumpPadData,
    PointModuleData, MorphCameraData, OctolithFlagData, FlagBaseData,
    TeleporterData, NodeDefenseData, LightSourceData, ArtifactData,
    CameraSequenceData, ForceFieldData, enemy_spawn::FirstHuntData,
    FhDoorData, FhItemSpawnData, FhTriggerVolumeData, FhAreaVolumeData,
    FhPlatformData, FhJumpPadData, FhMorphCameraData>;

struct EntityInstance {
    std::uint16_t type = 0;
    EntityKind kind = EntityKind::Unknown;
    std::int16_t entity_id = -1;
    std::uint16_t layer_mask = 0;
    std::string node_name;
    formats::Vector3Fx position;
    formats::Vector3Fx up_vector;
    formats::Vector3Fx facing_vector;
    TypedEntityData typed_data;
    std::vector<std::uint8_t> payload;
};

// The first native Scene boundary. It owns the decoded resources needed by a
// room and exposes engine-neutral entity instances for the simulation layer.
// Asset lookup remains in Store, so this works from an NDS without a setup
// extraction and from an already extracted directory.
class Room {
public:
    [[nodiscard]] static Room load(const assets::Store& assets,
                                   const RoomDefinition& definition);

    [[nodiscard]] const RoomDefinition& definition() const noexcept {
        return definition_;
    }
    [[nodiscard]] const model::File& model() const;
    // Node layer filtering is a property of the match the room is loaded
    // into, not of the file, so the caller applies it after loading.
    [[nodiscard]] model::File& mutable_model();
    [[nodiscard]] const collision::File& collision() const;
    [[nodiscard]] const std::vector<EntityInstance>& entities() const noexcept {
        return entities_;
    }
    [[nodiscard]] bool has_entities() const noexcept {
        return entities_file_.has_value();
    }
    [[nodiscard]] const std::optional<node::NodeData>& node_data()
        const noexcept {
        return node_data_;
    }
    [[nodiscard]] bool has_node_data() const noexcept {
        return node_data_.has_value();
    }
    [[nodiscard]] const std::vector<std::uint8_t>& animation_bytes()
        const noexcept {
        return animation_bytes_;
    }
    [[nodiscard]] bool has_animation() const noexcept {
        return !animation_bytes_.empty();
    }

private:
    Room() = default;

    RoomDefinition definition_;
    std::optional<model::File> model_;
    std::optional<collision::File> collision_;
    std::optional<entity::File> entities_file_;
    std::optional<node::NodeData> node_data_;
    std::vector<std::uint8_t> animation_bytes_;
    std::vector<EntityInstance> entities_;
};

} // namespace fruityprime::scene
