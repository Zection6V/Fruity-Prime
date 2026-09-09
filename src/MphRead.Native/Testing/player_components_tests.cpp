#include "../Entities/Players/DynamicLightEntity.hpp"
#include "../Entities/Players/PlayerCamera.hpp"
#include "../Entities/Players/PlayerDialog.hpp"
#include "../Entities/Players/PlayerDraw.hpp"
#include "../Entities/Players/PlayerEntity.hpp"
#include "../Entities/Players/PlayerPause.hpp"
#include "../Entities/Players/PlayerProcess.hpp"
#include "../Entities/Players/PlayerScan.hpp"
#include "../Entities/Players/PlayerSound.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    using namespace fruityprime;
    using namespace fruityprime::players;

    DynamicLightEntityBase lights;
    lights.reset_lights({1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                        {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 0.0F});
    LightSource source;
    source.volume.kind = scene::VolumeKind::Sphere;
    source.volume.sphere_position = {0.0F, 0.0F, 0.0F};
    source.volume.sphere_radius = 2.0F;
    source.light1_enabled = true;
    source.light1_vector = {0.0F, 1.0F, 0.0F};
    source.light1_color = {1.0F, 0.25F, 0.0F};
    const std::vector<LightSource> sources{source};
    lights.update_light_sources({0.0F, 0.0F, 0.0F}, sources,
                                {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                                {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                                1.0F / 30.0F);
    assert(lights.light1_color().x > 0.0F);
    assert(lights.light1_vector().y > 0.0F);
    lights.reset_lights({1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                        {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 0.0F});
    lights.set_use_room_lights(true);
    lights.update_light_sources({0.0F, 0.0F, 0.0F}, sources,
                                {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                                {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
                                1.0F / 30.0F);
    assert(lights.light1_vector().y > 0.0F);

    net::PlayerState player;
    player.flags = net::PlayerState::FlagActive
        | net::PlayerState::FlagSpawned;
    player.position = {1.0F, 2.0F, 3.0F};
    player.facing = {0.0F, 0.0F, 1.0F};
    players::Profile profile = players::profile(0);
    PlayerCamera camera;
    camera.reset();
    assert(camera.info().position.z == 1.0F);
    assert(camera.info().target.z == 0.0F);
    camera.info().set_shake(0.25F);
    camera.info().set_shake(0.125F);
    assert(camera.info().shake == 0.25F);
    camera.update(player, profile, 1.0F / 60.0F);
    assert(camera.info().position.y > player.position.y);
    camera.switch_camera(CameraType::Third1, player.facing);
    camera.update(player, profile, 1.0F / 60.0F);
    assert(camera.type() == CameraType::Third1);
    camera.update(player, profile, 1.0F / 60.0F, false,
                  net::Vec3{0.0F, 0.0F, 0.0F});
    assert(camera.info().position.x == 0.0F
        && camera.info().position.y == 0.0F
        && camera.info().position.z == 0.0F);

    const DrawPacket alt = PlayerDraw::build(
        player, profile, CameraType::Third1, false, false, 16.0F);
    assert(alt.mode == DrawMode::Biped);
    player.flags |= net::PlayerState::FlagSpectating;
    assert(PlayerDraw::build(player, profile, CameraType::First, true,
                             false, 0.0F).mode == DrawMode::Hidden);

    PlayerDialog dialog;
    dialog.show(DialogType::Okay, "one\ntwo\nthree\nfour", 5.0F);
    assert(dialog.page_count() == 2);
    dialog.confirm();
    assert(dialog.page_index() == 1);
    dialog.confirm();
    assert(dialog.type() == DialogType::None);

    PlayerPause pause;
    pause.open();
    pause.update(0.2F);
    assert(pause.state() == PauseState::Open);
    pause.move(1);
    assert(pause.selected_item() == 1);
    pause.close();
    pause.update(0.2F);
    assert(pause.state() == PauseState::Closed);

    PlayerScan scan;
    ScanTarget target;
    target.scan_id = 42;
    target.entity_id = 7;
    target.distance = 11.0F;
    target.scan_time = 2.5F;
    scan.update_target(&target);
    scan.begin_scan();
    scan.update(1.0F, &target);
    assert(scan.scanning());
    scan.update(1.5F, &target);
    // PlayerScan.cs reaches the time in the first branch and completes on the
    // following update.
    assert(!scan.scan_complete());
    scan.update(0.0F, &target);
    assert(scan.scan_complete());
    assert(scan.scan_seconds() == 2.5F);
    scan.set_combat_visor();
    assert(!scan.scanning() && scan.visor() == Visor::Combat);

    PlayerScan logged_scan;
    target.already_logged = true;
    logged_scan.update_target(&target);
    logged_scan.begin_scan();
    logged_scan.update(0.0F, &target);
    assert(logged_scan.show_dialog_confirm());
    assert(!logged_scan.scan_complete());
    logged_scan.update(0.0F, &target);
    assert(logged_scan.scan_complete());

    player.flags &= static_cast<std::uint8_t>(
        ~net::PlayerState::FlagSpectating);
    assert(PlayerProcess::can_process(player));
    const auto death = PlayerSound::death(2, {1.0F, 2.0F, 3.0F});
    assert(death.cue == gameplay::SoundCue::PlayerDeath);
    assert(death.slot == 2);
    static_assert(static_cast<int>(HunterSfx::Damage) == 0);
    static_assert(static_cast<int>(HunterSfx::Spawn) == 4);
    static_assert(static_cast<int>(HunterSfx::MissileCharge) == 16);

    PlayerSoundState sound_state;
    PlayerSoundState::MuteState mute;
    sound_state.update_scan_sfx(1, true);
    sound_state.stop_timed_sfx(mute);
    assert(mute.timed_sfx_mute == 1);
    assert(sound_state.scan_sfx_on()[1]);
    sound_state.restart_timed_sfx(mute);
    sound_state.door_unlock_sfx_timer = 2.0F / 30.0F;
    sound_state.door_chime_sfx_timer = 2.0F / 30.0F;
    sound_state.force_field_sfx_timer = 1.0F / 30.0F;
    const auto sounds = sound_state.update_timed_sounds(1.0F / 30.0F, mute);
    assert(sounds.play_door_unlock);
    assert(sounds.play_door_chime);
    assert(sounds.play_force_field);
    assert(sounds.play_scan[1]);

    std::vector<strings::TableEntry> names;
    names.push_back(strings::TableEntry{
        "W001", 'W', "Power Beam", {}, 0, '\0', {}, {}});
    names.push_back(strings::TableEntry{
        "H001", 'H', "Samus", {}, 0, '\0', {}, {}});
    names.push_back(strings::TableEntry{
        "A001", 'A', "Bomb", {}, 0, '\0', {}, {}});
    PlayerEntity::WeaponNameTable(std::move(names));
    PlayerEntity::LoadWeaponNames();
    assert(PlayerEntity::WeaponNames()[0] == "Power Beam");
    assert(PlayerEntity::HunterNames()[0] == "Samus");
    assert(PlayerEntity::AltAttackNames()[0] == "Bomb");
    assert(PlayerEntity::AltAttackNames()[7].empty());

    PlayerRuntimeState defaults;
    assert(defaults.Team == formats::Team::None);
    assert(defaults.TeamIndex == -1);
    assert(defaults.CurAlpha == 1.0F);
    assert(defaults.TimeSinceShot == 0);
    assert(defaults.RespawnTimer == 0);
    assert(PlayerEntity::ScanIds[0][0] == 0);
    assert(PlayerEntity::ScanIds[1][0] == 232);
    assert(PlayerEntity::ScanIds[4][2] == 530);
    assert(PlayerEntity::ScanIds[7][2] == 224);
    PlayerEntity::GeneratePlayerVolumes();
    const auto& samus_pickup = PlayerEntity::PlayerVolume(
        0, PlayerStatics::Volume::PickupLow);
    assert(samus_pickup.Type == formats::VolumeType::Sphere);
    assert(samus_pickup.SphereRadius > 0.0F);
    AvailableArray beam_array;
    beam_array[formats::BeamType::Missile] = true;
    assert(beam_array[formats::BeamType::Missile]);

    std::cout << "native player component contracts passed\n";
    return 0;
}
