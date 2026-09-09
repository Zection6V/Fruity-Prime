// Native counterpart of src/MphRead/Entities/Players/PlayerInput.cs.
#include "Entities/Players/player_controls.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Entities/gameplay.hpp"
#include "../gameplay_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fruityprime::gameplay {

using namespace detail;

void Session::set_input(std::uint8_t slot, Input input) {
    static_cast<void>(player_index(slot));
    const auto found = std::find_if(inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (found == inputs_.end()) {
        throw std::logic_error("gameplay input slot is not active");
    }
    found->input = input;
    found->input_received = true;
}

void Session::set_network_aim(std::uint8_t slot, net::Vec3 aim) noexcept {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (found == inputs_.end()) {
        return;
    }
    found->input.aim = aim;
    found->input_received = true;
}

void Session::note_network_input(std::uint8_t slot) noexcept {
    const auto found = std::find_if(
        inputs_.begin(), inputs_.end(),
        [slot](const RuntimeInput& value) { return value.slot == slot; });
    if (found != inputs_.end()) {
        found->input_received = true;
    }
}


void Session::apply_input(net::PlayerState& player, const Input& input) {
    const net::Vec3 previous_position = player.position;
    const net::Vec3 previous_speed = player.speed;
    auto runtime = std::find_if(
        inputs_.begin(), inputs_.end(), [&player](const RuntimeInput& value) {
            return value.slot == player.slot_index;
        });
    const bool current_alt_form = (player.flags
        & net::PlayerState::FlagAltForm) != 0;
    const bool requested_alt_form = has_button(
        input.buttons, net::IntentButtons::AltFormState);
    const players::Profile& values = runtime != inputs_.end()
        ? players::profile(runtime->hunter) : players::profile(0);
    const bool control_locked = runtime != inputs_.end()
        && runtime->control_lock_ticks > 0;
    const bool alt_form = control_locked ? current_alt_form
        : runtime != inputs_.end() && runtime->biped_lock
        ? false
        : runtime != inputs_.end() && runtime->prevent_form_switch
            ? current_alt_form : requested_alt_form;
    const net::Vec3 forward = normalized_or(
        {input.aim.x, 0.0F, input.aim.z}, player.facing);
    const net::Vec3 right{-forward.z, 0.0F, forward.x};
    net::Vec3 desired{};
    if (has_button(input.buttons, net::IntentButtons::MoveUp)) {
        desired = add(desired, forward);
    }
    if (has_button(input.buttons, net::IntentButtons::MoveDown)) {
        desired = subtract(desired, forward);
    }
    if (!alt_form || values.alt_form_strafe != 0) {
        if (has_button(input.buttons, net::IntentButtons::MoveRight)) {
            desired = add(desired, right);
        }
        if (has_button(input.buttons, net::IntentButtons::MoveLeft)) {
            desired = subtract(desired, right);
        }
    }
    desired = normalized_or(desired, {});
    if (control_locked) {
        desired = {};
    }
    const float speed = config_.walk_speed * (alt_form
        ? values.alt_speed_scale() : values.walk_speed_scale());
    const net::Vec3 target = multiply(desired, speed);
    const float blend = std::clamp(config_.air_acceleration * config_.tick_seconds,
                                   0.0F, 1.0F);
    player.speed.x += (target.x - player.speed.x) * blend;
    player.speed.z += (target.z - player.speed.z) * blend;

    bool grounded = player.position.y <= world_min_.y + 0.001F
        && player.speed.y <= 0.0F;
    if (!grounded) {
        const net::Vec3 ground_end = subtract(
            player.position, {0.0F, 0.05F, 0.0F});
        const auto ground = collision::sweep_sphere(
            room_.collision(),
            {player.position.x, player.position.y, player.position.z},
            {ground_end.x, ground_end.y, ground_end.z},
            config_.body_radius, 0x2000);
        grounded = ground.has_value() && ground->normal.y > 0.5F;
    }
    if (grounded && has_button(input.buttons, net::IntentButtons::Jump)) {
        player.speed.y = config_.jump_speed;
        SoundEvent jump_event;
        jump_event.cue = SoundCue::PlayerJump;
        jump_event.slot = player.slot_index;
        jump_event.position = player.position;
        emit_sound(jump_event);
    } else if (!grounded) {
        const float gravity = runtime != inputs_.end()
            && runtime->gravity_override_active
            ? runtime->gravity_override : config_.gravity;
        player.speed.y += gravity * config_.tick_seconds;
    } else {
        player.speed.y = 0.0F;
    }
    const net::Vec3 next_position = add(
        player.position, multiply(player.speed, config_.tick_seconds));
    const auto hit = collision::sweep_sphere(
        room_.collision(),
        {player.position.x, player.position.y, player.position.z},
        {next_position.x, next_position.y, next_position.z},
        config_.body_radius, 0x2000);
    if (hit.has_value()) {
        player.position = {hit->center.x + hit->normal.x * 0.001F,
                           hit->center.y + hit->normal.y * 0.001F,
                           hit->center.z + hit->normal.z * 0.001F};
        const collision::Vec3 velocity{
            player.speed.x, player.speed.y, player.speed.z};
        const float normal_speed = velocity.x * hit->normal.x
            + velocity.y * hit->normal.y + velocity.z * hit->normal.z;
        if (normal_speed < 0.0F) {
            player.speed.x -= hit->normal.x * normal_speed;
            player.speed.y -= hit->normal.y * normal_speed;
            player.speed.z -= hit->normal.z * normal_speed;
        }
    } else {
        player.position = next_position;
    }
    if (!control_locked) {
        player.facing = forward;
    }
    if (has_button(input.buttons, net::IntentButtons::Zoom)
        || has_button(input.buttons, net::IntentButtons::ZoomedState)) {
        player.flags |= net::PlayerState::FlagZoomed;
    } else {
        player.flags &= static_cast<std::uint8_t>(
            ~net::PlayerState::FlagZoomed);
    }
    if (alt_form) {
        player.flags |= net::PlayerState::FlagAltForm;
    } else {
        player.flags &= static_cast<std::uint8_t>(~net::PlayerState::FlagAltForm);
    }
    if (runtime != inputs_.end() && runtime->control_lock_ticks > 0) {
        --runtime->control_lock_ticks;
    }
    constrain_to_world(player);
    players::PlayerEntity::SessionMovementChanged(
        *this, player.slot_index, previous_position, previous_speed);
}

} // namespace fruityprime::gameplay
