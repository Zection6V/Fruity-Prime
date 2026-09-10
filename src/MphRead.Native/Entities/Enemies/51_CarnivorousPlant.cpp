#include "enemy_decode_common.hpp"
#include "Metadata/entity_metadata.hpp"
// Native counterpart of src/MphRead/Entities/Enemies/51_CarnivorousPlant.cs.
// This file is deliberately present even when the managed class currently
// shares a native controller; the descriptor and entry point prevent a
// many-classes-in-one gameplay.cpp regression.
#include "51_CarnivorousPlant.hpp"
#include "enemy_common.hpp"
#include "enemy_scene.hpp"

namespace fruityprime::gameplay {

namespace {

// Native counterpart of Enemy51Entity.  A plant does not move, does not
// look for anybody, and has no states: it stands where it was authored and
// hurts whatever walks into it.
class Enemy51Entity final {
public:
    Enemy51Entity(const EnemyScene& scene, EnemyState& agent,
                  std::span<const net::PlayerState> players) noexcept
        : scene_(scene), agent_(agent), players_(players) {}

    // Enemy51Entity.EnemyProcess: ContactDamagePlayer and nothing else.
    void EnemyProcess() {
        const net::Vec3 center = to_net(
            agent_.carnivorous_plant.hurt_volume.center());
        const float radius =
            agent_.carnivorous_plant.hurt_volume.sphere_radius
            + PlayerBodyRadius;
        for (const auto& player : players_) {
            if (distance_squared(player.position, center)
                    > radius * radius) {
                continue;
            }
            if (scene_.ContactDamage) {
                scene_.ContactDamage(agent_, player.slot_index,
                                     agent_.carnivorous_plant.damage);
            }
        }
    }

private:
    // A player's own body, which the overlap counts.
    static constexpr float PlayerBodyRadius = 0.45F;

    const EnemyScene& scene_;
    EnemyState& agent_;
    std::span<const net::PlayerState> players_;
};

} // namespace

void Session::update_carnivorous_plant(EnemyState& agent) {
    if (!agent.carnivorous_plant.supported) {
        static_cast<void>(update_generic_enemy(agent));
        return;
    }
    agent.velocity = {};
    agent.state = 0;
    agent.target_slot = 0xff;
    EnemyScene scene;
    scene.ContactDamage = [this](EnemyState& target, std::uint8_t slot,
                                 std::uint32_t damage) {
        for (auto& player : players_) {
            if (player.slot_index == slot) {
                apply_enemy_contact_damage(
                    target, player, static_cast<std::uint16_t>(damage));
                return;
            }
        }
    };
    Enemy51Entity(scene, agent, players_).EnemyProcess();
}
} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_51_carnivorous_plant::kModule.managed_class.size() != 0);

namespace fruityprime::enemy::module_51_carnivorous_plant {

void EnemyInitialize(gameplay::EnemyState& agent) noexcept {
    // A plant's energy and its damage are authored per instance, so both
    // come off the spawner's own fields rather than being the same for
    // every one in the room.
    agent.health = agent.health_max = agent.carnivorous_plant.health;
    agent.body_radius = 1843.0F / 4096.0F;
    agent.velocity = {};
    agent.state = agent.next_state = agent.sub_id = 0;
}

} // namespace fruityprime::enemy::module_51_carnivorous_plant

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
