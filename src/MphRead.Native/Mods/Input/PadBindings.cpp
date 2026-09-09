#include "Mods/Network/pad_bindings.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace fruityprime::input::pad_bindings {
namespace {

constexpr std::array<GamepadButtons, kActionCount> kDefaults{{
    /* Shoot      */ GamepadButtons::RightTrigger,
    /* Zoom       */ GamepadButtons::LeftTrigger,
    /* Jump       */ GamepadButtons::A,
    /* Morph      */ GamepadButtons::B,
    /* Scan       */ GamepadButtons::X,
    /* ScanVisor  */ GamepadButtons::Y,
    /* Scoreboard */ GamepadButtons::Back,
    /* NextWeapon */ GamepadButtons::RightBumper | GamepadButtons::DpadRight,
    /* PrevWeapon */ GamepadButtons::LeftBumper | GamepadButtons::DpadLeft,
    /* Missile    */ GamepadButtons::DpadUp,
    /* PowerBeam  */ GamepadButtons::DpadDown,
    /* Menu       */ GamepadButtons::Start
}};

std::array<GamepadButtons, kActionCount> current = kDefaults;

constexpr std::array<PadAction, kActionCount> kActions{{
    PadAction::Shoot, PadAction::Jump, PadAction::Morph, PadAction::Zoom,
    PadAction::ScanVisor, PadAction::Scan, PadAction::NextWeapon,
    PadAction::PrevWeapon, PadAction::Missile, PadAction::PowerBeam,
    PadAction::Scoreboard, PadAction::Menu
}};

[[nodiscard]] constexpr std::size_t index(PadAction action) noexcept {
    return static_cast<std::size_t>(action);
}

[[nodiscard]] std::string action_name(PadAction action) {
    switch (action) {
    case PadAction::Shoot: return "Shoot";
    case PadAction::Zoom: return "Zoom";
    case PadAction::Jump: return "Jump";
    case PadAction::Morph: return "Morph";
    case PadAction::Scan: return "Scan";
    case PadAction::ScanVisor: return "ScanVisor";
    case PadAction::Scoreboard: return "Scoreboard";
    case PadAction::NextWeapon: return "NextWeapon";
    case PadAction::PrevWeapon: return "PrevWeapon";
    case PadAction::Missile: return "Missile";
    case PadAction::PowerBeam: return "PowerBeam";
    case PadAction::Menu: return "Menu";
    }
    return {};
}

void trim(std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
}

[[nodiscard]] bool parse_button(std::string_view value,
                                GamepadButtons& result) {
    if (value == "None") {
        return true;
    }
    const auto add = [&result](GamepadButtons button) {
        result |= button;
        return true;
    };
    if (value == "A") return add(GamepadButtons::A);
    if (value == "B") return add(GamepadButtons::B);
    if (value == "X") return add(GamepadButtons::X);
    if (value == "Y") return add(GamepadButtons::Y);
    if (value == "LeftBumper") return add(GamepadButtons::LeftBumper);
    if (value == "RightBumper") return add(GamepadButtons::RightBumper);
    if (value == "Back") return add(GamepadButtons::Back);
    if (value == "Start") return add(GamepadButtons::Start);
    if (value == "LeftThumb") return add(GamepadButtons::LeftThumb);
    if (value == "RightThumb") return add(GamepadButtons::RightThumb);
    if (value == "DpadUp") return add(GamepadButtons::DpadUp);
    if (value == "DpadRight") return add(GamepadButtons::DpadRight);
    if (value == "DpadDown") return add(GamepadButtons::DpadDown);
    if (value == "DpadLeft") return add(GamepadButtons::DpadLeft);
    if (value == "LeftTrigger") return add(GamepadButtons::LeftTrigger);
    if (value == "RightTrigger") return add(GamepadButtons::RightTrigger);
    return false;
}

[[nodiscard]] bool parse_buttons(std::string value,
                                 GamepadButtons& result) {
    trim(value);
    if (value.empty()) {
        return false;
    }
    result = GamepadButtons::None;
    std::size_t begin = 0;
    while (begin < value.size()) {
        const std::size_t end = value.find_first_of(",|", begin);
        std::string token = value.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        trim(token);
        if (!parse_button(token, result)) {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return true;
}

[[nodiscard]] bool parse_action(std::string_view value,
                                PadAction& result) {
    for (const PadAction action : kActions) {
        if (action_name(action) == value) {
            result = action;
            return true;
        }
    }
    return false;
}

} // namespace

const std::array<PadAction, kActionCount>& actions() noexcept {
    return kActions;
}

GamepadButtons get(PadAction action) noexcept {
    const std::size_t value = index(action);
    return value < current.size() ? current[value] : GamepadButtons::None;
}

void set(PadAction action, GamepadButtons buttons) noexcept {
    const std::size_t value = index(action);
    if (value < current.size()) {
        current[value] = buttons;
    }
}

GamepadButtons default_binding(PadAction action) noexcept {
    const std::size_t value = index(action);
    return value < kDefaults.size() ? kDefaults[value]
                                    : GamepadButtons::None;
}

void reset() noexcept {
    current = kDefaults;
}

std::string name(PadAction action) {
    switch (action) {
    case PadAction::Shoot: return "Fire / alt attack";
    case PadAction::Zoom: return "Zoom";
    case PadAction::Jump: return "Jump / boost";
    case PadAction::Morph: return "Morph ball";
    case PadAction::Scan: return "Scan";
    case PadAction::ScanVisor: return "Scan visor";
    case PadAction::Scoreboard: return "Map / scoreboard";
    case PadAction::NextWeapon: return "Next weapon";
    case PadAction::PrevWeapon: return "Previous weapon";
    case PadAction::Missile: return "Missile";
    case PadAction::PowerBeam: return "Power beam";
    case PadAction::Menu: return "Menu";
    }
    return action_name(action);
}

std::string button_name(GamepadButtons button) {
    switch (button) {
    case GamepadButtons::A: return "A";
    case GamepadButtons::B: return "B";
    case GamepadButtons::X: return "X";
    case GamepadButtons::Y: return "Y";
    case GamepadButtons::LeftBumper: return "LB";
    case GamepadButtons::RightBumper: return "RB";
    case GamepadButtons::LeftTrigger: return "LT";
    case GamepadButtons::RightTrigger: return "RT";
    case GamepadButtons::Back: return "Back";
    case GamepadButtons::Start: return "Start";
    case GamepadButtons::LeftThumb: return "Left stick";
    case GamepadButtons::RightThumb: return "Right stick";
    case GamepadButtons::DpadUp: return "D-pad up";
    case GamepadButtons::DpadDown: return "D-pad down";
    case GamepadButtons::DpadLeft: return "D-pad left";
    case GamepadButtons::DpadRight: return "D-pad right";
    case GamepadButtons::None: return "None";
    }
    return std::to_string(static_cast<std::uint16_t>(button));
}

std::string describe(GamepadButtons buttons) {
    if (buttons == GamepadButtons::None) {
        return "unbound";
    }
    constexpr std::array<GamepadButtons, 16> named{{
        GamepadButtons::A, GamepadButtons::B, GamepadButtons::X,
        GamepadButtons::Y, GamepadButtons::LeftBumper,
        GamepadButtons::RightBumper, GamepadButtons::Back,
        GamepadButtons::Start, GamepadButtons::LeftThumb,
        GamepadButtons::RightThumb, GamepadButtons::DpadUp,
        GamepadButtons::DpadRight, GamepadButtons::DpadDown,
        GamepadButtons::DpadLeft, GamepadButtons::LeftTrigger,
        GamepadButtons::RightTrigger
    }};
    std::string result;
    for (const GamepadButtons button : named) {
        if ((buttons & button) != button) {
            continue;
        }
        if (!result.empty()) {
            result += " or ";
        }
        result += button_name(button);
    }
    return result;
}

std::string setting_key(PadAction action) {
    return "pad_" + action_name(action);
}

bool try_load(std::string_view key, std::string_view value) {
    constexpr std::string_view prefix = "pad_";
    if (!key.starts_with(prefix)) {
        return false;
    }
    PadAction action{};
    if (!parse_action(key.substr(prefix.size()), action)) {
        return false;
    }
    GamepadButtons buttons{};
    if (!parse_buttons(std::string(value), buttons)) {
        return false;
    }
    set(action, buttons);
    return true;
}

} // namespace fruityprime::input::pad_bindings
