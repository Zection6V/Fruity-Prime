#include "45_SlenchTurret.hpp"
#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/41_Slench.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "41_Slench.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_slench(EnemyState& agent) {
    const auto& profile = agent.slench;
    if (!profile.supported) {
        return;
    }

    constexpr std::uint8_t Initial = 0;
    constexpr std::uint8_t Intro = 1;
    constexpr std::uint8_t ShieldRaise = 2;
    constexpr std::uint8_t Idle = 3;
    constexpr std::uint8_t ShootTear = 4;
    constexpr std::uint8_t ShieldLower = 5;
    constexpr std::uint8_t Return = 6;
    constexpr std::uint8_t Attach = 7;
    constexpr std::uint8_t Detach = 8;
    constexpr std::uint8_t RollingDone = 9;
    constexpr std::uint8_t Roam = 10;
    constexpr std::uint8_t SlamReady = 11;
    constexpr std::uint8_t Slam = 12;
    constexpr std::uint8_t SlamReturn = 13;
    constexpr std::uint8_t Dead = 14;

    constexpr std::uint16_t EyeClosed = 0x0001;
    constexpr std::uint16_t Rolling = 0x0002;
    constexpr std::uint16_t Floating = 0x0004;
    constexpr std::uint16_t Bouncy = 0x0008;
    constexpr std::uint16_t PatternFlip1 = 0x0010;
    constexpr std::uint16_t PatternFlip2 = 0x0020;
    constexpr std::uint16_t Detached = 0x0040;
    constexpr std::uint16_t TargetingPlayer = 0x0080;
    constexpr std::uint16_t Vulnerable = 0x0100;
    constexpr std::uint16_t Wobbling = 0x0200;

    const float seconds = config_.tick_seconds;
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
    const std::size_t phase_index = std::min<std::size_t>(
        agent.slench_phase, profile.phases.size() - 1);
    const auto& phase = profile.phases[phase_index];
    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };

    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);
    if (agent.health == 0) {
        agent.slench_state = Dead;
        agent.state = Dead;
        agent.slench_flags |= EyeClosed;
        agent.slench_shielded = true;
        agent.invulnerable = true;
        agent.target_slot = 0xff;
        agent.velocity = {};
        if (agent.slench_death_timer == 0) {
            agent.slench_death_timer = 36u * 2u;
        }
        decrement(agent.slench_death_timer);
        if (agent.slench_death_timer == 0) {
            const auto id = agent.id;
            static_cast<void>(destroy_enemy(id));
        }
        return;
    }

    std::size_t target_index = players_.size();
    float nearest_squared = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance <= 45.0F * 45.0F && distance < nearest_squared) {
            nearest_squared = distance;
            target_index = index;
        }
    }

    // The linked Slench turret records are independent enemy spawners in the
    // room entity file. Treat them as the native counterpart of the synapse
    // message graph, without requiring a renderer-side child entity.
    const auto set_linked_turrets = [this](bool enabled) {
        const auto turret_type = static_cast<std::uint8_t>(
            formats::EnemyType::SlenchTurret);
        for (auto& turret : enemies_) {
            if (turret.enemy_type == turret_type && turret.active) {
                // Enemy45Entity.HandleMessage owns what waking a turret
                // means -- including that it sets the state it will be in
                // *next* frame rather than this one, which is the whole
                // difference between a state machine and a flag.
                enemy::module_45_slench_turret::HandleMessage(
                    turret,
                    static_cast<std::uint16_t>(
                        enabled ? formats::Message::ActivateTurret
                                : formats::Message::DeactivateTurret),
                    0);
                turret.target_slot = 0xff;
                turret.turret_shot_timer = 0;
            }
        }
    };
    const auto fixed = [](std::int32_t value) noexcept {
        return static_cast<float>(value) / 4096.0F;
    };
    const auto enter_state = [&agent, &set_linked_turrets, &phase](
                                 std::uint8_t state,
                                 std::uint32_t timer) {
        const std::uint8_t previous_state = agent.slench_state;
        agent.slench_state = state;
        agent.state = state;
        agent.slench_state_timer = timer;
        agent.slench_shot_timer = 0;
        if (state == ShootTear) {
            agent.slench_static_shot_timer = 0;
            agent.slench_static_shot_cooldown = 0;
            agent.slench_static_shot_counter = 0;
            agent.slench_firing_tear = true;
        }
        agent.slench_flags &= static_cast<std::uint16_t>(
            ~(TargetingPlayer | Vulnerable | Wobbling));
        switch (state) {
        case Initial:
        case ShieldLower:
        case Slam:
        case SlamReturn:
            agent.slench_flags |= EyeClosed;
            agent.slench_shielded = true;
            agent.invulnerable = true;
            break;
        case Intro:
        case ShieldRaise:
        case Idle:
        case ShootTear:
        case Return:
        case Attach:
            agent.slench_flags &= static_cast<std::uint16_t>(~Detached);
            agent.slench_flags |= EyeClosed;
            agent.slench_shielded = true;
            agent.invulnerable = true;
            break;
        case Detach:
            agent.slench_flags |= Detached;
            agent.slench_flags &= static_cast<std::uint16_t>(
                ~(Rolling | Floating | Bouncy | PatternFlip1 | PatternFlip2));
            agent.slench_pattern_angle = 0.0F;
            agent.slench_float_base_y = 0.0F;
            agent.slench_drop_speed = 0.0F;
            agent.slench_slam_timer = 0;
            agent.slench_roam_timer = static_cast<std::uint32_t>(
                phase.roam_time) * 2u;
            if (agent.slench.subtype == 3) {
                agent.slench_flags |= Rolling | EyeClosed;
                agent.slench_roll_timer = static_cast<std::uint32_t>(
                    std::max<std::int32_t>(0, phase.roll_time));
                agent.slench_shielded = true;
                agent.invulnerable = true;
            } else {
                agent.slench_flags |= Floating;
                agent.slench_flags &= static_cast<std::uint16_t>(~EyeClosed);
                agent.slench_shielded = false;
                agent.invulnerable = false;
            }
            break;
        case RollingDone:
            agent.slench_flags &= static_cast<std::uint16_t>(
                ~(Rolling | Bouncy | PatternFlip1 | PatternFlip2));
            agent.slench_flags |= Floating | Vulnerable;
            agent.slench_shielded = false;
            agent.invulnerable = false;
            break;
        case Roam:
            agent.slench_flags |= Detached;
            if (agent.slench.subtype == 3
                && (agent.slench_flags & Rolling) != 0) {
                agent.slench_flags |= EyeClosed;
                agent.slench_shielded = true;
                agent.invulnerable = true;
            } else {
                agent.slench_flags &= static_cast<std::uint16_t>(~EyeClosed);
                agent.slench_flags |= Vulnerable;
                agent.slench_shielded = false;
                agent.invulnerable = false;
            }
            agent.slench_roam_timer = static_cast<std::uint32_t>(
                phase.roam_time) * 2u;
            break;
        case SlamReady:
            agent.slench_flags |= EyeClosed;
            agent.slench_state_after_slam = previous_state;
            agent.slench_wobble_timer = 0;
            agent.slench_shielded = true;
            agent.invulnerable = true;
            break;
        case Dead:
            agent.slench_flags |= EyeClosed;
            agent.slench_death_timer = 36u * 2u;
            agent.slench_shielded = true;
            agent.invulnerable = true;
            break;
        default:
            break;
        }
        // The C# owner disables every linked turret while leaving the
        // static phase.  Individual synapses enable their own indexed
        // turret when their Appear animation completes.
        if (state != Idle && state != ShootTear) {
            set_linked_turrets(false);
        }
    };

    // Enemy41Entity owns the three Enemy44Entity children.  Keep the same
    // ownership signal in the native session so a dead synapse can open the
    // eye cycle, while an Initial/Appear child still counts as alive.
    std::uint8_t synapses_total = 0;
    std::uint8_t synapses_alive = 0;
    for (const auto& child : enemies_) {
        if (child.parent_enemy_id != agent.id
            || !child.slench_synapse.supported) {
            continue;
        }
        ++synapses_total;
        if (child.active && child.slench_part_state != 5) { // Dead
            ++synapses_alive;
        }
    }
    agent.slench_synapses_alive = synapses_alive;

    if (agent.slench_state >= ShieldRaise && agent.slench_state < Roam) {
        if (synapses_total != 0 && synapses_alive == 0) {
            // The managed controller enters ShieldLower here.  Keep the
            // authored detach/return sequence; going directly to Roam makes
            // the body vulnerable before the shield has actually lowered.
            enter_state(ShieldLower, 0);
        } else {
            // ChangeState(Appear) is issued to one Initial synapse per frame
            // by the managed Slench process.  The animation is represented by
            // a fixed 60-frame transition in this animation-independent host.
            for (auto& child : enemies_) {
                if (child.parent_enemy_id != agent.id
                    || !child.slench_synapse.supported
                    || child.slench_part_state != 0) { // Initial
                    continue;
                }
                const auto& child_phase = child.slench_synapse.phases[
                    phase_index];
                child.slench_part_state = 1; // Appear
                child.slench_part_timer = 30u * 2u;
                child.slench_part_heal_timer = 0;
                child.slench_part_reappear_timer = 0;
                child.health = child.health_max = child_phase.health;
                child.body_radius = child_phase.collision_radius;
                child.visible = true;
                child.invulnerable = true;
                break;
            }
        }
    }

    if (target_index != players_.size()) {
        auto& target = players_[target_index];
        agent.target_slot = target.slot_index;
        const net::Vec3 target_position = add(target.position,
                                               {0.0F, 0.5F, 0.0F});
        const net::Vec3 to_target = subtract(target_position, agent.position);
        const net::Vec3 horizontal = normalized_or(
            {to_target.x, 0.0F, to_target.z}, agent.facing);
        if (length_squared(horizontal) > 0.0F) {
            // C# keeps target X/Z separately and lets RotateToTarget advance
            // the transform.  Do not overwrite FacingVector here: doing so
            // makes every targeting state instantly aligned and bypasses the
            // authored turn increments.
            agent.slench_target_horizontal = horizontal;
        }
    } else {
        agent.target_slot = 0xff;
    }

    const auto rotate_to_target = [&agent](net::Vec3 target,
                                            float increment) noexcept {
        const net::Vec3 desired = normalized_or(
            {target.x - agent.position.x, 0.0F,
             target.z - agent.position.z}, agent.facing);
        const net::Vec3 current = normalized_or(
            {agent.facing.x, 0.0F, agent.facing.z}, desired);
        const float cosine = std::clamp(
            dot(current, desired), -1.0F, 1.0F);
        const float angle = std::acos(cosine) * 180.0F
            / 3.14159265358979323846F;
        if (angle <= std::max(0.0F, increment)) {
            agent.facing = desired;
            return true;
        }
        const net::Vec3 turn_axis{0.0F, 1.0F, 0.0F};
        const float sign = current.x * desired.z - current.z * desired.x;
        agent.facing = normalized_or(
            rotate_about_axis(current, turn_axis,
                              (sign >= 0.0F ? 1.0F : -1.0F)
                                  * std::max(0.0F, increment)),
            desired);
        return false;
    };
    const auto move_to_position = [&agent, seconds](net::Vec3 target,
                                                     float increment) noexcept {
        const net::Vec3 delta = subtract(target, agent.position);
        const float distance = std::sqrt(std::max(0.0F,
                                                  length_squared(delta)));
        if (distance <= std::max(0.0F, increment)) {
            agent.position = target;
            agent.velocity = {};
            return true;
        }
        const net::Vec3 old_position = agent.position;
        agent.position = add(agent.position,
                             multiply(delta, increment / distance));
        agent.velocity = seconds > 0.0F
            ? multiply(subtract(agent.position, old_position),
                       1.0F / seconds) : net::Vec3{};
        return false;
    };
    const auto collision_blocked = [&agent, this](net::Vec3 target) noexcept {
        return collision::sweep_sphere(
                   room_.collision(), to_collision(agent.position),
                   to_collision(target), std::max(0.0F, agent.body_radius))
            .has_value();
    };
    const auto drop_to_floor = [&agent, &collision_blocked, seconds,
                                frame_step]() noexcept {
        constexpr float acceleration = 1.1F * 30.0F;
        constexpr float maximum_speed = 1.1F * 30.0F;
        agent.slench_drop_speed = std::clamp(
            agent.slench_drop_speed - acceleration
                * static_cast<float>(frame_step),
            -maximum_speed, maximum_speed);
        const float displacement = agent.slench_drop_speed * seconds;
        const net::Vec3 target = add(agent.position,
                                     {0.0F, displacement, 0.0F});
        if (!collision_blocked(target)) {
            agent.position = target;
            agent.slench_hit_floor = false;
            return;
        }
        agent.slench_drop_speed = -agent.slench_drop_speed * 0.5F;
        if (std::abs(agent.slench_drop_speed) < 0.05F * 30.0F) {
            agent.slench_drop_speed = 0.0F;
            agent.slench_hit_floor = true;
        }
    };
    const auto process_recoil = [&agent]() noexcept {
        if (agent.slench_recoil_timer >= 10u * 2u) {
            return;
        }
        static constexpr std::array<float, 10> recoil{
            0.2F, 0.4F, 0.55F, 0.7F, 0.775F,
            0.85F, 0.875F, 0.9F, 0.95F, 1.0F};
        float factor = 1.0F;
        if (agent.slench_recoil_timer <= 4u * 2u + 1u) {
            factor = recoil[std::min<std::size_t>(
                agent.slench_recoil_timer, recoil.size() - 1)];
        } else {
            const float remaining = static_cast<float>(
                9u * 2u + 1u - agent.slench_recoil_timer);
            factor = remaining / (5.0F * 2.0F);
        }
        agent.position = add(agent.slench_destination2,
                             multiply(agent.slench_destination1, factor));
        ++agent.slench_recoil_timer;
    };
    const auto set_up_slam = [&agent, &phase, &enter_state,
                              frame_step](net::Vec3 target) noexcept {
        if (phase.slam_range <= 0.0F) {
            return false;
        }
        if (agent.slench_slam_timer
            < static_cast<std::uint32_t>(phase.slam_delay) * 2u) {
            agent.slench_slam_timer += frame_step;
            return false;
        }
        const net::Vec3 delta = subtract(target, agent.position);
        const float distance = std::sqrt(std::max(0.0F,
                                                  length_squared(delta)));
        if (distance > phase.slam_range || distance <= 2.9F * 0.75F) {
            return false;
        }
        agent.slench_destination2 = agent.position;
        const net::Vec3 direction = multiply(delta, 1.0F / distance);
        agent.slench_destination1 = add(
            agent.position,
            multiply(direction, distance - 2.9F * 0.75F));
        agent.slench_slam_timer = 0;
        enter_state(SlamReady, 0);
        return true;
    };

    // TargetingPlayer is set by the detached movement state, but the actual
    // secondary beam is emitted by EnemyProcess before the state transition.
    // Keep that ordering so a turn that reaches the player on this frame can
    // fire immediately, while static Slench Tears remain a separate weapon.
    if ((agent.slench_flags & TargetingPlayer) != 0
        && target_index != players_.size()) {
        if (agent.slench_shot_timer > 0) {
            decrement(agent.slench_shot_timer);
        } else {
            const net::Vec3 target = add(
                players_[target_index].position, {0.0F, 0.5F, 0.0F});
            const net::Vec3 direction = normalized_or(
                subtract(target, agent.position), agent.facing);
            agent.slench_firing_tear = false;
            spawn_enemy_projectile(
                agent,
                add(agent.position, multiply(agent.facing, 2.9F)),
                direction);
            agent.slench_shot_timer = static_cast<std::uint32_t>(
                std::max<std::int32_t>(1, phase.static_shot_cooldown));
            agent.slench_destination2 = agent.position;
            agent.slench_destination1 = multiply(agent.facing, -1.0F);
            agent.slench_recoil_timer = 0;
        }
    }
    process_recoil();

    switch (agent.slench_state) {
    case Initial:
        agent.velocity = {};
        if (target_index != players_.size()
            && distance_squared(players_[target_index].position,
                                agent.position) <= 16.0F * 16.0F) {
            enter_state(Intro, 60u * 2u);
        }
        return;
    case Intro:
        agent.velocity = {};
        decrement(agent.slench_state_timer);
        if (agent.slench_state_timer == 0) {
            enter_state(ShieldRaise, 30u * 2u);
        }
        return;
    case ShieldRaise:
        agent.velocity = {};
        decrement(agent.slench_state_timer);
        if (agent.slench_state_timer == 0) {
            const std::uint32_t minimum = static_cast<std::uint32_t>(
                phase.min_static_shot_timer) * 2u;
            const std::uint32_t maximum = static_cast<std::uint32_t>(
                phase.max_static_shot_timer) * 2u;
            const std::uint32_t range = maximum >= minimum
                ? maximum - minimum + 1u : 1u;
            enter_state(Idle, minimum + rng_.random2(range));
        }
        return;
    case Idle:
        agent.velocity = {};
        if (agent.slench_state_timer > 0) {
            decrement(agent.slench_state_timer);
            return;
        }
        agent.slench_static_shots_remaining = static_cast<std::uint8_t>(
            std::min<std::uint16_t>(phase.static_shot_count, 255));
        enter_state(ShootTear, 0);
        return;
    case ShootTear:
        agent.velocity = {};
        if (agent.slench_static_shot_timer < 36u * 2u) {
            agent.slench_static_shot_timer += frame_step;
            if (target_index != players_.size()) {
                const net::Vec3 target = add(
                    players_[target_index].position, {0.0F, 0.5F, 0.0F});
                static_cast<void>(rotate_to_target(
                    target, fixed(phase.angle_increment3) * 0.5F
                        * static_cast<float>(frame_step)));
            }
            return;
        }
        if (agent.slench_static_shot_cooldown > 0) {
            decrement(agent.slench_static_shot_cooldown);
            return;
        }
        if (agent.slench_static_shots_remaining != 0) {
            agent.slench_firing_tear = true;
            spawn_enemy_projectile(
                agent, add(agent.position, multiply(agent.facing, 2.9F)),
                agent.facing);
            --agent.slench_static_shots_remaining;
            ++agent.slench_static_shot_counter;
            agent.slench_static_shot_cooldown = static_cast<std::uint32_t>(
                phase.static_shot_cooldown) * 2u;
            if (agent.slench_static_shots_remaining == 0) {
                agent.slench_state_timer = 36u * 2u;
            }
            return;
        }
        if (agent.slench_state_timer > 0) {
            decrement(agent.slench_state_timer);
            return;
        }
        enter_state(Roam, std::max<std::uint32_t>(
            90u, static_cast<std::uint32_t>(phase.roam_time) * 2u));
        return;
    case ShieldLower:
        agent.velocity = {};
        if (agent.slench_state_timer > 0) {
            decrement(agent.slench_state_timer);
            return;
        }
        if (agent.slench.subtype == 3
            || (rotate_to_target(agent.slench_detached_facing,
                                 fixed(phase.angle_increment1) * 0.5F
                                     * static_cast<float>(frame_step))
                && move_to_position(
                    agent.slench_detached_position,
                    fixed(phase.move_increment1) * 0.5F
                        * static_cast<float>(frame_step)))) {
            enter_state(Detach, 0);
        }
        return;
    case Return:
        if (length_squared(subtract(agent.slench_detached_position,
                                    agent.position)) <= 0.000001F) {
            enter_state(Attach, 0);
        } else {
            rotate_to_target(agent.slench_detached_position,
                             fixed(phase.angle_increment1) * 0.5F
                                 * static_cast<float>(frame_step));
            move_to_position(agent.slench_detached_position,
                             fixed(phase.move_increment2) * 0.5F
                                 * static_cast<float>(frame_step));
        }
        return;
    case Attach:
        if (rotate_to_target(agent.slench_detached_facing,
                             fixed(phase.angle_increment1) * 0.5F
                                 * static_cast<float>(frame_step))
            && move_to_position(
                agent.slench_start_position,
                fixed(phase.move_increment2) * 0.5F
                    * static_cast<float>(frame_step))) {
            for (auto& child : enemies_) {
                if (child.parent_enemy_id != agent.id
                    || !child.slench_synapse.supported) {
                    continue;
                }
                child.slench_part_state = 0; // Initial
                child.slench_part_timer = 0;
                child.slench_part_heal_timer = 0;
                child.slench_part_reappear_timer = 0;
            }
            enter_state(ShieldRaise, 30u * 2u);
        }
        return;
    case Detach:
        enter_state(Roam, 0);
        return;
    case RollingDone: {
        const net::Vec3 target = {
            agent.slench_detached_position.x,
            agent.slench_float_base_y,
            agent.slench_detached_position.z};
        if (length_squared(subtract(target, agent.position)) <= 0.000001F) {
            enter_state(Roam, 0);
        } else {
            rotate_to_target(target, fixed(phase.angle_increment4) * 0.5F
                                         * static_cast<float>(frame_step));
            move_to_position(target, fixed(phase.move_increment3) * 0.5F
                                       * static_cast<float>(frame_step));
        }
        return;
    }
    case SlamReady: {
        agent.position = agent.slench_destination2;
        const net::Vec3 target = agent.slench_destination1;
        rotate_to_target(target, fixed(phase.angle_increment5) * 0.5F
                                   * static_cast<float>(frame_step));
        const std::uint32_t rotation = phase.wobble_rotation_increment;
        const std::uint32_t cycles = phase.wobble_cycles;
        const std::uint32_t wobble_time = rotation == 0
            ? 1u : 360u / rotation * cycles;
        agent.slench_wobble_timer += frame_step;
        if (agent.slench_wobble_timer >= wobble_time * 2u) {
            enter_state(Slam, 0);
        } else {
            agent.slench_wobble_angle += static_cast<float>(rotation)
                * 0.5F * static_cast<float>(frame_step);
            while (agent.slench_wobble_angle >= 360.0F) {
                agent.slench_wobble_angle -= 360.0F;
            }
            const float maximum = phase.max_wobble_distance;
            const float factor = std::max(
                0.01F, maximum - static_cast<float>(
                    agent.slench_wobble_timer) * maximum
                    / static_cast<float>(wobble_time * 2u * 2u));
            agent.position = add(
                agent.slench_destination2,
                multiply(rotate_about_axis(
                             agent.up, agent.facing,
                             agent.slench_wobble_angle), factor));
        }
        return;
    }
    case Slam:
        if (move_to_position(
                agent.slench_destination1,
                fixed(phase.move_increment4) * 0.5F
                    * static_cast<float>(frame_step))) {
            enter_state(SlamReturn, 0);
        }
        return;
    case SlamReturn:
        if (move_to_position(
                agent.slench_destination2,
                fixed(phase.move_increment5) * 0.5F
                    * static_cast<float>(frame_step))) {
            enter_state(agent.slench_state_after_slam, 0);
        }
        return;
    case Roam: {
        const std::uint32_t phase_health = static_cast<std::uint32_t>(
            agent.health_max / 3u)
            * (2u - std::min<std::uint8_t>(agent.slench_phase, 2u));
        if (agent.health <= phase_health) {
            if (agent.slench_phase + 1u < profile.phases.size()) {
                ++agent.slench_phase;
            }
            ++agent.slench_cycle_count;
            enter_state(Return, 0);
            return;
        }

        if (agent.slench_roam_timer > 0) {
            decrement(agent.slench_roam_timer);
        }
        if (agent.slench_roam_timer == 0) {
            enter_state(Return, 0);
            return;
        }
        if (agent.health <= phase_health + agent.health_max / 12u) {
            agent.slench_flags |= Wobbling;
        }

        const net::Vec3 horizontal = normalized_or(
            {agent.slench_target_horizontal.x, 0.0F,
             agent.slench_target_horizontal.z},
            {0.0F, 0.0F, 1.0F});
        const bool rolling = (agent.slench_flags & Rolling) != 0;
        if (rolling) {
            if (agent.slench_roll_timer > 0) {
                decrement(agent.slench_roll_timer);
            }
            if (agent.slench_roll_timer == 0) {
                enter_state(RollingDone, 0);
                return;
            }
            drop_to_floor();
            if (agent.slench_hit_floor) {
                agent.slench_pattern_angle += fixed(
                    phase.rolling_angle_increment) * 0.5F
                    * static_cast<float>(frame_step);
                if (agent.slench_pattern_angle >= 360.0F) {
                    agent.slench_pattern_angle -= 360.0F;
                    agent.slench_flags ^= PatternFlip1;
                }
                const bool flipped = (agent.slench_flags & PatternFlip1) != 0;
                const float factor = phase.rolling_speed;
                const float angle = flipped
                    ? 360.0F - agent.slench_pattern_angle
                    : agent.slench_pattern_angle + 180.0F;
                const net::Vec3 offset = multiply(horizontal,
                                                   flipped ? -factor : factor);
                const net::Vec3 center = add(
                    agent.slench_detached_position, offset);
                agent.position = add(
                    center,
                    multiply(rotate_about_axis(horizontal,
                                               {0.0F, 1.0F, 0.0F}, angle),
                             factor));
                agent.facing = normalized_or(
                    subtract(agent.position, center), agent.facing);
            }
        } else {
            agent.slench_pattern_angle += fixed(
                phase.floating_angle_increment) * 0.5F
                * static_cast<float>(frame_step);
            if (agent.slench_pattern_angle >= 360.0F) {
                agent.slench_pattern_angle -= 360.0F;
                agent.slench_flags ^= PatternFlip1;
            }
            const bool flipped = (agent.slench_flags & PatternFlip1) != 0;
            const float factor = phase.floating_speed;
            const float angle = flipped
                ? 540.0F - agent.slench_pattern_angle
                : agent.slench_pattern_angle;
            net::Vec3 base = add(
                agent.slench_detached_position,
                multiply(horizontal, flipped ? factor : -factor));
            base.y = agent.slench_float_base_y;
            const net::Vec3 orbit_axis = normalized_or(
                cross(horizontal, {0.0F, 1.0F, 0.0F}),
                {1.0F, 0.0F, 0.0F});
            agent.position = add(
                base,
                multiply(rotate_about_axis(horizontal, orbit_axis, angle),
                         factor));
            if (agent.slench.subtype != 0) {
                const float increment = fixed(phase.move_increment3) * 0.5F
                    * static_cast<float>(frame_step);
                agent.slench_float_base_y +=
                    (agent.slench_flags & PatternFlip2) != 0
                    ? -increment : increment;
                const float limit = fixed(phase.roll_time);
                if (agent.slench_float_base_y < 0.0F
                    || agent.slench_float_base_y >= limit) {
                    agent.slench_flags ^= PatternFlip2;
                }
                agent.position = add(
                    agent.position,
                    multiply(horizontal, agent.slench_float_base_y));
            }
            const bool aligned = rotate_to_target(
                target_index != players_.size()
                    ? add(players_[target_index].position,
                          {0.0F, 0.5F, 0.0F})
                    : add(agent.position, agent.facing),
                fixed(phase.angle_increment4) * 0.5F
                    * static_cast<float>(frame_step));
            if (!aligned) {
                agent.slench_flags &= static_cast<std::uint16_t>(
                    ~TargetingPlayer);
            } else if (target_index != players_.size()) {
                agent.slench_flags = rng_.random2(100) < 50
                    ? static_cast<std::uint16_t>(
                        agent.slench_flags | TargetingPlayer)
                    : static_cast<std::uint16_t>(
                        agent.slench_flags & ~TargetingPlayer);
            }
            if (target_index != players_.size()
                && (agent.slench.subtype == 2
                    || agent.slench.subtype == 3)) {
                static_cast<void>(set_up_slam(add(
                    players_[target_index].position, {0.0F, 0.5F, 0.0F})));
                if (agent.slench_state != Roam) {
                    return;
                }
            }
        }

        if (target_index != players_.size()
            && distance_squared(players_[target_index].position,
                                agent.position) <= 3.9F * 3.9F
            && agent.attack_timer <= 0.0F) {
            apply_enemy_contact_damage(agent, players_[target_index],
                                       profile.contact_damage);
            agent.attack_timer = 0.5F;
        }
        return;
    }
    case Dead:
        return;
    default:
        enter_state(Initial, 0);
        return;
    }
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

SlenchProfile decode_slench_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin, std::string_view room_name) noexcept {
    SlenchProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::Slench)
        || fields.size() < 64) {
        return result;
    }

    // Enemy41Entity has no subtype word in S00. The four story rooms are the
    // selector used by the managed constructor; unknown/custom rooms retain
    // the Alinos (v1) defaults just like the C# fallback.
    if (room_name == "UNIT4_B1") {
        result.subtype = 1;
    } else if (room_name == "UNIT2_B2") {
        result.subtype = 2;
    } else if (room_name == "UNIT3_B2") {
        result.subtype = 3;
    }
    result.hurt_volume = detail::read_volume(fields, 0, origin);

    struct PhaseValues {
        std::uint16_t scan_id1;
        std::uint16_t scan_id2;
        std::int32_t angle_increment1;
        std::int32_t health;
        std::int32_t angle_increment2;
        std::uint16_t min_static_shot_timer;
        std::uint16_t max_static_shot_timer;
        std::int16_t static_shot_cooldown;
        std::uint8_t static_shot_count;
        std::uint8_t padding17;
        std::int32_t angle_increment3;
        std::int32_t move_increment1;
        std::int32_t move_increment2;
        std::int32_t angle_increment4;
        std::int32_t roam_time;
        std::int32_t move_increment3;
        std::int32_t roll_time;
        std::int32_t floating_angle_increment;
        std::int32_t rolling_angle_increment;
        std::int32_t floating_speed;
        std::int32_t rolling_speed;
        std::int32_t angle_increment5;
        std::int32_t slam_range;
        std::int32_t move_increment4;
        std::int32_t move_increment5;
        std::uint16_t slam_delay;
        std::uint8_t wobble_cycles;
        std::uint8_t wobble_rotation_increment;
        std::int32_t max_wobble_distance;
        std::uint16_t magic;
        std::uint16_t padding5e;
    };
    // Metadata.Enemy41Values is laid out as four variants with three phase
    // rows each. Values here are copied in their authored fixed-point form;
    // conversion below keeps the gameplay controller independent of C#'s
    // Fixed helper and preserves the room-specific state machine cadence.
    static constexpr std::array<std::array<PhaseValues, 3>, 4> values{{
        {{
            {227, 201, 16384, 200, 4096, 60, 120, 30, 1, 0, 4096,
             1024, 614, 8192, 600, 410, 81920, 10240, 0, 12288, 0, 0,
             0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
            {0, 0, 16384, 0, 4096, 60, 120, 30, 2, 0, 4096, 1024, 614,
             16384, 450, 410, 81920, 18432, 0, 15565, 0, 0, 0, 0, 0,
             0, 2, 64, 2048, 0xBEEF, 0},
            {0, 0, 16384, 0, 4096, 60, 120, 20, 2, 0, 4096, 1024, 614,
             16384, 300, 410, 81920, 26624, 0, 13517, 0, 0, 0, 0, 0,
             0, 2, 64, 2048, 0xBEEF, 0}
        }},
        {{
            {202, 203, 16384, 1200, 4096, 60, 120, 25, 1, 0, 4096,
             1024, 614, 16384, 600, 410, 81920, 14336, 0, 14336, 0, 0,
             0, 0, 0, 0, 2, 64, 2048, 0xBEEF, 0},
            {0, 0, 16384, 0, 4096, 60, 120, 25, 2, 0, 4096, 1024, 614,
             16384, 540, 819, 81920, 18432, 0, 16384, 0, 0, 0, 0, 0,
             0, 2, 64, 2048, 0xBEEF, 0},
            {0, 0, 16384, 0, 4096, 60, 120, 25, 3, 0, 4096, 1024, 614,
             16384, 450, 1229, 81920, 24576, 0, 16384, 0, 0, 0, 0, 0,
             0, 2, 64, 2048, 0xBEEF, 0}
        }},
        {{
            {204, 205, 32768, 900, 4096, 60, 120, 25, 2, 0, 4096,
             1024, 614, 16384, 750, 819, 81920, 14336, 0, 14336, 0,
             16384, 40960, 4096, 3277, 150, 10, 32, 2048, 0xBEEF, 0},
            {0, 0, 32768, 0, 4096, 60, 120, 25, 3, 0, 4096, 1024, 614,
             16384, 660, 819, 81920, 18432, 0, 16384, 0, 32768, 40960,
             4915, 3277, 120, 10, 48, 2048, 0xBEEF, 0},
            {0, 0, 32768, 0, 4096, 60, 120, 20, 3, 0, 4096, 1024, 614,
             16384, 600, 819, 81920, 24576, 0, 16384, 0, 32768, 40960,
             6144, 3277, 120, 12, 55, 2048, 0xBEEF, 0}
        }},
        {{
            {206, 223, 32768, 800, 4096, 60, 120, 20, 2, 0, 4096,
             1024, 614, 16384, 1500, 819, 600, 14336, 12288, 14336,
             22528, 32768, 40960, 4096, 3277, 150, 10, 50, 2048, 0xBEEF,
             0},
            {0, 0, 32768, 0, 4096, 60, 120, 20, 3, 0, 4096, 1024, 614,
             16384, 1500, 600, 819, 18432, 14336, 16384, 24576, 32768,
             40960, 4915, 3277, 120, 10, 55, 2048, 0xBEEF, 0},
            {0, 0, 32768, 36, 4096, 60, 120, 20, 4, 0, 4096, 1024, 614,
             16384, 1800, 900, 819, 26624, 16384, 16384, 24576, 32768,
             49152, 6144, 3277, 120, 12, 65, 2048, 0xBEEF, 0}
        }}
    }};

    static constexpr std::array<std::uint8_t, 4> projectile_weapons{
        0, 6, 2, 5};
    static constexpr std::array<std::uint16_t, 4> projectile_damage{
        3, 2, 4, 5};
    static constexpr std::array<std::uint8_t, 4> projectile_draw_func{
        21, 4, 2, 3};
    static constexpr std::array<std::uint16_t, 4> projectile_color{
        9055, 15711, 32767, 32404};
    static constexpr std::array<std::uint8_t, 4> projectile_collision{
        242, 9, 89, 10};
    static constexpr std::array<std::uint8_t, 4> projectile_muzzle{
        65, 64, 60, 62};
    static constexpr std::array<float, 4> projectile_speed{
        2662.0F / 4096.0F * 60.0F,
        1638.0F / 4096.0F * 60.0F,
        3276.0F / 4096.0F * 60.0F,
        1433.0F / 4096.0F * 60.0F};
    static constexpr std::array<float, 4> projectile_lifetime{
        255.0F / 60.0F, 255.0F / 60.0F, 90.0F / 60.0F,
        255.0F / 60.0F};

    const std::size_t subtype = std::min<std::size_t>(
        result.subtype, values.size() - 1);
    result.health = static_cast<std::uint16_t>(values[subtype][0].health);
    result.contact_damage = 35;
    result.projectile_weapon = projectile_weapons[subtype];
    result.projectile_damage = projectile_damage[subtype];
    result.projectile_draw_func = projectile_draw_func[subtype];
    result.projectile_color = projectile_color[subtype];
    result.projectile_collision_effect = projectile_collision[subtype];
    result.projectile_muzzle_effect = projectile_muzzle[subtype];
    result.projectile_speed = projectile_speed[subtype];
    result.projectile_lifetime = projectile_lifetime[subtype];
    for (std::size_t phase = 0; phase < result.phases.size(); ++phase) {
        const auto& source = values[subtype][phase];
        auto& destination = result.phases[phase];
        destination.scan_id1 = source.scan_id1;
        destination.scan_id2 = source.scan_id2;
        destination.angle_increment1 = source.angle_increment1;
        destination.health = source.health;
        destination.angle_increment2 = source.angle_increment2;
        destination.min_static_shot_timer = source.min_static_shot_timer;
        destination.max_static_shot_timer = source.max_static_shot_timer;
        destination.static_shot_cooldown = source.static_shot_cooldown;
        destination.static_shot_count = source.static_shot_count;
        destination.padding17 = source.padding17;
        destination.angle_increment3 = source.angle_increment3;
        destination.move_increment1 = source.move_increment1;
        destination.move_increment2 = source.move_increment2;
        destination.angle_increment4 = source.angle_increment4;
        destination.roam_time = source.roam_time;
        destination.move_increment3 = source.move_increment3;
        destination.roll_time = source.roll_time;
        destination.floating_angle_increment = source.floating_angle_increment;
        destination.rolling_angle_increment = source.rolling_angle_increment;
        destination.floating_speed = static_cast<float>(
            source.floating_speed) / 4096.0F;
        destination.rolling_speed = static_cast<float>(
            source.rolling_speed) / 4096.0F;
        destination.angle_increment5 = source.angle_increment5;
        destination.slam_range = static_cast<float>(source.slam_range)
            / 4096.0F;
        destination.move_increment4 = source.move_increment4;
        destination.move_increment5 = source.move_increment5;
        destination.slam_delay = source.slam_delay;
        destination.wobble_cycles = source.wobble_cycles;
        destination.wobble_rotation_increment =
            source.wobble_rotation_increment;
        destination.max_wobble_distance = static_cast<float>(
            source.max_wobble_distance) / 4096.0F;
        destination.magic = source.magic;
        destination.padding5e = source.padding5e;
    }
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy
