#include "../Entities/Players/PlayerCamera.hpp"
#include "../Entities/Players/PlayerCollision.hpp"
#include "../Entities/Players/PlayerDialog.hpp"
#include "../Entities/Players/PlayerProcess.hpp"

#include "Metadata/player_values.hpp"
#include "Entities/runtime_entities.hpp"
#include "Metadata/weapon_metadata.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using fruityprime::formats::Hunter;
using fruityprime::players::AltAttackAttacker;
using fruityprime::players::AltAttackHit;
using fruityprime::players::AltAttackTarget;
using fruityprime::players::CameraType;
using fruityprime::players::PlayerCamera;
using fruityprime::players::PlayerDialog;
using fruityprime::players::PlayerProcess;

[[nodiscard]] bool near_equal(float a, float b, float epsilon = 1e-3F) {
    return std::fabs(a - b) <= epsilon;
}

[[nodiscard]] const fruityprime::entities::PlayerValues& samus_values() {
    return fruityprime::metadata::PlayerValuesTable[0];
}

// A jump pad locks the player out of steering, and a morphed player is locked
// out for longer because a ball falls more slowly than a biped.
void test_jump_pad() {
    const fruityprime::net::Vec3 launch{0.0F, 0.6F, 0.0F};
    const auto biped = PlayerProcess::activate_jump_pad(
        launch, /*lock_time=*/10, /*alt_form=*/false,
        /*time_since_jump_pad=*/100.0F, samus_values());
    assert(biped.play_sfx);
    assert(biped.control_lock == 20);
    assert(biped.speed.y == 0.6F);

    // A pad hit again within five doubled frames does not restack the sound.
    const auto quick = PlayerProcess::activate_jump_pad(
        launch, 10, false, 5.0F, samus_values());
    assert(!quick.play_sfx);

    // A short pad still holds control for five doubled frames.
    const auto brief = PlayerProcess::activate_jump_pad(
        launch, 1, false, 100.0F, samus_values());
    assert(brief.control_lock == 2);
    assert(brief.control_lock_min == 10);

    const auto morphed = PlayerProcess::activate_jump_pad(
        launch, 10, /*alt_form=*/true, 100.0F, samus_values());
    assert(morphed.control_lock > biped.control_lock);

    // A purely horizontal launch has no flight time to extend, and must not
    // divide by its zero vertical speed.
    const auto flat = PlayerProcess::activate_jump_pad(
        {1.0F, 0.0F, 0.0F}, 10, true, 100.0F, samus_values());
    assert(flat.control_lock == 20);
}

// A pickup is split with the halfturret, and the larger half goes to whichever
// of the two is further behind.
void test_gain_health() {
    // No halfturret: the whole amount, capped.
    auto gain = PlayerProcess::gain_health(50, 100, 30, false, 0);
    assert(gain.health == 80);
    gain = PlayerProcess::gain_health(90, 100, 30, false, 0);
    assert(gain.health == 100);

    // A dead player gains nothing.
    gain = PlayerProcess::gain_health(0, 100, 30, false, 0);
    assert(gain.health == 0);

    // Player behind the turret: the player takes the larger half of an odd
    // amount.
    gain = PlayerProcess::gain_health(20, 100, 5, true, 60);
    assert(gain.health == 20 + 3);
    assert(gain.halfturret_health == 60 + 2);

    // Turret behind the player: the turret takes the larger half.
    gain = PlayerProcess::gain_health(80, 100, 5, true, 10);
    assert(gain.health == 80 + 2);
    assert(gain.halfturret_health == 10 + 3);

    // The turret caps at 100 independently of the player.
    gain = PlayerProcess::gain_health(50, 100, 100, true, 98);
    assert(gain.halfturret_health == PlayerProcess::HalfturretHealthMax);
    assert(gain.health <= 100);
}

// Unmorphing reclaims what the halfturret has left and ends an alt attack
// before the form changes.
void test_exit_alt_form() {
    auto exit = PlayerProcess::exit_alt_form(true, 40, true);
    assert(exit.release_halfturret);
    assert(exit.reclaimed_health == 40);
    assert(exit.end_alt_attack);

    // A destroyed turret returns nothing rather than a negative amount.
    exit = PlayerProcess::exit_alt_form(true, -5, false);
    assert(exit.release_halfturret);
    assert(exit.reclaimed_health == 0);
    assert(!exit.end_alt_attack);

    exit = PlayerProcess::exit_alt_form(false, 40, false);
    assert(!exit.release_halfturret);
    assert(exit.reclaimed_health == 0);
}

// The match-end camera orbits the winner, and is pulled in to the first wall
// it would otherwise pass through.
void test_match_end_camera() {
    PlayerCamera camera;
    camera.reset();
    fruityprime::players::MatchEndWinner winner;
    winner.position = {0.0F, 0.0F, 0.0F};
    winner.facing = {0.0F, 0.0F, 1.0F};
    winner.field70 = 0.0F;
    winner.field74 = 1.0F;

    camera.update_match_end_camera(winner, 0.0F, samus_values());
    assert(camera.type() == CameraType::Third2);
    // A biped's target is pushed ten units ahead of them.
    assert(near_equal(camera.info().target.z, 10.0F));
    assert(near_equal(camera.info().position.y, 1.75F));
    assert(near_equal(camera.info().up.y, 1.0F));
    assert(camera.info().shake == 0.0F);
    // NormalFov is stored as fixed point and doubled: 39 * 2.
    assert(near_equal(camera.info().fov_degrees, 78.0F, 0.5F));

    // A ball is watched from further back and higher, and its target is not
    // pushed ahead.
    winner.alt_form = true;
    camera.update_match_end_camera(winner, 0.0F, samus_values());
    assert(near_equal(camera.info().target.z, 0.0F));
    assert(near_equal(camera.info().position.y, 3.0F));

    // The shot drifts sideways as the results screen stays up.
    winner.alt_form = false;
    winner.gun_vec2 = {1.0F, 0.0F, 0.0F};
    camera.update_match_end_camera(winner, 0.0F, samus_values());
    const float still_x = camera.info().position.x;
    camera.update_match_end_camera(winner, 2.0F, samus_values());
    assert(camera.info().position.x > still_x);

    // A wall pulls the camera in to where it was hit, plus a small nudge off
    // the surface so the near plane does not clip into it.
    bool asked = false;
    camera.update_match_end_camera(
        winner, 0.0F, samus_values(),
        [&asked](fruityprime::net::Vec3, fruityprime::net::Vec3,
                 float& distance, fruityprime::net::Vec3& plane) {
            asked = true;
            distance = 0.5F;
            plane = {0.0F, 1.0F, 0.0F};
            return true;
        });
    assert(asked);
    // Half way along a line that started 1.75 above the winner, plus 0.05.
    assert(camera.info().position.y < 1.75F);
}

// Handing the camera back to its own player puts it behind them, and the two
// camera modes back along different vectors.
void test_resume_own_camera() {
    PlayerCamera camera;
    camera.reset();
    camera.switch_camera(CameraType::Third1, {0.0F, 0.0F, 1.0F});
    camera.resume_own_camera({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F},
                             /*field80=*/0.0F, /*field84=*/1.0F,
                             samus_values());
    const float back = static_cast<float>(samus_values().Field78) / 4096.0F;
    // Third1 backs along the player's cached pair, which is +Z here.
    assert(near_equal(camera.info().position.z, -back));
    // Only the target is raised, so the camera stays level.
    assert(camera.info().target.y > 0.0F);
    assert(near_equal(camera.info().position.y, 0.0F));
    assert(near_equal(camera.saved_position().z, -back));

    camera.switch_camera(CameraType::First, {0.0F, 0.0F, 1.0F});
    camera.resume_own_camera({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 0.0F,
                             1.0F, samus_values());
    // The other branch backs along the facing vector from above the alt-form
    // collision centre, so both the camera and its target are raised.
    assert(camera.info().position.y > 0.0F);
    assert(near_equal(camera.info().position.z, -back));
}

// An external camera taking over holds the alt-form direction and restarts the
// morph blend.
void test_refresh_external_camera() {
    PlayerCamera camera;
    camera.reset();
    assert(!camera.alt_dir_override());
    camera.refresh_external_camera();
    assert(camera.alt_dir_override());
    assert(camera.time_since_morph_camera() == 0.0F);
}

// The camera's cached basis is what the match-end orbit starts from.
void test_match_end_basis() {
    PlayerCamera camera;
    camera.reset();
    const auto basis = camera.set_up_match_end_camera();
    assert(basis.field70 == camera.info().field48);
    assert(basis.field74 == camera.info().field4c);
    assert(basis.gun_vec2.x == camera.info().field50);
    assert(basis.gun_vec2.y == 0.0F);
    assert(basis.gun_vec2.z == camera.info().field54);
    assert(basis.facing.z == camera.info().facing.z);
}

[[nodiscard]] fruityprime::formats::CollisionVolume sphere(
    fruityprime::formats::Vector3 position, float radius) {
    fruityprime::formats::CollisionVolume volume;
    volume.Type = fruityprime::formats::VolumeType::Sphere;
    volume.SpherePosition = position;
    volume.SphereRadius = radius;
    return volume;
}

// Spire's rocks hit without ending the attack; Noxus's disc is flat, so it
// misses something directly overhead however close it is.
void test_alt_attack_enemy1() {
    AltAttackAttacker spire;
    spire.hunter = Hunter::Spire;
    spire.alt_attack = true;
    spire.volume = sphere({0.0F, 0.0F, 0.0F}, 1.0F);
    spire.spire_rock_left = {2.0F, 0.0F, 0.0F};
    spire.spire_rock_right = {-2.0F, 0.0F, 0.0F};

    AltAttackTarget target;
    target.hurt_volume = sphere({2.4F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::SpireRock);
    // The rocks keep spinning after a hit.
    assert(!fruityprime::players::alt_attack_ends_on_hit(
        AltAttackHit::SpireRock));

    // The other rock reaches it too.
    target.hurt_volume = sphere({-2.4F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::SpireRock);

    target.hurt_volume = sphere({8.0F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::None);

    // CollisionDetection.CheckSphereOverlapVolume uses an inclusive boundary
    // and accepts cylinder/box hurt volumes too.
    target.hurt_volume = sphere({3.0F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::SpireRock);
    target.hurt_volume = {};
    target.hurt_volume.Type = fruityprime::formats::VolumeType::Cylinder;
    target.hurt_volume.CylinderPosition = {2.0F, -0.5F, 0.0F};
    target.hurt_volume.CylinderVector = {0.0F, 1.0F, 0.0F};
    target.hurt_volume.CylinderDot = 1.0F;
    target.hurt_volume.CylinderRadius = 0.25F;
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::SpireRock);
    target.hurt_volume = {};
    target.hurt_volume.Type = fruityprime::formats::VolumeType::Box;
    target.hurt_volume.BoxPosition = {1.75F, -0.25F, -0.25F};
    target.hurt_volume.BoxVector1 = {1.0F, 0.0F, 0.0F};
    target.hurt_volume.BoxVector2 = {0.0F, 1.0F, 0.0F};
    target.hurt_volume.BoxVector3 = {0.0F, 0.0F, 1.0F};
    target.hurt_volume.BoxDot1 = 0.5F;
    target.hurt_volume.BoxDot2 = 0.5F;
    target.hurt_volume.BoxDot3 = 0.5F;
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::SpireRock);

    // An invincible enemy is untouchable however close.
    target.hurt_volume = sphere({2.0F, 0.0F, 0.0F}, 0.5F);
    target.invincible = true;
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::None);
    target.invincible = false;
    target.no_bomb_damage = true;
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::None);
    target.no_bomb_damage = false;

    // Not attacking, no hit.
    spire.alt_attack = false;
    assert(fruityprime::players::check_alt_attack_hit_enemy1(spire, target)
           == AltAttackHit::None);

    AltAttackAttacker noxus;
    noxus.hunter = Hunter::Noxus;
    noxus.volume = sphere({0.0F, 0.0F, 0.0F}, 1.0F);
    noxus.alt_attack_startup = 5.0F;
    noxus.alt_attack_time = 10.0F;

    AltAttackTarget beside;
    beside.hurt_volume = sphere({1.5F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy1(noxus, beside)
           == AltAttackHit::NoxusDisc);
    assert(fruityprime::players::alt_attack_ends_on_hit(
        AltAttackHit::NoxusDisc));

    // Directly overhead, within the same distance: the disc is flat, so this
    // is a miss.
    AltAttackTarget above;
    above.hurt_volume = sphere({0.0F, 1.5F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy1(noxus, above)
           == AltAttackHit::None);

    // Before the startup has elapsed, nothing lands.
    noxus.alt_attack_time = 4.0F;
    assert(fruityprime::players::check_alt_attack_hit_enemy1(noxus, beside)
           == AltAttackHit::None);
}

// Trace's and Weavel's spins are plain sphere overlaps, and both end on a hit.
void test_alt_attack_enemy2() {
    AltAttackAttacker trace;
    trace.hunter = Hunter::Trace;
    trace.alt_attack = true;
    trace.volume = sphere({0.0F, 0.0F, 0.0F}, 1.0F);

    AltAttackTarget target;
    target.hurt_volume = sphere({1.2F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy2(trace, target)
           == AltAttackHit::TraceSpin);

    trace.hunter = Hunter::Weavel;
    assert(fruityprime::players::check_alt_attack_hit_enemy2(trace, target)
           == AltAttackHit::WeavelSpin);
    assert(fruityprime::players::alt_attack_ends_on_hit(
        AltAttackHit::WeavelSpin));

    // Exactly on top of each other still counts as an overlap.
    target.hurt_volume = sphere({0.0F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy2(trace, target)
           == AltAttackHit::WeavelSpin);

    target.hurt_volume = sphere({5.0F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy2(trace, target)
           == AltAttackHit::None);

    // A hunter without a spin never lands one, however close.
    trace.hunter = Hunter::Samus;
    target.hurt_volume = sphere({0.5F, 0.0F, 0.0F}, 0.5F);
    assert(fruityprime::players::check_alt_attack_hit_enemy2(trace, target)
           == AltAttackHit::None);
}

// A dialog asked for outside the story, or before the HUD exists, is refused
// rather than crashing on an uninitialized HUD object.
void test_dialog_gate() {
    PlayerDialog dialog;
    PlayerDialog::DialogContext context;
    context.initialized = false;
    context.single_player = true;
    context.is_main_player = true;
    assert(!dialog.show_dialog(context, fruityprime::players::DialogType::Okay,
                               "hello", 1.0F));
    assert(dialog.type() == fruityprime::players::DialogType::None);

    context.initialized = true;
    context.single_player = false;
    assert(!dialog.show_dialog(context, fruityprime::players::DialogType::Okay,
                               "hello", 1.0F));

    context.single_player = true;
    context.is_main_player = false;
    assert(!dialog.show_dialog(context, fruityprime::players::DialogType::Okay,
                               "hello", 1.0F));

    context.is_main_player = true;
    assert(dialog.show_dialog(context, fruityprime::players::DialogType::Okay,
                              "hello", 1.0F));
    assert(dialog.type() == fruityprime::players::DialogType::Okay);
}

// A prompt puts the scan visor away and closing it brings the visor back --
// but only when the dialog was what put it away.
void test_dialog_visor() {
    PlayerDialog dialog;
    PlayerDialog::DialogContext context;
    context.initialized = true;
    context.single_player = true;
    context.is_main_player = true;
    context.scan_visor = true;

    assert(dialog.show_dialog(context, fruityprime::players::DialogType::Okay,
                              "prompt", 1.0F));
    assert(dialog.silent_visor_switch());
    assert(dialog.close_dialogs(context));
    assert(!dialog.silent_visor_switch());

    // A scan dialog is what the scan visor is for, so it is left alone.
    assert(dialog.show_dialog(context, fruityprime::players::DialogType::Scan,
                              "scan", 1.0F));
    assert(!dialog.silent_visor_switch());
    assert(!dialog.close_dialogs(context));

    // An overlay is a notice, not a prompt: it never takes the visor.
    assert(dialog.show_dialog(context,
                              fruityprime::players::DialogType::Overlay,
                              "notice", 1.0F));
    assert(!dialog.silent_visor_switch());

    // Closing behind the gate does nothing at all.
    PlayerDialog::DialogContext blocked;
    assert(!dialog.close_dialogs(blocked));
}

// The halfturret dies once: a second call spawns no second death effect.
void test_halfturret_die() {
    fruityprime::runtime::HalfturretEntity turret(1, 0, {0.0F, 0.0F, 0.0F});
    turret.set_health(50);
    assert(turret.die());
    assert(turret.health() == 0);
    assert(!turret.die());

    turret.set_grounded(true);
    assert(turret.grounded());
    turret.reset_grounded_state();
    assert(!turret.grounded());
}

void test_halfturret_damage_cooldown() {
    fruityprime::runtime::HalfturretEntity turret(1, 0, {0.0F, 0.0F, 0.0F});
    turret.set_health(50);
    fruityprime::messaging::MessageInfo activate{};
    activate.cartridge_message = 48;
    turret.handle_message(activate);
    turret.on_take_damage(1);
    assert(turret.health() == 50);
    turret.take_damage(1);
    assert(turret.health() == 49);
    assert(near_equal(turret.cooldown_factor(), 0.7F));
    assert(turret.process(2.0F));
    assert(near_equal(turret.cooldown_factor(), 1.15F));
    assert(turret.process(2.0F));
    assert(near_equal(turret.cooldown_factor(), 1.5F));
}

// The turret's ballistic solve aims above a distant target, and says so when
// the target is out of reach.
void test_halfturret_aim() {
    const auto& weapon = fruityprime::metadata::weapon_table::WeaponsMP[3];
    fruityprime::net::Vec3 aim{};

    // Level with the muzzle and close: the shot is aimed slightly up to carry
    // over the drop.
    assert(fruityprime::runtime::HalfturretEntity::update_aim(
        {0.0F, 0.0F, 0.0F}, {6.0F, 0.0F, 0.0F}, weapon, 0, aim));
    assert(aim.y > 0.0F);
    const float length = std::sqrt(aim.x * aim.x + aim.y * aim.y
                                   + aim.z * aim.z);
    assert(near_equal(length, 1.0F));

    // Standing exactly on the target: any direction will do, and the managed
    // code picks +X rather than leaving a zero vector.
    static_cast<void>(fruityprime::runtime::HalfturretEntity::update_aim(
        {1.0F, 2.0F, 3.0F}, {1.0F, 2.0F, 3.0F}, weapon, 0, aim));
    assert(aim.x == 1.0F && aim.y == 0.0F && aim.z == 0.0F);

    // Far enough away that the shot cannot reach: the aim is still usable and
    // the caller is told it will fall short.
    const bool reachable =
        fruityprime::runtime::HalfturretEntity::update_aim(
            {0.0F, 0.0F, 0.0F}, {5000.0F, 0.0F, 0.0F}, weapon, 0, aim);
    assert(!reachable);
    const float far_length = std::sqrt(aim.x * aim.x + aim.y * aim.y
                                        + aim.z * aim.z);
    assert(near_equal(far_length, 1.0F));
}

} // namespace

int main() {
    test_jump_pad();
    test_gain_health();
    test_exit_alt_form();
    test_match_end_camera();
    test_resume_own_camera();
    test_refresh_external_camera();
    test_match_end_basis();
    test_alt_attack_enemy1();
    test_alt_attack_enemy2();
    test_dialog_gate();
    test_dialog_visor();
    test_halfturret_die();
    test_halfturret_damage_cooldown();
    test_halfturret_aim();
    std::cout << "player action tests passed\n";
    return 0;
}
