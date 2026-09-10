// Native counterpart of src/MphRead/Entities/EnemyInstanceEntity.cs.
#include <memory>
#include "Entities/gameplay_helpers.hpp"
#include "Formats/collision_runtime.hpp"
#include "Entities/Enemies/enemy_scene.hpp"
#include "Entities/gameplay.hpp"
#include <span>
#include <array>
#include <limits>
#include <cmath>
#include "Entities/runtime_entities.hpp"
#include "runtime_entity_helpers.hpp"

namespace fruityprime::runtime {

using namespace detail;

EnemyInstanceEntity::EnemyInstanceEntity(std::uint32_t id,
                                         std::uint8_t enemy_type,
                                         net::Vec3 position) noexcept
    : Entity(id, Kind::EnemyInstance, position), enemy_type_(enemy_type) {}

void EnemyInstanceEntity::set_states(std::uint8_t state_a,
                                     std::uint8_t state_b) noexcept {
    state_a_ = state_a;
    state_b_ = state_b;
    sub_id_ = state_a;
}

EnemyInstanceEntity::ContactDamage
EnemyInstanceEntity::ContactDamagePlayer(
    const std::size_t slot, const net::Vec3 player_position,
    const std::uint32_t damage, const bool knockback) const noexcept {
    ContactDamage contact;
    if (!hit_player(slot)) {
        // Contact is dealt once per player per frame, however many times
        // the volumes overlapped during it.
        return contact;
    }
    contact.hit = true;
    contact.damage = damage;
    contact.direction = velocity_;
    if (!knockback) {
        return contact;
    }
    const float x = player_position.x - position_.x;
    const float y = player_position.y - position_.y;
    const float z = player_position.z - position_.z;
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length <= 0.0F) {
        // Standing exactly on it: there is no direction to be pushed in.
        return contact;
    }
    // Divided by five times the distance, so the shove is strongest at
    // point blank and fades as the player is already moving away -- the
    // opposite of a normalised push, and deliberately so.
    const float factor = length * 5.0F;
    contact.knockback = {x / factor, 0.0F, z / factor};
    return contact;
}

EnemyInstanceEntity::EnemySfx EnemyInstanceEntity::PlayEnemySfx(
    const std::int32_t sfx) noexcept {
    EnemySfx result;
    if (sfx == -1) {
        return result;
    }
    result.play = true;
    if ((sfx & 0x20000) != 0) {
        // Never start a second copy while one is playing, and play it on
        // the enemy rather than around it.
        result.recency = std::numeric_limits<float>::max();
        result.source_only = true;
    } else if ((sfx & 0x80000) != 0) {
        // Restart it every time.
        result.recency = 0.0F;
    }
    result.sfx = sfx & ~0xA0000;
    return result;
}

bool EnemyInstanceEntity::SeekTargetFacing(const net::Vec3 target,
                                           const net::Vec3 up,
                                           std::uint16_t& steps,
                                           const float angle) noexcept {
    constexpr float Pi = 3.14159265358979323846F;
    const float radians = angle * Pi / 180.0F;
    const float dot = target.x * facing_.x + target.y * facing_.y
        + target.z * facing_.z;
    bool finished = false;
    if (steps > 0 && dot < std::cos(radians)) {
        // Which way round is decided by the cross product's up component:
        // the shorter way, which is the only one that looks deliberate.
        const float cross_y = target.z * facing_.x - target.x * facing_.z;
        const float turn = radians * (cross_y <= 0.0F ? 1.0F : -1.0F);
        const float sine = std::sin(turn);
        const float cosine = std::cos(turn);
        const net::Vec3 turned{facing_.x * cosine + facing_.z * sine,
                               facing_.y,
                               -facing_.x * sine + facing_.z * cosine};
        const float length = std::sqrt(turned.x * turned.x
                                       + turned.y * turned.y
                                       + turned.z * turned.z);
        facing_ = length > 0.0F
            ? net::Vec3{turned.x / length, turned.y / length,
                        turned.z / length}
            : facing_;
        --steps;
    } else {
        // Out of patience, or close enough: snap to it.
        facing_ = target;
        finished = true;
    }
    up_ = up;
    return finished;
}

void EnemyInstanceEntity::UpdateHurtVolume() noexcept {
    // The managed alternative transforms the volume by the enemy's whole
    // matrix.  This head keeps facing and up rather than a matrix, and no
    // ported enemy needs the turning form yet, so the volume is carried
    // rather than turned and that is said here rather than approximated.
    hurt_volume_ = hurt_volume_init_.moved(
        {position_.x, position_.y, position_.z});
}

bool EnemyInstanceEntity::CallSubroutine(
    const std::span<const metadata::EnemySubroutine> subroutines,
    const std::function<bool(std::uint8_t)>& behavior) noexcept {
    return metadata::call_subroutine(subroutines, sub_id_, state_b_,
                                     behavior);
}

bool EnemyInstanceEntity::take_damage(std::uint32_t damage) noexcept {
    if (!active_ || (flags_ & Invincible) != 0 || damage == 0) {
        return false;
    }
    const auto amount = std::min<std::uint32_t>(damage, health_);
    health_ = static_cast<std::uint16_t>(health_ - amount);
    if (health_ == 0) {
        active_ = false;
    }
    return amount != 0;
}

void EnemyInstanceEntity::handle_message(
    const messaging::MessageInfo& message) noexcept {
    if (cartridge_message(message, 7)) { // Damage
        const std::uint32_t amount = message.parameter1 > 0
            ? static_cast<std::uint32_t>(message.parameter1) : 1u;
        static_cast<void>(take_damage(amount));
        return;
    }
    if (cartridge_message(message, 21) // Death
        || cartridge_message(message, 6)) { // Destroyed
        health_ = 0;
        active_ = false;
        return;
    }
    Entity::handle_message(message);
}

bool EnemyInstanceEntity::process(float seconds) noexcept {
    if (!active_) {
        return false;
    }
    seconds = finite_seconds(seconds);
    advance_age(seconds);
    if ((flags_ & Static) == 0) {
        position_ = add(position_, multiply(velocity_, seconds));
    }
    return active_;
}

} // namespace fruityprime::runtime

namespace fruityprime::gameplay {

EnemyScene::Blocking HandleBlockingCollision(
    EnemyState& agent, const net::Vec3 prev_position,
    const scene::EntityVolume& volume, const bool update_speed,
    const std::function<std::size_t(net::Vec3, float,
                                    std::span<collision::Result>)>&
        check_in_radius,
    const std::function<std::size_t(net::Vec3, net::Vec3, float,
                                    std::span<collision::Result>)>&
        sphere_between_points) {
    EnemyScene::Blocking blocking;
    // Thirty is the cartridge's own limit and is not a tuning knob: an
    // enemy in a corner touches several faces at once and each one has to
    // push it out, but a stack this size is what the game allocates.
    std::array<collision::Result, 30> results{};
    std::size_t count = 0;
    // A cylinder is swept from where the enemy was to where it is, half a
    // unit up; anything else is asked about where it stands.  The
    // difference matters for something moving fast enough to pass through
    // a wall between one frame and the next.
    net::Vec3 point_two{};
    const bool cylinder = volume.kind == scene::VolumeKind::Cylinder;
    if (cylinder) {
        const net::Vec3 point_one{prev_position.x, prev_position.y + 0.5F,
                                  prev_position.z};
        point_two = {agent.position.x, agent.position.y + 0.5F,
                     agent.position.z};
        if (sphere_between_points) {
            count = sphere_between_points(point_one, point_two,
                                          volume.cylinder_radius, results);
        }
    } else if (check_in_radius) {
        count = check_in_radius(agent.position, agent.body_radius, results);
    }
    blocking.with_ground = false;
    if (count == 0) {
        return blocking;
    }
    for (std::size_t index = 0; index < count && index < results.size();
         ++index) {
        const collision::Result& result = results[index];
        const net::Vec3 normal{result.plane.x, result.plane.y, result.plane.z};
        float depth;
        if (result.field0 != 0) {
            depth = agent.body_radius - result.field14;
        } else if (cylinder) {
            depth = agent.body_radius + result.plane.w
                - (point_two.x * normal.x + point_two.y * normal.y
                   + point_two.z * normal.z);
        } else {
            depth = agent.body_radius + result.plane.w
                - (agent.position.x * normal.x + agent.position.y * normal.y
                   + agent.position.z * normal.z);
        }
        if (depth <= 0.0F) {
            continue;
        }
        // A face is ground or wall by how far from level it is, and a
        // tenth is the cartridge's line.  Nothing here asks which way up
        // the enemy is, so a Zoomer on a ceiling reads its ceiling as
        // ground -- which is what the game does too.
        if (result.plane.y < 0.1F && result.plane.y > -0.1F) {
            blocking.with_wall = true;
        } else {
            blocking.with_ground = true;
        }
        agent.position = {agent.position.x + normal.x * depth,
                          agent.position.y + normal.y * depth,
                          agent.position.z + normal.z * depth};
        if (update_speed) {
            const float along = agent.velocity.x * normal.x
                + agent.velocity.y * normal.y + agent.velocity.z * normal.z;
            if (along < 0.0F) {
                agent.velocity = {agent.velocity.x + normal.x * -along,
                                  agent.velocity.y + normal.y * -along,
                                  agent.velocity.z + normal.z * -along};
            }
        }
    }
    blocking.any = true;
    return blocking;
}


bool SeekTargetFacing(EnemyState& agent, const net::Vec3 target,
                      std::uint16_t& steps, const float angle) {
    constexpr float Radians = 3.14159265358979323846F / 180.0F;
    const float radians = angle * Radians;
    const float facing_dot = target.x * agent.facing.x
        + target.y * agent.facing.y + target.z * agent.facing.z;
    if (steps == 0 || facing_dot >= std::cos(radians)) {
        agent.facing = target;
        return true;
    }
    // The shorter way round, which is the only one that reads as
    // deliberate.
    const float cross_y = target.z * agent.facing.x
        - target.x * agent.facing.z;
    const float turn = radians * (cross_y <= 0.0F ? 1.0F : -1.0F);
    const float sine = std::sin(turn);
    const float cosine = std::cos(turn);
    const net::Vec3 turned{
        agent.facing.x * cosine + agent.facing.z * sine,
        agent.facing.y,
        -agent.facing.x * sine + agent.facing.z * cosine};
    const float length = std::sqrt(turned.x * turned.x + turned.y * turned.y
                                   + turned.z * turned.z);
    if (length > 0.0F) {
        agent.facing = {turned.x / length, turned.y / length,
                        turned.z / length};
    }
    --steps;
    return false;
}

std::span<const collision::Instance* const> Session::room_collision_parts() {
    const collision::File* file = &room_.collision();
    if (room_collision_source_ != file) {
        room_collision_info_ = std::make_unique<collision::Info>(
            collision::Info::from_file(*file));
        room_collision_instance_ = collision::Instance{
            "room", true, room_collision_info_.get(), false, {}, {}};
        room_collision_list_[0] = &room_collision_instance_;
        room_collision_source_ = file;
    }
    return {room_collision_list_.data(), room_collision_list_.size()};
}

EnemyScene Session::build_enemy_scene(const net::Vec3 prev_position) {
    const auto check_in_radius =
        [this](const net::Vec3 position, const float radius,
               std::span<collision::Result> results) {
            return collision::check_in_radius(
                room_collision_parts(),
                formats::Vector3{position.x, position.y, position.z}, radius,
                false, collision::TestFlags::None, results);
        };
    const auto sphere_between =
        [this](const net::Vec3 from, const net::Vec3 to, const float radius,
               std::span<collision::Result> results) {
            return collision::check_sphere_between_points(
                room_collision_parts(),
                formats::Vector3{from.x, from.y, from.z},
                formats::Vector3{to.x, to.y, to.z}, radius, false,
                collision::TestFlags::None, results);
        };

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
    scene.NearbyKin = [this](const EnemyState& self, const std::uint8_t kind,
                             net::Vec3& away) {
        for (const auto& other : enemies_) {
            if (other.id == self.id || other.enemy_type != kind
                || other.health == 0) {
                continue;
            }
            const float reach = self.body_radius + other.body_radius;
            if (detail::distance_squared(self.position, other.position)
                    > reach * reach) {
                continue;
            }
            away = {self.position.x - other.position.x,
                    self.position.y - other.position.y,
                    self.position.z - other.position.z};
            return true;
        }
        return false;
    };
    scene.CheckInRadius = check_in_radius;
    scene.BlockingCollision = [prev_position, check_in_radius, sphere_between](
                                  EnemyState& agent,
                                  const scene::EntityVolume& volume,
                                  const bool update_speed) {
        return HandleBlockingCollision(agent, prev_position, volume,
                                       update_speed, check_in_radius,
                                       sphere_between);
    };
    scene.Blocked = [this](net::Vec3 from, net::Vec3 to, float radius) {
        return collision::sweep_sphere(
                   room_.collision(),
                   collision::Vec3{from.x, from.y, from.z},
                   collision::Vec3{to.x, to.y, to.z}, radius, 0x2000)
            .has_value();
    };
    scene.CameraShake = [](float) {
        // The camera belongs to the player's own view, which the gameplay
        // session does not own.  Said here rather than shaken quietly.
    };
    scene.SeekFacing = [](EnemyState& agent, net::Vec3 desired,
                          std::uint16_t& steps, float angle) {
        return SeekTargetFacing(agent, desired, steps, angle);
    };
    return scene;
}

} // namespace fruityprime::gameplay


