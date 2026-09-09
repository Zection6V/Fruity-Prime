#include "Mods/Input/input.hpp"
#include "Mods/Network/pad_bindings.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool has(fruityprime::net::IntentButtons buttons,
         fruityprime::net::IntentButtons bit) {
    return (static_cast<std::uint32_t>(buttons)
            & static_cast<std::uint32_t>(bit)) != 0;
}

} // namespace

int main() {
    try {
        using fruityprime::input::Action;
        using fruityprime::input::GamepadButtons;
        using fruityprime::input::GamepadState;
        using fruityprime::input::State;
        using fruityprime::net::IntentButtons;

        State state;
        state.set(Action::MoveUp, true);

        GamepadState sample;
        sample.connected = true;
        sample.buttons = GamepadButtons::A | GamepadButtons::B
            | GamepadButtons::X
            | GamepadButtons::RightBumper;
        sample.right_trigger = 200;
        sample.left_x = -20'000;
        require(sample.down(GamepadButtons::A),
                "GamepadState::Down did not report a pressed button");
        require(!sample.down(GamepadButtons::Start),
                "GamepadState::Down reported an unpressed button");
        fruityprime::input::apply_gamepad(state, sample);

        require(has(state.buttons(), IntentButtons::MoveUp),
                "keyboard action was lost when gamepad input was added");
        require(has(state.buttons(), IntentButtons::MoveLeft),
                "left stick did not map to move left");
        require(has(state.buttons(), IntentButtons::Jump),
                "A did not map to jump");
        require(has(state.buttons(), IntentButtons::Morph)
                    && has(state.buttons(), IntentButtons::AltFormState),
                "B did not map to morph state");
        require(has(state.buttons(), IntentButtons::Shoot),
                "right trigger did not map to shoot");
        require(has(state.buttons(), IntentButtons::NextWeapon),
                "right shoulder did not map to next weapon");
        require(state.scan(), "X did not map to the local scan action");

        state.set(Action::MoveUp, false);
        require(!has(state.buttons(), IntentButtons::MoveUp),
                "keyboard release did not remove its action");
        require(has(state.buttons(), IntentButtons::MoveLeft),
                "keyboard release cleared an unrelated gamepad action");

        sample = {};
        sample.connected = true;
        fruityprime::input::apply_gamepad(state, sample);
        require(state.buttons() == IntentButtons::None,
                "clearing a gamepad sample left stale actions");
        require(!state.scan(),
                "clearing a gamepad sample left stale local scan action");

        state.set(Action::MoveRight, true);
        sample.connected = false;
        fruityprime::input::apply_gamepad(state, sample);
        require(has(state.buttons(), IntentButtons::MoveRight),
                "disconnecting a gamepad cleared keyboard input");

        State custom;
        fruityprime::input::GamepadConfig custom_config;
        custom_config.bindings.shoot = GamepadButtons::A;
        sample = {};
        sample.connected = true;
        sample.buttons = GamepadButtons::A;
        fruityprime::input::apply_gamepad(custom, sample, custom_config);
        require(has(custom.buttons(), IntentButtons::Shoot),
                "custom shoot binding was not applied");

        using fruityprime::input::PadAction;
        using fruityprime::input::pad_bindings::actions;
        using fruityprime::input::pad_bindings::describe;
        using fruityprime::input::pad_bindings::get;
        using fruityprime::input::pad_bindings::reset;
        using fruityprime::input::pad_bindings::setting_key;
        using fruityprime::input::pad_bindings::try_load;
        reset();
        require(actions().front() == PadAction::Shoot
                    && actions().back() == PadAction::Menu,
                "pad action order did not match the managed settings order");
        require(describe(get(PadAction::NextWeapon))
                    == "RB or D-pad right",
                "default pad binding description did not match");
        require(try_load("pad_NextWeapon", "LeftBumper, DpadLeft"),
                "managed pad binding line did not load");
        require(get(PadAction::NextWeapon)
                    == (GamepadButtons::LeftBumper | GamepadButtons::DpadLeft),
                "loaded pad binding did not replace the complete flag set");
        require(setting_key(PadAction::Scoreboard) == "pad_Scoreboard",
                "pad setting key did not match the managed name");
        reset();

        fruityprime::input::Gamepad device;
        GamepadState polled;
        static_cast<void>(device.poll(polled));
        std::cout << "gamepad mapping tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
