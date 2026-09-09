// Authoritative enemy damage and linked-part destruction.
//
// EnemyInstanceEntity owns these transitions in the managed runtime. The
// native implementation keeps them in a combat boundary so projectile and
// enemy module code share one damage contract without growing the session
// orchestrator.
#include "Entities/gameplay.hpp"
#include "Metadata/metadata.hpp"
#include "Entities/Enemies/gorea_common.hpp"
#include "gameplay_helpers.hpp"

#include <algorithm>
#include <limits>

namespace fruityprime::gameplay {
using namespace detail;

bool Session::damage_enemy(std::uint32_t id, std::uint32_t damage,
                           std::uint8_t source_weapon) noexcept {
    auto enemy = std::find_if(
        enemies_.begin(), enemies_.end(),
        [id](const EnemyState& value) { return value.id == id; });
    if (enemy == enemies_.end()) {
        return false;
    }

    // Enemy49 is the visible lock for a ForceFieldEntity.  Beam/projectile
    // collision decides whether a hit is effective and may arm its return
    // shot; the public damage entry point itself never drains the one-point
    // lock, matching Enemy49Entity.EnemyTakeDamage.
    if (enemy->force_field_entity_id >= 0) {
        return false;
    }

    // Gorea is a linked boss tree. The body/limb classes deliberately keep a
    // 0xffff engine-health sentinel, while the arm, seal and meteor classes
    // expose their own accumulated damage/lifetime. Handle those contracts
    // before the ordinary health subtraction below; otherwise a hit would
    // either kill an invincible parent or turn every attached part into a
    // generic enemy.
    const auto gorea_type = static_cast<formats::EnemyType>(
        enemy->enemy_type);
    switch (gorea_type) {
    case formats::EnemyType::Gorea1A:
    case formats::EnemyType::GoreaLeg:
    case formats::EnemyType::Gorea1B:
    case formats::EnemyType::Gorea2:
        enemy->health = 65535;
        return false;
    case formats::EnemyType::GoreaHead:
        if (!enemy->gorea_beam_collidable) {
            enemy->health = 65535;
            return false;
        }
        enemy->gorea_damage = damage;
        enemy->health = 65535;
        return true;
    case formats::EnemyType::GoreaArm:
        if (!enemy->gorea_activated || (enemy->gorea_flags & 1u) != 0
            || damage == 0 || (enemy->invulnerable
                                && enemy->gorea_state_timer == 0)) {
            enemy->health = 65535;
            return false;
        }
        {
            const auto previous_damage = enemy->gorea_damage;
            const auto parent = std::find_if(
                enemies_.begin(), enemies_.end(),
                [parent_id = enemy->parent_enemy_id](const EnemyState& value) {
                    return parent_id != 0 && value.id == parent_id
                        && value.enemy_type == static_cast<std::uint8_t>(
                            formats::EnemyType::Gorea1A);
                });
            const bool shock_coil_imperialist = parent != enemies_.end()
                && parent->gorea_weapon_index == 5
                && metadata::beam_type_from_native_weapon_slot(source_weapon)
                    == 4; // Imperialist
            if (enemy->gorea_state_timer != 0 || shock_coil_imperialist) {
                // Enemy26Entity rejects the hit as ordinary damage while its
                // regen timer is live, and the Shock Coil phase has the same
                // special Imperialist-only break rule.
                enemy->gorea_damage = 121;
            } else {
                enemy->gorea_damage = std::min<std::uint32_t>(
                    121u, enemy->gorea_damage + damage);
            }
            enemy->health = 65535;
            if (enemy->gorea_damage <= 120u) {
                const auto damage_for_threshold = enemy->gorea_damage == 120u
                    ? 119u : enemy->gorea_damage;
                if (damage_for_threshold / 10u > previous_damage / 10u) {
                    spawn_effect(44, enemy->position,
                                 {1.0F, 0.0F, 0.0F}, enemy->id);
                }
                enemy->gorea_damage_timer = 10u * 2u;
            } else {
                enemy->gorea_state_timer = 0;
                enemy->gorea_damage = 120u;
                enemy->gorea_flags |= 1u; // GoreaArmFlags.Bit0
                enemy->gorea_activated = false;
                enemy->visible = false;
                enemy->invulnerable = true;
                enemy->gorea_damage_timer = 0;
            }
        }
        return true;
    case formats::EnemyType::GoreaSealSphere1:
        if (!enemy->gorea_activated || damage == 0 || enemy->invulnerable) {
            enemy->health = 65535;
            return false;
        }
        {
            const auto previous_damage = enemy->gorea_damage;
            enemy->gorea_damage = std::min<std::uint32_t>(
                3000u, enemy->gorea_damage + damage);
            // Enemy29Entity flashes only while a hit remains inside the
            // current 1000-point phase.  The 1000 boundary is consumed by
            // Gorea1B's phase transition and therefore does not produce the
            // shoulder-hit effect itself.
            const auto current_phase = enemy->gorea_damage / 1000u;
            if (current_phase == previous_damage / 1000u
                && enemy->gorea_damage / 10u > previous_damage / 10u) {
                SoundEvent sound;
                sound.cue = SoundCue::Gorea1BDamage;
                sound.enemy_type = enemy->enemy_type;
                sound.entity_id = enemy->id;
                sound.position = enemy->position;
                emit_sound(sound);
                spawn_effect(44, enemy->position,
                             {1.0F, 0.0F, 0.0F}, enemy->id);
            }
        }
        enemy->health = 65535;
        enemy->gorea_damage_timer = 10u * 2u;
        return false;
    case formats::EnemyType::Trocra:
        if (!enemy->active || !enemy->visible || damage == 0) {
            return false;
        }
        enemy->health = enemy->health > damage
            ? static_cast<std::uint16_t>(enemy->health - damage) : 0;
        if (enemy->health == 0) {
            enemy->visible = false;
            enemy->trocra_destroy_processed = false;
            // Enemy30Entity.EnemyTakeDamage only performs the drop/message
            // side effect.  The actual record is consumed by the next
            // EnemyProcess pass, so beam death does not get the contact
            // explosion that DieAndSpawnEffect owns.
        }
        return false;
    case formats::EnemyType::GoreaSealSphere2:
        if (!enemy->gorea_activated || damage == 0) {
            enemy->health = 65535;
            return false;
        }
        {
            const auto parent = std::find_if(
                enemies_.begin(), enemies_.end(),
                [parent_id = enemy->parent_enemy_id](const EnemyState& value) {
                    return parent_id != 0 && value.id == parent_id
                        && value.enemy_type == static_cast<std::uint8_t>(
                            formats::EnemyType::Gorea2);
                });
            const bool is_omega_cannon =
                metadata::beam_type_from_native_weapon_slot(source_weapon)
                == 8; // Omega Cannon
            const bool damage_was_vulnerable = !enemy->invulnerable;
            const bool phase_locked = enemy->gorea_damage >= 720u
                && (!is_omega_cannon || parent == enemies_.end()
                    || (parent->gorea_flags & (1u << 9)) != 0);
            const bool teleporting = parent != enemies_.end()
                && (parent->gorea_flags & (gorea::Gorea2Teleporting
                                            | gorea::Gorea2Teleporting2)) != 0;
            if (is_omega_cannon && parent != enemies_.end()) {
                parent->gorea_flags |= 1u << 9; // Gorea2Flags.Bit9
            }
            if (parent != enemies_.end()) {
                // Enemy32Entity stops the active laser as soon as the chest
                // sphere receives a hit, including hits later rejected by a
                // teleport/phase guard.
                parent->gorea_flags &= ~gorea::Gorea2LaserActive;
            }
            enemy->health = 65535;
            if (phase_locked || teleporting || enemy->invulnerable) {
                return true;
            }
            enemy->gorea_damage = std::min<std::uint32_t>(
                840u, enemy->gorea_damage + damage);
            if (parent != enemies_.end()) {
                if (enemy->gorea_damage >= 840u) {
                    parent->gorea_flags |= 1u << 11; // Gorea2Flags.Bit11
                } else {
                    // Enemy32Entity uses the first still-lit chest slot as
                    // the 40-point flash window.  This is intentionally not
                    // the same 120-point phase test used by Gorea2's light
                    // state machine.
                    std::uint32_t index = 0;
                    while (index < 32u
                           && (parent->gorea_light_mask & (1u << index)) == 0) {
                        ++index;
                    }
                    index = std::min<std::uint32_t>(index, 6u);
                    const auto diff = static_cast<std::int32_t>(
                        enemy->gorea_damage) - static_cast<std::int32_t>(
                            120u * index);
                    bool spawn_damage_effect = damage >= 120u;
                    for (std::int32_t threshold = 40; threshold <= 80;
                         threshold += 40) {
                        if (diff - static_cast<std::int32_t>(damage)
                                < threshold
                            && threshold <= diff) {
                            spawn_damage_effect = true;
                            break;
                        }
                    }
                    if (spawn_damage_effect) {
                        parent->gorea_flags |= gorea::Gorea2DamageFlash
                            | gorea::Gorea2HitFlash;
                        enemy->invulnerable = true;
                        spawn_effect(44, enemy->position,
                                     {1.0F, 0.0F, 0.0F}, enemy->id);
                    }
                }
            }
            if (damage != 0 && damage_was_vulnerable) {
                SoundEvent sound;
                sound.cue = SoundCue::Gorea2Damage;
                sound.enemy_type = enemy->enemy_type;
                sound.entity_id = enemy->id;
                sound.position = enemy->position;
                emit_sound(sound);
            }
            enemy->gorea_damage_timer = 10u * 2u;
        }
        return false;
    case formats::EnemyType::GoreaMeteor:
        if (!enemy->active || damage == 0 || enemy->health == 0) {
            return false;
        }
        // Enemy33Entity tests the post-subtraction health in EnemyTakeDamage:
        // non-lethal hits flash/shake, while a lethal hit goes directly to
        // the item-drop plus goreaMeteorDestroy path.
        enemy->health = enemy->health > damage
            ? static_cast<std::uint16_t>(enemy->health - damage) : 0;
        if (enemy->health != 0) {
            enemy->gorea_meteor_shake_timer = 30u * 2u;
            spawn_effect(176, enemy->position,
                         enemy->facing, enemy->id);
            enemy->gorea_damage_timer = 30u * 2u;
        }
        if (enemy->health == 0) {
            const auto drop_roll = rng_.random2(100u);
            if (drop_roll < 40u) {
                spawn_item_drop(ItemType::HealthSmall, enemy->position);
            } else if (drop_roll < 80u) {
                spawn_item_drop(ItemType::UASmall, enemy->position);
            }
            detonate_gorea_meteor(*enemy, 177); // goreaMeteorDestroy
        }
        return false;
    default:
        break;
    }

    // Enemy50 is a linked collision volume, not a second health pool.  The
    // managed implementation temporarily applies the hit-zone effectiveness
    // and forwards the accepted amount to its Fire Spawn/Ithrak owner.
    if (enemy->enemy_type == static_cast<std::uint8_t>(
            formats::EnemyType::HitZone)) {
        if (!enemy->active || !enemy->hit_zone_collidable || damage == 0
            || enemy->parent_enemy_id == 0) {
            return false;
        }
        const auto parent = std::find_if(
            enemies_.begin(), enemies_.end(),
            [parent_id = enemy->parent_enemy_id](const EnemyState& value) {
                return value.id == parent_id;
            });
        if (parent == enemies_.end() || !parent->active
            || parent->health == 0) {
            return false;
        }
        const auto parent_id = parent->id;
        const bool accepted = damage_enemy(parent_id, damage, source_weapon);
        // The child is kept at full health so a later collision still
        // forwards a delta, unless the lethal parent path removed the tree.
        const auto child = std::find_if(
            enemies_.begin(), enemies_.end(),
            [id](const EnemyState& value) { return value.id == id; });
        if (child != enemies_.end()) {
            child->health = child->health_max;
        }
        return accepted;
    }

    // Cretaphid's eyes and crystal are linked children. The parent body is
    // only a collision shell while the current eye ring is active; hits must
    // be accepted by the child state that the authored phase exposes.
    if (enemy->cretaphid.supported && enemy->cretaphid_part_kind != 0) {
        if (!enemy->active || damage == 0 || enemy->parent_enemy_id == 0) {
            return false;
        }
        const auto parent = std::find_if(
            enemies_.begin(), enemies_.end(),
            [parent_id = enemy->parent_enemy_id](const EnemyState& value) {
                return value.id == parent_id;
            });
        if (parent == enemies_.end() || !parent->active
            || !parent->cretaphid.supported
            || parent->cretaphid_state >= 4) {
            return false;
        }

        constexpr std::uint8_t EyePart = 1;
        constexpr std::uint8_t CrystalPart = 2;
        if (enemy->cretaphid_part_kind == EyePart) {
            const bool vulnerable_state = enemy->state == 1
                || enemy->state == 2 || enemy->state == 4;
            if (!vulnerable_state || enemy->invulnerable
                || enemy->cretaphid_part_state == 2) {
                return false;
            }
        } else if (enemy->cretaphid_part_kind != CrystalPart
                   || !parent->cretaphid_crystal_open
                   || enemy->invulnerable || enemy->health == 0) {
            return false;
        }

        const auto amount = std::min<std::uint32_t>(damage, enemy->health);
        enemy->health = static_cast<std::uint16_t>(enemy->health - amount);
        if (amount != 0) {
            SoundEvent damage_event;
            damage_event.cue = SoundCue::EnemyDamage;
            damage_event.enemy_type = enemy->enemy_type;
            damage_event.entity_id = enemy->id;
            damage_event.position = enemy->position;
            emit_sound(damage_event);
        }
        if (enemy->health == 0) {
            SoundEvent death_event;
            death_event.cue = SoundCue::EnemyDeath;
            death_event.enemy_type = enemy->enemy_type;
            death_event.entity_id = enemy->id;
            death_event.position = enemy->position;
            emit_sound(death_event);
            enemy->health = 0;
            enemy->cretaphid_part_state = 2; // Dead
            enemy->cretaphid_part_timer = 0;
            enemy->state = 7;
            enemy->visible = enemy->cretaphid_part_kind == CrystalPart;
            enemy->invulnerable = true;
            if (enemy->cretaphid_part_kind == CrystalPart) {
                // The parent consumes the terminal crystal transition on its
                // next update, preserving the delayed defeat boundary.
                parent->cretaphid_crystal_open = false;
                parent->cretaphid_state = 4; // Defeated
                parent->cretaphid_state_timer = 36u * 2u;
                parent->health = 1;
                parent->invulnerable = true;
            }
        } else if (enemy->cretaphid_part_kind == EyePart && amount != 0) {
            enemy->cretaphid_part_state = 1; // Damaged
            const auto phase_index = std::min<std::size_t>(
                parent->cretaphid_phase,
                parent->cretaphid.phases.size() - 1);
            const auto timer = parent->cretaphid.phases[phase_index]
                .eye_state_timer2[enemy->cretaphid_part_index];
            enemy->cretaphid_part_timer = std::max<std::uint32_t>(
                2u, static_cast<std::uint32_t>(timer) * 2u);
            enemy->state = 3;
            enemy->invulnerable = true;
        }
        return amount != 0;
    }

    if (enemy->cretaphid.supported) {
        // The body is damageable only during the final exposed transition;
        // ordinary eye/crystal phases are routed through the linked parts.
        if (!enemy->active || damage == 0 || enemy->invulnerable
            || enemy->cretaphid_state != 4) {
            return false;
        }
        const auto amount = std::min<std::uint32_t>(damage, enemy->health);
        enemy->health = static_cast<std::uint16_t>(enemy->health - amount);
        if (amount != 0) {
            SoundEvent damage_event;
            damage_event.cue = SoundCue::EnemyDamage;
            damage_event.enemy_type = enemy->enemy_type;
            damage_event.entity_id = enemy->id;
            damage_event.position = enemy->position;
            emit_sound(damage_event);
        }
        if (enemy->health == 0) {
            enemy->health = 1;
            enemy->cretaphid_state_timer = 36u * 2u;
            enemy->invulnerable = true;
        }
        return amount != 0;
    }

    // Enemy42Entity is a forwarding child, not an independent health pool.
    // The managed shield only forwards a hit while the eye is open and
    // vulnerable; otherwise it consumes the hit and remains at full health.
    if (enemy->slench_shield.supported) {
        if (!enemy->active || damage == 0 || enemy->parent_enemy_id == 0) {
            return false;
        }
        const auto parent = std::find_if(
            enemies_.begin(), enemies_.end(),
            [parent_id = enemy->parent_enemy_id](const EnemyState& value) {
                return value.id == parent_id;
            });
        if (parent == enemies_.end() || !parent->active
            || !parent->slench.supported || parent->slench_shielded
            || parent->invulnerable) {
            enemy->health = enemy->health_max;
            return false;
        }
        const bool accepted = damage_enemy(parent->id, damage, source_weapon);
        enemy->health = enemy->health_max;
        return accepted;
    }

    // Enemy44Entity accepts damage only in Idle.  A non-lethal hit enters the
    // short damaged animation; a lethal hit is held at one health until the
    // dying animation reaches Dead, matching the managed child lifecycle.
    if (enemy->slench_synapse.supported) {
        constexpr std::uint8_t SynapseIdle = 2;
        constexpr std::uint8_t SynapseDamaged = 3;
        constexpr std::uint8_t SynapseDying = 4;
        if (!enemy->active || damage == 0 || enemy->invulnerable
            || enemy->slench_part_state != SynapseIdle) {
            return false;
        }
        const auto amount = std::min<std::uint32_t>(damage, enemy->health);
        enemy->health = static_cast<std::uint16_t>(enemy->health - amount);
        if (amount != 0) {
            SoundEvent damage_event;
            damage_event.cue = SoundCue::EnemyDamage;
            damage_event.enemy_type = enemy->enemy_type;
            damage_event.entity_id = enemy->id;
            damage_event.position = enemy->position;
            emit_sound(damage_event);
        }
        if (enemy->health == 0) {
            SoundEvent death_event;
            death_event.cue = SoundCue::EnemyDeath;
            death_event.enemy_type = enemy->enemy_type;
            death_event.entity_id = enemy->id;
            death_event.position = enemy->position;
            emit_sound(death_event);
            enemy->health = 1;
            enemy->slench_part_state = SynapseDying;
            enemy->slench_part_timer = 12u * 2u;
            enemy->invulnerable = true;
        } else if (amount != 0) {
            enemy->slench_part_state = SynapseDamaged;
            enemy->slench_part_timer = 12u * 2u;
            enemy->invulnerable = true;
        }
        return amount != 0;
    }

    if (!enemy->active || damage == 0
        || enemy->blastcap_exploded || enemy->invulnerable
        || (enemy->slench.supported && enemy->slench_shielded)) {
        return false;
    }
    const auto previous_health = enemy->health;
    const auto amount = std::min<std::uint32_t>(damage, enemy->health);
    enemy->health = static_cast<std::uint16_t>(enemy->health - amount);
    if (amount != 0 && enemy->enemy_type == static_cast<std::uint8_t>(
            formats::EnemyType::Temroid)) {
        // EnemyInstanceEntity resets _timeSinceDamage at the common damage
        // boundary. Enemy02Entity's State00/State07 Behavior04 consumes the
        // first frame of that counter to enter its attack choreography.
        enemy->temroid_time_since_damage = 0;
    }
    if (amount != 0 && enemy->quadtroid.supported) {
        // BeamProjectileEntity calls the same EnemyTakeDamage boundary. The
        // controller consumes this one-frame latch to reproduce Quadtroid's
        // damage threshold/stun transition instead of silently treating the
        // hit as a generic health subtraction.
        enemy->quadtroid_previous_health = previous_health;
        enemy->quadtroid_damage_taken = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(amount,
                                    std::numeric_limits<std::uint16_t>::max()));
        enemy->quadtroid_hit_by_beam = true;
    }
    if (amount != 0) {
        SoundEvent damage_event;
        damage_event.cue = SoundCue::EnemyDamage;
        damage_event.enemy_type = enemy->enemy_type;
        damage_event.entity_id = enemy->id;
        damage_event.position = enemy->position;
        emit_sound(damage_event);
    }
    if (enemy->health == 0
        && enemy->enemy_type == static_cast<std::uint8_t>(
            formats::EnemyType::Blastcap)) {
        // Enemy16Entity does not disappear when the hit empties its health.
        // It becomes an invulnerable, invisible gas cloud for 150*2 scene
        // frames and only then reports Destroyed to its spawner.
        enemy->health = 1;
        enemy->blastcap_exploded = true;
        enemy->visible = false;
        enemy->velocity = {};
        enemy->state = 3;
        enemy->blastcap_cloud_tick = 0;
        enemy->blastcap_cloud_timer = 150u * 2u;
        enemy->blastcap_next_state = 3;
        enemy->blastcap_initial_cloud_hit = false;
        spawn_effect(4, enemy->position, {1.0F, 0.0F, 0.0F}, enemy->id,
                     2.5F);
        constexpr float cloud_radius = 2.0F;
        const float combined_radius = cloud_radius + config_.body_radius;
        for (auto& player : players_) {
            if (!detail::objective_player(player)
                || detail::distance_squared(player.position, enemy->position)
                    >= combined_radius * combined_radius) {
                continue;
            }
            enemy->blastcap_initial_cloud_hit = true;
            apply_enemy_contact_damage(*enemy, player, 2);
        }
        return amount != 0;
    }
    if (enemy->health == 0 && enemy->slench.supported) {
        // Enemy41 keeps its body instance alive through the defeat animation
        // so linked turrets/room messages can observe the terminal state.
        // The native host has no animation dependency here, but retains the
        // same delayed destruction boundary for spawner/save bookkeeping.
        SoundEvent death_event;
        death_event.cue = SoundCue::EnemyDeath;
        death_event.enemy_type = enemy->enemy_type;
        death_event.entity_id = enemy->id;
        death_event.position = enemy->position;
        emit_sound(death_event);
        enemy->slench_state = 14; // Dead
        enemy->slench_state_timer = 36u * 2u;
        enemy->slench_death_timer = 36u * 2u;
        enemy->slench_shielded = true;
        enemy->invulnerable = true;
        enemy->velocity = {};
        return amount != 0;
    }
    if (enemy->health == 0) {
        SoundEvent death_event;
        death_event.cue = SoundCue::EnemyDeath;
        death_event.enemy_type = enemy->enemy_type;
        death_event.entity_id = enemy->id;
        death_event.position = enemy->position;
        emit_sound(death_event);
        static_cast<void>(destroy_enemy(id));
    }
    return amount != 0;
}

bool Session::destroy_enemy(std::uint32_t id, bool out_of_range) noexcept {
    const auto enemy = std::find_if(
        enemies_.begin(), enemies_.end(),
        [id](const EnemyState& value) { return value.id == id; });
    if (enemy == enemies_.end()) {
        return false;
    }

    auto spawner = std::find_if(
        enemy_spawns_.begin(), enemy_spawns_.end(),
        [entity_id = enemy->spawner_entity_id](const EnemySpawnRuntime& value) {
            return value.entity_id == entity_id && value.spawner != nullptr;
        });
    if (spawner != enemy_spawns_.end()
        && spawner->spawner->on_enemy_destroyed(out_of_range)) {
        complete_enemy_spawner(spawner->entity_id,
                               spawner->spawner->enemy_type(), spawner->data);
    }
    const auto parent_id = enemy->id;
    const bool force_field_lock = enemy->force_field_entity_id >= 0;
    const auto force_field_entity_id = enemy->force_field_entity_id;
    if (force_field_lock) {
        const auto field = std::find_if(
            force_fields_.begin(), force_fields_.end(),
            [force_field_entity_id](const ForceFieldRuntime& value) {
                return value.entity_id == force_field_entity_id;
            });
        if (field != force_fields_.end() && field->lock_id == parent_id) {
            field->active = false;
            field->lock_id = 0;
        }
    }
    const bool remove_children = enemy->slench.supported
        || enemy->cretaphid.supported
        || std::any_of(
            enemies_.begin(), enemies_.end(),
            [parent_id](const EnemyState& value) {
                return value.parent_enemy_id == parent_id;
            });
    enemies_.erase(enemy);
    if (remove_children) {
        enemies_.erase(
            std::remove_if(
                enemies_.begin(), enemies_.end(),
                [parent_id](const EnemyState& value) {
                    return value.parent_enemy_id == parent_id;
                }),
            enemies_.end());
    }
    return true;
}

} // namespace fruityprime::gameplay
