#include "enemy_decode_common.hpp"
#include "Metadata/entity_metadata.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/51_CarnivorousPlant.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "51_CarnivorousPlant.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_carnivorous_plant(EnemyState& agent) {
    if (!agent.carnivorous_plant.supported) {
        static_cast<void>(update_generic_enemy(agent));
        return;
    }

    // Enemy51Entity is a static, no-max-distance object. Its EnemyProcess only
    // calls ContactDamagePlayer against the transformed S07 hurt sphere; it
    // has no target acquisition, movement, or generic attack state.
    const float seconds = config_.tick_seconds;
    agent.velocity = {};
    agent.state = 0;
    agent.target_slot = 0xff;
    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);

    const net::Vec3 hurt_center = to_net(
        agent.carnivorous_plant.hurt_volume.center());
    const float radius = agent.carnivorous_plant.hurt_volume.sphere_radius
        + config_.body_radius;
    for (auto& player : players_) {
        if (distance_squared(player.position, hurt_center) > radius * radius
            || agent.attack_timer > 0.0F) {
            continue;
        }
        apply_enemy_contact_damage(agent, player,
                                   agent.carnivorous_plant.damage);
        // The managed collision path can hit again on the next frame. Keep a
        // small native guard only for the multi-player loop in one tick.
        agent.attack_timer = 0.0F;
    }
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_51_carnivorous_plant::kModule.managed_class.size() != 0);

namespace fruityprime::enemy {

CarnivorousPlantProfile decode_carnivorous_plant_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    CarnivorousPlantProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::CarnivorousPlant)
        || fields.size() < 72) {
        return result;
    }
    result.health = detail::read_u16(fields, 0);
    result.damage = detail::read_u16(fields, 2);
    result.subtype = detail::read_u32(fields, 4);
    // Enemy51Entity does not read a CollisionVolume from S07. It constructs a
    // fixed local sphere at (0, 409/4096, 0), then transforms it by the
    // spawner transform. Keep that authored detail instead of interpreting the
    // following bytes as a generic volume union.
    result.hurt_volume.kind = scene::VolumeKind::Sphere;
    result.hurt_volume.sphere_position = {
        origin.x, origin.y + 409.0F / 4096.0F, origin.z};
    result.hurt_volume.sphere_radius = 1843.0F / 4096.0F;
    if (const auto* object = metadata::object_info(result.subtype);
        object != nullptr) {
        result.model_name = object->name;
    }
    result.supported = !result.model_name.empty();
    return result;
}

} // namespace fruityprime::enemy
