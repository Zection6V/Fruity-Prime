#pragma once

#include "Formats/formats_display.hpp"
#include "Messaging.hpp"
#include "Entities/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::game {
class StorySave;
}

namespace fruityprime::entities::static_entities {

class World;

// Native EntityBase equivalent for records which are authored in a room's
// entity table.  Dynamic gameplay objects (players, beams, bombs, and enemy
// instances) remain in runtime_entities.hpp; this layer owns the fixed room
// entities and their Activate/Deactivate/Open/Close message boundary.
class Entity {
public:
    virtual ~Entity() = default;

    [[nodiscard]] std::int16_t id() const noexcept { return id_; }
    [[nodiscard]] scene::EntityKind kind() const noexcept { return kind_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool hidden() const noexcept { return hidden_; }
    [[nodiscard]] float alpha() const noexcept { return alpha_; }
    [[nodiscard]] const scene::VolumePoint& position() const noexcept {
        return position_;
    }
    [[nodiscard]] const scene::VolumePoint& up() const noexcept { return up_; }
    [[nodiscard]] const scene::VolumePoint& facing() const noexcept {
        return facing_;
    }
    [[nodiscard]] std::string_view node_name() const noexcept {
        return node_name_;
    }
    [[nodiscard]] std::int32_t parent_id() const noexcept {
        return parent_id_;
    }
    [[nodiscard]] bool parent_linked() const noexcept {
        return parent_linked_;
    }

    void set_active(bool value) noexcept { active_ = value; }
    void set_hidden(bool value) noexcept { hidden_ = value; }
    void set_alpha(float value) noexcept;
    virtual void set_position(scene::VolumePoint value) noexcept;
    void set_parent_id(std::int32_t value) noexcept { parent_id_ = value; }

    // Returns true while the entity remains present in the room.
    [[nodiscard]] virtual bool process(float seconds) noexcept;
    virtual void handle_message(const messaging::MessageInfo& message) noexcept;
    [[nodiscard]] virtual bool targetable() const noexcept { return true; }

    // EntityBase.OnScanned: the scan visor read this entity.  Most entities
    // do nothing; a few unlock a log entry or start a message.
    virtual void on_scanned() noexcept {}

    // EntityBase.ScanVisible: whether the scan visor can pick this entity up
    // at all, which is not the same as whether it is drawn -- an entity in a
    // room part the player cannot see is not scannable either.
    [[nodiscard]] virtual bool scan_visible() const noexcept {
        return node_ref_visible_;
    }
    void set_node_ref_visible(bool value) noexcept {
        node_ref_visible_ = value;
    }

    // EntityBase.GetDisplayVolumes: the viewer's collision-volume overlay.
    // Entities that have no volume to show leave it empty.
    virtual void get_display_volumes(
        std::vector<formats::DisplayVolume>& out) const {
        static_cast<void>(out);
    }

protected:
    // EntityBase.ScanVisible reads this: the entity's room part is one the
    // player can currently see.
    bool node_ref_visible_ = true;

    explicit Entity(const scene::EntityInstance& source) noexcept;

    void advance_age(float seconds) noexcept;
    [[nodiscard]] static bool is_activation_message(
        messaging::Message message) noexcept;
    [[nodiscard]] static bool is_deactivation_message(
        messaging::Message message) noexcept;

    std::int16_t id_ = -1;
    scene::EntityKind kind_ = scene::EntityKind::Unknown;
    bool active_ = true;
    bool initialized_ = true;
    bool hidden_ = false;
    float alpha_ = 1.0F;
    float age_seconds_ = 0.0F;
    scene::VolumePoint position_{};
    scene::VolumePoint up_{0.0F, 1.0F, 0.0F};
    scene::VolumePoint facing_{0.0F, 0.0F, 1.0F};
    std::string_view node_name_;
    std::int32_t parent_id_ = -1;
    bool parent_linked_ = false;
    scene::VolumePoint parent_relative_position_{};

    friend class World;
    void link_parent(const Entity& parent) noexcept;
    void sync_parent(const Entity& parent) noexcept;
};

class PlatformEntity final : public Entity {
public:
    explicit PlatformEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::PlatformData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] std::size_t path_index() const noexcept { return path_index_; }
    [[nodiscard]] float path_progress() const noexcept { return path_progress_; }
    [[nodiscard]] bool moving() const noexcept { return moving_; }
    void set_moving(bool value) noexcept { moving_ = value; }
    void set_path_position(std::size_t index) noexcept;

    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;

private:
    scene::PlatformData data_;
    std::size_t path_index_ = 0;
    float path_progress_ = 0.0F;
    bool moving_ = false;
    bool reverse_ = false;
};

class ObjectEntity final : public Entity {
public:
    explicit ObjectEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::ObjectData& data() const noexcept { return data_; }
    [[nodiscard]] float effect_timer() const noexcept { return effect_timer_; }
    [[nodiscard]] std::uint32_t effect_count() const noexcept {
        return effect_count_;
    }
    [[nodiscard]] std::uint8_t state() const noexcept { return state_; }
    void set_state(std::uint8_t value) noexcept {
        state_ = value > 2 ? 2 : value;
    }

    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    void set_position(scene::VolumePoint value) noexcept override;

private:
    scene::ObjectData data_;
    std::uint8_t state_ = 0;
    float effect_timer_ = 0.0F;
    std::uint32_t effect_count_ = 0;
};

class PlayerSpawnEntity final : public Entity {
public:
    explicit PlayerSpawnEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::PlayerSpawnData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] bool available() const noexcept {
        return data_.availability != 0;
    }
    [[nodiscard]] std::uint32_t cooldown_ticks() const noexcept {
        return cooldown_ticks_;
    }
    void set_cooldown_ticks(std::uint32_t value) noexcept {
        cooldown_ticks_ = value;
    }

    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;

private:
    scene::PlayerSpawnData data_;
    std::uint32_t cooldown_ticks_ = 0;
};

class ItemSpawnEntity final : public Entity {
public:
    explicit ItemSpawnEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::ItemSpawnData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] bool item_present() const noexcept { return item_present_; }
    [[nodiscard]] std::uint32_t spawn_count() const noexcept {
        return spawn_count_;
    }
    [[nodiscard]] std::uint32_t cooldown_ticks() const noexcept {
        return cooldown_ticks_;
    }
    void on_item_picked_up() noexcept;

    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;

private:
    scene::ItemSpawnData data_;
    std::uint32_t spawn_count_ = 0;
    std::uint32_t cooldown_ticks_ = 0;
    bool item_present_ = false;
};

class DoorEntity final : public Entity {
public:
    explicit DoorEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::DoorData& data() const noexcept { return data_; }
    [[nodiscard]] bool locked() const noexcept { return locked_; }
    [[nodiscard]] float open_fraction() const noexcept { return open_fraction_; }
    void set_locked(bool value) noexcept { locked_ = value; }

    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool targetable() const noexcept override { return !locked_; }

private:
    scene::DoorData data_;
    bool locked_ = false;
    bool opening_ = false;
    float open_fraction_ = 0.0F;
};

class VolumeEntity : public Entity {
public:
    [[nodiscard]] const scene::EntityVolume& volume() const noexcept {
        return volume_;
    }
    [[nodiscard]] bool contains(scene::VolumePoint point) const noexcept {
        return active_ && volume_.contains(point);
    }

    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    void set_position(scene::VolumePoint value) noexcept override;

protected:
    VolumeEntity(const scene::EntityInstance& source,
                 const scene::EntityVolume& volume);

    scene::EntityVolume volume_;
};

class AreaVolumeEntity final : public VolumeEntity {
public:
    explicit AreaVolumeEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::AreaVolumeData& data() const noexcept {
        return data_;
    }
    void set_position(scene::VolumePoint value) noexcept override;

private:
    scene::AreaVolumeData data_;
};

class TriggerVolumeEntity final : public VolumeEntity {
public:
    explicit TriggerVolumeEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::TriggerVolumeData& data() const noexcept {
        return data_;
    }
    void set_position(scene::VolumePoint value) noexcept override;
    [[nodiscard]] bool relay() const noexcept { return data_.subtype == 2; }
    [[nodiscard]] std::int32_t parent_target() const noexcept {
        return data_.parent_id;
    }
    [[nodiscard]] std::int32_t child_target() const noexcept {
        return data_.child_id;
    }

private:
    scene::TriggerVolumeData data_;
};

class JumpPadEntity final : public VolumeEntity {
public:
    explicit JumpPadEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::JumpPadData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] std::uint32_t cooldown_ticks() const noexcept {
        return cooldown_ticks_;
    }
    [[nodiscard]] bool ready() const noexcept { return cooldown_ticks_ == 0; }
    void trigger() noexcept;
    void set_position(scene::VolumePoint value) noexcept override;

    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    scene::JumpPadData data_;
    std::uint32_t cooldown_ticks_ = 0;
};

class MorphCameraEntity final : public VolumeEntity {
public:
    explicit MorphCameraEntity(const scene::EntityInstance& source);
    void set_position(scene::VolumePoint value) noexcept override;
};

class NodeDefenseEntity final : public VolumeEntity {
public:
    explicit NodeDefenseEntity(const scene::EntityInstance& source);
    void set_position(scene::VolumePoint value) noexcept override;
};

class FlagBaseEntity final : public VolumeEntity {
public:
    explicit FlagBaseEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::FlagBaseData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] std::uint32_t team_id() const noexcept { return team_id_; }
    void set_position(scene::VolumePoint value) noexcept override;

private:
    scene::FlagBaseData data_;
    std::uint32_t team_id_ = 0;
};

class LightSourceEntity final : public VolumeEntity {
public:
    explicit LightSourceEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::LightSourceData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] bool light1_enabled() const noexcept {
        return data_.light1_enabled;
    }
    [[nodiscard]] const scene::VolumePoint& light1_vector() const noexcept {
        return data_.light1_vector;
    }
    [[nodiscard]] formats::Vector3 light1_color() const noexcept {
        return {data_.light1_color[0] / 255.0F,
                data_.light1_color[1] / 255.0F,
                data_.light1_color[2] / 255.0F};
    }
    [[nodiscard]] bool light2_enabled() const noexcept {
        return data_.light2_enabled;
    }
    [[nodiscard]] const scene::VolumePoint& light2_vector() const noexcept {
        return data_.light2_vector;
    }
    [[nodiscard]] formats::Vector3 light2_color() const noexcept {
        return {data_.light2_color[0] / 255.0F,
                data_.light2_color[1] / 255.0F,
                data_.light2_color[2] / 255.0F};
    }
    void set_position(scene::VolumePoint value) noexcept override;

private:
    scene::LightSourceData data_;
};

class TeleporterEntity final : public Entity {
public:
    explicit TeleporterEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::TeleporterData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] bool ready() const noexcept { return cooldown_seconds_ <= 0.0F; }
    [[nodiscard]] float cooldown_seconds() const noexcept {
        return cooldown_seconds_;
    }
    void trigger() noexcept;

    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    scene::TeleporterData data_;
    float cooldown_seconds_ = 0.0F;
};

class OctolithFlagEntity final : public Entity {
public:
    explicit OctolithFlagEntity(const scene::EntityInstance& source);

    [[nodiscard]] std::uint8_t team_id() const noexcept { return team_id_; }
    [[nodiscard]] bool captured() const noexcept { return captured_; }
    void capture() noexcept { captured_ = true; }
    void reset() noexcept { captured_ = false; }

private:
    std::uint8_t team_id_ = 0;
    bool captured_ = false;
};

class ArtifactEntity final : public Entity {
public:
    explicit ArtifactEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::ArtifactData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] std::uint8_t model_id() const noexcept {
        return data_.model_id;
    }
    [[nodiscard]] std::uint8_t artifact_id() const noexcept {
        return data_.artifact_id;
    }
    [[nodiscard]] std::uint16_t scan_id() const noexcept { return scan_id_; }
    [[nodiscard]] bool collected() const noexcept { return collected_; }
    void collect() noexcept;

    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;

private:
    scene::ArtifactData data_;
    bool collected_ = false;
    std::uint16_t scan_id_ = 0;
};

class ForceFieldEntity final : public Entity {
public:
    explicit ForceFieldEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::ForceFieldData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] const scene::VolumePoint& field_up() const noexcept {
        return up_;
    }
    [[nodiscard]] const scene::VolumePoint& field_facing() const noexcept {
        return facing_;
    }
    [[nodiscard]] const scene::VolumePoint& field_right() const noexcept {
        return right_;
    }
    [[nodiscard]] float plane_distance() const noexcept {
        return plane_distance_;
    }
    [[nodiscard]] float width() const noexcept { return data_.width; }
    [[nodiscard]] float height() const noexcept { return data_.height; }
    [[nodiscard]] std::uint16_t scan_id() const noexcept { return scan_id_; }
    [[nodiscard]] bool process(float seconds) noexcept override;
    void handle_message(const messaging::MessageInfo& message) noexcept override;
    [[nodiscard]] bool targetable() const noexcept override { return false; }

private:
    void refresh_scan_id() noexcept;

    scene::ForceFieldData data_;
    scene::VolumePoint right_{1.0F, 0.0F, 0.0F};
    float plane_distance_ = 0.0F;
    std::uint16_t scan_id_ = 0;
};

class PointModuleEntity final : public Entity {
public:
    static constexpr std::int16_t StartId = 50;

    explicit PointModuleEntity(const scene::EntityInstance& source);

    [[nodiscard]] const scene::PointModuleData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] PointModuleEntity* next() const noexcept { return next_; }
    [[nodiscard]] PointModuleEntity* previous() const noexcept {
        return previous_;
    }
    void set_current() noexcept;
    [[nodiscard]] bool process(float seconds) noexcept override;

private:
    scene::PointModuleData data_;
    PointModuleEntity* next_ = nullptr;
    PointModuleEntity* previous_ = nullptr;
    World* world_ = nullptr;

    friend class World;
};

// First Hunt records share EntityKind values with MPH, but their payloads and
// class behavior are distinct. Keep the managed Fh* classes distinct after
// parsing instead of silently constructing an MPH class with empty data.
class FhDoorEntity final : public Entity {
public:
    explicit FhDoorEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::FhDoorData& data() const noexcept {
        return data_;
    }
private:
    scene::FhDoorData data_;
};

class FhItemSpawnEntity final : public Entity {
public:
    explicit FhItemSpawnEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::FhItemSpawnData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] bool spawned() const noexcept { return spawned_; }
    [[nodiscard]] bool process(float seconds) noexcept override;
private:
    scene::FhItemSpawnData data_;
    bool spawned_ = false;
};

class FhTriggerVolumeEntity final : public VolumeEntity {
public:
    explicit FhTriggerVolumeEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::FhTriggerVolumeData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] std::int32_t parent_target() const noexcept {
        return data_.parent_id;
    }
    [[nodiscard]] std::int32_t child_target() const noexcept {
        return data_.child_id;
    }
private:
    scene::FhTriggerVolumeData data_;
};

class FhAreaVolumeEntity final : public VolumeEntity {
public:
    explicit FhAreaVolumeEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::FhAreaVolumeData& data() const noexcept {
        return data_;
    }
private:
    scene::FhAreaVolumeData data_;
};

class FhJumpPadEntity final : public VolumeEntity {
public:
    explicit FhJumpPadEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::FhJumpPadData& data() const noexcept {
        return data_;
    }
private:
    scene::FhJumpPadData data_;
};

class FhMorphCameraEntity final : public VolumeEntity {
public:
    explicit FhMorphCameraEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::FhMorphCameraData& data() const noexcept {
        return data_;
    }
private:
    scene::FhMorphCameraData data_;
};

class FhEnemySpawnEntity final : public Entity {
public:
    explicit FhEnemySpawnEntity(const scene::EntityInstance& source);
    [[nodiscard]] const enemy_spawn::FirstHuntData& data() const noexcept {
        return data_;
    }
private:
    enemy_spawn::FirstHuntData data_;
};

class FhPlatformEntity final : public Entity {
public:
    explicit FhPlatformEntity(const scene::EntityInstance& source);
    [[nodiscard]] const scene::FhPlatformData& data() const noexcept {
        return data_;
    }
    [[nodiscard]] bool process(float seconds) noexcept override;
private:
    enum class MoveState : std::uint8_t {
        Sleep, MoveForward, Wait, MoveBackward
    };
    void update_movement() noexcept;

    scene::FhPlatformData data_;
    MoveState state_ = MoveState::Sleep;
    std::size_t from_index_ = 0;
    std::size_t to_index_ = 1;
    std::uint32_t delay_ = 0;
    std::uint32_t move_timer_ = 0;
    scene::VolumePoint velocity_{};
};

// Owns all fixed room entities.  It is intentionally independent of the
// renderer and gameplay Session, so the same decoded world can be used by the
// Win32 host, the headless map test, and a future Android frontend.
class World {
public:
    using MessageSink = std::function<void(const messaging::MessageInfo&)>;

    World() = default;
    explicit World(const std::vector<scene::EntityInstance>& source);

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;
    ~World() = default;

    void clear() noexcept {
        entities_.clear();
        current_point_module_ = nullptr;
        story_save_ = nullptr;
        story_room_id_ = -1;
    }
    void process(float seconds) noexcept;
    // Apply the managed StorySave room-state contract to the fixed entities
    // after a room has been decoded.  Multiplayer callers intentionally do
    // not invoke this; their room entities start from authored data.
    void apply_story_save(std::int32_t room_id, game::StorySave& save) noexcept;
    [[nodiscard]] std::size_t dispatch(const messaging::MessageInfo& message)
        noexcept;
    void set_message_sink(MessageSink sink) { message_sink_ = std::move(sink); }

    [[nodiscard]] std::size_t size() const noexcept { return entities_.size(); }
    [[nodiscard]] const std::vector<std::unique_ptr<Entity>>& entities()
        const noexcept {
        return entities_;
    }
    [[nodiscard]] PointModuleEntity* current_point_module() const noexcept {
        return current_point_module_;
    }
    void set_current(PointModuleEntity* entity) noexcept;
    [[nodiscard]] Entity* find(std::int16_t id) noexcept;
    [[nodiscard]] const Entity* find(std::int16_t id) const noexcept;
    [[nodiscard]] std::size_t count(scene::EntityKind kind) const noexcept;

private:
    void add(const scene::EntityInstance& source);
    void sync_parent_transforms() noexcept;
    void update_story_state(const Entity& entity,
                            const messaging::MessageInfo& message) noexcept;

    std::vector<std::unique_ptr<Entity>> entities_;
    PointModuleEntity* current_point_module_ = nullptr;
    MessageSink message_sink_;
    game::StorySave* story_save_ = nullptr;
    std::int32_t story_room_id_ = -1;
};

} // namespace fruityprime::entities::static_entities
