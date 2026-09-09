#include "Mods/Input/gamepad_probe.hpp"

#include "Mods/Input/input.hpp"
#include "Mods/Network/pad_bindings.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace fruityprime::input {
namespace {

[[nodiscard]] float axis_value(std::int16_t value) noexcept {
    return std::clamp(static_cast<float>(value) / 32'767.0F, -1.0F, 1.0F);
}

[[nodiscard]] std::string action_summary(GamepadButtons buttons) {
    const auto add = [buttons](std::string& text, GamepadButtons match,
                               std::string_view action) {
        if ((buttons & match) == GamepadButtons::None) {
            return;
        }
        if (!text.empty()) {
            text += ", ";
        }
        text += action;
    };
    std::string result;
    add(result, GamepadButtons::RightTrigger, "shoot/alt-attack");
    add(result, GamepadButtons::LeftTrigger, "zoom");
    add(result, GamepadButtons::A, "jump/boost");
    add(result, GamepadButtons::B, "morph");
    add(result, GamepadButtons::X, "scan");
    add(result, GamepadButtons::Y, "scan visor");
    add(result, GamepadButtons::Back, "scoreboard");
    add(result, GamepadButtons::RightBumper | GamepadButtons::DpadRight,
        "next weapon");
    add(result, GamepadButtons::LeftBumper | GamepadButtons::DpadLeft,
        "previous weapon");
    add(result, GamepadButtons::DpadUp, "missile");
    add(result, GamepadButtons::DpadDown, "power beam");
    add(result, GamepadButtons::Start, "pause menu");
    return result.empty() ? "(nothing bound)" : result;
}

[[nodiscard]] std::string describe(const GamepadState& state) {
    if (!state.connected) {
        return "no pad";
    }
    std::ostringstream text;
    text << (state.name.empty() ? "gamepad" : state.name)
         << "  L(" << std::fixed << std::setprecision(2)
         << std::setw(5) << axis_value(state.left_x) << ','
         << std::setw(5) << axis_value(state.left_y) << ")"
         << " R(" << std::setw(5) << axis_value(state.right_x) << ','
         << std::setw(5) << axis_value(state.right_y) << ")"
         << " LT" << std::setprecision(2)
         << static_cast<float>(state.left_trigger) / 255.0F
         << " RT" << static_cast<float>(state.right_trigger) / 255.0F;
    if (state.buttons != GamepadButtons::None) {
        text << "  " << pad_bindings::describe(state.buttons)
             << "  -> " << action_summary(state.buttons);
    }
    return text.str();
}

} // namespace

int run_gamepad_probe(double seconds) {
    seconds = std::max(0.0, seconds);
    Gamepad device;
    std::cout << "[gamepad] watching for " << std::fixed << std::setprecision(1)
              << seconds << " s\n";
    std::cout << "[gamepad] buttons: ";
    bool first = true;
    for (const PadAction action : pad_bindings::actions()) {
        if (!first) {
            std::cout << ", ";
        }
        first = false;
        std::cout << pad_bindings::name(action) << ' '
                  << pad_bindings::describe(pad_bindings::get(action));
    }
    std::cout << '\n';

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(seconds));
    bool ever_connected = false;
    bool ever_moved = false;
    GamepadState state;
    std::string last;
    do {
        const bool connected = device.poll(state);
        if (!connected) {
            state = {};
        }
        ever_connected |= state.connected;
        const std::string current = describe(state);
        if (current != last) {
            last = current;
            std::cout << "  " << current << '\n';
            const float left_x = axis_value(state.left_x);
            const float left_y = axis_value(state.left_y);
            const float right_x = axis_value(state.right_x);
            const float right_y = axis_value(state.right_y);
            ever_moved |= state.connected
                && (state.buttons != GamepadButtons::None
                    || std::fabs(left_x) > 0.5F || std::fabs(left_y) > 0.5F
                    || std::fabs(right_x) > 0.5F
                    || std::fabs(right_y) > 0.5F
                    || state.left_trigger > 166
                    || state.right_trigger > 166);
        }
        if (seconds > 0.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    } while (std::chrono::steady_clock::now() < deadline);

    if (!ever_connected) {
        std::cout << "[gamepad] FAIL: no XInput pad was seen\n";
        return 1;
    }
    if (!ever_moved) {
        std::cout << "[gamepad] a pad is connected, but no input was observed\n";
        return 1;
    }
    std::cout << "[gamepad] PASS: a pad is connected and its input arrives\n";
    return 0;
}

} // namespace fruityprime::input
