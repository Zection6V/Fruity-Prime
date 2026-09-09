// Native counterpart of src/MphRead/Entities/Players/PlayerProcess.cs.
#include "PlayerProcess.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::players {


PlayerProcess::AimVectors PlayerProcess::update_aim_vecs(
    const entities::PlayerValues& values, net::Vec3 camera_position,
    net::Vec3 facing, net::Vec3 up, net::Vec3 gun_vec2, net::Vec3 look,
    float view_bob_degrees, bool fixed_weapon) noexcept {
    const auto fixed = [](std::int32_t value) {
        return static_cast<float>(value) / 4096.0F;
    };
    const auto add = [](net::Vec3 a, net::Vec3 b) {
        return net::Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
    };
    const auto subtract = [](net::Vec3 a, net::Vec3 b) {
        return net::Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
    };
    const auto scale = [](net::Vec3 a, float factor) {
        return net::Vec3{a.x * factor, a.y * factor, a.z * factor};
    };
    const auto dot = [](net::Vec3 a, net::Vec3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const auto normalize = [&dot](net::Vec3 value, net::Vec3 fallback) {
        const float length_squared = dot(value, value);
        if (!std::isfinite(length_squared) || length_squared <= 0.000001F) {
            return fallback;
        }
        const float inverse = 1.0F / std::sqrt(length_squared);
        return net::Vec3{value.x * inverse, value.y * inverse,
                         value.z * inverse};
    };

    AimVectors result;
    result.gun_draw_pos = add(
        add(scale(facing, fixed(values.FieldB8)), camera_position),
        add(scale(gun_vec2, fixed(values.FieldB0)),
            scale(up, fixed(values.FieldB4))));
    // FieldB4 already lifts it; this is the walk bob on top.
    constexpr float DegreesToRadians = 3.14159265358979323846F / 180.0F;
    result.gun_draw_pos.y += (20.0F / 4096.0F)
        * std::cos(view_bob_degrees * DegreesToRadians);

    result.aim_position = add(
        camera_position, scale(look, fixed(values.AimDistance)));
    if (fixed_weapon) {
        // Rides rigidly with the camera instead of lagging half a step
        // behind the aim point.  The shot direction is unaffected: it is
        // recomputed from the aim position wherever a beam actually fires.
        result.aim_vec = facing;
    } else {
        net::Vec3 aim = subtract(result.aim_position, result.gun_draw_pos);
        const net::Vec3 projected = scale(facing, dot(aim, facing));
        result.aim_vec = normalize(
            add(aim, scale(subtract(projected, aim), 0.5F)), look);
    }
    result.muzzle_pos = add(
        result.gun_draw_pos,
        scale(result.aim_vec, fixed(values.MuzzleOffset)));
    return result;
}

bool PlayerProcess::can_process(const net::PlayerState& player) noexcept {
    return (player.flags & net::PlayerState::FlagActive) != 0
        && (player.flags & net::PlayerState::FlagSpectating) == 0;
}

gameplay::Input PlayerProcess::apply_form_locks(
    gameplay::Input input, const net::PlayerState& player,
    bool prevent_form_switch, bool biped_lock, bool control_locked) noexcept {
    if (control_locked || biped_lock) {
        input.buttons = static_cast<net::IntentButtons>(
            static_cast<std::uint32_t>(input.buttons)
            & ~static_cast<std::uint32_t>(net::IntentButtons::AltFormState));
    } else if (prevent_form_switch) {
        const bool alt_form = (player.flags & net::PlayerState::FlagAltForm) != 0;
        if (alt_form) {
            input.buttons = static_cast<net::IntentButtons>(
                static_cast<std::uint32_t>(input.buttons)
                | static_cast<std::uint32_t>(net::IntentButtons::AltFormState));
        } else {
            input.buttons = static_cast<net::IntentButtons>(
                static_cast<std::uint32_t>(input.buttons)
                & ~static_cast<std::uint32_t>(net::IntentButtons::AltFormState));
        }
    }
    if (control_locked) {
        input.buttons = static_cast<net::IntentButtons>(
            static_cast<std::uint32_t>(input.buttons)
            & ~(static_cast<std::uint32_t>(net::IntentButtons::MoveUp)
                | static_cast<std::uint32_t>(net::IntentButtons::MoveDown)
                | static_cast<std::uint32_t>(net::IntentButtons::MoveLeft)
                | static_cast<std::uint32_t>(net::IntentButtons::MoveRight)
                | static_cast<std::uint32_t>(net::IntentButtons::Jump)
                | static_cast<std::uint32_t>(net::IntentButtons::Shoot)));
    }
    return input;
}

void PlayerProcess::submit(gameplay::Session& session, std::uint8_t slot,
                           gameplay::Input input,
                           const net::PlayerState& player,
                           bool prevent_form_switch, bool biped_lock,
                           bool control_locked) {
    if (!can_process(player)) {
        return;
    }
    session.set_input(slot, apply_form_locks(
        input, player, prevent_form_switch, biped_lock, control_locked));
}

PlayerProcess::JumpPadResult PlayerProcess::activate_jump_pad(
    net::Vec3 vector, std::uint16_t lock_time, bool alt_form,
    float time_since_jump_pad,
    const entities::PlayerValues& values) noexcept {
    JumpPadResult result;
    // Five doubled frames: hitting a second pad sooner than that is one launch
    // as far as the sound is concerned.
    result.play_sfx = time_since_jump_pad > 5.0F * 2.0F;
    result.speed = vector;
    const auto doubled = static_cast<std::uint16_t>(lock_time * 2);
    result.control_lock = doubled;
    result.control_lock_min = std::max<std::uint16_t>(doubled, 5 * 2);
    if (alt_form && vector.y != 0.0F) {
        // A ball falls under a different gravity than a biped, so it stays in
        // the air longer for the same launch.  The extra lock is the
        // difference between the two flight times, expressed in frames, so the
        // player regains control at the top of the arc either way.
        const float accel_y = vector.y;
        const float alt_gravity =
            static_cast<float>(values.AltAirGravity) / 4096.0F;
        const float biped_gravity =
            static_cast<float>(values.BipedGravity) / 4096.0F;
        if (alt_gravity != 0.0F && biped_gravity != 0.0F) {
            const float alt_factor = -accel_y / alt_gravity;
            const float biped_factor = -accel_y / biped_gravity;
            const float lock_increase =
                ((accel_y * biped_factor
                  + biped_gravity * (biped_factor * biped_factor) / 2.0F)
                 - (accel_y * alt_factor
                    + alt_gravity * (alt_factor * alt_factor) / 2.0F))
                    / accel_y
                + 2.0F;
            result.control_lock = static_cast<std::uint16_t>(
                result.control_lock
                + static_cast<std::uint16_t>(lock_increase * 2.0F));
        }
    }
    return result;
}

PlayerProcess::HealthGain PlayerProcess::gain_health(
    std::int32_t health, std::int32_t health_max, std::int32_t amount,
    bool has_halfturret, std::int32_t halfturret_health) noexcept {
    HealthGain result{health, halfturret_health};
    if (health <= 0) {
        // A dead player gains nothing; the pickup is still consumed.
        return result;
    }
    if (has_halfturret) {
        // The larger half goes to whichever of the two is further behind, so
        // an odd pickup does not always favour the player.
        if (health <= halfturret_health) {
            result.health = health + (amount - amount / 2);
            result.halfturret_health = halfturret_health + amount / 2;
        } else {
            result.health = health + amount / 2;
            result.halfturret_health =
                halfturret_health + (amount - amount / 2);
        }
        if (result.halfturret_health > HalfturretHealthMax) {
            result.halfturret_health = HalfturretHealthMax;
        }
    } else {
        result.health = health + amount;
    }
    if (result.health > health_max) {
        result.health = health_max;
    }
    return result;
}

PlayerProcess::ExitAltFormResult PlayerProcess::exit_alt_form(
    bool has_halfturret, std::int32_t halfturret_health,
    bool alt_attack) noexcept {
    ExitAltFormResult result;
    if (has_halfturret) {
        result.release_halfturret = true;
        // Only what the turret has left comes back; a destroyed one returns
        // nothing.
        result.reclaimed_health = std::max<std::int32_t>(0, halfturret_health);
    }
    result.end_alt_attack = alt_attack;
    return result;
}

} // namespace fruityprime::players
