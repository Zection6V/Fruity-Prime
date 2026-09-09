// Projectile/effect simulation for the native Session.
//
// This is the native counterpart of BeamProjectileEntity and
// BeamEffectEntity. It remains renderer-independent: the session emits
// transient effect, particle, and sound records for the frontend to consume.
#include "Entities/gameplay.hpp"
#include "Metadata/metadata.hpp"
#include "gameplay_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fruityprime::gameplay {
using namespace detail;

void Session::spawn_projectile(const net::PlayerState& player,
                               const Input& input) {
    const metadata::WeaponInfo& profile =
        weapon_profile(player.current_weapon);
    std::uint16_t damage = profile.uncharged_damage;
    const auto runtime = std::find_if(
        inputs_.begin(), inputs_.end(), [&player](const RuntimeInput& value) {
            return value.slot == player.slot_index;
        });
    if (runtime != inputs_.end() && runtime->inventory.double_damage_ticks > 0) {
        damage = static_cast<std::uint16_t>(std::min<std::uint32_t>(
            std::numeric_limits<std::uint16_t>::max(),
            static_cast<std::uint32_t>(damage) * 2u));
    }
    const net::Vec3 direction = normalized_or(input.aim, player.facing);
    Projectile projectile{
        add(player.position, {0.0F, 0.9F, 0.0F}), direction,
        player.slot_index, player.current_weapon, damage,
        profile.projectile_speed, 0.0F
    };
    const std::size_t weapon_index = std::min<std::size_t>(
        player.current_weapon, metadata::WeaponCount - 1);
    const auto& visual = metadata::weapon_visual_info(
        static_cast<std::uint8_t>(weapon_index));
    projectile.back_position = projectile.position;
    projectile.spawn_position = projectile.position;
    projectile.past_positions.fill(projectile.position);
    projectile.draw_func_id = visual.draw_func_ids[0];
    projectile.color = color_from_rgb555(visual.colors[0]);
    projectile.collision_effect = visual.collision_effects[0];
    projectile.muzzle_effect = visual.muzzle_effects[0];
    projectile.damage_dir_type = visual.damage_dir_types[0];
    projectile.damage_interpolation = visual.damage_interpolations[0];
    const auto muzzle_position = projectile.position;
    projectiles_.push_back(std::move(projectile));
    SoundEvent shot_event;
    shot_event.cue = SoundCue::BeamShot;
    shot_event.slot = player.slot_index;
    shot_event.weapon = player.current_weapon;
    shot_event.position = muzzle_position;
    emit_sound(shot_event);
    if (visual.muzzle_effects[0] != 0xff) {
        spawn_effect(visual.muzzle_effects[0], muzzle_position, direction,
                     player.slot_index);
    }
}

bool Session::consume_weapon_ammo(RuntimeInput& runtime,
                                  std::uint8_t weapon) noexcept {
    if (runtime.inventory.infinite_ammo) {
        return true;
    }
    const metadata::WeaponInfo& profile = weapon_profile(weapon);
    if (profile.ammo_cost == 0) {
        return true;
    }
    const std::size_t ammo_type = profile.ammo_type;
    if (ammo_type >= runtime.inventory.ammo.size()
        || runtime.inventory.ammo[ammo_type] < profile.ammo_cost) {
        return false;
    }
    runtime.inventory.ammo[ammo_type] = static_cast<std::uint16_t>(
        runtime.inventory.ammo[ammo_type] - profile.ammo_cost);
    return true;
}

std::uint32_t Session::spawn_effect(
    std::uint16_t effect_id, net::Vec3 position, net::Vec3 direction,
    std::uint32_t source_entity_id, float lifespan_seconds, bool no_splat,
    bool persistent, bool element_extension, net::Vec3 up) {
    if (effect_id == 0 || effect_id >= metadata::EffectCount
        || !std::isfinite(lifespan_seconds) || lifespan_seconds <= 0.0F) {
        return 0;
    }
    EffectState effect;
    effect.id = next_effect_id_++;
    effect.effect_id = effect_id;
    effect.source_entity_id = source_entity_id;
    effect.position = position;
    effect.direction = normalized_or(direction, {0.0F, 0.0F, 1.0F});
    effect.up = normalized_or(up, {0.0F, 1.0F, 0.0F});
    effect.remaining_seconds = lifespan_seconds;
    effect.lifespan_seconds = lifespan_seconds;
    effect.no_splat = no_splat;
    effect.persistent = persistent;
    effect.element_extension = element_extension;
    effects_.push_back(effect);
    return effect.id;
}

void Session::update_effect_transform(std::uint32_t effect_id,
                                      net::Vec3 position,
                                      net::Vec3 direction,
                                      net::Vec3 up) noexcept {
    if (effect_id == 0) {
        return;
    }
    const auto effect = std::find_if(
        effects_.begin(), effects_.end(),
        [effect_id](const EffectState& value) {
            return value.id == effect_id && !value.detached;
        });
    if (effect == effects_.end()) {
        return;
    }
    effect->position = position;
    effect->direction = normalized_or(direction,
                                      {0.0F, 0.0F, 1.0F});
    effect->up = normalized_or(up, {0.0F, 1.0F, 0.0F});
}

void Session::detach_effect(std::uint32_t effect_id,
                            bool set_expired) noexcept {
    if (effect_id == 0) {
        return;
    }
    const auto effect = std::find_if(
        effects_.begin(), effects_.end(),
        [effect_id](const EffectState& value) {
            return value.id == effect_id;
        });
    if (effect == effects_.end()) {
        return;
    }
    effect->detached = true;
    effect->detached_set_expired = set_expired;
    effect->remaining_seconds = 0.0F;
}

void Session::detach_effect_from_runtime(std::uint32_t effect_id,
                                          bool set_expired) noexcept {
    detach_effect(effect_id, set_expired);
}

void Session::add_single_particle(SingleParticleType type, net::Vec3 position,
                                  net::Vec3 color, float alpha,
                                  float scale) noexcept {
    // The managed renderer caps this frame-local queue at 200 entries. Keep
    // the cap here as well so a scan/death burst cannot grow a headless or
    // network session without bound.
    constexpr std::size_t MaxSingleParticles = 200;
    if (single_particles_.size() >= MaxSingleParticles
        || !std::isfinite(alpha) || alpha <= 0.0F
        || !std::isfinite(scale) || scale <= 0.0F
        || !std::isfinite(position.x) || !std::isfinite(position.y)
        || !std::isfinite(position.z) || !std::isfinite(color.x)
        || !std::isfinite(color.y) || !std::isfinite(color.z)) {
        return;
    }
    single_particles_.push_back(SingleParticleState{
        type, position, color, std::clamp(alpha, 0.0F, 1.0F), scale});
}

void Session::emit_sound(SoundEvent event) noexcept {
    // A pathological room can produce many simultaneous collision cues. Keep
    // this frame-local queue bounded just like the particle queue, and make a
    // missing heap allocation non-fatal to gameplay.
    constexpr std::size_t MaxSoundEvents = 256;
    if (sound_events_.size() >= MaxSoundEvents) {
        return;
    }
    try {
        sound_events_.push_back(event);
    } catch (...) {
    }
}

void Session::update_effects() {
    for (std::size_t i = 0; i < effects_.size();) {
        auto& effect = effects_[i];
        if (effect.detached) {
            effects_.erase(effects_.begin()
                           + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (effect.persistent) {
            ++i;
            continue;
        }
        effect.remaining_seconds -= config_.tick_seconds;
        if (effect.remaining_seconds <= 0.0F) {
            effects_.erase(effects_.begin()
                           + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
}

void Session::update_projectiles() {
    constexpr float projectile_radius = 0.08F;
    for (std::size_t i = 0; i < projectiles_.size();) {
        auto& projectile = projectiles_[i];
        projectile.back_position = projectile.position;
        // The managed renderer updates this history every other 60 Hz frame
        // and consumes every other point. Retaining the same cadence keeps
        // the ribbon length stable instead of making it grow with frame rate.
        // BeamProjectileEntity keys this update to the scene frame, not to
        // the projectile's age. A projectile spawned mid-frame therefore
        // still joins the same 30 Hz trail cadence as every other projectile.
        if ((tick_count_ & 1u) == 0u) {
            for (std::size_t history = projectile.past_positions.size() - 1;
                 history > 0; --history) {
                projectile.past_positions[history] =
                    projectile.past_positions[history - 1];
            }
            projectile.past_positions[0] = projectile.position;
        }
        const net::Vec3 next = add(
            projectile.position,
            multiply(projectile.direction,
                     projectile.speed * config_.tick_seconds));
        const auto room_hit = collision::sweep_sphere(
            room_.collision(), to_collision(projectile.position),
            to_collision(next), projectile_radius);
        net::Vec3 impact_position = projectile.position;
        if (room_hit.has_value()) {
            impact_position = {room_hit->contact.x, room_hit->contact.y,
                               room_hit->contact.z};
        }
        bool remove = room_hit.has_value();
        BeamImpactKind impact_kind = BeamImpactKind::Surface;
        float impact_fraction = room_hit.has_value()
            ? room_hit->fraction : 1.0F;
        // ForceFieldEntity is a finite plane, not part of the room collision
        // mesh. Test it before players/enemies so a beam cannot pass through
        // an active field and hit something on the other side. If its linked
        // Enemy49 lock occupies the intersection, leave the beam for the
        // enemy pass so the lock can arm its return shot.
        for (const auto& field : force_fields_) {
            if (!field.active) {
                continue;
            }
            const net::Vec3 start_delta = subtract(projectile.position,
                                                   field.position);
            const net::Vec3 end_delta = subtract(next, field.position);
            const float start_distance = dot(start_delta, field.facing);
            const float end_distance = dot(end_delta, field.facing);
            const float denominator = end_distance - start_distance;
            if (std::min(start_distance, end_distance) > projectile_radius
                || std::max(start_distance, end_distance) < -projectile_radius) {
                continue;
            }
            const float fraction = std::abs(denominator)
                > std::numeric_limits<float>::epsilon()
                ? std::clamp(-start_distance / denominator, 0.0F, 1.0F)
                : 0.0F;
            const net::Vec3 point = add(
                projectile.position, multiply(
                    subtract(next, projectile.position), fraction));
            const net::Vec3 between = subtract(point, field.position);
            if (std::abs(dot(between, field.up)) > field.height
                + projectile_radius
                || std::abs(dot(between, field.right)) > field.width
                    + projectile_radius
                || fraction > impact_fraction) {
                continue;
            }
            bool lock_at_intersection = false;
            if (field.lock_id != 0) {
                const auto lock = std::find_if(
                    enemies_.begin(), enemies_.end(),
                    [id = field.lock_id](const EnemyState& value) {
                        return value.id == id && value.active;
                    });
                lock_at_intersection = lock != enemies_.end()
                    && distance_squared(lock->position, point)
                        <= (lock->body_radius + projectile_radius)
                            * (lock->body_radius + projectile_radius);
            }
            impact_fraction = fraction;
            impact_position = point;
            if (lock_at_intersection) {
                remove = false;
            } else {
                remove = true;
                impact_kind = BeamImpactKind::Surface;
            }
        }
        if (!remove) {
            const auto owner = projectile.owner_enemy_id == 0
                ? std::find_if(players_.begin(), players_.end(),
                    [&projectile](const net::PlayerState& player) {
                        return player.slot_index == projectile.owner_slot;
                    })
                : players_.end();
            for (auto& target : players_) {
                if (target.slot_index == projectile.owner_slot
                    || target.health == 0
                    || (target.flags & net::PlayerState::FlagActive) == 0
                    || (target.flags & net::PlayerState::FlagSpectating) != 0) {
                    continue;
                }
                if (config_.team_mode && !config_.friendly_fire
                    && owner != players_.end()
                    && target.team == owner->team) {
                    continue;
                }
                const net::Vec3 segment = subtract(next, projectile.position);
                const float segment_length_squared = length_squared(segment);
                float segment_fraction = 0.0F;
                if (segment_length_squared
                    > std::numeric_limits<float>::epsilon()) {
                    segment_fraction = std::clamp(
                        (target.position.x - projectile.position.x) * segment.x
                            + (target.position.y - projectile.position.y) * segment.y
                            + (target.position.z - projectile.position.z) * segment.z,
                        0.0F, segment_length_squared) / segment_length_squared;
                }
                const net::Vec3 closest = add(
                    projectile.position, multiply(segment, segment_fraction));
                if (distance_squared(closest, target.position)
                    <= (config_.body_radius + projectile_radius)
                        * (config_.body_radius + projectile_radius)) {
                    if (!network_damage_authority_) {
                        // A non-authority client's projectile is a local visual
                        // echo.  The authority already resolves this contact;
                        // letting the echo mutate health would double-count a
                        // kill before the next snapshot arrives.
                        impact_position = closest;
                        impact_kind = BeamImpactKind::Player;
                        remove = true;
                        break;
                    }
                    const std::uint16_t old_health = target.health;
                    target.health = old_health > projectile.damage
                        ? static_cast<std::uint16_t>(old_health - projectile.damage)
                        : 0;
                    target.damage_sequence = static_cast<std::uint8_t>(
                        target.damage_sequence + 1);
                    target.attacker_slot = projectile.owner_slot;
                    target.damage_beam = projectile.weapon;
                    target.hit_direction = projectile.direction;
                    impact_position = closest;
                    impact_kind = BeamImpactKind::Player;
                    if (target.health != old_health) {
                        SoundEvent damage_event;
                        damage_event.cue = SoundCue::PlayerDamage;
                        damage_event.slot = target.slot_index;
                        damage_event.position = target.position;
                        emit_sound(damage_event);
                    }
                    if (target.health == 0 && old_health != 0) {
                        SoundEvent death_event;
                        death_event.cue = SoundCue::PlayerDeath;
                        death_event.slot = target.slot_index;
                        death_event.position = target.position;
                        emit_sound(death_event);
                        increment_saturating(target.deaths);
                        target.flags &= static_cast<std::uint8_t>(
                            ~(net::PlayerState::FlagSpawned
                              | net::PlayerState::FlagAltForm
                              | net::PlayerState::FlagZoomed));
                        target.speed = {};
                        const auto target_runtime = std::find_if(
                            inputs_.begin(), inputs_.end(),
                            [&target](const RuntimeInput& value) {
                                return value.slot == target.slot_index;
                            });
                        if (target_runtime != inputs_.end()) {
                            if (config_.survival_mode
                                && target.deaths > config_.survival_lives) {
                                target_runtime->eliminated = true;
                                target_runtime->respawn_ticks = 0;
                            } else {
                                target_runtime->eliminated = false;
                                target_runtime->respawn_ticks =
                                    config_.respawn_ticks;
                            }
                            target_runtime->fire_cooldown = 0;
                        }
                        const auto attacker = std::find_if(
                            players_.begin(), players_.end(),
                            [&projectile](const net::PlayerState& value) {
                                return value.slot_index == projectile.owner_slot;
                            });
                        if (attacker != players_.end()
                            && attacker->slot_index != target.slot_index) {
                            increment_saturating(attacker->kills);
                            if (attacker->points
                                < std::numeric_limits<std::int16_t>::max()) {
                                ++attacker->points;
                            }
                            spawn_item_drop(
                                projectile.weapon == 1
                                    ? ItemType::MissileSmall
                                    : ItemType::UASmall,
                                target.position);
                        }
                    }
                    remove = true;
                    break;
                }
            }
        }
        if (!remove && projectile.owner_enemy_id == 0) {
            const net::Vec3 segment = subtract(next, projectile.position);
            const float segment_length_squared = length_squared(segment);
            const auto has_active_hit_zone = [this](std::uint32_t parent_id) {
                return std::any_of(
                    enemies_.begin(), enemies_.end(),
                    [parent_id](const EnemyState& value) {
                        return value.parent_enemy_id == parent_id
                            && value.enemy_type == static_cast<std::uint8_t>(
                                formats::EnemyType::HitZone)
                            && value.active && value.hit_zone_collidable;
                    });
            };
            for (auto& target : enemies_) {
                const bool hit_zone = target.enemy_type
                    == static_cast<std::uint8_t>(formats::EnemyType::HitZone);
                if (!target.active
                    || (!target.visible && !target.gorea_targetable
                        && !target.gorea_beam_collidable && !hit_zone)) {
                    continue;
                }
                if (hit_zone && !target.hit_zone_collidable) {
                    continue;
                }
                if (!hit_zone
                    && (target.firespawn.supported || target.ithrak.supported)
                    && has_active_hit_zone(target.id)) {
                    continue;
                }
                // While Slench's eye is closed, the managed body is
                // invincible and the actual damage targets are its visible
                // shield/synapses. Do not let the broad body sphere consume
                // the beam before those child volumes get a chance to test
                // it; once every synapse is dead the body becomes the target
                // again during the vulnerable roam state.
                if (target.slench.supported && target.slench_shielded
                    && target.slench_synapses_alive != 0) {
                    continue;
                }
                if (target.cretaphid.supported
                    && target.cretaphid_part_kind == 0
                    && target.invulnerable) {
                    continue;
                }
                float segment_fraction = 0.0F;
                if (segment_length_squared
                    > std::numeric_limits<float>::epsilon()) {
                    segment_fraction = std::clamp(
                        (target.position.x - projectile.position.x) * segment.x
                            + (target.position.y - projectile.position.y) * segment.y
                            + (target.position.z - projectile.position.z) * segment.z,
                        0.0F, segment_length_squared) / segment_length_squared;
                }
                const net::Vec3 closest = add(
                    projectile.position, multiply(segment, segment_fraction));
                const float radius = target.body_radius + projectile_radius;
                if (distance_squared(closest, target.position)
                    > radius * radius) {
                    continue;
                }

                impact_position = closest;
                impact_kind = BeamImpactKind::Enemy;

                if (target.force_field_entity_id >= 0) {
                    const auto beam_type =
                        metadata::beam_type_from_native_weapon_slot(
                            projectile.weapon);
                    const bool effective = target.force_field_type <= 7
                        && beam_type
                            == static_cast<std::int32_t>(
                                target.force_field_type);
                    if (!effective && projectile.owner_enemy_id == 0
                        && projectile.owner_slot == 0
                        && target.force_field_shot_timer == 0) {
                        target.force_field_shot_timer = target.force_field_type
                            == 7 ? 30u * 2u : 1u;
                        target.target_slot = projectile.owner_slot;
                    }
                } else {
                    const auto packed_effectiveness = hit_zone
                        ? target.hit_zone_effectiveness
                        : target.firespawn.supported
                        ? target.firespawn.effectiveness
                        : metadata::enemy_info(target.enemy_type).effectiveness;
                    const auto effectiveness = metadata::decode_effectiveness(
                        packed_effectiveness)[std::min<std::size_t>(
                            projectile.weapon, metadata::WeaponCount - 1)];
                    const float multiplier = metadata::damage_multiplier(
                        effectiveness);
                    if (multiplier > 0.0F) {
                        const auto scaled_damage = static_cast<std::uint32_t>(
                            std::max(1.0, std::round(
                                static_cast<double>(projectile.damage)
                                    * multiplier)));
                        static_cast<void>(damage_enemy(
                            target.id, scaled_damage, projectile.weapon));
                    }
                }
                // A beam is consumed by an enemy even when its effectiveness
                // is zero; this matches the managed collision path and keeps
                // a zero-damage hit from repeatedly testing the same target.
                remove = true;
                break;
            }
        }
        projectile.lifetime += config_.tick_seconds;
        const float lifetime_limit = projectile.max_lifetime > 0.0F
            ? projectile.max_lifetime
            : weapon_profile(projectile.weapon).lifetime_seconds;
        if (remove || projectile.lifetime >= lifetime_limit) {
            if (remove) {
                SoundEvent impact_event;
                impact_event.cue = SoundCue::BeamImpact;
                impact_event.impact = impact_kind;
                impact_event.slot = projectile.owner_slot;
                impact_event.weapon = projectile.weapon;
                impact_event.position = impact_position;
                emit_sound(impact_event);
            }
            if (remove && projectile.collision_effect != 0xff) {
                const auto source_entity_id = projectile.owner_enemy_id != 0
                    ? projectile.owner_enemy_id
                    : static_cast<std::uint32_t>(projectile.owner_slot);
                spawn_effect(projectile.collision_effect, impact_position,
                             projectile.direction, source_entity_id);
            }
            projectiles_.erase(projectiles_.begin()
                               + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        projectile.position = next;
        if (projectile.draw_func_id == 0
            || projectile.draw_func_id == 6
            || projectile.draw_func_id == 7
            || projectile.draw_func_id == 12) {
            // BeamProjectileEntity.Draw00/Draw06/Draw07/Draw12 adds the
            // transient fuzzball while the projectile is still in flight.
            // Collision frames are removed above and are represented by the
            // collision effect instead, matching the managed draw guard.
            const net::Vec3 particle_color = projectile.draw_func_id == 0
                ? projectile.color : net::Vec3{1.0F, 1.0F, 1.0F};
            add_single_particle(SingleParticleType::Fuzzball,
                                projectile.position, particle_color,
                                1.0F, 0.25F);
        }
        ++i;
    }
}

} // namespace fruityprime::gameplay
