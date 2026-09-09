// Enemy dispatch for the native Session.
//
// The managed tree creates one EnemyXXEntity per cartridge EnemyType. Keep
// that boundary visible here: individual controllers live beside their
// corresponding source file under Entities/Enemies, while Session owns only
// the lifetime-safe iteration over the dynamic vector.
#include "Entities/gameplay.hpp"

namespace fruityprime::gameplay {

void Session::update_enemies() {
    for (std::size_t index = 0; index < enemies_.size();) {
        EnemyState& agent = enemies_[index];
        if (!agent.active) {
            enemies_.erase(enemies_.begin()
                           + static_cast<std::ptrdiff_t>(index));
            continue;
        }

        const auto id = agent.id;
        switch (static_cast<formats::EnemyType>(agent.enemy_type)) {
        case formats::EnemyType::WarWasp:
            update_warwasp(agent);
            break;
        case formats::EnemyType::Zoomer:
            update_zoomer(agent);
            break;
        case formats::EnemyType::Temroid:
            update_temroid(agent);
            break;
        case formats::EnemyType::Petrasyl1:
            update_petrasyl1(agent);
            break;
        case formats::EnemyType::Petrasyl2:
            update_petrasyl2(agent);
            break;
        case formats::EnemyType::Petrasyl3:
            update_petrasyl3(agent);
            break;
        case formats::EnemyType::Petrasyl4:
            update_petrasyl4(agent);
            break;
        case formats::EnemyType::BarbedWarWasp:
            update_barbed_warwasp(agent);
            break;
        case formats::EnemyType::Shriekbat:
            update_shriekbat(agent);
            break;
        case formats::EnemyType::Geemer:
            update_geemer(agent);
            break;
        case formats::EnemyType::Blastcap:
            update_blastcap(agent);
            break;
        case formats::EnemyType::AlimbicTurret:
            update_alimbic_turret(agent);
            break;
        case formats::EnemyType::Cretaphid:
            update_cretaphid(agent);
            break;
        case formats::EnemyType::CretaphidEye:
            update_cretaphid_eye(agent);
            break;
        case formats::EnemyType::CretaphidCrystal:
            update_cretaphid_crystal(agent);
            break;
        case formats::EnemyType::PsychoBit1:
            update_psychobit(agent);
            break;
        case formats::EnemyType::Gorea1A:
            update_gorea_1a(agent);
            break;
        case formats::EnemyType::GoreaHead:
            update_gorea_head(agent);
            break;
        case formats::EnemyType::GoreaArm:
            update_gorea_arm(agent);
            break;
        case formats::EnemyType::GoreaLeg:
            update_gorea_leg(agent);
            break;
        case formats::EnemyType::Gorea1B:
            update_gorea_1b(agent);
            break;
        case formats::EnemyType::GoreaSealSphere1:
            update_gorea_seal_sphere_1(agent);
            break;
        case formats::EnemyType::Trocra:
            update_trocra(agent);
            break;
        case formats::EnemyType::Gorea2:
            update_gorea_2(agent);
            break;
        case formats::EnemyType::GoreaSealSphere2:
            update_gorea_seal_sphere_2(agent);
            break;
        case formats::EnemyType::GoreaMeteor:
            update_gorea_meteor(agent);
            break;
        case formats::EnemyType::Voldrum2:
            update_voldrum2(agent);
            break;
        case formats::EnemyType::Voldrum1:
            update_voldrum1(agent);
            break;
        case formats::EnemyType::Quadtroid:
            update_quadtroid(agent);
            break;
        case formats::EnemyType::CrashPillar:
            update_crash_pillar(agent);
            break;
        case formats::EnemyType::FireSpawn:
            update_firespawn(agent);
            break;
        case formats::EnemyType::Spawner:
            // Enemy40 is normally owned by enemy_spawns_. Keep a safe
            // fallback if a caller materializes it as an EnemyState.
            static_cast<void>(update_generic_enemy(agent));
            break;
        case formats::EnemyType::Slench:
            update_slench(agent);
            break;
        case formats::EnemyType::SlenchShield:
            update_slench_shield(agent);
            break;
        case formats::EnemyType::SlenchNest:
            update_slench_nest(agent);
            break;
        case formats::EnemyType::SlenchSynapse:
            update_slench_synapse(agent);
            break;
        case formats::EnemyType::SlenchTurret:
            update_slench_turret(agent);
            break;
        case formats::EnemyType::LesserIthrak:
            update_lesser_ithrak(agent);
            break;
        case formats::EnemyType::GreaterIthrak:
            update_greater_ithrak(agent);
            break;
        case formats::EnemyType::ForceFieldLock:
            update_force_field_lock(agent);
            break;
        case formats::EnemyType::HitZone:
            update_hit_zone(agent);
            break;
        case formats::EnemyType::CarnivorousPlant:
            update_carnivorous_plant(agent);
            break;
        default:
            static_cast<void>(update_generic_enemy(agent));
            break;
        }

        // A controller can destroy itself or a linked parent. Advance only
        // when the same vector element still occupies this index.
        if (index < enemies_.size() && enemies_[index].id == id) {
            ++index;
        }
    }

    // A Gorea2 state may emit a meteor while its own EnemyState is being
    // updated. Publish such linked children only after the current pass; a
    // vector reallocation in the middle of a controller would invalidate the
    // same reference the managed entity graph keeps stable.
    if (!pending_enemy_spawns_.empty()) {
        for (auto& pending : pending_enemy_spawns_) {
            enemies_.push_back(std::move(pending));
        }
        pending_enemy_spawns_.clear();
    }
}

} // namespace fruityprime::gameplay
