#include "24_Gorea1A.hpp"
#include "enemy_common.hpp"
#include "gorea_common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

namespace fruityprime::gameplay {

namespace {

constexpr std::uint32_t ArmDead = 1u;
constexpr std::uint32_t ArmShouldFire = 1u << 1;
constexpr std::uint32_t ArmCharged = 1u << 2;
constexpr std::uint32_t ArmHasFired = 1u << 3;
// Behavior04 is an edge-triggered managed subroutine.  These two native
// bits remember that the corresponding shoulder already emitted its kill
// effect/drop; they are cleared when that shoulder is regenerated.
constexpr std::uint32_t ArmDeathProcessedLeft = 1u << 8;
constexpr std::uint32_t ArmDeathProcessedRight = 1u << 9;

[[nodiscard]] constexpr std::uint8_t type_id(formats::EnemyType type) noexcept {
    return static_cast<std::uint8_t>(type);
}

[[nodiscard]] bool is_arm(const EnemyState& enemy) noexcept {
    return enemy.enemy_type == type_id(formats::EnemyType::GoreaArm);
}

[[nodiscard]] bool is_dead_arm(const EnemyState& enemy) noexcept {
    return is_arm(enemy) && (enemy.gorea_flags & ArmDead) != 0;
}

void select_animation(EnemyState& enemy, std::uint8_t state) noexcept {
    // These are the animation indices used by Enemy24Entity.  The native
    // simulation keeps the frame cursor even when a frontend has no model;
    // the frame-gated attacks below therefore retain the managed event order.
    switch (state) {
    case 0:
        gorea::set_animation(enemy, 0, 45);
        break;
    case 1:
        gorea::set_animation(enemy, 17, 30, true);
        break;
    case 2:
        gorea::set_animation(enemy, 25, 30, true);
        break;
    case 3:
        gorea::set_animation(enemy, 23, 30, true);
        break;
    case 5:
        gorea::set_animation(enemy,
                             (enemy.gorea_arm_bits & 1u) == 0 ? 10
                                 : (enemy.gorea_arm_bits & 2u) == 0 ? 9 : 8,
                             36);
        break;
    case 6:
        gorea::set_animation(enemy, 15, 72);
        break;
    case 7:
        gorea::set_animation(enemy, 13, 42);
        break;
    case 8:
        gorea::set_animation(enemy, 6, 78);
        break;
    case 11:
        // Behavior17 switches to animation 19 and State11 waits for its
        // no-loop completion before handing control back to _nextState.
        gorea::set_animation(enemy, 19, 60);
        break;
    case 12:
        gorea::set_animation(enemy, 5, 48);
        break;
    case 13:
        gorea::set_animation(enemy, 22, 45);
        break;
    default:
        gorea::set_animation(enemy, 17, 30, true);
        break;
    }
}

} // namespace

void Session::update_gorea_1a(EnemyState& agent) {
    // This is the native Enemy24Entity state machine.  Gorea1A is not a
    // normal EnemyInstanceEntity: its health is a 0xffff sentinel, its arms
    // and head are child entities, and the encounter changes owner to
    // Enemy28Entity after both arms have been lost.
    if (!agent.active) {
        return;
    }

    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    const float frame_scale = static_cast<float>(step);
    agent.health = 65535;
    agent.health_max = 65535;
    agent.invulnerable = true;
    agent.target_slot = 0xff;
    agent.velocity = {};

    // A managed subroutine writes _state2 for the following frame while
    // leaving the animation it just selected intact. Keep that pending
    // transition separate from gorea_next_state, which the attack callbacks
    // also use as their continuation value.
    if (agent.gorea_state_pending) {
        agent.gorea_previous_state = agent.gorea_state;
        agent.gorea_state = agent.gorea_pending_state;
        agent.gorea_state_pending = false;
        agent.gorea_state_initialized = true;
    }
    if (agent.gorea_arm_bits == 0) {
        agent.gorea_arm_bits = 3;
    }
    if (!agent.gorea_state_initialized) {
        select_animation(agent, agent.gorea_state);
        agent.gorea_state_initialized = true;
    }
    // C# updates the Gorea1A model after its state callback and only on even
    // scene frames. At the start of native tick N, apply the update committed
    // by the preceding even tick so the callback observes the same cursor.
    if (gorea::should_advance_animation(tick_count_)) {
        static_cast<void>(gorea::advance_animation(agent, step));
    }

    std::size_t target_index = gorea::nearest_player(
        players_, agent.position, 45.0F);
    if (target_index != players_.size()) {
        agent.target_slot = players_[target_index].slot_index;
        const net::Vec3 target = add(players_[target_index].position,
                                     {0.0F, 0.5F, 0.0F});
        agent.gorea_target_facing = normalized_or(
            {target.x - agent.position.x, 0.0F,
             target.z - agent.position.z}, agent.facing);
    }

    std::size_t arm_count = 0;
    std::size_t living_arms = 0;
    EnemyState* phase_child = nullptr;
    EnemyState* head_child = nullptr;
    for (auto& child : enemies_) {
        if (child.parent_enemy_id != agent.id) {
            continue;
        }
        if (is_arm(child)) {
            ++arm_count;
            if (!is_dead_arm(child)) {
                ++living_arms;
                agent.gorea_arm_bits |= static_cast<std::uint8_t>(
                    1u << std::min<std::uint8_t>(child.gorea_index, 1));
                child.gorea_activated = agent.visible
                    && agent.gorea_state != 13;
                child.visible = child.gorea_activated;
                child.invulnerable = (agent.gorea_flags
                                      & gorea::Gorea1AArmInvulnerable) != 0;
                child.gorea_weapon_index = agent.gorea_weapon_index;
            } else {
                agent.gorea_arm_bits &= static_cast<std::uint8_t>(
                    ~(1u << std::min<std::uint8_t>(child.gorea_index, 1)));
                const std::uint32_t processed_bit = child.gorea_index == 0
                    ? ArmDeathProcessedLeft : ArmDeathProcessedRight;
                if ((agent.gorea_flags & processed_bit) == 0) {
                    // Enemy24Entity.Behavior04: one shoulder-kill effect and,
                    // while the opposite arm remains alive, one large drop.
                    spawn_effect(45, child.position,
                                 {1.0F, 0.0F, 0.0F}, child.id);
                    const bool other_arm_alive = std::any_of(
                        enemies_.begin(), enemies_.end(),
                        [&child, id = agent.id](const EnemyState& other) {
                            return other.parent_enemy_id == id
                                && is_arm(other)
                                && other.id != child.id
                                && !is_dead_arm(other);
                        });
                    if (other_arm_alive) {
                        const ItemType drop = rng_.random2(100u) >= 80u
                            ? ItemType::UABig : ItemType::HealthBig;
                        spawn_item_drop(drop, child.position);
                    }
                    agent.gorea_flags |= processed_bit;
                }
            }
        } else if (child.enemy_type == type_id(formats::EnemyType::Gorea1B)) {
            phase_child = &child;
        } else if (child.enemy_type == type_id(formats::EnemyType::GoreaHead)) {
            head_child = &child;
        }
    }

    // EnemyInstanceEntity.EnemyProcess does not advance state/animation while
    // the 1A body is hidden. State13 is the one intentional exception: it is
    // the handoff callback that must run once to activate Gorea1B.
    if (!agent.visible && agent.gorea_state != 13) {
        return;
    }

    // Enemy24Entity.Process counts down this timer only while an arm is
    // dead.  When it crosses below zero Behavior20 regenerates all dead arms
    // and restarts the reveal state.
    if (living_arms != arm_count && agent.gorea_field23e >= 0) {
        agent.gorea_field23e -= static_cast<std::int32_t>(step);
    }
    if (living_arms == arm_count && agent.gorea_field240 >= 0) {
        agent.gorea_field240 -= static_cast<std::int32_t>(step);
    }
    if (living_arms != arm_count && agent.gorea_field23e < 0) {
        agent.gorea_field23e = 510 * 2;
        agent.gorea_arm_bits = 3;
        for (auto& child : enemies_) {
            if (!is_arm(child) || child.parent_enemy_id != agent.id
                || !is_dead_arm(child)) {
                continue;
            }
            child.gorea_flags &= ~ArmDead;
            child.gorea_flags &= ~(ArmShouldFire | ArmCharged | ArmHasFired);
            child.gorea_damage = 0;
            child.gorea_state_timer = 60u * 2u;
            child.gorea_damage_timer = 0;
            child.gorea_activated = true;
            child.visible = true;
            child.invulnerable = false;
            agent.gorea_flags &= child.gorea_index == 0
                ? ~ArmDeathProcessedLeft : ~ArmDeathProcessedRight;
        }
        agent.gorea_flags |= gorea::Gorea1AReveal;
        agent.gorea_flags |= gorea::Gorea1AArmInvulnerable;
        agent.gorea_flags |= gorea::Gorea1AHeadFlashPending;
        agent.gorea_state_timer = 30u * 2u;
        gorea::enter_state(agent, 0);
        select_animation(agent, 0);
        return;
    }

    auto enter = [this, &agent](std::uint8_t state) {
        gorea::enter_state(agent, state);
        select_animation(agent, state);
        agent.gorea_state_initialized = true;
        if (state == 5 || state == 6) {
            agent.gorea_ammo = 0;
        }
        if (state == 6) {
            for (auto& child : enemies_) {
                if (is_arm(child) && child.parent_enemy_id == agent.id
                    && !is_dead_arm(child)) {
                    child.gorea_flags |= ArmCharged | ArmShouldFire;
                }
            }
        }
        if (state == 7) {
            agent.gorea_weapon_index = static_cast<std::uint8_t>(
                (agent.gorea_weapon_index + 1) % 6);
            agent.gorea_field240 = static_cast<std::int32_t>(
                (rng_.random2(90u) + 150u) * 2u);
        }
    };

    if (arm_count != 0 && living_arms == 0) {
        agent.gorea_flags |= gorea::Gorea1AArmsDown;
        if (agent.gorea_state != 13) {
            agent.gorea_next_state = 1;
            enter(13);
        }
        agent.visible = false;
        agent.invulnerable = true;
        for (auto& child : enemies_) {
            if (child.parent_enemy_id == agent.id && is_arm(child)) {
                child.visible = false;
                child.gorea_activated = false;
            }
        }
        // Enemy28.Activate is a one-time handoff.  The 1A parent remains in
        // this hidden state for the rest of the phase, so do not reset the
        // already-running 1B child to State00 on every parent tick.
        if (phase_child != nullptr && !phase_child->gorea_activated) {
            phase_child->gorea_activated = true;
            phase_child->visible = true;
            phase_child->invulnerable = true;
            phase_child->gorea_state_timer = 120u * 2u;
            phase_child->gorea_state = 0;
        }
        return;
    }

    const auto respawn_head_flash = [&]() {
        if (head_child == nullptr) {
            return;
        }
        if (head_child->gorea_head_flash_effect_id != 0) {
            detach_effect(head_child->gorea_head_flash_effect_id, false);
            head_child->gorea_head_flash_effect_id = 0;
        }
        constexpr float EyeFlashFacingOffset = 2949.0F / 4096.0F;
        constexpr float EyeFlashUpOffset = -939.0F / 4096.0F;
        const net::Vec3 flash_position = add(
            head_child->position,
            add(multiply(agent.facing, EyeFlashFacingOffset),
                multiply(agent.up, EyeFlashUpOffset)));
        head_child->gorea_head_flash_effect_id = spawn_effect(
            104, flash_position, agent.facing, head_child->id,
            0.25F, false, true, false, agent.up);
    };

    // The C# state 00 has a reveal animation followed by the roar animation;
    // only after both are complete does the subroutine select State01.
    if (agent.gorea_state == 0 && agent.visible) {
        if ((agent.gorea_flags & gorea::Gorea1AHeadFlashPending) != 0) {
            agent.gorea_flags &= ~gorea::Gorea1AHeadFlashPending;
            respawn_head_flash();
        }
        if (agent.gorea_flags & gorea::Gorea1AReveal) {
            agent.gorea_flags &= ~gorea::Gorea1AReveal;
            spawn_effect(175, agent.position, agent.facing, agent.id);
        }
        gorea::decrement(agent.gorea_state_timer, step);
        if (agent.gorea_state_timer != 0 || !gorea::animation_ended(agent)) {
            return;
        }
        if (agent.gorea_model_animation == 0) {
            gorea::set_animation(agent, 22, 45);
            return;
        }
        agent.gorea_flags &= ~gorea::Gorea1AArmInvulnerable;
        enter(1);
    }

    if (agent.gorea_state == 13) {
        // Enemy24.State13 hands the encounter to Enemy28. The managed code
        // performs this while the 1A body is hidden, so it must not be
        // swallowed by the normal visible-body early return.
        agent.visible = false;
        agent.gorea_activated = false;
        agent.invulnerable = true;
        agent.gorea_flags |= gorea::Gorea1AArmInvulnerable;
        for (auto& child : enemies_) {
            if (child.parent_enemy_id != agent.id) {
                continue;
            }
            if (is_arm(child)) {
                child.gorea_activated = false;
                child.visible = false;
                child.gorea_beam_collidable = false;
                child.invulnerable = true;
            }
        }
        // State13 is reached every frame while the 1A parent remains hidden,
        // but Enemy28.Activate is a one-time handoff.  Reinitializing the
        // child here would keep its state at 00 forever and would prevent
        // the phase-1B wind-up/grapple machine from ever running.
        if (phase_child != nullptr && !phase_child->gorea_activated) {
            phase_child->gorea_activated = true;
            phase_child->visible = true;
            phase_child->gorea_state = 0;
            phase_child->gorea_state_initialized = false;
            phase_child->gorea_state_timer = 120u * 2u;
        }
        // Metadata.Enemy24Subroutines[13] is Behavior01 -> State00. Keep
        // the parent in that hidden state until Enemy28 calls Activate again.
        gorea::enter_state(agent, 0);
        agent.gorea_state_initialized = false;
        select_animation(agent, 0);
        return;
    }
    if (!agent.visible) {
        return;
    }

    // The body volume in the managed class is a large sphere, but its
    // player hit is a short-range horizontal check.  Keep the same knockback
    // and damage boundary here instead of relying on generic enemy contact.
    if (target_index != players_.size()
        && distance_squared(players_[target_index].position,
                           agent.position) <= 3.0F * 3.0F
        && agent.gorea_attack_timer == 0) {
        const net::Vec3 between = subtract(
            players_[target_index].position, agent.position);
        const net::Vec3 horizontal{between.x, 0.0F, between.z};
        players_[target_index].speed = add(
            players_[target_index].speed,
            multiply(normalized_or(horizontal, agent.facing), 0.25F));
        apply_enemy_contact_damage(agent, players_[target_index], 10);
        agent.gorea_attack_timer = 30u * 2u;
    }
    gorea::decrement(agent.gorea_attack_timer, step);

    const net::Vec3 target = target_index == players_.size()
        ? agent.position : players_[target_index].position;
    const float distance = target_index == players_.size()
        ? 1000.0F : std::sqrt(distance_squared(target, agent.position));
    const bool target_alt = target_index != players_.size()
        && (players_[target_index].flags & net::PlayerState::FlagAltForm) != 0;
    auto apply_melee_hit = [this, &agent, target_index](
        std::uint16_t damage, float max_horizontal_squared) {
        if (target_index == players_.size()) {
            return;
        }
        auto& player = players_[target_index];
        const net::Vec3 between = subtract(player.position, agent.position);
        if (length_squared({between.x, 0.0F, between.z})
                >= max_horizontal_squared) {
            return;
        }
        const net::Vec3 direction = length_squared(between)
            > 1.0F / 128.0F
            ? normalized_or(between, agent.facing) : agent.facing;
        const net::Vec3 impulse = add(
            multiply(direction, 1.5F), {0.0F, 682.0F / 4096.0F, 0.0F});
        player.speed = add(player.speed, impulse);
        player.hit_direction = normalized_or(impulse, agent.facing);
        apply_enemy_contact_damage(agent, player, damage);
    };

    const auto in_volume = [&agent](net::Vec3 position) {
        if (agent.gorea_volume_radius <= 0.0F) {
            return true;
        }
        return distance_squared(position, agent.gorea_anchor)
            <= agent.gorea_volume_radius * agent.gorea_volume_radius;
    };

    const auto update_target_facing = [&]() {
        if (target_index == players_.size()) {
            return false;
        }
        const net::Vec3 between = subtract(
            players_[target_index].position, agent.position);
        const net::Vec3 horizontal{between.x, 0.0F, between.z};
        if (length_squared(horizontal) <= 1.0F / 128.0F) {
            return false;
        }
        agent.gorea_target_facing = normalized_or(horizontal, agent.facing);
        return true;
    };

    const auto check_facing_angle = [&](float minimum_cosine) {
        if (target_index == players_.size()) {
            return false;
        }
        const net::Vec3 facing = normalized_or(
            {agent.facing.x, 0.0F, agent.facing.z},
            {0.0F, 0.0F, 1.0F});
        const net::Vec3 between = subtract(
            players_[target_index].position, agent.position);
        const net::Vec3 horizontal{between.x, 0.0F, between.z};
        if (length_squared(horizontal) <= 1.0F / 128.0F) {
            return false;
        }
        return dot(facing, normalized_or(horizontal, facing))
            > minimum_cosine;
    };

    const auto get_elbow_node_vectors = [&](const EnemyState& arm,
                                             net::Vec3& position,
                                             net::Vec3& up,
                                             net::Vec3& facing) {
        const std::size_t index = std::min<std::size_t>(arm.gorea_index, 1);
        const char* const node_name = index == 0 ? "L_Elbow" : "R_Elbow";
        formats::Matrix4 transform{};
        if (!sample_gorea_1a_node(agent, node_name, transform)) {
            position = gorea::local_position(
                agent, {index == 0 ? -0.65F : 0.65F, 0.35F, 0.4F});
            facing = normalized_or(agent.facing, {0.0F, 0.0F, 1.0F});
            up = normalized_or(agent.up, {0.0F, 1.0F, 0.0F});
            return false;
        }
        position = {transform.m41, transform.m42, transform.m43};
        // Enemy26.GetNodeVectors uses Row1 as facing.  Its up axis is Row0
        // for the left arm and Row2 for the right arm, matching the managed
        // elbow-vector helper rather than a generic entity basis.
        facing = {transform.m21, transform.m22, transform.m23};
        if (index == 0) {
            up = {transform.m11, transform.m12, transform.m13};
        } else {
            up = {transform.m31, transform.m32, transform.m33};
        }
        return true;
    };

    const auto seek_target_facing = [&](net::Vec3 desired) {
        desired = normalized_or({desired.x, 0.0F, desired.z}, agent.facing);
        const net::Vec3 facing = normalized_or(
            {agent.facing.x, 0.0F, agent.facing.z}, agent.facing);
        constexpr float half_angle_degrees = 1.5F;
        constexpr float fixed_tolerance = 7.0F / 4096.0F;
        constexpr float Pi = 3.14159265358979323846F;
        const float cosine = std::cos(half_angle_degrees * Pi / 180.0F);
        if (std::fabs(cosine - dot(desired, facing)) > fixed_tolerance) {
            const net::Vec3 rotation_axis{0.0F, 1.0F, 0.0F};
            const float sign = cross(desired, facing).y > 0.0F
                ? -1.0F : 1.0F;
            agent.facing = normalized_or(
                rotate_about_axis(facing, rotation_axis,
                                  sign * half_angle_degrees), facing);
            return false;
        }
        agent.facing = desired;
        return true;
    };

    const auto stop_and_set_up = [&]() {
        static constexpr std::array<std::uint8_t, 6> shot_cooldowns{
            6, 23, 20, 40, 23, 8};
        static constexpr std::array<std::uint8_t, 6> auto_cooldowns{
            22, 28, 20, 36, 23, 18};
        agent.gorea_flags &= ~gorea::Gorea1AWeaponError;
        for (auto& child : enemies_) {
            if (!is_arm(child) || child.parent_enemy_id != agent.id
                || is_dead_arm(child)) {
                continue;
            }
            const auto index = std::min<std::size_t>(
                agent.gorea_weapon_index, shot_cooldowns.size() - 1);
            child.gorea_cooldown = static_cast<std::uint16_t>(
                (child.gorea_index == 0 ? shot_cooldowns[index]
                                        : auto_cooldowns[index]) * 2u);
        }
        agent.gorea_field23c = 60 * 2;
        respawn_head_flash();
    };

    const auto set_shot_animation = [&]() {
        const bool left_dead = (agent.gorea_arm_bits & 1u) == 0;
        const bool right_dead = (agent.gorea_arm_bits & 2u) == 0;
        std::uint8_t animation = 8;
        if (left_dead) {
            animation = 10;
            for (auto& child : enemies_) {
                if (is_arm(child) && child.parent_enemy_id == agent.id
                    && child.gorea_index == 0) {
                    child.gorea_flags &= ~ArmShouldFire;
                }
                if (is_arm(child) && child.parent_enemy_id == agent.id
                    && child.gorea_index == 1 && !is_dead_arm(child)) {
                    child.gorea_flags |= ArmShouldFire;
                }
            }
        } else if (right_dead) {
            animation = 9;
            for (auto& child : enemies_) {
                if (!is_arm(child) || child.parent_enemy_id != agent.id
                    || is_dead_arm(child)) {
                    continue;
                }
                if (child.gorea_index == 0) {
                    child.gorea_flags |= ArmShouldFire;
                } else {
                    child.gorea_flags &= ~ArmShouldFire;
                }
            }
        } else {
            switch (rng_.random2(3u)) {
            case 0:
                animation = 9;
                break;
            case 1:
                animation = 10;
                break;
            default:
                animation = 8;
                break;
            }
            for (auto& child : enemies_) {
                if (!is_arm(child) || child.parent_enemy_id != agent.id
                    || is_dead_arm(child)) {
                    continue;
                }
                if (animation == 8
                    || (animation == 9 && child.gorea_index == 0)
                    || (animation == 10 && child.gorea_index == 1)) {
                    child.gorea_flags |= ArmShouldFire;
                } else {
                    child.gorea_flags &= ~ArmShouldFire;
                }
            }
        }
        gorea::set_animation(agent, animation, 36);
        for (auto& child : enemies_) {
            if (is_arm(child) && child.parent_enemy_id == agent.id
                && !is_dead_arm(child)) {
                child.gorea_cooldown = 12;
            }
        }
    };

    const auto start_shots = [&]() {
        static constexpr std::array<std::uint8_t, 6> charge_chances{
            40, 100, 40, 30, 100, 100};
        static constexpr std::array<std::uint8_t, 6> shot_cooldowns{
            6, 23, 20, 40, 23, 8};
        static constexpr std::array<std::uint8_t, 6> charge_effects{
            48, 46, 49, 47, 50, 41};
        const bool charge = rng_.random2(100u)
            < charge_chances[std::min<std::uint8_t>(
                agent.gorea_weapon_index, 5)];
        agent.gorea_ammo = 0;
        for (auto& child : enemies_) {
            if (!is_arm(child) || child.parent_enemy_id != agent.id
                || is_dead_arm(child)) {
                continue;
            }
            child.gorea_flags &= ~(ArmShouldFire | ArmCharged | ArmHasFired);
            child.gorea_cooldown = static_cast<std::uint16_t>(
                shot_cooldowns[std::min<std::size_t>(
                    agent.gorea_weapon_index, shot_cooldowns.size() - 1)] * 2u);
            if (charge) {
                child.gorea_flags |= ArmCharged | ArmShouldFire;
                // Enemy24.CreateChargeEffect replaces the arm's held shot
                // entry with the weapon-specific charge loop. Detach the
                // previous entry without expiring its particles first, then
                // let Enemy26 update this handle from the animated elbow.
                if (child.gorea_arm_effect_id != 0) {
                    detach_effect(child.gorea_arm_effect_id, false);
                    child.gorea_arm_effect_id = 0;
                }
                child.gorea_arm_effect_id = spawn_effect(
                    charge_effects[std::min<std::uint8_t>(
                        agent.gorea_weapon_index,
                        static_cast<std::uint8_t>(charge_effects.size() - 1))],
                    child.position, child.facing, child.id, 0.25F, false,
                    true, false, child.up);
            }
        }
        if (charge) {
            agent.gorea_field242 = static_cast<std::int32_t>(
                (rng_.random2(30u) + 60u) * 2u);
            agent.gorea_next_state = 6;
            gorea::set_animation(agent, 15, 72);
        } else {
            agent.gorea_field242 = 0;
            agent.gorea_next_state = 5;
            set_shot_animation();
        }
    };

    const auto try_sprinting_room_in_volume = [&]() {
        if (target_index == players_.size()) {
            return false;
        }
        const float speed_factor = agent.gorea_speed_factor != 0.0F
            ? agent.gorea_speed_factor : 341.0F / 4096.0F;
        const net::Vec3 offset = multiply(agent.facing,
                                           speed_factor * 5.0F * 30.0F);
        if (!in_volume(add(agent.position, offset))
            || !update_target_facing()) {
            return false;
        }
        agent.gorea_next_state = agent.gorea_state;
        agent.gorea_field244 = 0;
        agent.gorea_field23c = 150 * 2;
        gorea::set_animation(agent, 23, 60, true);
        return true;
    };

    const auto run_behavior = [&](std::uint8_t behavior) {
        switch (behavior) {
        case 0: // Behavior00
            if (!gorea::animation_ended(agent)) {
                return false;
            }
            gorea::set_animation(agent, 17, 30, true);
            if (agent.gorea_state == 9) {
                agent.gorea_target_facing = {};
            }
            return true;
        case 1: // Behavior01
            return true;
        case 2: // Behavior02
            return true;
        case 3: // Behavior03
            return gorea::animation_ended(agent);
        case 4: { // Behavior04: process one newly observed dead arm.
            for (std::uint8_t index = 0; index < 2; ++index) {
                if ((agent.gorea_arm_bits & (1u << index)) == 0) {
                    agent.gorea_flags = (agent.gorea_flags
                                         & ~static_cast<std::uint32_t>(1u << 3))
                        | (index == 0 ? static_cast<std::uint32_t>(1u << 3)
                                      : 0u);
                    agent.velocity = {};
                    gorea::set_animation(agent,
                                         static_cast<std::uint8_t>(20 + index),
                                         36);
                    return true;
                }
            }
            return false;
        }
        case 5: // Behavior05
            if (agent.gorea_animation_frame
                    < agent.gorea_animation_length - 1) {
                return false;
            }
            for (auto& child : enemies_) {
                if (is_arm(child) && child.parent_enemy_id == agent.id
                    && !is_dead_arm(child)) {
                    child.gorea_flags |= ArmShouldFire;
                }
            }
            static constexpr std::array<std::uint8_t, 6> weapon_animations{
                11, 3, 7, 4, 12, 14};
            gorea::set_animation(agent, weapon_animations[
                                      std::min<std::uint8_t>(
                                          agent.gorea_weapon_index, 5)], 60);
            return true;
        case 6: // Behavior06
            if (agent.gorea_arm_bits != 0
                || agent.gorea_animation_frame != 5
                || phase_child == nullptr) {
                return false;
            }
            phase_child->gorea_model_animation =
                (agent.gorea_flags & (1u << 3)) != 0 ? 5 : 6;
            phase_child->gorea_animation_frame = 5;
            phase_child->gorea_animation_length = gorea::authored_animation_length(
                phase_child->enemy_type, phase_child->gorea_model_animation, 45);
            phase_child->gorea_animation_loop = false;
            phase_child->gorea_animation_ended = false;
            return true;
        case 7: { // Behavior07
            const bool outside = !in_volume(add(agent.position,
                                                agent.velocity));
            if (!outside) {
                return false;
            }
            agent.velocity = {};
            bool update = true;
            if (agent.gorea_state == 3) {
                if (agent.gorea_next_state == 5) {
                    start_shots();
                    agent.gorea_field23c = static_cast<std::int32_t>(
                        (rng_.random2(60u) + 90u) * 2u);
                    update = false;
                } else if (agent.gorea_next_state == 3) {
                    agent.gorea_next_state = 4;
                }
            }
            if (update) {
                gorea::set_animation(agent, 17, 30, true);
                stop_and_set_up();
            }
            return true;
        }
        case 8: // Behavior08
            gorea::decrement_signed(agent.gorea_field23c, step);
            if (agent.gorea_field23c != 0) {
                return false;
            }
            agent.velocity = {};
            if (agent.gorea_next_state == 5) {
                start_shots();
                agent.gorea_field23c = static_cast<std::int32_t>(
                    (rng_.random2(60u) + 90u) * 2u);
            } else {
                if (agent.gorea_next_state == 3) {
                    agent.gorea_next_state = 4;
                }
                gorea::set_animation(agent, 17, 30, true);
                stop_and_set_up();
            }
            return true;
        case 9: // Behavior09
            if (!target_alt
                || length_squared({target.x - agent.position.x, 0.0F,
                                    target.z - agent.position.z}) >= 25.0F) {
                return false;
            }
            agent.velocity = {};
            agent.gorea_next_state = agent.gorea_state;
            // State08 consumes animation frame 60 after Behavior09 selects
            // this same no-loop animation.
            gorea::set_animation(agent, 13, 78);
            return true;
        case 10: // Behavior10
            if (target_alt
                || length_squared({target.x - agent.position.x, 0.0F,
                                    target.z - agent.position.z}) >= 37.5F) {
                return false;
            }
            agent.velocity = {};
            if ((agent.gorea_arm_bits & 1u) != 0) {
                gorea::set_animation(agent, 6, 48);
            } else {
                gorea::set_animation(agent, 5, 48);
            }
            return true;
        case 11: // Behavior11
            if (target_index == players_.size()
                || distance * distance <= 19.0F * 19.0F) {
                return false;
            }
            if (agent.gorea_field244 > 0) {
                --agent.gorea_field244;
            }
            return agent.gorea_field244 == 0
                && try_sprinting_room_in_volume();
        case 12: // Behavior12
            gorea::decrement_signed(agent.gorea_field23c, step);
            if (agent.gorea_field23c > 0) {
                return false;
            }
            start_shots();
            agent.gorea_field23c = static_cast<std::int32_t>(
                (rng_.random2(60u) + 90u) * 2u);
            return true;
        case 13: // Behavior13
            return gorea::animation_ended(agent)
                && (agent.gorea_flags & gorea::Gorea1AWeaponError) != 0
                && try_sprinting_room_in_volume();
        case 14: // Behavior14
            if (!gorea::animation_ended(agent) || !target_alt
                || length_squared({target.x - agent.position.x, 0.0F,
                                    target.z - agent.position.z}) >= 25.0F) {
                return false;
            }
            agent.velocity = {};
            agent.gorea_next_state = agent.gorea_state;
            gorea::set_animation(agent, 13, 78);
            return true;
        case 15: // Behavior15
            gorea::decrement_signed(agent.gorea_field23c, step);
            if (agent.gorea_field23c > 0
                || agent.gorea_animation_frame
                    < agent.gorea_animation_length - 1) {
                return false;
            }
            gorea::set_animation(agent, 17, 30, true);
            for (auto& child : enemies_) {
                if (is_arm(child) && child.parent_enemy_id == agent.id) {
                    child.gorea_flags &= ~ArmShouldFire;
                    if (child.gorea_arm_effect_id != 0) {
                        // Behavior15 calls StopShots(..., detach: true), so
                        // the charge particles drain after the arm releases
                        // its ownership instead of being hard-killed.
                        detach_effect(child.gorea_arm_effect_id, false);
                        child.gorea_arm_effect_id = 0;
                    }
                }
            }
            agent.gorea_field23c = 210 * 2;
            return true;
        case 16: // Behavior16
            if (agent.gorea_animation_frame
                    < agent.gorea_animation_length - 1) {
                return false;
            }
            start_shots();
            return true;
        case 17: // Behavior17
            if (head_child == nullptr || head_child->gorea_damage < 1000u) {
                return false;
            }
            head_child->gorea_damage = 0;
            agent.gorea_next_state = agent.gorea_state;
            gorea::set_animation(agent, 19, 60);
            return true;
        case 18: // Behavior18
            if (length_squared(agent.gorea_target_facing) > 0.0F
                || !check_facing_angle(-1.0F)) {
                return false;
            }
            agent.gorea_field23c = 60 * 2;
            return true;
        case 19: // Behavior19
            --agent.gorea_field23c;
            if (agent.gorea_field23c > 0) {
                return false;
            }
            agent.velocity = {};
            gorea::set_animation(agent, 17, 30, true);
            stop_and_set_up();
            return true;
        case 20: // Behavior20
            if (agent.gorea_field23e >= 0) {
                return false;
            }
            agent.gorea_field23e = 510 * 2;
            for (const auto& child : enemies_) {
                if (is_arm(child) && child.parent_enemy_id == agent.id
                    && is_dead_arm(child)) {
                    return true;
                }
            }
            return false;
        case 21: // Behavior21
            if (agent.gorea_field240 >= 0) {
                return false;
            }
            agent.gorea_field240 = static_cast<std::int32_t>(
                (rng_.random2(90u) + 150u) * 2u);
            return true;
        case 22: // Behavior22
            --agent.gorea_field23c;
            if (agent.gorea_field23c > 0) {
                return false;
            }
            agent.velocity = {};
            gorea::set_animation(agent, 17, 30, true);
            stop_and_set_up();
            return true;
        default:
            return false;
        }
    };

    const auto call_subroutine = [&](std::uint8_t state) {
        const auto try_behavior = [&](std::uint8_t behavior,
                                      std::uint8_t next_state) {
            if (!run_behavior(behavior)) {
                return false;
            }
            agent.gorea_pending_state = next_state;
            agent.gorea_state_pending = true;
            return true;
        };
        switch (state) {
        case 0: return try_behavior(0, 1);
        case 1:
            return try_behavior(11, 3) || try_behavior(18, 2)
                || try_behavior(4, 9) || try_behavior(9, 8)
                || try_behavior(10, 12) || try_behavior(19, 4)
                || try_behavior(20, 10) || try_behavior(21, 7);
        case 2:
            return try_behavior(4, 9) || try_behavior(11, 3)
                || try_behavior(9, 8) || try_behavior(10, 12)
                || try_behavior(22, 4) || try_behavior(20, 10)
                || try_behavior(21, 7) || try_behavior(7, 4);
        case 3:
            return try_behavior(7, 14) || try_behavior(8, 14)
                || try_behavior(9, 8) || try_behavior(10, 12);
        case 4:
            return try_behavior(4, 9) || try_behavior(11, 3)
                || try_behavior(9, 8) || try_behavior(10, 12)
                || try_behavior(12, 14);
        case 5:
            return try_behavior(4, 9) || try_behavior(13, 3)
                || try_behavior(14, 8) || try_behavior(10, 12)
                || try_behavior(15, 1) || try_behavior(16, 14)
                || try_behavior(17, 11);
        case 6:
            return try_behavior(4, 9) || try_behavior(5, 5);
        case 7:
            return try_behavior(4, 9) || try_behavior(0, 1);
        case 8: return try_behavior(3, 14);
        case 9:
            return try_behavior(6, 13) || try_behavior(4, 9)
                || try_behavior(0, 1);
        case 10:
            return try_behavior(4, 9) || try_behavior(0, 1);
        case 11: return try_behavior(3, 14);
        case 12: return try_behavior(0, 2);
        case 13: return try_behavior(1, 0);
        case 14: return try_behavior(2, 14);
        default: return false;
        }
    };

    switch (agent.gorea_state) {
    case 1: {
        // State01 only aims and delegates the transition decision to its
        // ordered subroutine list.  Distance-only branching here used to
        // skip the authored facing timeout, shoulder death and weapon timer.
        if (length_squared(agent.gorea_target_facing) > 0.0F) {
            if (seek_target_facing(agent.gorea_target_facing)) {
                agent.gorea_target_facing = {};
            }
        } else {
            gorea::set_animation(agent, 17, 30, true);
            update_target_facing();
        }
        if (call_subroutine(1)) {
            return;
        }
        break;
    }
    case 2: {
        update_target_facing();
        if (length_squared(agent.gorea_target_facing) > 0.0F
            && seek_target_facing(agent.gorea_target_facing)) {
            agent.gorea_target_facing = {};
        }
        const float speed = (agent.gorea_speed_factor != 0.0F
            ? agent.gorea_speed_factor : 341.0F / 4096.0F) / 2.0F;
        agent.velocity = multiply(
            normalized_or({agent.facing.x, 0.0F, agent.facing.z}, agent.facing),
            speed * frame_scale);
        agent.position = add(agent.position, agent.velocity);
        if (call_subroutine(2)) {
            return;
        }
        break;
    }
    case 3: {
        if (length_squared(agent.gorea_target_facing) > 0.0F
            && seek_target_facing(agent.gorea_target_facing)) {
            agent.gorea_target_facing = {};
        }
        agent.velocity = multiply(
            normalized_or({agent.facing.x, 0.0F, agent.facing.z}, agent.facing),
            (agent.gorea_speed_factor != 0.0F
                ? agent.gorea_speed_factor : 341.0F / 4096.0F)
                * 2.5F * frame_scale);
        agent.position = add(agent.position, agent.velocity);
        if (call_subroutine(3)) {
            return;
        }
        break;
    }
    case 4:
        agent.velocity = {};
        update_target_facing();
        if (length_squared(agent.gorea_target_facing) > 0.0F
            && seek_target_facing(agent.gorea_target_facing)) {
            agent.gorea_target_facing = {};
        }
        gorea::set_animation(agent, 16, 36);
        if (call_subroutine(4)) {
            return;
        }
        break;
    case 5: {
        agent.velocity = {};
        if (agent.gorea_animation_frame >= 8) {
            static constexpr std::array<std::uint8_t, 6> shot_cooldowns{
                6, 23, 20, 40, 23, 8};
            static constexpr std::array<std::uint8_t, 6> auto_cooldowns{
                22, 28, 20, 36, 23, 18};
            static constexpr std::array<std::uint8_t, 6> shot_effects{
                54, 51, 55, 53, 56, 52};
            for (auto& child : enemies_) {
                if (!is_arm(child) || child.parent_enemy_id != agent.id
                    || is_dead_arm(child) || !child.gorea_activated
                    || (child.gorea_flags & ArmShouldFire) == 0) {
                    continue;
                }
                const auto bit = static_cast<std::uint16_t>(
                    1u << std::min<std::uint8_t>(child.gorea_index, 1));
                const std::size_t weapon_index = std::min<std::size_t>(
                    agent.gorea_weapon_index, shot_cooldowns.size() - 1);
                const bool charged = (child.gorea_flags & ArmCharged) != 0;
                const std::uint16_t frame = agent.gorea_animation_frame;
                if (charged) {
                    const auto shot_frame = static_cast<std::uint16_t>(
                        shot_cooldowns[weapon_index] * 2u);
                    const auto auto_frame = static_cast<std::uint16_t>(
                        auto_cooldowns[weapon_index] * 2u);
                    if (frame < shot_frame || frame > auto_frame
                        || (agent.gorea_weapon_index != 5
                            && (frame % 8u) != 7u)) {
                        continue;
                    }
                } else if ((agent.gorea_ammo & bit) != 0
                           || frame < child.gorea_cooldown) {
                    continue;
                }
                child.gorea_weapon_index = agent.gorea_weapon_index;
                net::Vec3 elbow_position{};
                net::Vec3 elbow_up{};
                net::Vec3 elbow_facing{};
                static_cast<void>(get_elbow_node_vectors(
                    child, elbow_position, elbow_up, elbow_facing));
                constexpr float ElbowMuzzleOffset = 8343.0F / 4096.0F;
                const net::Vec3 origin = add(
                    elbow_position, multiply(elbow_facing, ElbowMuzzleOffset));
                spawn_enemy_projectile(
                    child, origin,
                    gorea::aim_at(origin, add(target, {0.0F, 0.5F, 0.0F}),
                                  agent.facing));
                // CreateShotEffectLoose intentionally uses the elbow's up
                // vector as the effect-facing vector (the managed helper
                // calls GetElbowNodeVectors with its last two out values
                // swapped). Keep that authored transform separate from the
                // projectile's GetArmAim origin above.
                const net::Vec3 effect_facing = normalized_or(
                    elbow_up, {0.0F, 1.0F, 0.0F});
                const net::Vec3 effect_up = normalized_or(
                    elbow_facing, agent.facing);
                const net::Vec3 effect_origin = add(
                    elbow_position,
                    multiply(effect_facing, ElbowMuzzleOffset));
                spawn_effect(
                    shot_effects[std::min<std::size_t>(
                        weapon_index, shot_effects.size() - 1)],
                    effect_origin, effect_facing, child.id, 0.25F, false,
                    false, false, effect_up);
                if (!charged) {
                    agent.gorea_ammo |= bit;
                    child.gorea_flags &= ~ArmShouldFire;
                }
            }
        }
        if (call_subroutine(5)) {
            return;
        }
        break;
    }
    case 6:
        agent.velocity = {};
        if (call_subroutine(6)) {
            return;
        }
        break;
    case 7:
        agent.velocity = {};
        if (agent.gorea_model_animation != 8
            && agent.gorea_model_animation != 9
            && agent.gorea_model_animation != 10
            && agent.gorea_model_animation != 13) {
            agent.gorea_weapon_index = static_cast<std::uint8_t>(
                (agent.gorea_weapon_index + 1) % 6);
            gorea::set_animation(agent, 13, 36);
        }
        if (call_subroutine(7)) {
            return;
        }
        break;
    case 8:
        agent.velocity = {};
        if ((agent.gorea_ammo & 1u) == 0
            && agent.gorea_animation_frame >= 60) {
            apply_melee_hit(40, 25.0F);
            spawn_effect(71, agent.position,
                         {1.0F, 0.0F, 0.0F}, agent.id);
            agent.gorea_ammo |= 1u;
        }
        if (call_subroutine(8)) {
            return;
        }
        break;
    case 9:
        agent.velocity = {};
        if (call_subroutine(9)) {
            return;
        }
        break;
    case 10:
        agent.velocity = {};
        if (call_subroutine(10)) {
            return;
        }
        break;
    case 11:
        agent.velocity = {};
        if (call_subroutine(11)) {
            return;
        }
        break;
    case 12:
        agent.velocity = {};
        if ((agent.gorea_ammo & 1u) == 0
            && agent.gorea_animation_frame >= 24) {
            apply_melee_hit(25, 37.5F);
            agent.gorea_ammo |= 1u;
        }
        if (call_subroutine(12)) {
            return;
        }
        break;
    case 13:
        // The one-frame handoff is handled above, before the hidden-body
        // return, just as Enemy24.State13 activates Enemy28.
        break;
    case 14:
        if (call_subroutine(14)) {
            // Behavior02 is an immediate trampoline to the continuation that
            // the preceding attack behavior stored in _nextState.
            agent.gorea_pending_state = agent.gorea_next_state == 0
                ? 1 : agent.gorea_next_state;
            agent.gorea_state_pending = true;
            return;
        }
        break;
    default:
        break;
    }
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_24_gorea_1a::kModule.managed_class.size() != 0);
