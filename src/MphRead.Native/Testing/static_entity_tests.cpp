#include "Entities/static_entities.hpp"
#include "GameState.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

fruityprime::formats::Vector3Fx fixed_position(float x, float y, float z) {
    return {{fruityprime::formats::Fixed::to_int(x)},
            {fruityprime::formats::Fixed::to_int(y)},
            {fruityprime::formats::Fixed::to_int(z)}};
}

fruityprime::scene::EntityInstance source(
    fruityprime::scene::EntityKind kind, std::int16_t id) {
    const auto position = fixed_position(0.0F, 0.0F, 0.0F);
    return {static_cast<std::uint16_t>(kind), kind, id, 0, "",
            position, fixed_position(0.0F, 1.0F, 0.0F),
            fixed_position(0.0F, 0.0F, 1.0F), {}, {}};
}

fruityprime::raw::FhRawCollisionVolume fh_sphere(
    float x, float y, float z, float radius) {
    fruityprime::raw::FhRawCollisionVolume volume{};
    volume.type = fruityprime::formats::FhVolumeType::Sphere;
    volume.data.sphere.position = fixed_position(x, y, z);
    volume.data.sphere.radius = {
        fruityprime::formats::Fixed::to_int(radius)};
    return volume;
}

void require(bool value, const std::string& message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

fruityprime::messaging::MessageInfo message(
    fruityprime::messaging::Message kind, std::int32_t target) {
    fruityprime::messaging::MessageInfo result;
    result.message = kind;
    result.target = target;
    return result;
}

} // namespace

int main() {
    try {
        auto platform = source(fruityprime::scene::EntityKind::Platform, 10);
        fruityprime::scene::PlatformData platform_data;
        platform_data.active = true;
        platform_data.forward_speed = 2.0F;
        platform_data.flags = 0x0002'0000u | 0x0020'0000u;
        platform_data.position_count = 2;
        platform_data.positions[0] = {0.0F, 0.0F, 0.0F};
        platform_data.positions[1] = {1.0F, 0.0F, 0.0F};
        platform.typed_data = platform_data;

        auto child_object = source(fruityprime::scene::EntityKind::Object, 17);
        child_object.position = fixed_position(2.0F, 0.0F, 0.0F);
        fruityprime::scene::ObjectData child_object_data;
        child_object_data.linked_entity = 10;
        child_object_data.volume.kind = fruityprime::scene::VolumeKind::Sphere;
        child_object_data.volume.sphere_position = {2.0F, 0.0F, 0.0F};
        child_object_data.volume.sphere_radius = 1.0F;
        child_object.typed_data = child_object_data;

        auto player_spawn = source(
            fruityprime::scene::EntityKind::PlayerSpawn, 18);
        player_spawn.typed_data = fruityprime::scene::PlayerSpawnData{
            1, false, 0};

        auto item_spawn = source(fruityprime::scene::EntityKind::ItemSpawn, 19);
        fruityprime::scene::ItemSpawnData item_spawn_data;
        item_spawn_data.parent_id = 10;
        item_spawn_data.enabled = true;
        item_spawn_data.max_spawn_count = 1;
        item_spawn.typed_data = item_spawn_data;

        auto area = source(fruityprime::scene::EntityKind::AreaVolume, 11);
        fruityprime::scene::AreaVolumeData area_data;
        area_data.active = true;
        area_data.volume.kind = fruityprime::scene::VolumeKind::Sphere;
        area_data.volume.sphere_position = {0.0F, 0.0F, 0.0F};
        area_data.volume.sphere_radius = 2.0F;
        area.typed_data = area_data;

        auto jump_pad = source(fruityprime::scene::EntityKind::JumpPad, 12);
        fruityprime::scene::JumpPadData jump_data;
        jump_data.active = true;
        jump_data.cooldown_time = 3;
        jump_data.parent_id = 10;
        jump_data.volume = area_data.volume;
        jump_pad.typed_data = jump_data;

        auto door = source(fruityprime::scene::EntityKind::Door, 13);
        fruityprime::scene::DoorData door_data;
        door_data.locked = false;
        door.typed_data = door_data;

        auto artifact = source(fruityprime::scene::EntityKind::Artifact, 14);
        fruityprime::scene::ArtifactData artifact_data;
        artifact_data.active = true;
        artifact_data.model_id = 2;
        artifact_data.artifact_id = 1;
        artifact.typed_data = artifact_data;

        auto teleporter = source(fruityprime::scene::EntityKind::Teleporter, 15);
        fruityprime::scene::TeleporterData teleporter_data;
        teleporter_data.active = true;
        teleporter.typed_data = teleporter_data;

        auto force_field = source(
            fruityprime::scene::EntityKind::ForceField, 23);
        force_field.position = fixed_position(2.0F, 3.0F, 4.0F);
        fruityprime::scene::ForceFieldData force_field_data;
        force_field_data.type = 4;
        force_field_data.width = 5.0F;
        force_field_data.height = 6.0F;
        force_field_data.active = false;
        force_field.typed_data = force_field_data;

        auto light = source(fruityprime::scene::EntityKind::LightSource, 24);
        fruityprime::scene::LightSourceData light_data;
        light_data.volume = area_data.volume;
        light_data.light1_enabled = true;
        light_data.light1_color = {255, 128, 0};
        light_data.light1_vector = {1.0F, 2.0F, 3.0F};
        light_data.light2_enabled = false;
        light_data.light2_color = {0, 64, 255};
        light_data.light2_vector = {4.0F, 5.0F, 6.0F};
        light.typed_data = light_data;

        auto flag_base = source(fruityprime::scene::EntityKind::FlagBase, 25);
        fruityprime::scene::FlagBaseData flag_base_data;
        flag_base_data.team_id = 3;
        flag_base_data.volume = area_data.volume;
        flag_base.typed_data = flag_base_data;

        auto fh_platform = source(
            fruityprime::scene::EntityKind::Platform, 60);
        fruityprime::scene::FhPlatformData fh_platform_data;
        fh_platform_data.position_count = 2;
        fh_platform_data.positions[0] = {0.0F, 0.0F, 0.0F};
        fh_platform_data.positions[1] = {2.0F, 0.0F, 0.0F};
        fh_platform_data.speed = 1.0F;
        fh_platform.typed_data = fh_platform_data;

        auto fh_door = source(fruityprime::scene::EntityKind::Door, 61);
        fh_door.typed_data = fruityprime::scene::FhDoorData{
            "unit1", 1, 2};
        auto fh_item_spawn = source(
            fruityprime::scene::EntityKind::ItemSpawn, 62);
        fh_item_spawn.typed_data = fruityprime::scene::FhItemSpawnData{
            fruityprime::formats::FhItemType::Missile, 1, 30, 0};
        auto fh_trigger = source(
            fruityprime::scene::EntityKind::TriggerVolume, 63);
        fruityprime::scene::FhTriggerVolumeData fh_trigger_data;
        fh_trigger_data.subtype = fruityprime::formats::FhTriggerType::Sphere;
        fh_trigger_data.sphere = fh_sphere(1.0F, 0.0F, 0.0F, 2.0F);
        fh_trigger_data.parent_id = 61;
        fh_trigger.typed_data = fh_trigger_data;
        auto fh_area = source(
            fruityprime::scene::EntityKind::AreaVolume, 64);
        fh_area.position = fixed_position(10.0F, 0.0F, 0.0F);
        fruityprime::scene::FhAreaVolumeData fh_area_data;
        fh_area_data.subtype = fruityprime::formats::FhTriggerType::Sphere;
        fh_area_data.sphere = fh_sphere(1.0F, 0.0F, 0.0F, 2.0F);
        fh_area.typed_data = fh_area_data;
        auto fh_jump = source(fruityprime::scene::EntityKind::JumpPad, 65);
        fruityprime::scene::FhJumpPadData fh_jump_data;
        fh_jump_data.volume_type = fruityprime::formats::FhTriggerType::Sphere;
        fh_jump_data.sphere = fh_sphere(0.0F, 0.0F, 0.0F, 3.0F);
        fh_jump_data.speed = 4.0F;
        fh_jump.typed_data = fh_jump_data;
        auto fh_morph = source(
            fruityprime::scene::EntityKind::MorphCamera, 66);
        fh_morph.typed_data = fruityprime::scene::FhMorphCameraData{
            fh_sphere(0.0F, 0.0F, 0.0F, 4.0F)};
        auto fh_enemy = source(
            fruityprime::scene::EntityKind::EnemySpawn, 67);
        fruityprime::enemy_spawn::FirstHuntData fh_enemy_data;
        fh_enemy_data.enemy_type = fruityprime::formats::FhEnemyType::Zoomer;
        fh_enemy.typed_data = fh_enemy_data;

        auto relay_parent = source(fruityprime::scene::EntityKind::Door, 20);
        relay_parent.typed_data = door_data;
        auto relay_child = source(fruityprime::scene::EntityKind::Door, 21);
        relay_child.typed_data = door_data;
        auto relay = source(fruityprime::scene::EntityKind::TriggerVolume, 22);
        fruityprime::scene::TriggerVolumeData relay_data;
        relay_data.subtype = 2; // TriggerType.Relay
        relay_data.parent_id = 20;
        relay_data.child_id = 21;
        relay.typed_data = relay_data;

        auto unknown_payload = source(
            fruityprime::scene::EntityKind::CameraSequence, 16);

        std::vector<fruityprime::scene::EntityInstance> point_modules;
        for (std::int16_t id = 50; id <= 56; ++id) {
            auto point = source(
                fruityprime::scene::EntityKind::PointModule, id);
            fruityprime::scene::PointModuleData point_data;
            point_data.previous_id = id == 50 ? 0 : id - 1;
            point_data.next_id = id == 56 ? 0 : id + 1;
            point_data.active = true;
            point.typed_data = point_data;
            point_modules.push_back(point);
        }

        std::vector<fruityprime::scene::EntityInstance> sources{
            platform, child_object, player_spawn, item_spawn, area, jump_pad,
            door, artifact, teleporter, relay_parent, relay_child, relay,
            unknown_payload, force_field, light, flag_base, fh_platform,
            fh_door, fh_item_spawn, fh_trigger, fh_area, fh_jump, fh_morph,
            fh_enemy};
        sources.insert(sources.end(), point_modules.begin(),
                       point_modules.end());
        fruityprime::entities::static_entities::World world(sources);
        require(world.size() == sources.size(),
                "static entity world did not retain every fixed record");
        require(world.count(fruityprime::scene::EntityKind::AreaVolume) == 2,
                "area volume was not constructed");
        require(world.find(11) != nullptr
                    && world.find(11)->kind()
                           == fruityprime::scene::EntityKind::AreaVolume,
                "static entity ID lookup failed");
        require(world.current_point_module() == nullptr,
                "point-module current state initialized too early");
        for (std::int16_t id = 50; id <= 56; ++id) {
            require(!world.find(id)->active(),
                    "point module did not start hidden");
        }

        auto* platform_entity = dynamic_cast<
            fruityprime::entities::static_entities::PlatformEntity*>(
                world.find(10));
        require(platform_entity != nullptr && platform_entity->moving(),
                "platform entity did not start moving");
        fruityprime::game::StorySave story_save;
        story_save.set_room_state(27, 10, 1);
        world.apply_story_save(27, story_save);
        require(!platform_entity->active() && !platform_entity->moving(),
                "persisted inactive platform state was not restored");
        story_save.set_room_state(27, 10, 3);
        world.apply_story_save(27, story_save);
        require(platform_entity->active() && platform_entity->moving(),
                "persisted active platform state was not restored");
        auto* object_entity = dynamic_cast<
            fruityprime::entities::static_entities::ObjectEntity*>(
                world.find(17));
        require(object_entity != nullptr && object_entity->state() == 0,
                "object room state did not initialize from authored flags");
        require(world.dispatch(message(
                    fruityprime::messaging::Message::Activate, 17)) == 1
                    && object_entity->state() == 2
                    && story_save.room_state_value(27, 17) == 2,
                "object activation did not persist its three-state value");
        world.process(0.25F);
        require(world.current_point_module() == world.find(50),
                "point-module chain did not select StartId");
        for (std::int16_t id = 50; id <= 56; ++id) {
            require(world.find(id)->active() == (id <= 54),
                    "point-module initial five-member chain is wrong");
        }
        auto* point52 = dynamic_cast<
            fruityprime::entities::static_entities::PointModuleEntity*>(
                world.find(52));
        require(point52 != nullptr && point52->previous() == world.find(51)
                    && point52->next() == world.find(53),
                "point-module links were not initialized");
        point52->set_current();
        require(world.current_point_module() == point52,
                "PointModuleEntity.SetCurrent did not update Current");
        for (std::int16_t id = 50; id <= 56; ++id) {
            require(world.find(id)->active() == (id >= 52),
                    "point-module replacement chain is wrong");
        }
        require(std::fabs(platform_entity->position().x - 0.5F) < 0.01F,
                "platform path interpolation was incorrect");
        const auto* child_entity = world.find(17);
        require(child_entity != nullptr
                    && std::fabs(child_entity->position().x - 2.5F) < 0.01F,
                "child entity did not follow its moving parent");
        const auto* item_spawn_entity = dynamic_cast<
            fruityprime::entities::static_entities::ItemSpawnEntity*>(
                world.find(19));
        require(item_spawn_entity != nullptr
                    && item_spawn_entity->item_present()
                    && std::fabs(item_spawn_entity->position().x - 0.5F)
                           < 0.01F,
                "item spawn did not follow its parent or create its item");
        const auto* jump_volume = dynamic_cast<
            fruityprime::entities::static_entities::VolumeEntity*>(
                world.find(12));
        require(jump_volume != nullptr
                    && std::fabs(jump_volume->volume().center().x - 0.5F)
                           < 0.01F,
                "child volume did not follow its moving parent");

        auto* area_entity = dynamic_cast<
            fruityprime::entities::static_entities::VolumeEntity*>(
                world.find(11));
        require(area_entity != nullptr
                    && area_entity->contains({1.5F, 0.0F, 0.0F})
                    && !area_entity->contains({2.5F, 0.0F, 0.0F}),
                "volume entity containment was incorrect");

        auto* jump_entity = dynamic_cast<
            fruityprime::entities::static_entities::JumpPadEntity*>(
                world.find(12));
        require(jump_entity != nullptr && jump_entity->ready(),
                "jump pad was not ready initially");
        jump_entity->trigger();
        require(!jump_entity->ready() && jump_entity->cooldown_ticks() == 3,
                "jump pad trigger did not arm its cooldown");
        world.process(1.0F / 60.0F);
        require(jump_entity->cooldown_ticks() == 2,
                "jump pad cooldown did not tick");

        auto* door_entity = dynamic_cast<
            fruityprime::entities::static_entities::DoorEntity*>(
                world.find(13));
        door_entity->handle_message(message(
            fruityprime::messaging::Message::Open, 13));
        world.process(0.125F);
        require(door_entity->open_fraction() > 0.45F,
                "door open message did not animate the door");
        door_entity->handle_message(message(
            fruityprime::messaging::Message::Close, 13));
        world.process(0.25F);
        require(door_entity->open_fraction() == 0.0F,
                "door close message did not close the door");

        auto* artifact_entity = dynamic_cast<
            fruityprime::entities::static_entities::ArtifactEntity*>(
                world.find(14));
        require(artifact_entity->model_id() == 2
                    && artifact_entity->artifact_id() == 1
                    && artifact_entity->scan_id() == 41,
                "artifact identity/scan properties did not match managed values");
        artifact_entity->collect();
        static_cast<void>(artifact_entity->process(1.0F / 60.0F));
        require(artifact_entity->collected() && !artifact_entity->active(),
                "artifact collection state was not retained");
        require(artifact_entity->scan_id() == 0,
                "inactive artifact retained its managed scan target");

        auto* teleporter_entity = dynamic_cast<
            fruityprime::entities::static_entities::TeleporterEntity*>(
                world.find(15));
        teleporter_entity->trigger();
        require(!teleporter_entity->ready(),
                "teleporter trigger did not arm its cooldown");

        auto* force_field_entity = dynamic_cast<
            fruityprime::entities::static_entities::ForceFieldEntity*>(
                world.find(23));
        require(force_field_entity != nullptr && !force_field_entity->active()
                    && force_field_entity->alpha() == 0.0F
                    && force_field_entity->width() == 5.0F
                    && force_field_entity->height() == 6.0F
                    && force_field_entity->scan_id() == 0
                    && std::fabs(force_field_entity->plane_distance() - 4.0F)
                           < 0.001F
                    && std::fabs(force_field_entity->field_right().x - 1.0F)
                           < 0.001F,
                "force field construction did not match managed state");
        fruityprime::messaging::MessageInfo lock_message;
        lock_message.cartridge_message = 17;
        force_field_entity->handle_message(lock_message);
        static_cast<void>(force_field_entity->process(1.0F / 60.0F));
        require(force_field_entity->active()
                    && std::fabs(force_field_entity->alpha() - 1.0F / 62.0F)
                           < 0.00001F
                    && force_field_entity->scan_id() == 290,
                "force field lock/fade-in did not match managed behavior");
        fruityprime::messaging::MessageInfo unlock_message;
        unlock_message.cartridge_message = 16;
        force_field_entity->handle_message(unlock_message);
        static_cast<void>(force_field_entity->process(1.0F / 60.0F));
        require(!force_field_entity->active()
                    && force_field_entity->alpha() == 0.0F
                    && force_field_entity->scan_id() == 0,
                "force field unlock/fade-out did not match managed behavior");

        const auto* light_entity = dynamic_cast<const
            fruityprime::entities::static_entities::LightSourceEntity*>(
                world.find(24));
        require(light_entity != nullptr,
                "light source entity was not constructed");
        const auto light1_color = light_entity->light1_color();
        const auto light2_color = light_entity->light2_color();
        require(light_entity->light1_enabled()
                    && !light_entity->light2_enabled()
                    && light_entity->light1_vector().y == 2.0F
                    && light_entity->light2_vector().z == 6.0F
                    && light1_color.x == 1.0F
                    && std::fabs(light1_color.y - 128.0F / 255.0F) < 0.00001F
                    && light2_color.z == 1.0F,
                "light source properties did not match managed values");
        const auto* flag_base_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FlagBaseEntity*>(
                world.find(25));
        require(flag_base_entity != nullptr
                    && flag_base_entity->team_id() == 3
                    && flag_base_entity->data().team_id == 3,
                "flag base did not retain its managed Data property");

        auto* fh_platform_entity = dynamic_cast<
            fruityprime::entities::static_entities::FhPlatformEntity*>(
                world.find(60));
        const auto* fh_door_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FhDoorEntity*>(
                world.find(61));
        const auto* fh_item_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FhItemSpawnEntity*>(
                world.find(62));
        const auto* fh_trigger_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FhTriggerVolumeEntity*>(
                world.find(63));
        const auto* fh_area_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FhAreaVolumeEntity*>(
                world.find(64));
        const auto* fh_jump_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FhJumpPadEntity*>(
                world.find(65));
        const auto* fh_morph_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FhMorphCameraEntity*>(
                world.find(66));
        const auto* fh_enemy_entity = dynamic_cast<const
            fruityprime::entities::static_entities::FhEnemySpawnEntity*>(
                world.find(67));
        require(fh_platform_entity != nullptr && fh_door_entity != nullptr
                    && fh_item_entity != nullptr && fh_trigger_entity != nullptr
                    && fh_area_entity != nullptr && fh_jump_entity != nullptr
                    && fh_morph_entity != nullptr && fh_enemy_entity != nullptr,
                "First Hunt payload was routed into an MPH entity class");
        require(fh_door_entity->data().model_id == 2
                    && fh_item_entity->spawned()
                    && fh_trigger_entity->parent_target() == 61
                    && fh_area_entity->contains({11.0F, 0.0F, 0.0F})
                    && fh_jump_entity->data().speed == 4.0F
                    && fh_morph_entity->contains({3.5F, 0.0F, 0.0F})
                    && fh_enemy_entity->data().enemy_type
                        == fruityprime::formats::FhEnemyType::Zoomer,
                "First Hunt entity data or moved volume was not retained");
        const float fh_platform_before = fh_platform_entity->position().x;
        static_cast<void>(fh_platform_entity->process(1.0F / 60.0F));
        require(std::fabs(fh_platform_entity->position().x
                              - fh_platform_before - 0.5F) < 0.001F,
                "First Hunt platform did not apply its managed per-frame speed");

        std::vector<fruityprime::messaging::MessageInfo> relayed;
        world.set_message_sink([&](const auto& info) {
            relayed.push_back(info);
        });
        auto relay_message = message(
            fruityprime::messaging::Message::Activate, 22);
        relay_message.sender = 7;
        relay_message.execute_frame = 12;
        require(world.dispatch(relay_message) == 1 && relayed.size() == 2,
                "relay volume did not forward to both linked entities");
        require(relayed[0].target == 20 && relayed[1].target == 21
                    && relayed[0].sender == 7 && relayed[1].sender == 7
                    && relayed[0].execute_frame == 13
                    && relayed[1].execute_frame == 13,
                "relay message target or delay was not preserved");

        const auto stop = message(fruityprime::messaging::Message::Stop, 10);
        require(world.dispatch(stop) == 1 && !platform_entity->moving(),
                "targeted message dispatched to the wrong entity set");
        require(story_save.room_state_value(27, 10) == 0,
                "platform deactivation did not persist its room state");
        require(world.dispatch(message(
                    fruityprime::messaging::Message::Deactivate, -1))
                    == world.size(),
                "broadcast static-entity message was not dispatched");
        require(!area_entity->active() && !teleporter_entity->active(),
                "broadcast deactivation did not update volume/entity state");

        std::cout << "static entity tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
