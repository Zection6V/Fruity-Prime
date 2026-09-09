#pragma once

#include "00_WarWasp.hpp"
#include "01_Zoomer.hpp"
#include "02_Temroid.hpp"
#include "03_Petrasyl1.hpp"
#include "04_Petrasyl2.hpp"
#include "05_Petrasyl3.hpp"
#include "06_Petrasyl4.hpp"
#include "10_BarbedWarWasp.hpp"
#include "11_Shriekbat.hpp"
#include "12_Geemer.hpp"
#include "16_Blastcap.hpp"
#include "18_AlimbicTurret.hpp"
#include "19_Cretaphid.hpp"
#include "20_CretaphidEye.hpp"
#include "21_CretaphidCrystal.hpp"
#include "23_PsychoBit.hpp"
#include "24_Gorea1A.hpp"
#include "25_GoreaHead.hpp"
#include "26_GoreaArm.hpp"
#include "27_GoreaLeg.hpp"
#include "28_Gorea1B.hpp"
#include "29_GoreaSealSphere1.hpp"
#include "30_Trocra.hpp"
#include "31_Gorea2.hpp"
#include "32_GoreaSealSphere2.hpp"
#include "33_GoreaMeteor.hpp"
#include "35_Voldrum.hpp"
#include "36_Voldrum.hpp"
#include "37_Quadtroid.hpp"
#include "38_CrashPillar.hpp"
#include "39_FireSpawn.hpp"
#include "40_EnemySpawner.hpp"
#include "41_Slench.hpp"
#include "42_SlenchShield.hpp"
#include "43_SlenchNest.hpp"
#include "44_SlenchSynapse.hpp"
#include "45_SlenchTurret.hpp"
#include "46_LesserIthrak.hpp"
#include "47_GreaterIthrak.hpp"
#include "49_ForceFieldLock.hpp"
#include "50_HitZone.hpp"
#include "51_CarnivorousPlant.hpp"

#include <array>
#include <span>

namespace fruityprime::enemy {

inline constexpr std::array kModules{
    module_00_war_wasp::kModule,
    module_01_zoomer::kModule,
    module_02_temroid::kModule,
    module_03_petrasyl1::kModule,
    module_04_petrasyl2::kModule,
    module_05_petrasyl3::kModule,
    module_06_petrasyl4::kModule,
    module_10_barbed_war_wasp::kModule,
    module_11_shriekbat::kModule,
    module_12_geemer::kModule,
    module_16_blastcap::kModule,
    module_18_alimbic_turret::kModule,
    module_19_cretaphid::kModule,
    module_20_cretaphid_eye::kModule,
    module_21_cretaphid_crystal::kModule,
    module_23_psychobit::kModule,
    module_24_gorea_1a::kModule,
    module_25_gorea_head::kModule,
    module_26_gorea_arm::kModule,
    module_27_gorea_leg::kModule,
    module_28_gorea_1b::kModule,
    module_29_gorea_seal_sphere_1::kModule,
    module_30_trocra::kModule,
    module_31_gorea_2::kModule,
    module_32_gorea_seal_sphere_2::kModule,
    module_33_gorea_meteor::kModule,
    module_35_voldrum_2::kModule,
    module_36_voldrum_1::kModule,
    module_37_quadtroid::kModule,
    module_38_crash_pillar::kModule,
    module_39_fire_spawn::kModule,
    module_40_enemy_spawner::kModule,
    module_41_slench::kModule,
    module_42_slench_shield::kModule,
    module_43_slench_nest::kModule,
    module_44_slench_synapse::kModule,
    module_45_slench_turret::kModule,
    module_46_lesser_ithrak::kModule,
    module_47_greater_ithrak::kModule,
    module_49_force_field_lock::kModule,
    module_50_hit_zone::kModule,
    module_51_carnivorous_plant::kModule};

[[nodiscard]] inline std::span<const ModuleDescriptor> modules() noexcept {
    return kModules;
}

} // namespace fruityprime::enemy

