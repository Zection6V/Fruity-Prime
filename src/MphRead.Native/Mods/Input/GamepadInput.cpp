#include "Mods/Input/input.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::input {
namespace {

[[nodiscard]] constexpr std::uint32_t bit(Action action) noexcept {
    switch (action) {
    case Action::MoveLeft:
        return static_cast<std::uint32_t>(net::IntentButtons::MoveLeft);
    case Action::MoveRight:
        return static_cast<std::uint32_t>(net::IntentButtons::MoveRight);
    case Action::MoveUp:
        return static_cast<std::uint32_t>(net::IntentButtons::MoveUp);
    case Action::MoveDown:
        return static_cast<std::uint32_t>(net::IntentButtons::MoveDown);
    case Action::Shoot:
        return static_cast<std::uint32_t>(net::IntentButtons::Shoot);
    case Action::Zoom:
        return static_cast<std::uint32_t>(net::IntentButtons::Zoom);
    case Action::Jump:
        return static_cast<std::uint32_t>(net::IntentButtons::Jump);
    case Action::Morph:
        return static_cast<std::uint32_t>(net::IntentButtons::Morph);
    case Action::Boost:
        return static_cast<std::uint32_t>(net::IntentButtons::Boost);
    case Action::AltAttack:
        return static_cast<std::uint32_t>(net::IntentButtons::AltAttack);
    case Action::ScanVisor:
        return static_cast<std::uint32_t>(net::IntentButtons::ScanVisor);
    case Action::Scan:
        return 0;
    case Action::NextWeapon:
        return static_cast<std::uint32_t>(net::IntentButtons::NextWeapon);
    case Action::PrevWeapon:
        return static_cast<std::uint32_t>(net::IntentButtons::PrevWeapon);
    case Action::RollLeft:
        return static_cast<std::uint32_t>(net::IntentButtons::RollLeft);
    case Action::RollRight:
        return static_cast<std::uint32_t>(net::IntentButtons::RollRight);
    case Action::RollUp:
        return static_cast<std::uint32_t>(net::IntentButtons::RollUp);
    case Action::RollDown:
        return static_cast<std::uint32_t>(net::IntentButtons::RollDown);
    case Action::ZoomedState:
        return static_cast<std::uint32_t>(net::IntentButtons::ZoomedState);
    case Action::AltFormState:
        return static_cast<std::uint32_t>(net::IntentButtons::AltFormState);
    case Action::InPlayState:
        return static_cast<std::uint32_t>(net::IntentButtons::InPlayState);
    case Action::SpectatingState:
        return static_cast<std::uint32_t>(net::IntentButtons::SpectatingState);
    }
    return 0;
}

[[nodiscard]] constexpr bool has_button(GamepadButtons buttons,
                                         GamepadButtons button) noexcept {
    return (buttons & button) != GamepadButtons::None;
}

} // namespace

void State::rebuild_buttons() noexcept {
    buttons_ = static_cast<net::IntentButtons>(
        static_cast<std::uint32_t>(keyboard_buttons_)
        | static_cast<std::uint32_t>(gamepad_buttons_));
}

void State::clear() noexcept {
    keyboard_buttons_ = net::IntentButtons::None;
    gamepad_buttons_ = net::IntentButtons::None;
    buttons_ = net::IntentButtons::None;
    aim_ = {0.0F, 0.0F, 1.0F};
    scan_keyboard_ = false;
    scan_gamepad_ = false;
}

void State::set(Action action, bool held) noexcept {
    if (action == Action::Scan) {
        scan_keyboard_ = held;
        return;
    }
    std::uint32_t value = static_cast<std::uint32_t>(keyboard_buttons_);
    const std::uint32_t mask = bit(action);
    if (held) {
        value |= mask;
    } else {
        value &= ~mask;
    }
    keyboard_buttons_ = static_cast<net::IntentButtons>(value);
    rebuild_buttons();
}

void State::clear_gamepad() noexcept {
    gamepad_buttons_ = net::IntentButtons::None;
    scan_gamepad_ = false;
    rebuild_buttons();
}

void State::set_gamepad(Action action, bool held) noexcept {
    if (action == Action::Scan) {
        scan_gamepad_ = held;
        return;
    }
    std::uint32_t value = static_cast<std::uint32_t>(gamepad_buttons_);
    const std::uint32_t mask = bit(action);
    if (held) {
        value |= mask;
    } else {
        value &= ~mask;
    }
    gamepad_buttons_ = static_cast<net::IntentButtons>(value);
    rebuild_buttons();
}

void State::set_aim(net::Vec3 aim) noexcept {
    const float length_squared = aim.x * aim.x + aim.y * aim.y
        + aim.z * aim.z;
    if (!std::isfinite(length_squared) || length_squared <= 0.000001F) {
        aim_ = {0.0F, 0.0F, 1.0F};
        return;
    }
    const float inverse_length = 1.0F / std::sqrt(length_squared);
    aim_ = {aim.x * inverse_length, aim.y * inverse_length,
            aim.z * inverse_length};
}

void State::invalidate_mouse_delta() noexcept {
    mouse_delta_invalid_ = true;
}

bool State::consume_mouse_delta_invalidation() noexcept {
    const bool invalid = mouse_delta_invalid_;
    mouse_delta_invalid_ = false;
    return invalid;
}

void apply_gamepad(State& state, const GamepadState& gamepad,
                   GamepadConfig config) noexcept {
    state.clear_gamepad();
    if (!gamepad.connected) {
        return;
    }

    constexpr float AxisMaximum = 32'767.0F;
    const float raw_x = std::clamp(
        static_cast<float>(gamepad.left_x) / AxisMaximum, -1.0F, 1.0F);
    const float raw_y = std::clamp(
        static_cast<float>(gamepad.left_y) / AxisMaximum, -1.0F, 1.0F);
    const float magnitude = std::sqrt(raw_x * raw_x + raw_y * raw_y);
    float move_x = 0.0F;
    float move_y = 0.0F;
    const float dead_zone = std::clamp(config.dead_zone, 0.0F, 0.9F);
    if (magnitude > dead_zone) {
        const float scaled = std::min(
            (magnitude - dead_zone) / (1.0F - dead_zone), 1.0F);
        move_x = raw_x / magnitude * scaled;
        move_y = raw_y / magnitude * scaled;
    }
    const float walk_threshold = std::clamp(config.walk_threshold, 0.0F, 1.0F);
    auto buttons = gamepad.buttons;
    constexpr std::uint8_t TriggerThreshold = 166;
    if (gamepad.left_trigger >= TriggerThreshold) {
        buttons |= GamepadButtons::LeftTrigger;
    }
    if (gamepad.right_trigger >= TriggerThreshold) {
        buttons |= GamepadButtons::RightTrigger;
    }
    const auto bound = [buttons](GamepadButtons binding) {
        return has_button(buttons, binding);
    };
    state.set_gamepad(Action::MoveLeft,
                      move_x < -walk_threshold
                          || has_button(buttons, GamepadButtons::DpadLeft));
    state.set_gamepad(Action::MoveRight,
                      move_x > walk_threshold
                          || has_button(buttons, GamepadButtons::DpadRight));
    state.set_gamepad(Action::MoveUp,
                      move_y > walk_threshold
                          || has_button(buttons, GamepadButtons::DpadUp));
    state.set_gamepad(Action::MoveDown,
                      move_y < -walk_threshold
                          || has_button(buttons, GamepadButtons::DpadDown));
    state.set_gamepad(Action::RollLeft,
                      move_x < -walk_threshold
                          || has_button(buttons, GamepadButtons::DpadLeft));
    state.set_gamepad(Action::RollRight,
                      move_x > walk_threshold
                          || has_button(buttons, GamepadButtons::DpadRight));
    state.set_gamepad(Action::RollUp,
                      move_y > walk_threshold
                          || has_button(buttons, GamepadButtons::DpadUp));
    state.set_gamepad(Action::RollDown,
                      move_y < -walk_threshold
                          || has_button(buttons, GamepadButtons::DpadDown));
    state.set_gamepad(Action::Jump, bound(config.bindings.jump));
    state.set_gamepad(Action::Boost, bound(config.bindings.jump));
    state.set_gamepad(Action::Morph, bound(config.bindings.morph));
    state.set_gamepad(Action::AltFormState, bound(config.bindings.morph));
    state.set_gamepad(Action::Shoot, bound(config.bindings.shoot));
    state.set_gamepad(Action::AltAttack, bound(config.bindings.shoot));
    state.set_gamepad(Action::Zoom, bound(config.bindings.zoom));
    state.set_gamepad(Action::Scan, bound(config.bindings.scan));
    state.set_gamepad(Action::ScanVisor, bound(config.bindings.scan_visor));
    state.set_gamepad(Action::PrevWeapon,
                      bound(config.bindings.prev_weapon));
    state.set_gamepad(Action::NextWeapon,
                      bound(config.bindings.next_weapon));
}

} // namespace fruityprime::input
