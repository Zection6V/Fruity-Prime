#include "28_Gorea1B.hpp"
#include "enemy_common.hpp"
#include "gorea_common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace fruityprime::gameplay {

namespace {

constexpr std::uint32_t ArmDead = 1u;

[[nodiscard]] constexpr std::uint8_t type_id(formats::EnemyType type) noexcept {
    return static_cast<std::uint8_t>(type);
}

[[nodiscard]] bool in_volume(const EnemyState& enemy,
                             net::Vec3 position) noexcept {
    const float radius = enemy.gorea_volume_radius > 0.01F
        ? enemy.gorea_volume_radius : 20.0F;
    return distance_squared(position, enemy.gorea_anchor)
        <= radius * radius;
}

constexpr std::size_t GrapplePointCount = 24;
constexpr float GrapplePointThresholdSquared = 1.0F / 128.0F;
constexpr float GrappleSegmentLength = 2986.0F / 4096.0F;

void set_grapple_line(EnemyState& enemy, net::Vec3 first,
                      net::Vec3 last) noexcept {
    const net::Vec3 between = subtract(last, first);
    for (std::size_t index = 0; index < GrapplePointCount; ++index) {
        const float factor = static_cast<float>(index)
            / static_cast<float>(GrapplePointCount - 1);
        enemy.gorea_grapple_points[index] = add(
            first, multiply(between, factor));
    }
}

[[nodiscard]] net::Vec3 grapple_point(const EnemyState& enemy,
                                      float index) noexcept {
    const float clamped = std::clamp(
        index, 0.0F, static_cast<float>(GrapplePointCount - 1));
    const auto lower = static_cast<std::size_t>(clamped);
    const float fraction = clamped - static_cast<float>(lower);
    if (lower + 1 >= GrapplePointCount || fraction == 0.0F) {
        return enemy.gorea_grapple_points[lower];
    }
    return add(enemy.gorea_grapple_points[lower], multiply(
        subtract(enemy.gorea_grapple_points[lower + 1],
                 enemy.gorea_grapple_points[lower]), fraction));
}

[[nodiscard]] float grapple_length(const EnemyState& enemy) noexcept {
    float result = 0.0F;
    for (std::size_t index = 1; index < GrapplePointCount; ++index) {
        result += std::sqrt(std::max(
            0.0F, distance_squared(enemy.gorea_grapple_points[index],
                                    enemy.gorea_grapple_points[index - 1])));
    }
    return result;
}

void adjust_grapple_segment_lengths(EnemyState& enemy,
                                    float factor) noexcept {
    for (std::size_t index = 1; index < GrapplePointCount; ++index) {
        const net::Vec3 between = subtract(
            enemy.gorea_grapple_points[index],
            enemy.gorea_grapple_points[index - 1]);
        if (length_squared(between) <= GrapplePointThresholdSquared) {
            continue;
        }
        const net::Vec3 scaled = multiply(normalized_or(
            between, {}), factor);
        const net::Vec3 remaining = subtract(scaled, between);
        for (std::size_t update = index; update < GrapplePointCount;
             ++update) {
            enemy.gorea_grapple_points[update] = add(
                enemy.gorea_grapple_points[update], remaining);
        }
    }
}

void update_grapple_spring(EnemyState& enemy) noexcept {
    const float distance = std::sqrt(std::max(
        0.0F, distance_squared(enemy.gorea_grapple_points[0],
                                enemy.gorea_grapple_points[1])));
    enemy.gorea_grapple_points[1] = add(
        enemy.gorea_grapple_points[1],
        multiply(enemy.gorea_grapple_field10, 0.5F));
    if (distance <= 1.0F / 128.0F) {
        return;
    }

    net::Vec3 start = subtract(enemy.gorea_grapple_points[1],
                               enemy.gorea_grapple_points[0]);
    for (std::size_t index = 2; index < GrapplePointCount; ++index) {
        const net::Vec3 between = subtract(
            enemy.gorea_grapple_points[index],
            enemy.gorea_grapple_points[index - 1]);
        if (length_squared(between) <= GrapplePointThresholdSquared) {
            continue;
        }
        const float axis_length_squared = length_squared(start);
        const net::Vec3 projected = axis_length_squared <= 0.0F
            ? net::Vec3{}
            : multiply(start, dot(between, start) / axis_length_squared);
        const net::Vec3 update = add(
            enemy.gorea_grapple_points[index],
            multiply(subtract(projected, between),
                     enemy.gorea_grapple_field30));
        enemy.gorea_grapple_points[index] = update;
        start = subtract(update, enemy.gorea_grapple_points[index - 1]);
    }
}

void update_grapple_draw_values(EnemyState& enemy,
                                net::Vec3 sphere_position,
                                std::uint32_t step) noexcept {
    if (!enemy.gorea_grappling) {
        return;
    }
    enemy.gorea_grapple_points[0] = sphere_position;
    const net::Vec3 between = subtract(
        enemy.gorea_grapple_points[GrapplePointCount - 1],
        enemy.gorea_grapple_points[0]);
    if (length_squared(between) > GrapplePointThresholdSquared) {
        enemy.gorea_grapple_field21c += 1.5F / 2.0F
            * static_cast<float>(step);
        while (enemy.gorea_grapple_field21c >= 360.0F) {
            enemy.gorea_grapple_field21c -= 360.0F;
        }
    }
    if (static_cast<int>(enemy.gorea_grapple_field38)
        < static_cast<int>(GrapplePointCount - 1)) {
        enemy.gorea_grapple_field38 += enemy.gorea_grapple_field34
            / 2.0F * static_cast<float>(step);
    }
    enemy.gorea_grapple_int += enemy.gorea_grapple_field28
        / 2.0F * static_cast<float>(step);
    if (std::round(enemy.gorea_grapple_int)
        > static_cast<float>(GrapplePointCount)) {
        enemy.gorea_grapple_int -=
            std::fmod(enemy.gorea_grapple_int, 1.0F);
    }
}

void seek_grapple_target_facing(EnemyState& enemy,
                                net::Vec3 target,
                                float angle_degrees = 3.0F) noexcept {
    target.y = 0.0F;
    if (length_squared(target) <= GrapplePointThresholdSquared) {
        return;
    }
    target = normalized_or(target, enemy.facing);
    const net::Vec3 facing = normalized_or(
        {enemy.facing.x, 0.0F, enemy.facing.z},
        {0.0F, 0.0F, 1.0F});
    constexpr float Pi = 3.14159265358979323846F;
    const float cosine = std::cos(angle_degrees * Pi / 180.0F);
    if (dot(target, facing) < cosine) {
        const net::Vec3 cross_product = cross(target, facing);
        const float angle = cross_product.y > 0.0F
            ? -angle_degrees : angle_degrees;
        enemy.facing = normalized_or(rotate_about_axis(
            facing, {0.0F, 1.0F, 0.0F}, angle), facing);
        if (enemy.gorea_grappling) {
            const net::Vec3 first = enemy.gorea_grapple_points[0];
            const net::Vec3 last = enemy.gorea_grapple_points[
                GrapplePointCount - 1];
            set_grapple_line(enemy, first, add(first, rotate_about_axis(
                subtract(last, first), {0.0F, 1.0F, 0.0F}, angle)));
        }
    } else {
        enemy.facing = target;
    }
}

[[nodiscard]] bool check_facing_position(const EnemyState& enemy,
                                         net::Vec3 position,
                                         float minimum_cosine) noexcept {
    const net::Vec3 facing = {enemy.facing.x, 0.0F, enemy.facing.z};
    const net::Vec3 between = subtract(position, enemy.position);
    if (length_squared(facing) <= GrapplePointThresholdSquared
        || length_squared({between.x, 0.0F, between.z})
            <= GrapplePointThresholdSquared) {
        return false;
    }
    return dot(normalized_or({between.x, 0.0F, between.z}, {}),
               normalized_or(facing, {})) > minimum_cosine;
}

void select_animation(EnemyState& enemy, std::uint8_t state) noexcept {
    // Enemy28Entity's state callbacks use animation completion as their
    // subroutine boundary. These lengths are the gameplay-relevant cursor;
    // a renderer can map the same indices to the model table later.
    switch (state) {
    case 0: gorea::set_animation(enemy, 3, 45); break;
    case 1: gorea::set_animation(enemy, 3, 30, true); break;
    case 2: gorea::set_animation(enemy, 3, 30, true); break;
    case 3: gorea::set_animation(enemy, 3, 30, true); break;
    case 4: gorea::set_animation(enemy, 1, 30); break;
    case 5: gorea::set_animation(enemy, 2, 30, true); break;
    case 6: gorea::set_animation(enemy, 2, 60, true); break;
    case 7: gorea::set_animation(enemy, 4, 90, true); break;
    case 8: gorea::set_animation(enemy, 2, 90, true); break;
    case 9: gorea::set_animation(enemy, 8, 90); break;
    case 10: gorea::set_animation(enemy, 3, 30, true); break;
    // Enemy28.State11 does not select a new model group. The old native
    // mapping used group 9, but Gorea1B_Anim.bin has groups 0..8 only; keep
    // the ordinary loop pose instead of asking the renderer for a nonexistent
    // group.
    case 11: gorea::set_animation(enemy, 3, 60, true); break;
    // Behavior03 selects animation 4 before entering State12. State12 then
    // advances 4 -> 2 -> 8 before Deactivate() reopens the 1A phase.
    case 12: gorea::set_animation(enemy, 4, 90); break;
    case 13: gorea::set_animation(enemy, 3, 30, true); break;
    default: gorea::set_animation(enemy, 3, 30, true); break;
    }
}

} // namespace

void Session::update_gorea_1b(EnemyState& agent) {
    // Enemy28Entity is a complete linked state machine in the managed tree:
    // it walks inside the S11 sphere, grapples the player, swings/throws the
    // player, and advances the seal-sphere phase counter. Keep those
    // gameplay transitions here; only the 24-segment rope mesh is a frontend
    // concern.
    auto parent = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.parent_enemy_id](const EnemyState& value) {
            return parent_id != 0 && value.id == parent_id
                && value.enemy_type == type_id(formats::EnemyType::Gorea1A);
        });
    if (parent == enemies_.end()) {
        agent.active = false;
        return;
    }

    const std::uint32_t step = gorea::frame_step(config_.tick_seconds);
    // target_slot is cleared below for the ordinary reacquisition pass, but
    // a grapple can be interrupted before that pass runs. Keep the slot from
    // the previous frame so every deactivation path releases the same biped
    // lock that State06/07/08/09 acquired.
    const std::uint8_t grapple_slot = agent.target_slot;
    const float frame_scale = static_cast<float>(step);
    agent.health = 65535;
    agent.health_max = 65535;
    agent.invulnerable = true;
    // Enemy28 keeps the same victim for the whole rope state machine.  A
    // free target is reacquired every frame only while not grappling; clearing
    // this field unconditionally would silently switch victims during the
    // hold/swing/lift states.
    if (!agent.gorea_grappling) {
        agent.target_slot = 0xff;
    }

    auto sphere = std::find_if(
        enemies_.begin(), enemies_.end(),
        [id = agent.id](const EnemyState& value) {
            return value.parent_enemy_id == id
                && value.enemy_type == type_id(
                    formats::EnemyType::GoreaSealSphere1);
        });

    auto set_biped_lock = [this](std::uint8_t slot, bool locked) {
        const auto input = std::find_if(
            inputs_.begin(), inputs_.end(),
            [slot](const RuntimeInput& value) { return value.slot == slot; });
        if (input != inputs_.end()) {
            input->biped_lock = locked;
        }
    };

    auto target_for_slot = [this](std::uint8_t slot) {
        return std::find_if(
            players_.begin(), players_.end(),
            [slot](const net::PlayerState& value) {
                return value.slot_index == slot && objective_player(value);
            });
    };

    const auto trocra_type = type_id(formats::EnemyType::Trocra);

    // Enemy28Entity owns the thirty Trocra references in the managed tree.
    // The room spawners themselves are still native runtime entities, so keep
    // the same ownership boundary here: activating a phase selects the next
    // inactive Trocra spawners, while the per-crystal link is represented by
    // parent_enemy_id and trocra_slot on the spawned EnemyState.
    auto activate_trocra_spawns = [&]() {
        std::size_t remaining = agent.gorea_phases_left == 3 ? 1 : 2;
        for (auto& spawn : enemy_spawns_) {
            if (spawn.spawner == nullptr
                || spawn.spawner->enemy_type() != trocra_type
                || spawn.spawner->spawner_active()) {
                continue;
            }
            spawn.spawner->activate(true);
            if (agent.gorea_phases_left != 1 && --remaining == 0) {
                break;
            }
        }
    };

    auto deactivate_trocra_spawns = [&]() {
        for (auto& spawn : enemy_spawns_) {
            if (spawn.spawner != nullptr
                && spawn.spawner->enemy_type() == trocra_type) {
                spawn.spawner->activate(false);
            }
        }
    };

    auto explode_linked_trocras = [&]() {
        for (auto& child : enemies_) {
            if (!child.active || child.parent_enemy_id != agent.id
                || child.enemy_type != trocra_type
                || child.trocra_state == 3) {
                continue;
            }
            child.health = 0;
            child.visible = false;
            child.invulnerable = true;
            child.velocity = {};
            spawn_effect(75, child.position, {1.0F, 0.0F, 0.0F}, child.id);
        }
    };

    auto select_trocra = [&]() {
        std::array<bool, 30> used{};
        for (const auto& child : enemies_) {
            if (!child.active || child.parent_enemy_id != agent.id
                || child.enemy_type != trocra_type
                || child.trocra_state == 3
                || child.trocra_slot >= used.size()) {
                continue;
            }
            used[child.trocra_slot] = true;
        }

        const auto free_slot = std::find(used.begin(), used.end(), false);
        if (free_slot == used.end()) {
            return;
        }
        const auto slot = static_cast<std::uint8_t>(
            std::distance(used.begin(), free_slot));

        auto first = enemies_.end();
        auto preferred = enemies_.end();
        for (auto iterator = enemies_.begin(); iterator != enemies_.end();
             ++iterator) {
            auto& child = *iterator;
            if (!child.active || child.health == 0
                || child.parent_enemy_id != 0
                || child.enemy_type != trocra_type
                || child.trocra_state != 0) {
                continue;
            }
            if (first == enemies_.end()) {
                first = iterator;
            }
            const net::Vec3 between = subtract(child.position,
                                               agent.position);
            if (length_squared({between.x, 0.0F, between.z})
                    > 1.0F / 128.0F
                && dot(normalized_or({between.x, 0.0F, between.z},
                                     agent.facing), agent.facing) < 0.0F) {
                preferred = iterator;
                break;
            }
        }
        if (first == enemies_.end()) {
            return;
        }
        auto chosen = preferred == enemies_.end() ? first : preferred;
        chosen->parent_enemy_id = agent.id;
        chosen->trocra_slot = slot;
        chosen->trocra_state = 1;
        chosen->trocra_state_timer = 0;
        chosen->trocra_field174 = chosen->position;
        chosen->trocra_previous_position = chosen->position;
        chosen->velocity = {};
    };

    auto advance_trocra_launches = [&]() {
        for (auto& child : enemies_) {
            if (!child.active || child.parent_enemy_id != agent.id
                || child.enemy_type != trocra_type
                || child.trocra_state == 0 || child.trocra_state == 3) {
                continue;
            }

            if (child.trocra_state == 1) {
                const net::Vec3 between = subtract(child.position,
                                                   agent.position);
                const net::Vec3 horizontal{between.x, 0.0F, between.z};
                if (length_squared(horizontal) <= 1.0F / 128.0F) {
                    child.trocra_state = 2;
                    child.trocra_state_timer = 45u * 2u;
                    child.trocra_field174 = child.position;
                    child.velocity = {};
                    continue;
                }
                const net::Vec3 destination = add(
                    agent.gorea_anchor,
                    add(multiply(normalized_or(horizontal, agent.facing), 5.0F),
                        {0.0F, 10.0F, 0.0F}));
                const net::Vec3 direction = subtract(destination,
                                                      child.position);
                const float direction_length = std::sqrt(std::max(
                    0.0F, length_squared(direction)));
                if (direction_length > 0.01F) {
                    child.trocra_previous_position = child.position;
                    child.velocity = multiply(
                        direction,
                        ((1.0F / 7.0F) / 2.0F) * frame_scale
                            / direction_length);
                    child.position = add(child.position, child.velocity);
                } else {
                    child.velocity = {};
                }
                continue;
            }

            child.trocra_previous_position = child.position;
            child.trocra_field174 = add(child.trocra_field174,
                                        agent.velocity);
            if (child.trocra_state_timer > 0) {
                gorea::decrement(child.trocra_state_timer, step);
                const auto jitter = [this]() {
                    return (static_cast<float>(rng_.random2(4096u))
                            - 2048.0F) / 4096.0F;
                };
                child.position = add(child.trocra_field174,
                                     {jitter(), jitter(), jitter()});
            }
            if (child.trocra_state_timer != 0) {
                continue;
            }

            const auto target_index = gorea::nearest_player(
                players_, child.position, std::numeric_limits<float>::max());
            const net::Vec3 target = target_index == players_.size()
                ? add(child.position, agent.facing)
                : players_[target_index].position;
            const net::Vec3 direction = subtract(target, child.position);
            const net::Vec3 speed = length_squared(direction)
                    >= 1.0F / 128.0F
                ? multiply(normalized_or(direction, agent.facing), 0.35F)
                : net::Vec3{};
            child.velocity = speed;
            child.trocra_state = 3;
            SoundEvent sound;
            sound.cue = SoundCue::GoreaAttack3A;
            sound.enemy_type = child.enemy_type;
            sound.entity_id = child.id;
            sound.position = child.position;
            emit_sound(sound);
        }
    };

    auto release_grapple = [&](net::PlayerState* target) {
        if (target == nullptr && grapple_slot != 0xff) {
            const auto retained = target_for_slot(grapple_slot);
            if (retained != players_.end()) {
                target = &*retained;
            }
        }
        if (target != nullptr) {
            set_biped_lock(target->slot_index, false);
            target->speed = {};
        }
        agent.gorea_grappling = false;
        agent.gorea_grapple_phase = 0;
        agent.gorea_flags &= ~(gorea::Gorea1BGrappling
                               | gorea::Gorea1BSwingDirection);
        agent.gorea_grapple_timer = 0;
    };

    if (!agent.gorea_activated) {
        agent.gorea_flags &= ~gorea::Gorea1BActivated;
        if (agent.gorea_grappling) {
            auto target = target_for_slot(agent.target_slot);
            release_grapple(target == players_.end() ? nullptr : &*target);
        }
        agent.visible = false;
        agent.gorea_targetable = false;
        return;
    }

    const bool first_activation = (agent.gorea_flags
                                   & gorea::Gorea1BActivated) == 0;
    if (first_activation) {
        agent.gorea_flags |= gorea::Gorea1BActivated;
        activate_trocra_spawns();
        agent.facing = parent->facing;
        agent.up = parent->up;
    }

    agent.visible = agent.gorea_state != 13;
    agent.gorea_targetable = agent.visible;
    if (agent.gorea_volume_radius <= 0.01F) {
        agent.gorea_anchor = parent->position;
        agent.gorea_volume_radius = 20.0F;
    }

    if (!agent.gorea_state_initialized) {
        // Activate() does not reset Enemy28's model. When the next 1A phase
        // hands control back, State00 must observe the already-ended anim8
        // and make the sphere vulnerable immediately, just like C#.
        const bool preserve_phase_end = agent.gorea_state == 0
            && agent.gorea_model_animation == 8
            && gorea::animation_ended(agent);
        if (!preserve_phase_end) {
            select_animation(agent, agent.gorea_state);
        }
        agent.gorea_state_initialized = true;
        if (agent.gorea_state == 0) {
            agent.position = parent->position;
            agent.behavior_origin = parent->position;
            agent.gorea_flags &= ~gorea::Gorea1BIntroDone;
        }
    }
    // Enemy28 inherits EntityBase's post-callback animation update. Mirror
    // that one-frame delay and the managed 30 Hz animation cadence.
    if (gorea::should_advance_animation(tick_count_)) {
        static_cast<void>(gorea::advance_animation(agent, step));
    }

    if (sphere != enemies_.end()) {
        sphere->position = gorea::local_position(agent,
                                                 {0.0F, 0.0F, 0.35F});
        sphere->gorea_activated = agent.visible && agent.gorea_state != 13;
        sphere->gorea_targetable = sphere->gorea_activated;
        sphere->visible = false;
        sphere->invulnerable = !sphere->gorea_targetable;
    }

    auto enter = [&agent](std::uint8_t state) {
        gorea::enter_state(agent, state);
        select_animation(agent, state);
        agent.gorea_state_initialized = true;
    };

    auto begin_grapple = [&](net::Vec3 target_position) {
        // Enemy28.Func213BCB8: the endpoint starts at the seal sphere and
        // every one of the 24 points is initialized to the same straight
        // line. The old native controller kept only a scalar progress value,
        // which made the later spring/throw states fundamentally different.
        agent.gorea_grappling = true;
        agent.gorea_flags |= gorea::Gorea1BGrappling;
        agent.gorea_grapple_timer = 120u * 2u;
        agent.gorea_grapple_field38 = 0.0F;
        const net::Vec3 first = sphere == enemies_.end()
            ? agent.position : sphere->position;
        set_grapple_line(agent, first, target_position);
        agent.gorea_grapple_points[0] = first;
    };

    auto update_grapple_extension = [&](net::PlayerState& target) {
        const net::Vec3 current = grapple_point(
            agent, agent.gorea_grapple_field38);
        const net::Vec3 desired = add(target.position,
                                      {0.0F, 0.05F, 0.0F});
        net::Vec3 between = subtract(desired, current);
        if (length_squared(between) >= 0.25F * 0.25F) {
            between = normalized_or(between, {});
            agent.gorea_grapple_points[GrapplePointCount - 1] = add(
                agent.gorea_grapple_points[GrapplePointCount - 1],
                multiply(between, 0.3F / 2.0F * frame_scale));

            // This is the native equivalent of CollisionDetection's
            // CheckBetweenPoints call. A radius-zero sweep preserves the
            // managed bugfix boundary: the endpoint is only clamped when a
            // real room plane is crossed.
            if (const auto hit = collision::sweep_sphere(
                    room_.collision(),
                    to_collision(agent.gorea_grapple_points[0]),
                    to_collision(agent.gorea_grapple_points[
                        GrapplePointCount - 1]), 0.0F)) {
                net::Vec3 to_collision_point = {
                    hit->center.x - agent.gorea_grapple_points[0].x,
                    hit->center.y - agent.gorea_grapple_points[0].y,
                    hit->center.z - agent.gorea_grapple_points[0].z};
                const float collision_distance = std::sqrt(std::max(
                    0.0F, length_squared(to_collision_point)));
                if (collision_distance > 20.0F) {
                    to_collision_point = multiply(
                        normalized_or(to_collision_point, {}), 20.0F);
                }
                agent.gorea_grapple_points[GrapplePointCount - 1] = add(
                    agent.gorea_grapple_points[0], to_collision_point);
            }
            set_grapple_line(agent, agent.gorea_grapple_points[0],
                             agent.gorea_grapple_points[
                                 GrapplePointCount - 1]);
            agent.gorea_target_position = agent.gorea_grapple_points[
                GrapplePointCount - 1];
            return false;
        }

        agent.gorea_grapple_field234 = sphere == enemies_.end()
            ? 0 : static_cast<std::int32_t>(sphere->gorea_damage);
        agent.gorea_grapple_field38 = static_cast<float>(
            GrapplePointCount - 1);
        set_grapple_line(agent, agent.gorea_grapple_points[0], current);
        agent.gorea_flags |= gorea::Gorea1BGrappling;
        agent.gorea_grapple_phase = 2;
        const float distance = std::sqrt(std::max(
            0.0F, distance_squared(agent.gorea_grapple_points[0],
                                    agent.gorea_grapple_points[
                                        GrapplePointCount - 1])));
        float scale = 1.0F;
        if (distance > 14.0F) {
            scale = 1.0F - std::min(1.0F, distance / 17.5F) * 0.07F;
        }
        agent.gorea_grapple_field30 = scale;
        set_biped_lock(target.slot_index, true);
        target.flags &= static_cast<std::uint8_t>(
            ~net::PlayerState::FlagAltForm);
        spawn_effect(148, agent.gorea_grapple_points[
                         GrapplePointCount - 1], agent.facing, agent.id);
        const float v9 = (10.0F - (target.position.y - agent.position.y))
            / 24.0F;
        const float v10 = v9 * 30.0F * 30.0F;
        agent.gorea_grapple_field224 = v10 == 0.0F ? 0.0F
            : (GrappleSegmentLength - distance) / v10;
        return true;
    };

    auto pull_grapple_player_up = [&](net::PlayerState& target) {
        if (target.position.y <= agent.position.y) {
            return;
        }
        const float first_distance = std::sqrt(std::max(
            0.0F, distance_squared(agent.gorea_grapple_points[0],
                                    agent.gorea_grapple_points[1])));
        if (first_distance > GrapplePointThresholdSquared) {
            const float vertical_factor = (10.0F
                - (target.position.y - agent.position.y)) * 30.0F;
            if (vertical_factor > 0.0F) {
                agent.gorea_grapple_field224 = (17.5F
                    - grapple_length(agent)) / vertical_factor;
                const float difference = std::abs(
                    first_distance - GrappleSegmentLength);
                const float next_length = difference
                    > std::abs(agent.gorea_grapple_field224)
                    ? first_distance + agent.gorea_grapple_field224
                    : GrappleSegmentLength;
                adjust_grapple_segment_lengths(agent, next_length);
            }
        }
        target.position = add(agent.gorea_grapple_points[
            GrapplePointCount - 1], {0.0F, -0.05F, 0.0F});
    };

    auto settle_grapple_player = [&](net::PlayerState& target) {
        update_grapple_spring(agent);
        const float segment_distance = std::sqrt(std::max(
            0.0F, distance_squared(agent.gorea_grapple_points[0],
                                    agent.gorea_grapple_points[1])));
        adjust_grapple_segment_lengths(agent, segment_distance);
        net::Vec3 player_position = add(agent.gorea_grapple_points[
            GrapplePointCount - 1], {0.0F, -0.05F, 0.0F});
        const net::Vec3 last_between = subtract(
            agent.gorea_grapple_points[GrapplePointCount - 1],
            agent.gorea_grapple_points[GrapplePointCount - 2]);
        if (length_squared(last_between) > GrapplePointThresholdSquared) {
            player_position = add(player_position, multiply(
                normalized_or(last_between, {}), -0.5F));
        }
        target.position = player_position;
        agent.gorea_target_position = agent.gorea_grapple_points[
            GrapplePointCount - 1];
    };

    auto tick_grapple_damage = [&](net::PlayerState& target) {
        gorea::decrement(agent.gorea_grapple_damage_timer, step);
        if (agent.gorea_grapple_damage_timer == 0) {
            apply_enemy_contact_damage(agent, target, 2);
            spawn_effect(179, target.position, agent.facing, agent.id);
            agent.gorea_grapple_damage_timer =
                (rng_.random2(13u) + 7u) * 2u;
        }
    };

    auto slam_player = [&](net::PlayerState& target) {
        const net::Vec3 previous = target.position;
        target.position = add(agent.gorea_grapple_points[
            GrapplePointCount - 1], {0.0F, -0.05F, 0.0F});
        bool collided = false;
        if (const auto hit = collision::sweep_sphere(
                room_.collision(), to_collision(previous),
                to_collision(target.position), 0.0F)) {
            target.position = {hit->center.x, hit->center.y, hit->center.z};
            collided = true;
        } else if (target.position.y < parent->behavior_origin.y) {
            target.position.y = parent->behavior_origin.y;
            collided = true;
        }
        return collided;
    };

    // Enemy28Subroutines[2..4] all check these phase transitions before
    // their state-specific behavior. The threshold is 1000, 2000, then
    // 3000 damage, not a single terminal 3000-point check.
    if (sphere != enemies_.end() && agent.gorea_phases_left > 0
        && agent.gorea_state != 12 && agent.gorea_state != 13) {
        const std::uint32_t threshold = static_cast<std::uint32_t>(
            1000u * (4u - agent.gorea_phases_left));
        if (sphere->gorea_damage >= threshold) {
            release_grapple(nullptr);
            sphere->gorea_damage = 0;
            sphere->gorea_activated = false;
            sphere->gorea_targetable = false;
            sphere->invulnerable = true;
            if (agent.gorea_phases_left > 0) {
                --agent.gorea_phases_left;
            }
            explode_linked_trocras();
            enter(12);
            return;
        }
    }

    if (agent.gorea_state == 0) {
        // State00 has a phase-collapse continuation as well as the initial
        // intro. Preserve the managed 5/6 -> 7 transition before accepting
        // BehaviorXX, then make the sphere vulnerable only after the final
        // no-loop animation has ended.
        if ((agent.gorea_model_animation == 5
             || agent.gorea_model_animation == 6)
            && gorea::animation_ended(agent)) {
            gorea::set_animation(agent, 7, 90);
            return;
        }
        if (!gorea::animation_ended(agent)) {
            return;
        }
        if (sphere != enemies_.end()) {
            sphere->gorea_activated = true;
            sphere->gorea_targetable = true;
            sphere->invulnerable = false;
        }
        agent.gorea_flags |= gorea::Gorea1BIntroDone;
        gorea::set_animation(agent, 3, 30, true);
        enter(1);
        return;
    }

    if (agent.gorea_state == 13) {
        release_grapple(nullptr);
        agent.visible = false;
        agent.gorea_targetable = false;
        return;
    }

    std::size_t target_index = gorea::nearest_player(
        players_, agent.position, std::numeric_limits<float>::max());
    if (agent.gorea_grappling && agent.target_slot != 0xff) {
        const auto retained = target_for_slot(agent.target_slot);
        target_index = retained == players_.end()
            ? players_.size()
            : static_cast<std::size_t>(retained - players_.begin());
    }
    if (target_index == players_.size()) {
        if (agent.gorea_grappling) {
            release_grapple(nullptr);
            enter(11);
        }
        return;
    }

    auto& target = players_[target_index];
    agent.target_slot = target.slot_index;
    const net::Vec3 to_target = subtract(target.position, agent.position);
    const float distance = std::sqrt(std::max(0.0F,
                                              length_squared(to_target)));
    const net::Vec3 horizontal = normalized_or(
        {to_target.x, 0.0F, to_target.z}, agent.facing);
    agent.gorea_target_facing = horizontal;

    // CheckPlayerCollision runs before the state callback in Enemy28Entity.
    // The native session has no broadphase hit list, so use the same body
    // radius as the authored hurt volume and apply the hit at the same
    // fixed cadence.
    if (!agent.gorea_grappling && distance <= 3.9F
        && agent.gorea_attack_timer == 0) {
        const net::Vec3 between = {
            target.position.x - agent.position.x,
            0.0F,
            target.position.z - agent.position.z};
        // Enemy28 intentionally does not normalize this vector (unlike the
        // 1A/leg contact paths).
        target.speed = add(target.speed, multiply(between, 1.0F / 4.0F));
        apply_enemy_contact_damage(agent, target, 15);
        agent.gorea_attack_timer = 30u * 2u;
    }
    gorea::decrement(agent.gorea_attack_timer, step);

    switch (agent.gorea_state) {
    case 1: {
        net::Vec3 to_center = subtract(agent.gorea_anchor, agent.position);
        to_center.y = 0.0F;
        if (length_squared(to_center) > GrapplePointThresholdSquared) {
            agent.velocity = multiply(normalized_or(to_center, agent.facing),
                                      109.0F / 4096.0F / 2.0F
                                          * frame_scale);
            agent.position = add(agent.position, agent.velocity);
        } else {
            agent.velocity = {};
        }
        seek_grapple_target_facing(agent,
                                   subtract(target.position, agent.position),
                                   1.5F);
        if (in_volume(agent, agent.position)) {
            agent.velocity = {};
            enter(2);
        }
        break;
    }
    case 2: {
        agent.velocity = {};
        // Enemy28.UpdateTargetFacing uses the managed Bit2 as a one-frame
        // turn latch. Keep the target vector between calls instead of
        // snapping the body to the player's current heading.
        if ((agent.gorea_flags & gorea::Gorea1BTargetTurning) == 0) {
            agent.gorea_target_facing = horizontal;
            if (length_squared({to_target.x, 0.0F, to_target.z})
                > GrapplePointThresholdSquared) {
                agent.gorea_flags |= gorea::Gorea1BTargetTurning;
            }
        } else {
            seek_grapple_target_facing(agent, agent.gorea_target_facing, 1.5F);
            constexpr float Pi = 3.14159265358979323846F;
            if (dot(normalized_or(agent.gorea_target_facing, agent.facing),
                    normalized_or({agent.facing.x, 0.0F, agent.facing.z},
                                  agent.facing))
                >= std::cos(1.5F * Pi / 180.0F)) {
                agent.gorea_flags &= ~gorea::Gorea1BTargetTurning;
                agent.gorea_target_facing = {};
            }
        }
        if (agent.gorea_phases_left == 0) {
            enter(13);
        } else if (!in_volume(agent, target.position)) {
            enter(3);
        } else if (sphere != enemies_.end()
                   && std::sqrt(std::max(0.0F, distance_squared(
                       sphere->position, target.position))) < 20.0F
                   && check_facing_position(agent, target.position, -1.0F)) {
            enter(4);
        }
        break;
    }
    case 3: {
        seek_grapple_target_facing(agent,
                                   subtract(target.position, agent.position));
        agent.velocity = multiply(
            normalized_or({agent.facing.x, 0.0F, agent.facing.z},
                          agent.facing),
            54.0F / 4096.0F / 2.0F * frame_scale);
        const net::Vec3 next = add(agent.position, agent.velocity);
        if (!in_volume(agent, next)) {
            agent.velocity = {};
            agent.gorea_grapple_timer = 120u * 2u;
            enter(2);
        } else {
            agent.position = next;
        }
        break;
    }
    case 4:
        // Func213BCB8 creates the rope immediately; Behavior04 owns the
        // 120*2-frame wind-up before State05 starts extending it.
        if (!agent.gorea_grappling) {
            agent.velocity = {};
            gorea::set_animation(agent, 1, 30);
            begin_grapple(add(target.position, {0.0F, 0.05F, 0.0F}));
        } else if (gorea::animation_ended(agent)) {
            gorea::set_animation(agent, 3, 30, true);
        }
        seek_grapple_target_facing(agent,
                                   subtract(target.position, agent.position));
        agent.velocity = {};
        gorea::decrement(agent.gorea_grapple_timer, step);
        if (agent.gorea_grapple_timer == 0) {
            agent.gorea_grapple_timer = 120u * 2u;
            enter(5);
        }
        break;
    case 5:
        // Func2139F54 + UpdateGrappleDrawValues + Func213B90C are the
        // managed extension path. The endpoint, not a normalized progress
        // scalar, is authoritative here.
        seek_grapple_target_facing(agent,
                                   subtract(target.position, agent.position));
        update_grapple_draw_values(agent,
                                   sphere == enemies_.end()
                                       ? agent.position : sphere->position,
                                   step);
        if (update_grapple_extension(target)) {
            enter(6);
        } else {
            gorea::decrement(agent.gorea_grapple_timer, step);
            if (agent.gorea_grapple_timer == 0) {
                release_grapple(&target);
                agent.gorea_hold_timer = (rng_.random2(150u) + 150u) * 2u;
                enter(11);
            }
        }
        break;
    case 6: {
        set_biped_lock(target.slot_index, true);
        target.speed = {};
        update_grapple_draw_values(agent,
                                   sphere == enemies_.end()
                                       ? agent.position : sphere->position,
                                   step);
        agent.gorea_grapple_field10 = {0.0F, 1.0F / 30.0F, 0.0F};
        pull_grapple_player_up(target);
        settle_grapple_player(target);
        tick_grapple_damage(target);
        if (sphere != enemies_.end()
            && sphere->gorea_damage
                >= static_cast<std::uint32_t>(
                    std::max(0, agent.gorea_grapple_field234 + 35))) {
            enter(10);
        } else if (target.position.y - agent.position.y >= 10.0F) {
            // Behavior09: switch to the swing only after the rope has
            // actually lifted the player above the body.
            agent.gorea_grapple_field30 = 0.9F;
            agent.gorea_swing_timer = 150u * 2u;
            agent.gorea_hold_timer = 30u * 2u;
            agent.gorea_swing_direction = !agent.gorea_swing_direction;
            if (agent.gorea_swing_direction) {
                agent.gorea_flags |= gorea::Gorea1BSwingDirection;
            } else {
                agent.gorea_flags &= ~gorea::Gorea1BSwingDirection;
            }
            net::Vec3 to_center = subtract(target.position,
                                           agent.gorea_anchor);
            to_center.y = 0.0F;
            agent.velocity = length_squared(to_center)
                > GrapplePointThresholdSquared
                ? multiply(normalized_or(to_center, agent.facing),
                           68.0F / 4096.0F / 2.0F * frame_scale)
                : net::Vec3{};
            const net::Vec3 next = add(agent.position, agent.velocity);
            if (length_squared(agent.velocity) > 0.0F
                && !in_volume(agent, next)) {
                agent.velocity = {};
            }
            enter(7);
        }
        break;
    }
    case 7: {
        set_biped_lock(target.slot_index, true);
        if (length_squared(agent.velocity) > 0.0F) {
            const net::Vec3 next = add(agent.position, agent.velocity);
            if (in_volume(agent, next)) {
                agent.position = next;
            } else {
                agent.velocity = {};
            }
        }
        pull_grapple_player_up(target);
        update_grapple_draw_values(agent,
                                   sphere == enemies_.end()
                                       ? agent.position : sphere->position,
                                   step);
        net::Vec3 between = (agent.gorea_flags
                             & gorea::Gorea1BSwingDirection) != 0
            ? subtract(agent.gorea_grapple_points[0],
                       agent.gorea_grapple_points[1])
            : subtract(agent.gorea_grapple_points[1],
                       agent.gorea_grapple_points[0]);
        between.y = 0.0F;
        between.z = -between.z;
        if (length_squared(between) > GrapplePointThresholdSquared) {
            agent.gorea_grapple_field10 = normalized_or(between, {});
        }
        agent.gorea_grapple_field10 = add(
            multiply(agent.gorea_grapple_field10, 0.04F),
            agent.velocity);
        settle_grapple_player(target);
        tick_grapple_damage(target);
        gorea::decrement(agent.gorea_hold_timer, step);
        if (agent.gorea_hold_timer == 0) {
            agent.gorea_hold_timer = 60u * 2u;
            agent.gorea_swing_direction = !agent.gorea_swing_direction;
            if (agent.gorea_swing_direction) {
                agent.gorea_flags |= gorea::Gorea1BSwingDirection;
            } else {
                agent.gorea_flags &= ~gorea::Gorea1BSwingDirection;
            }
        }
        if (sphere != enemies_.end()
            && sphere->gorea_damage
                >= static_cast<std::uint32_t>(
                    std::max(0, agent.gorea_grapple_field234 + 35))) {
            enter(10);
        } else {
            gorea::decrement(agent.gorea_swing_timer, step);
            if (agent.gorea_swing_timer == 0) {
                agent.gorea_grapple_field30 = 3973.0F / 4096.0F;
                agent.velocity = {};
                enter(8);
            }
        }
        break;
    }
    case 8:
        set_biped_lock(target.slot_index, true);
        update_grapple_draw_values(agent,
                                   sphere == enemies_.end()
                                       ? agent.position : sphere->position,
                                   step);
        agent.gorea_grapple_field10 = {0.0F, 1.0F / 15.0F, 0.0F};
        pull_grapple_player_up(target);
        settle_grapple_player(target);
        tick_grapple_damage(target);
        if (sphere != enemies_.end()
            && sphere->gorea_damage
                >= static_cast<std::uint32_t>(
                    std::max(0, agent.gorea_grapple_field234 + 35))) {
            enter(10);
        } else if (target.position.y - agent.position.y >= 22.5F) {
            agent.gorea_grapple_field30 = 4046.0F / 4096.0F;
            agent.velocity = {};
            enter(9);
        }
        break;
    case 9: {
        set_biped_lock(target.slot_index, true);
        update_grapple_draw_values(agent,
                                   sphere == enemies_.end()
                                       ? agent.position : sphere->position,
                                   step);
        update_grapple_spring(agent);
        tick_grapple_damage(target);
        adjust_grapple_segment_lengths(agent, GrappleSegmentLength);
        agent.gorea_grapple_field10 = {0.0F, -1.0F / 1.5F, 0.0F};
        const net::Vec3 rope_direction = normalized_or(
            subtract(agent.gorea_grapple_points[0],
                     agent.gorea_grapple_points[1]),
            {0.0F, -1.0F, 0.0F});
        if (dot(rope_direction, normalized_or(
                agent.gorea_grapple_field10, {0.0F, -1.0F, 0.0F}))
            < -3956.0F / 4096.0F) {
            net::Vec3 to_center = subtract(agent.gorea_anchor,
                                           agent.position);
            to_center.y = 0.0F;
            const net::Vec3 direction = length_squared(to_center)
                > GrapplePointThresholdSquared
                ? normalized_or(to_center, agent.facing) : agent.facing;
            agent.gorea_grapple_field10 = multiply(
                direction, -1.0F / 1.5F);
        }
        if (slam_player(target)) {
            release_grapple(&target);
            agent.gorea_hold_timer =
                (rng_.random2(150u) + 150u) * 2u;
            enter(11);
        }
        break;
    }
    case 10:
        // Beam damage interrupts the grapple. Enemy28's state-10 subroutine
        // is Behavior00, which immediately chooses the idle hold duration;
        // StopGrappling then releases the biped lock in the same callback.
        release_grapple(&target);
        agent.gorea_hold_timer =
            (rng_.random2(150u) + 150u) * 2u;
        enter(11);
        break;
    case 11: {
        // Func213B2B4: a live linked crystal keeps the launch clock running;
        // when the last crystal is gone the next pass may choose State02.
        bool linked_trocra = false;
        for (const auto& child : enemies_) {
            if (child.active && child.parent_enemy_id == agent.id
                && child.enemy_type == trocra_type
                && (child.trocra_state == 1 || child.trocra_state == 2)) {
                linked_trocra = true;
                break;
            }
        }
        if (agent.gorea_hold_timer > 0) {
            if (!linked_trocra) {
                agent.gorea_swing_timer = 0;
            } else {
                gorea::decrement(agent.gorea_swing_timer, step);
            }
            if (agent.gorea_swing_timer == 0) {
                agent.gorea_swing_timer = 30u * 2u;
                select_trocra();
            }
        }

        seek_grapple_target_facing(agent,
                                   subtract(target.position, agent.position));
        agent.velocity = multiply(
            normalized_or({agent.facing.x, 0.0F, agent.facing.z},
                          agent.facing),
            54.0F / 4096.0F / 2.0F * frame_scale);
        const net::Vec3 next = add(agent.position, agent.velocity);
        if (in_volume(agent, next)) {
            agent.position = next;
        } else {
            agent.velocity = {};
        }
        advance_trocra_launches();
        gorea::decrement(agent.gorea_hold_timer, step);
        const bool linked_after_launch = std::any_of(
            enemies_.begin(), enemies_.end(),
            [&agent, trocra_type](const EnemyState& child) {
                return child.active && child.parent_enemy_id == agent.id
                    && child.enemy_type == trocra_type
                    && (child.trocra_state == 1
                        || child.trocra_state == 2);
            });
        if (agent.gorea_hold_timer == 0 && !linked_after_launch) {
            agent.gorea_hold_timer =
                (rng_.random2(150u) + 150u) * 2u;
            enter(2);
        }
        break;
    }
    case 12:
        // State12 is animation-gated in C#: 4 (phase collapse), then 2, then
        // 8. The parent 1A is reactivated only after animation 8 ends.
        if (!agent.visible) {
            return;
        }
        if (!gorea::animation_ended(agent)) {
            return;
        }
        if (agent.gorea_model_animation == 4) {
            gorea::set_animation(agent, 2, 90);
            return;
        }
        if (agent.gorea_model_animation == 2) {
            gorea::set_animation(agent, 8, 90);
            return;
        }
        if (agent.gorea_model_animation != 8) {
            gorea::set_animation(agent, 8, 90);
            return;
        }
        if (agent.gorea_phases_left == 0) {
            explode_linked_trocras();
            deactivate_trocra_spawns();
            enter(13);
            agent.visible = false;
            agent.gorea_targetable = false;
            agent.gorea_activated = false;
            if (sphere != enemies_.end()) {
                sphere->gorea_activated = false;
                sphere->gorea_targetable = false;
                sphere->invulnerable = true;
            }
            parent->visible = false;
            parent->gorea_activated = false;
            return;
        }
        // Enemy28.Deactivate calls Enemy24.Activate. Rebuild that linked
        // ownership transition explicitly so the next frame resumes 1A's
        // arm/weapon loop rather than leaving both phases hidden.
        parent->visible = true;
        parent->gorea_activated = true;
        parent->gorea_flags &= ~gorea::Gorea1AArmsDown;
        parent->gorea_state = 1;
        parent->gorea_state_initialized = false;
        parent->gorea_arm_bits = 3;
        parent->gorea_field23e = 510 * 2;
        for (auto& child : enemies_) {
            if (child.parent_enemy_id != parent->id
                || child.enemy_type != type_id(formats::EnemyType::GoreaArm)) {
                continue;
            }
            child.gorea_flags &= ~ArmDead;
            child.gorea_damage = 0;
            child.gorea_activated = true;
            child.visible = true;
            child.invulnerable = false;
        }
        agent.gorea_activated = false;
        agent.gorea_grapple_field234 = 0;
        agent.visible = false;
        agent.gorea_targetable = false;
        agent.gorea_state = 13;
        if (sphere != enemies_.end()) {
            sphere->gorea_activated = false;
            sphere->gorea_targetable = false;
            sphere->invulnerable = true;
        }
        break;
    default:
        enter(2);
        break;
    }
}

} // namespace fruityprime::gameplay

static_assert(fruityprime::enemy::module_28_gorea_1b::kModule.managed_class.size() != 0);
