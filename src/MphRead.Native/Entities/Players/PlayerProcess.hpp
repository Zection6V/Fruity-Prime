#pragma once

#include "Entities/entity_records.hpp"
#include "Entities/gameplay.hpp"

#include <cstdint>

namespace fruityprime::players {

// Native counterpart of PlayerProcess.cs.  This adapter contains the
// per-frame gates that belong to the PlayerEntity partial: dead/spectating
// players are not fed movement or fire, and form locks are applied before the
// input reaches Session's authoritative simulation.
class PlayerProcess final {
public:
    [[nodiscard]] static bool can_process(
        const net::PlayerState& player) noexcept;
    [[nodiscard]] static gameplay::Input apply_form_locks(
        gameplay::Input input, const net::PlayerState& player,
        bool prevent_form_switch, bool biped_lock,
        bool control_locked) noexcept;
    static void submit(gameplay::Session& session, std::uint8_t slot,
                       gameplay::Input input, const net::PlayerState& player,
                       bool prevent_form_switch = false,
                       bool biped_lock = false,
                       bool control_locked = false);


    // PlayerProcess.UpdateAimVecs.
    //
    // The arm cannon does not sit at the eye and point where the eye points.
    // It hangs off the camera by three fixed offsets -- along the facing,
    // along the camera's horizontal right, and along its up -- and then aims
    // at the point the player is aiming at, which is a little in front.  That
    // is what makes it swing across the view as you turn rather than being
    // painted onto it.
    //
    // Everything here is read by the renderer (which draws the cannon), by
    // the shot path (which fires from the muzzle) and by the bots (whose
    // vector functions ask where the muzzle is), so it belongs to the player
    // rather than to any one of them.
    struct AimVectors {
        // _gunDrawPos: where the cannon is drawn from.
        net::Vec3 gun_draw_pos{};
        // _aimVec: the direction it points, which lags the camera by half.
        net::Vec3 aim_vec{0.0F, 0.0F, 1.0F};
        // _muzzlePos: where a shot leaves it.
        net::Vec3 muzzle_pos{};
        // _aimPosition: the point in the world being aimed at.
        net::Vec3 aim_position{};
    };

    // `facing` is the player's own facing, `up` and `camera_position` the
    // camera's, and `gun_vec2` its horizontal right (CameraInfo.Field50/54).
    // `look` is where the camera is pointing, pitch included.
    // `view_bob_degrees` is _gunViewBob, the walk sway.
    [[nodiscard]] static AimVectors update_aim_vecs(
        const entities::PlayerValues& values, net::Vec3 camera_position,
        net::Vec3 facing, net::Vec3 up, net::Vec3 gun_vec2, net::Vec3 look,
        float view_bob_degrees, bool fixed_weapon) noexcept;

    // PlayerProcess.ActivateJumpPad.  The lock is how long the player
    // cannot steer for, in doubled frames; a pad that would otherwise
    // let go instantly is held for five.
    struct JumpPadResult {
        net::Vec3 speed{};
        std::uint16_t control_lock = 0;
        std::uint16_t control_lock_min = 0;
        // The pad SFX is only played when the last one was long enough
        // ago; two pads in a row do not stack the sound.
        bool play_sfx = false;
    };
    [[nodiscard]] static JumpPadResult activate_jump_pad(
        net::Vec3 vector, std::uint16_t lock_time, bool alt_form,
        float time_since_jump_pad,
        const entities::PlayerValues& values) noexcept;

    // PlayerProcess.GainHealth.  A player carrying a halfturret splits
    // the pickup with it, and the half that goes to whichever is
    // *behind* is the larger one -- odd amounts round toward the weaker
    // of the two rather than always toward the player.
    struct HealthGain {
        std::int32_t health = 0;
        std::int32_t halfturret_health = 0;
    };
    static constexpr std::int32_t HalfturretHealthMax = 100;
    [[nodiscard]] static HealthGain gain_health(
        std::int32_t health, std::int32_t health_max,
        std::int32_t amount, bool has_halfturret,
        std::int32_t halfturret_health) noexcept;

    // PlayerProcess.ExitAltForm, reduced to what it decides rather than
    // what it touches: the caller owns the animation, the camera switch
    // and the sound.
    struct ExitAltFormResult {
        // The halfturret is reclaimed first, so its remaining health is
        // added back to the player before it is destroyed.
        bool release_halfturret = false;
        std::int32_t reclaimed_health = 0;
        // The alt attack is ended before the unmorph starts, or its
        // hitbox outlives the form it belongs to.
        bool end_alt_attack = false;
    };
    [[nodiscard]] static ExitAltFormResult exit_alt_form(
        bool has_halfturret, std::int32_t halfturret_health,
        bool alt_attack) noexcept;
};

} // namespace fruityprime::players
