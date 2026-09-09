// Native counterpart of src/MphRead/Entities/RoomEntity.cs.
#include "Entities/static_entities.hpp"
#include "static_entity_helpers.hpp"
#include "GameState.hpp"

namespace fruityprime::entities::static_entities {

using namespace detail;

World::World(const std::vector<scene::EntityInstance>& source) {
    entities_.reserve(source.size());
    for (const auto& entity : source) {
        add(entity);
    }
    // Capture offsets before the first simulation step moves a parent.  The
    // managed EntityBase stores the inverse parent transform at link time;
    // doing the same here avoids baking a parent's first-frame displacement
    // into every child offset.
    for (const auto& entity : entities_) {
        if (entity->parent_id() < 0
            || entity->parent_id() == entity->id()) {
            continue;
        }
        const auto* parent = find(static_cast<std::int16_t>(
            entity->parent_id()));
        if (parent != nullptr) {
            entity->link_parent(*parent);
        }
    }
    for (const auto& entity : entities_) {
        auto* point = dynamic_cast<PointModuleEntity*>(entity.get());
        if (point == nullptr) {
            continue;
        }
        point->world_ = this;
        if (point->data_.next_id != 0) {
            point->next_ = dynamic_cast<PointModuleEntity*>(
                find(point->data_.next_id));
        }
        if (point->data_.previous_id != 0) {
            point->previous_ = dynamic_cast<PointModuleEntity*>(
                find(point->data_.previous_id));
        }
    }
}

void World::set_current(PointModuleEntity* entity) noexcept {
    if (current_point_module_ == entity) {
        return;
    }
    const auto update_chain = [](PointModuleEntity* point, bool active) {
        for (int index = 0; point != nullptr && index < 5; ++index) {
            point->set_active(active);
            point = point->next();
        }
    };
    update_chain(current_point_module_, false);
    current_point_module_ = entity;
    update_chain(current_point_module_, true);
}

void World::process(float seconds) noexcept {
    for (std::size_t index = 0; index < entities_.size();) {
        if (entities_[index]->process(seconds)) {
            ++index;
        } else {
            entities_.erase(entities_.begin()
                            + static_cast<std::ptrdiff_t>(index));
        }
    }
    sync_parent_transforms();
}

std::size_t World::dispatch(const messaging::MessageInfo& message) noexcept {
    struct Pending {
        messaging::MessageInfo message;
        std::uint8_t depth = 0;
    };
    constexpr std::size_t max_pending = 64;
    constexpr std::uint8_t max_relay_depth = 8;
    std::array<Pending, max_pending> pending{};
    std::size_t read_index = 0;
    std::size_t write_index = 1;
    pending[0] = {message, 0};
    std::size_t count = 0;
    while (read_index < write_index) {
        const Pending current = pending[read_index++];
        for (const auto& entity : entities_) {
            if (current.message.target >= 0
                && entity->id() != current.message.target) {
                continue;
            }
            entity->handle_message(current.message);
            update_story_state(*entity, current.message);
            ++count;

            const auto* trigger = dynamic_cast<const TriggerVolumeEntity*>(
                entity.get());
            if (trigger == nullptr || !trigger->relay()
                || current.depth >= max_relay_depth) {
                continue;
            }
            const std::array<std::int32_t, 2> targets{
                trigger->parent_target(), trigger->child_target()};
            for (const std::int32_t target : targets) {
                if (target < 0 || target == entity->id()) {
                    continue;
                }
                messaging::MessageInfo forwarded = current.message;
                forwarded.sender = current.message.sender;
                forwarded.target = target;
                if (message_sink_) {
                    forwarded.queued_frame = current.message.execute_frame;
                    forwarded.execute_frame = current.message.execute_frame
                        == std::numeric_limits<std::uint64_t>::max()
                        ? current.message.execute_frame
                        : current.message.execute_frame + 1;
                    message_sink_(forwarded);
                } else if (write_index < max_pending) {
                    pending[write_index++] = {forwarded,
                                              static_cast<std::uint8_t>(
                                                  current.depth + 1)};
                }
            }
        }
    }
    return count;
}

void World::update_story_state(
    const Entity& entity, const messaging::MessageInfo& message) noexcept {
    if (story_save_ == nullptr || story_room_id_ < 27
        || story_room_id_ > 92 || entity.id() < 0) {
        return;
    }

    const bool set_active = message.cartridge_message == 5;
    const bool activate = activation_message(message);
    const bool deactivate = deactivation_message(message);
    const bool unlock = message.cartridge_message == 16;
    const bool lock = message.cartridge_message == 17;
    if (!set_active && !activate && !deactivate && !unlock && !lock) {
        return;
    }

    const auto room_id = story_room_id_;
    const auto entity_id = static_cast<std::int32_t>(entity.id());
    const auto set_binary_state = [this, room_id, entity_id](bool active) {
        story_save_->set_room_state(room_id, entity_id, active ? 3 : 1);
    };

    switch (entity.kind()) {
    case scene::EntityKind::Object: {
        const auto* object = dynamic_cast<const ObjectEntity*>(&entity);
        if (object == nullptr) {
            return;
        }
        if (set_active) {
            const auto state = std::clamp(message.parameter1, 0, 2);
            story_save_->set_room_state(room_id, entity_id, state + 1);
        } else if (activate) {
            story_save_->set_room_state(room_id, entity_id, 3);
        } else if (deactivate) {
            story_save_->set_room_state(room_id, entity_id, 1);
        }
        return;
    }
    case scene::EntityKind::Platform: {
        const auto* platform = dynamic_cast<const PlatformEntity*>(&entity);
        if (platform == nullptr
            || (platform->data().flags & 0x0002'0000u) == 0
            || (platform->data().flags & 0x0020'0000u) == 0) {
            return;
        }
        if (set_active) {
            set_binary_state(message.parameter1 != 0);
        } else if (activate) {
            set_binary_state(true);
        } else if (deactivate) {
            set_binary_state(false);
        }
        return;
    }
    case scene::EntityKind::Door:
        if (unlock) {
            set_binary_state(false);
        } else if (lock) {
            set_binary_state(true);
        }
        return;
    case scene::EntityKind::PlayerSpawn:
    case scene::EntityKind::ItemSpawn:
    case scene::EntityKind::AreaVolume:
    case scene::EntityKind::TriggerVolume:
    case scene::EntityKind::JumpPad:
    case scene::EntityKind::Teleporter:
    case scene::EntityKind::Artifact:
        if (set_active) {
            set_binary_state(message.parameter1 != 0);
        } else if (activate) {
            set_binary_state(true);
        } else if (deactivate) {
            set_binary_state(false);
        }
        return;
    case scene::EntityKind::ForceField:
        if (unlock) {
            set_binary_state(false);
        } else if (lock) {
            set_binary_state(true);
        } else if (set_active) {
            set_binary_state(message.parameter1 != 0);
        } else if (activate) {
            set_binary_state(true);
        } else if (deactivate) {
            set_binary_state(false);
        }
        return;
    default:
        return;
    }
}

void World::apply_story_save(std::int32_t room_id,
                             game::StorySave& save) noexcept {
    if (room_id < 27 || room_id > 92) {
        story_save_ = nullptr;
        story_room_id_ = -1;
        return;
    }
    story_save_ = &save;
    story_room_id_ = room_id;

    // These are PlatformFlags.UseRoomState and PersistRoomState from the
    // managed PlatformEntity.  Keeping the values local avoids making the
    // cartridge-format scene header depend on the entity implementation.
    constexpr std::uint32_t use_room_state = 0x0002'0000u;
    constexpr std::uint32_t persist_room_state = 0x0020'0000u;

    for (const auto& entity : entities_) {
        if (entity == nullptr || entity->id() < 0) {
            continue;
        }
        const auto entity_id = static_cast<std::int32_t>(entity->id());
        switch (entity->kind()) {
        case scene::EntityKind::Object: {
            auto* object = dynamic_cast<ObjectEntity*>(entity.get());
            if (object == nullptr) {
                break;
            }
            const auto room_state = save.room_state_value(room_id, entity_id);
            if (room_state == -1) {
                // ObjectFlags.State stores 0..2, while StorySave stores the
                // corresponding packed values 1..3.
                save.set_room_state(room_id, entity_id,
                                    static_cast<std::int32_t>(object->state())
                                        + 1);
            } else {
                object->set_state(static_cast<std::uint8_t>(
                    std::clamp(room_state, 0, 2)));
            }
            break;
        }
        case scene::EntityKind::Platform: {
            auto* platform = dynamic_cast<PlatformEntity*>(entity.get());
            if (platform == nullptr
                || (platform->data().flags & use_room_state) == 0) {
                break;
            }
            if ((platform->data().flags & persist_room_state) != 0) {
                const auto state = save.init_room_state(
                    room_id, entity_id, platform->data().active);
                platform->set_active(state != 0);
                platform->set_moving(state != 0);
            } else if (save.room_state_value(room_id, entity_id) == 1
                       && platform->data().position_count != 0) {
                platform->set_path_position(
                    static_cast<std::size_t>(platform->data().position_count - 1));
            }
            break;
        }
        case scene::EntityKind::PlayerSpawn: {
            auto* spawn = dynamic_cast<PlayerSpawnEntity*>(entity.get());
            if (spawn != nullptr) {
                spawn->set_active(save.init_room_state(
                    room_id, entity_id, spawn->data().active) != 0);
            }
            break;
        }
        case scene::EntityKind::ItemSpawn: {
            auto* spawn = dynamic_cast<ItemSpawnEntity*>(entity.get());
            if (spawn != nullptr) {
                const auto state = save.init_room_state(
                    room_id, entity_id, spawn->data().enabled);
                spawn->set_active(spawn->data().always_active
                                      ? spawn->data().enabled
                                      : state != 0);
            }
            break;
        }
        case scene::EntityKind::Door: {
            auto* door = dynamic_cast<DoorEntity*>(entity.get());
            if (door != nullptr) {
                door->set_locked(save.init_room_state(
                    room_id, entity_id, door->data().locked) != 0);
            }
            break;
        }
        case scene::EntityKind::AreaVolume: {
            auto* volume = dynamic_cast<AreaVolumeEntity*>(entity.get());
            if (volume != nullptr) {
                const auto state = save.init_room_state(
                    room_id, entity_id, volume->data().active);
                volume->set_active(volume->data().always_active
                                       ? volume->data().active
                                       : state != 0);
            }
            break;
        }
        case scene::EntityKind::TriggerVolume: {
            auto* volume = dynamic_cast<TriggerVolumeEntity*>(entity.get());
            if (volume != nullptr) {
                const auto state = save.init_room_state(
                    room_id, entity_id, volume->data().active);
                volume->set_active(volume->data().always_active
                                       ? volume->data().active
                                       : state != 0);
            }
            break;
        }
        case scene::EntityKind::JumpPad: {
            auto* jump_pad = dynamic_cast<JumpPadEntity*>(entity.get());
            if (jump_pad != nullptr) {
                jump_pad->set_active(save.init_room_state(
                    room_id, entity_id, jump_pad->data().active) != 0);
            }
            break;
        }
        case scene::EntityKind::Teleporter: {
            auto* teleporter = dynamic_cast<TeleporterEntity*>(entity.get());
            if (teleporter != nullptr) {
                teleporter->set_active(save.init_room_state(
                    room_id, entity_id, teleporter->data().active) != 0);
            }
            break;
        }
        case scene::EntityKind::Artifact: {
            auto* artifact = dynamic_cast<ArtifactEntity*>(entity.get());
            if (artifact == nullptr) {
                break;
            }
            const auto& data = artifact->data();
            const auto state = save.init_room_state(
                room_id, entity_id, data.active, 2);
            artifact->set_active(state != 0);
            if (data.model_id < 8
                && save.found_artifact(data.artifact_id, data.model_id)) {
                artifact->set_active(false);
            } else if (data.model_id >= 8
                       && save.found_octolith(data.artifact_id)) {
                artifact->set_active(false);
            }
            break;
        }
        case scene::EntityKind::ForceField: {
            auto* force_field = dynamic_cast<ForceFieldEntity*>(entity.get());
            if (force_field != nullptr) {
                force_field->set_active(save.init_room_state(
                    room_id, entity_id, force_field->data().active) != 0);
            }
            break;
        }
        default:
            // Objects, point modules, and presentation-only records do not
            // use StorySave.InitRoomState in the managed constructors.
            break;
        }
    }
}

void World::sync_parent_transforms() noexcept {
    // Resolve one level per pass so a child of a moving child converges even
    // when the room's entity table is not topologically ordered.  The pass
    // bound also makes malformed cyclic authored links harmless.
    for (std::size_t pass = 0; pass < entities_.size(); ++pass) {
        for (const auto& entity : entities_) {
            if (entity->parent_id() < 0
                || entity->parent_id() == entity->id()) {
                continue;
            }
            const auto* parent = find(static_cast<std::int16_t>(
                entity->parent_id()));
            if (parent != nullptr) {
                entity->sync_parent(*parent);
            }
        }
    }
}

Entity* World::find(std::int16_t id) noexcept {
    const auto found = std::find_if(entities_.begin(), entities_.end(),
        [id](const auto& entity) { return entity->id() == id; });
    return found == entities_.end() ? nullptr : found->get();
}

const Entity* World::find(std::int16_t id) const noexcept {
    const auto found = std::find_if(entities_.begin(), entities_.end(),
        [id](const auto& entity) { return entity->id() == id; });
    return found == entities_.end() ? nullptr : found->get();
}

std::size_t World::count(scene::EntityKind kind) const noexcept {
    return static_cast<std::size_t>(std::count_if(
        entities_.begin(), entities_.end(),
        [kind](const auto& entity) { return entity->kind() == kind; }));
}

void World::add(const scene::EntityInstance& source) {
    std::unique_ptr<Entity> entity;
    switch (source.kind) {
    case scene::EntityKind::Platform:
        if (std::holds_alternative<scene::FhPlatformData>(source.typed_data)) {
            entity = std::make_unique<FhPlatformEntity>(source);
        } else {
            entity = std::make_unique<PlatformEntity>(source);
        }
        break;
    case scene::EntityKind::Object:
        entity = std::make_unique<ObjectEntity>(source);
        break;
    case scene::EntityKind::PlayerSpawn:
        entity = std::make_unique<PlayerSpawnEntity>(source);
        break;
    case scene::EntityKind::ItemSpawn:
        if (std::holds_alternative<scene::FhItemSpawnData>(source.typed_data)) {
            entity = std::make_unique<FhItemSpawnEntity>(source);
        } else {
            entity = std::make_unique<ItemSpawnEntity>(source);
        }
        break;
    case scene::EntityKind::Door:
        if (std::holds_alternative<scene::FhDoorData>(source.typed_data)) {
            entity = std::make_unique<FhDoorEntity>(source);
        } else {
            entity = std::make_unique<DoorEntity>(source);
        }
        break;
    case scene::EntityKind::AreaVolume:
        if (std::holds_alternative<scene::FhAreaVolumeData>(source.typed_data)) {
            entity = std::make_unique<FhAreaVolumeEntity>(source);
        } else {
            entity = std::make_unique<AreaVolumeEntity>(source);
        }
        break;
    case scene::EntityKind::TriggerVolume:
        if (std::holds_alternative<scene::FhTriggerVolumeData>(source.typed_data)) {
            entity = std::make_unique<FhTriggerVolumeEntity>(source);
        } else {
            entity = std::make_unique<TriggerVolumeEntity>(source);
        }
        break;
    case scene::EntityKind::JumpPad:
        if (std::holds_alternative<scene::FhJumpPadData>(source.typed_data)) {
            entity = std::make_unique<FhJumpPadEntity>(source);
        } else {
            entity = std::make_unique<JumpPadEntity>(source);
        }
        break;
    case scene::EntityKind::MorphCamera:
        if (std::holds_alternative<scene::FhMorphCameraData>(source.typed_data)) {
            entity = std::make_unique<FhMorphCameraEntity>(source);
        } else {
            entity = std::make_unique<MorphCameraEntity>(source);
        }
        break;
    case scene::EntityKind::NodeDefense:
        entity = std::make_unique<NodeDefenseEntity>(source);
        break;
    case scene::EntityKind::FlagBase:
        entity = std::make_unique<FlagBaseEntity>(source);
        break;
    case scene::EntityKind::LightSource:
        entity = std::make_unique<LightSourceEntity>(source);
        break;
    case scene::EntityKind::Teleporter:
        entity = std::make_unique<TeleporterEntity>(source);
        break;
    case scene::EntityKind::OctolithFlag:
        entity = std::make_unique<OctolithFlagEntity>(source);
        break;
    case scene::EntityKind::Artifact:
        entity = std::make_unique<ArtifactEntity>(source);
        break;
    case scene::EntityKind::ForceField:
        entity = std::make_unique<ForceFieldEntity>(source);
        break;
    case scene::EntityKind::PointModule:
        entity = std::make_unique<PointModuleEntity>(source);
        break;
    case scene::EntityKind::ItemInstance:
        entity = std::make_unique<BasicEntity>(source);
        break;
    case scene::EntityKind::EnemySpawn:
        if (std::holds_alternative<enemy_spawn::FirstHuntData>(
                source.typed_data)) {
            entity = std::make_unique<FhEnemySpawnEntity>(source);
        } else {
            entity = std::make_unique<BasicEntity>(source);
        }
        break;
    case scene::EntityKind::CameraSequence:
    case scene::EntityKind::BeamEffect:
    case scene::EntityKind::Bomb:
    case scene::EntityKind::EnemyInstance:
    case scene::EntityKind::Halfturret:
    case scene::EntityKind::Player:
    case scene::EntityKind::BeamProjectile:
        // These records are also represented by the dedicated gameplay or
        // camera runtime. Keep them in the fixed world so all room records
        // remain addressable by ID without duplicating their simulation.
        entity = std::make_unique<BasicEntity>(source);
        break;
    case scene::EntityKind::Unknown:
        break;
    }
    if (entity == nullptr) {
        return;
    }
    switch (source.kind) {
    case scene::EntityKind::Platform:
        entity->set_parent_id(data_or_default<scene::PlatformData>(source).parent_id);
        break;
    case scene::EntityKind::Object:
        entity->set_parent_id(data_or_default<scene::ObjectData>(source).linked_entity);
        break;
    case scene::EntityKind::ItemSpawn:
        entity->set_parent_id(data_or_default<scene::ItemSpawnData>(source).parent_id);
        break;
    case scene::EntityKind::JumpPad:
        entity->set_parent_id(data_or_default<scene::JumpPadData>(source).parent_id);
        break;
    case scene::EntityKind::Artifact:
        entity->set_parent_id(data_or_default<scene::ArtifactData>(source).linked_entity_id);
        break;
    case scene::EntityKind::PointModule:
        entity->set_parent_id(
            data_or_default<scene::PointModuleData>(source).previous_id);
        break;
    default:
        break;
    }
    entities_.push_back(std::move(entity));
}

} // namespace fruityprime::entities::static_entities
