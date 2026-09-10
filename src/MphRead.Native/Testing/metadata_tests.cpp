#include "Metadata/metadata.hpp"
#include "Metadata/metadata_lookup.hpp"
#include "Metadata/Rooms.hpp"
#include "Metadata/entity_metadata.hpp"
#include "Metadata/player_metadata.hpp"
#include "../Metadata/MetadataClasses.hpp"
#include "../Metadata/MetadataFacade.hpp"
#include "../Metadata/MetadataModels.hpp"
#include "../Metadata/MetadataValues.hpp"
#include "../Metadata/FrontendMeta.hpp"
#include "../Metadata/Weapons.hpp"

#include <cassert>
#include <stdexcept>
#include <string_view>

int main() {
    using fruityprime::metadata::Metadata;
    assert(Metadata::WeaponNames[7] == "Shock Coil");
    assert(Metadata::SequenceFiles[15] == "000F - SEQ_TELEPORT.minincsf");
    assert(Metadata::SequenceFiles[31] == "001F - SEQ_SHIP_LAND1.minincsf");
    assert(Metadata::SequenceFiles[47] == "002F - SEQ_GOREA_2.minincsf");
    assert(Metadata::GetObjectById(12).Name == "Generic_Console");
    assert(Metadata::GetObjectById(12).AnimationIds[0] == 2);
    assert(Metadata::GetPlatformById(1)->Name == "platform");
    assert(Metadata::GetPlatformById(2) == nullptr);
    assert(Metadata::Doors[3].LockName == "ThinDoorLock");
    assert(Metadata::InvisiblePlat.Name == "N/A");
    assert(Metadata::GetModelByName("AlimbicGhost_01") != nullptr);
    assert(Metadata::GetRoomById(128)->name == "Level MP1");
    assert(Metadata::BeamDrawEffects[22] == 246);
    assert(Metadata::SpireAltVectors.size() == 16);
    assert(Metadata::GunAnimationIds[7][12][2] == 9);
    assert(Metadata::MovieFiles[13] == std::nullopt);
    assert(Metadata::SingleParticles[1].second.second == "fuzzBall");
    assert(Metadata::PreloadResources[7].first == "cylBossLaserBurn");
    assert(*Metadata::GetEnemyModelName(
               fruityprime::formats::EnemyType::WarWasp)
           == "warwasp_lod0");
    assert(!Metadata::GetEnemyModelName(
                static_cast<fruityprime::formats::EnemyType>(7))
                .has_value());
    std::array<fruityprime::metadata::Effectiveness, 9> loaded_effectiveness{};
    Metadata::LoadEffectiveness(0x2AAAA, loaded_effectiveness);
    assert(loaded_effectiveness[0]
               == fruityprime::metadata::Effectiveness::Normal
           && loaded_effectiveness[8]
               == fruityprime::metadata::Effectiveness::Normal);
    assert(Metadata::Enemy00Subroutines.size() == 7);
    assert(Metadata::Enemy19Subroutines.size() == 27);
    assert(Metadata::Enemy47Subroutines.size() == 20);
    assert(Metadata::Enemy10Values.size() == 3);
    assert(Metadata::Enemy41Values.size() == 12);
    assert(Metadata::ModelMetadata.size() == 253);
    assert(Metadata::FirstHuntModels.size() == 62);
    assert(fruityprime::weapon_catalog::BotWeapons.size() == 5);
    assert(fruityprime::weapon_catalog::BotWeapons[0][4].UnchargedDamage
           == 20);
    assert(fruityprime::weapon_catalog::BotWeapons[4][6]
               .ChargedSplashDamage == 18);

    const auto& hunters = fruityprime::metadata::hunters();
    assert(hunters.size() == 8);
    assert(hunters[0].name == "Samus");
    assert(hunters[4].archive == "Nox");
    assert(hunters[7].alt_model_entry == "SamusAlt_lod0_Model.bin");
    assert(hunters[0].affinity_weapon == 1);
    assert(hunters[1].affinity_weapon == 2);

    const auto* trace = fruityprime::metadata::find_hunter("tRaCe");
    assert(trace != nullptr);
    assert(trace->id == fruityprime::metadata::Hunter::Trace);
    assert(fruityprime::metadata::parse_hunter("6") == 6);
    assert(fruityprime::metadata::parse_hunter("weavel") == 6);
    assert(!fruityprime::metadata::parse_hunter("not-a-hunter").has_value());
    assert(fruityprime::metadata::roll_hunter(1) < 7);

    const auto& weapons = fruityprime::metadata::weapons();
    assert(weapons.size() == fruityprime::metadata::WeaponCount);
    assert(weapons[0].display_name == "Power Beam");
    assert(weapons[4].can_zoom);
    assert(fruityprime::metadata::find_weapon("shock coil")->id == 7);
    assert(fruityprime::metadata::weapon_info(255).id == 8);
    assert(fruityprime::metadata::native_weapon_slot_from_beam(0) == 0);
    assert(fruityprime::metadata::native_weapon_slot_from_beam(1) == 2);
    assert(fruityprime::metadata::native_weapon_slot_from_beam(2) == 1);
    assert(fruityprime::metadata::beam_type_from_native_weapon_slot(1) == 2);
    assert(fruityprime::metadata::native_weapon_slot_from_beam(-1) == 0xff);

    const auto& power_visual = fruityprime::metadata::weapon_visual_info(0);
    const std::array<std::uint8_t, 2> power_draw{0, 0};
    const std::array<std::uint16_t, 2> power_colors{9055, 21407};
    const std::array<std::uint8_t, 2> power_collision{4, 95};
    const std::array<std::uint8_t, 2> power_muzzle{65, 65};
    assert(power_visual.draw_func_ids == power_draw);
    assert(power_visual.colors == power_colors);
    assert(power_visual.collision_effects == power_collision);
    assert(power_visual.muzzle_effects == power_muzzle);
    const auto& magmaul_visual = fruityprime::metadata::weapon_visual_info(6);
    const std::array<std::uint8_t, 2> magmaul_draw{4, 5};
    const std::array<std::uint8_t, 2> linear_interpolation{2, 2};
    assert(magmaul_visual.draw_func_ids == magmaul_draw);
    assert(magmaul_visual.damage_interpolations == linear_interpolation);
    const auto& shock_visual = fruityprime::metadata::weapon_visual_info(7);
    const std::array<std::uint8_t, 2> no_collision_effect{255, 255};
    assert(shock_visual.collision_effects == no_collision_effect);
    assert(fruityprime::metadata::weapon_visual_info(255).draw_func_ids[0]
           == 11);

    const auto& effects = fruityprime::metadata::effects();
    assert(effects.size() == fruityprime::metadata::EffectCount);
    assert(effects[0].name.empty());
    assert(effects[1].name == "powerBeam");
    assert(effects[1].archive == "effects");
    assert(effects[246].name == "enemyMortarProjectile");
    assert(fruityprime::metadata::effect_info(246)->id == 246);
    assert(fruityprime::metadata::effect_info(247) == nullptr);

    const auto& items = fruityprime::metadata::items();
    assert(items.size() == fruityprime::metadata::ItemTableCount);
    assert(fruityprime::metadata::item_info(-1)->name == "NONE");
    assert(fruityprime::metadata::item_info(0)->asset_name == "pick_health_B");
    assert(fruityprime::metadata::item_info(12)->weapon == 8);
    assert(fruityprime::metadata::item_info(21)->asset_name == "pick_wpn_all");
    assert(fruityprime::metadata::item_info(22)->weapon == 1);
    assert(fruityprime::metadata::item_info(23) == nullptr);
    assert(fruityprime::metadata::fh_items().size() ==
           fruityprime::metadata::FhItemCount);
    assert(fruityprime::metadata::fh_items()[7] == "pick_wpn_missile");

    const auto* mode = fruityprime::metadata::game_mode_info(14);
    assert(mode != nullptr);
    assert(mode->name == "PRIME HUNTER");
    assert(mode->objective);
    assert(fruityprime::metadata::game_mode_name(255) == "MATCH");
    assert(fruityprime::metadata::area_info(27) == 0);
    assert(fruityprime::metadata::area_info(44) == 1);
    assert(fruityprime::metadata::area_info(45) == 2);
    assert(fruityprime::metadata::area_info(88) == 7);
    assert(fruityprime::metadata::area_info(89) == 8);
    assert(fruityprime::metadata::area_info(93) == 8);
    assert(fruityprime::metadata::room_ids().size() >= 138);
    const auto [unit1_cx, unit1_cx_id] =
        fruityprime::metadata::get_room_by_name("UNIT1_CX");
    assert(unit1_cx != nullptr && unit1_cx->id == 0 && unit1_cx_id == 0);
    assert(fruityprime::metadata::get_room_by_id(1)->name == "UNIT1_CX");
    assert(fruityprime::metadata::room_by_id(1)->name == "UNIT1_CX");
    assert(fruityprime::metadata::get_room_by_id(128)->name == "Level MP1");
    assert(fruityprime::metadata::get_room_by_id(-1, true) == nullptr);
    bool bad_room_threw = false;
    try {
        static_cast<void>(fruityprime::metadata::get_room_by_id(-1));
    } catch (const std::invalid_argument&) {
        bad_room_threw = true;
    }
    assert(bad_room_threw);
    assert(fruityprime::metadata::time_limit(4, 0, 0) == 7200);
    assert(*fruityprime::metadata::encounter_node_data_override(28)
           == R"(levels\nodeData\unit1_C0_Boss_Node.bin)");
    assert(!fruityprime::metadata::encounter_node_data_override(27));
    assert(*fruityprime::metadata::ctf_node_data_override(93)
           == R"(levels\nodeData\mp1_CTF_node.bi))");
    assert(*fruityprime::metadata::ctf_node_data_override(119)
           == R"(levels\nodeData\e3Level_CTF_Node.bin)");
    assert(fruityprime::metadata::layer_names(0, false) == "FirstVisit");
    assert(fruityprime::metadata::layer_names(1, false) == "Escape");
    assert(fruityprime::metadata::layer_names(3, false) == "SpLayer3");
    assert(fruityprime::metadata::layer_names(1, true)
           == "Battle2P | PrimeHunter2P");
    assert(fruityprime::metadata::layer_names(3, true)
           == "Battle2P/3P | PrimeHunter2P/3P");

    const auto& enemies = fruityprime::metadata::enemies();
    assert(enemies.size() == fruityprime::metadata::EnemyCount);
    assert(enemies[0].scan_id == 214);
    assert(enemies[12].effectiveness == 0x2AAA8);
    assert(enemies[51].death_effect == 220);
    assert(fruityprime::metadata::enemy_info(255).id ==
           fruityprime::formats::EnemyType::CarnivorousPlant);
    const auto effectiveness = fruityprime::metadata::decode_effectiveness(
        enemies[12].effectiveness);
    assert(effectiveness[0] == fruityprime::metadata::Effectiveness::Zero);
    assert(effectiveness[1] == fruityprime::metadata::Effectiveness::Normal);
    assert(fruityprime::metadata::damage_multiplier(
               fruityprime::metadata::Effectiveness::Double) == 2.0F);

    const auto& objects = fruityprime::metadata::objects();
    assert(objects.size() == fruityprime::metadata::ObjectCount);
    assert(objects[12].name == "Generic_Console");
    assert(objects[12].has_animation);
    assert(objects[16].lighting);
    assert(objects[44].ignore_animation);
    assert(objects[47].recolor_id == 1);
    assert(fruityprime::metadata::object_info(53)->name == "WallSwitch");
    assert(fruityprime::metadata::object_info(54) == nullptr);

    const auto& platforms = fruityprime::metadata::platforms();
    assert(platforms.size() == fruityprime::metadata::PlatformCount);
    assert(fruityprime::metadata::platform_info(1) == nullptr);
    assert(fruityprime::metadata::platform_info(2) == nullptr);
    assert(fruityprime::metadata::platform_model_info(1)->name == "platform");
    assert(platforms[8].animation_ids[1] == 1);
    assert(platforms[32].has_animation);

    assert(fruityprime::metadata::door_info(0)->lock_name
           == "AlimbicDoorLock");
    assert(fruityprime::metadata::door_info(3)->radius == 2.0F);
    assert(fruityprime::metadata::door_info(4) == nullptr);
    assert(fruityprime::metadata::fh_doors()[2] == "door2_holo");
    assert(fruityprime::metadata::jump_pad_name(4) == "JumpPad_Lava");
    assert(fruityprime::metadata::jump_pad_name(6).empty());

    using fruityprime::metadata::MetaDir;
    using fruityprime::metadata::MdlSuffix;
    using fruityprime::metadata::ModelMetadata;
    const ModelMetadata menu_model("Main", MetaDir::MainMenu, "Intro");
    assert(menu_model.ModelPath == R"(main menu\Main_Model.bin)");
    assert(menu_model.AnimationPath.has_value());
    assert(*menu_model.AnimationPath == R"(main menu\Intro_Anim.bin)");
    assert(menu_model.Recolors.size() == 1);
    assert(menu_model.Recolors[0].TexturePath == menu_model.ModelPath);

    const auto recolored = ModelMetadata::from_recolors(
        "Hunter_mdl", {"red", "*Shared"}, "_mdl", true, std::nullopt,
        true, MdlSuffix::All, std::nullopt, std::nullopt, "shared.anim",
        true, true);
    assert(recolored.ModelPath == R"(models\Hunter_mdl_mdl_Model.bin)");
    assert(recolored.AnimationPath.has_value());
    assert(*recolored.AnimationPath == R"(models\Hunter_mdl_Anim.bin)");
    assert(recolored.Recolors[0].TexturePath
           == R"(models\Hunter_red_Tex.bin)");
    assert(recolored.Recolors[1].ModelPath
           == R"(models\Shared_Model.bin)");
    assert(recolored.AnimationShare == "shared.anim");
    assert(recolored.UseLightSources && recolored.FirstHunt);

    const auto removed = ModelMetadata::from_remove(
        "Enemy_v1", "_v1", true, std::nullopt, true, true);
    assert(*removed.AnimationPath == R"(models\Enemy_Anim.bin)");
    assert(*removed.CollisionPath == R"(models\Enemy_Collision.bin)");
    assert(removed.FirstHunt);

    const fruityprime::metadata::ObjectMetadata object("Console", true, 2,
                                                       true);
    assert(object.AnimationIds == std::vector<int>({0, 0, 0, 0}));
    bool rejected = false;
    try {
        (void) fruityprime::metadata::ObjectMetadata(
            "bad", false, 0, false, std::vector<int>{1, 2, 3});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    const fruityprime::metadata::PlatformMetadata platform("Lift", true,
                                                            std::vector<int>{0, 1, 2, 3});
    assert(platform.Animation);
    assert(platform.AnimationIds[1] == 1);
    const fruityprime::metadata::DoorMetadata door("Door", "Lock", 1.5F,
                                                   2.0F);
    assert(door.LockName == "Lock" && door.LockOffset == 1.5F
           && door.Radius == 2.0F);

    assert(fruityprime::metadata::Ad2Dm2.ModelPath
           == R"(stage\ad2_dm2_Model.bin)");
    assert(fruityprime::metadata::NavMapModelNames.size() == 7);
    assert(fruityprime::metadata::NavMapModelNames[6] == "unit4_1nav");
    assert(fruityprime::metadata::HudModels.size() == 20);
    assert(fruityprime::metadata::TouchToStartModels.size() == 1);
    assert(fruityprime::metadata::MultiplayerModels.size() == 14);
    assert(fruityprime::metadata::LogoModels.size() == 11);
    assert(fruityprime::metadata::FrontendModels.size() == 244);
    const auto* hud_icons = fruityprime::metadata::find_hud_model("icons");
    assert(hud_icons != nullptr);
    assert(hud_icons->Recolors[0].TexturePath
           == R"(models\icons_Tex.bin)");
    const auto* character = fruityprime::metadata::find_frontend_model(
        "big_samus");
    assert(character != nullptr);
    assert(character->ModelPath
           == R"(characterselect\big_samus_Model.bin)");
    assert(fruityprime::metadata::find_logo_model("missing") == nullptr);
    assert(fruityprime::metadata::MovieFiles.size() == 36);
    assert(fruityprime::metadata::MovieFiles[3]->TopScreenPath
           == R"(movies\04.vx)");
    assert(!fruityprime::metadata::MovieFiles[13].has_value());
    assert(!fruityprime::metadata::MovieFiles[34].has_value());
    assert(fruityprime::metadata::MovieFiles[35]->BottomScreenPath
           == R"(movies\36_bot.vx)");
    assert(fruityprime::metadata::MovieDisplayInfo.size() == 36);
    assert(fruityprime::metadata::MovieDisplayInfo[35]
           == "Bad Ending Part 2 (36_top/36_bot) - 35");
    assert(fruityprime::metadata::SpireAltVectors.size() == 16);
    assert(fruityprime::metadata::SpireAltVectors[0].x
           == fruityprime::metadata::player::SpireAltVectors[0].x);
    assert(fruityprime::metadata::MuzzleOffests[7].z == 0.32F);
    assert(fruityprime::metadata::GunAnimationIds[7][12][0] == 47);
    assert(fruityprime::metadata::PlayerValues.size() == 8);
    assert(fruityprime::metadata::PlayerValues[0].WalkBipedTraction == 450);

    const auto* model = fruityprime::metadata::get_model_by_name(
        "Guardian_lod0");
    assert(model != nullptr);
    assert(model->ModelPath == R"(_archives\Guardian\Guardian_lod0_Model.bin)");
    assert(model->Recolors.size() == 6);
    assert(model->AnimationShare.has_value());
    assert(*model->AnimationShare == R"(models\SamusSharedAnim_Anim.bin)");
    assert(model->UseLightSources);
    assert(fruityprime::metadata::get_model_by_name(
               "big_samus", fruityprime::metadata::MetaDir::CharSelect)
           != nullptr);
    assert(fruityprime::metadata::get_model_by_name(
               "missing", fruityprime::metadata::MetaDir::Models)
           == nullptr);
    assert(fruityprime::metadata::get_model_by_name(
               "doubleDamage_img", fruityprime::metadata::MetaDir::Hud)
           == &fruityprime::metadata::DoubleDamageImg);
    assert(fruityprime::metadata::get_model_by_name(
               "ad2_dm2", fruityprime::metadata::MetaDir::Models)
           == &fruityprime::metadata::Ad2Dm2);
    const auto* first_hunt = fruityprime::metadata::get_first_hunt_model_by_name(
        "morphBall");
    assert(first_hunt != nullptr && first_hunt->FirstHunt);
    assert(fruityprime::metadata::get_entity_by_path(
               R"(models\cylinderbase_model.bin)") != nullptr);
    assert(fruityprime::metadata::ToonTable.size() == 31);
    assert(fruityprime::metadata::PowerPalettes.size() == 4);
    assert(fruityprime::metadata::power_palette("Ice_Power")
           ->Values[7].Data == 32734);
    assert(fruityprime::metadata::HunterScales[1]
           == static_cast<float>(0x10F5) / 4096.0F);
    assert(fruityprime::metadata::HunterModels[7][2] == "SamusAlt_lod0");
    assert(fruityprime::metadata::ImaIndexTable[4] == 2);

    using MphReadNative::Weapons::GetAffinityBeam;
    MphReadNative::Weapons::Current = {};
    assert(MphReadNative::Weapons::Current.empty());
    MphReadNative::Weapons::SetCurrent(false);
    assert(MphReadNative::Weapons::Current.size() == 18
           && MphReadNative::Weapons::Current.data()
               == MphReadNative::Weapons::Weapons1P.data());
    MphReadNative::Weapons::SetCurrent(true);
    assert(MphReadNative::Weapons::Current.size() == 18
           && MphReadNative::Weapons::Current.data()
               == MphReadNative::Weapons::WeaponsMP.data());
    assert(GetAffinityBeam(fruityprime::formats::Hunter::Samus)
               == fruityprime::formats::BeamType::Missile
           && GetAffinityBeam(fruityprime::formats::Hunter::Guardian)
               == fruityprime::formats::BeamType::PowerBeam);
    bool bad_hunter_threw = false;
    try {
        static_cast<void>(GetAffinityBeam(
            static_cast<fruityprime::formats::Hunter>(9)));
    } catch (const std::out_of_range&) {
        bad_hunter_threw = true;
    }
    assert(bad_hunter_threw);
    return 0;
}
