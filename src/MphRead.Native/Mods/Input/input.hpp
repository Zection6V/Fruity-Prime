#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace fruityprime::input {

// Logical actions are kept separate from Win32 key codes so the gameplay
// session can be fed by a desktop window, a network client, a replay, or the
// future Android frontend without changing its input contract.
enum class Action : std::uint8_t {
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Shoot,
    Zoom,
    Jump,
    Morph,
    Boost,
    AltAttack,
    ScanVisor,
    // Scan is a local-only control.  It is intentionally not part of the
    // network IntentButtons wire contract; the scan visor itself is.
    Scan,
    NextWeapon,
    PrevWeapon,
    RollLeft,
    RollRight,
    RollUp,
    RollDown,
    ZoomedState,
    AltFormState,
    InPlayState,
    SpectatingState
};

// Canonical Xbox-shaped button flags. XInput's native bit values are not
// exposed outside gamepad.cpp, so the mapping stays portable and matches the
// managed input contract used by the network/session layer.
enum class GamepadButtons : std::uint16_t {
    None = 0,
    A = 1u << 0,
    B = 1u << 1,
    X = 1u << 2,
    Y = 1u << 3,
    LeftBumper = 1u << 4,
    RightBumper = 1u << 5,
    Back = 1u << 6,
    Start = 1u << 7,
    LeftThumb = 1u << 8,
    RightThumb = 1u << 9,
    DpadUp = 1u << 10,
    DpadRight = 1u << 11,
    DpadDown = 1u << 12,
    DpadLeft = 1u << 13,
    LeftTrigger = 1u << 14,
    RightTrigger = 1u << 15
};

[[nodiscard]] constexpr GamepadButtons operator|(GamepadButtons left,
                                                  GamepadButtons right) noexcept {
    return static_cast<GamepadButtons>(
        static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr GamepadButtons operator&(GamepadButtons left,
                                                  GamepadButtons right) noexcept {
    return static_cast<GamepadButtons>(
        static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
}

constexpr GamepadButtons& operator|=(GamepadButtons& left,
                                     GamepadButtons right) noexcept {
    left = left | right;
    return left;
}

enum class PadAction : std::uint8_t {
    Shoot,
    Zoom,
    Jump,
    Morph,
    Scan,
    ScanVisor,
    Scoreboard,
    NextWeapon,
    PrevWeapon,
    Missile,
    PowerBeam,
    Menu
};

struct GamepadBindings {
    GamepadButtons shoot = GamepadButtons::RightTrigger;
    GamepadButtons zoom = GamepadButtons::LeftTrigger;
    GamepadButtons jump = GamepadButtons::A;
    GamepadButtons morph = GamepadButtons::B;
    GamepadButtons scan = GamepadButtons::X;
    GamepadButtons scan_visor = GamepadButtons::Y;
    GamepadButtons scoreboard = GamepadButtons::Back;
    GamepadButtons next_weapon = GamepadButtons::RightBumper
        | GamepadButtons::DpadRight;
    GamepadButtons prev_weapon = GamepadButtons::LeftBumper
        | GamepadButtons::DpadLeft;
    GamepadButtons missile = GamepadButtons::DpadUp;
    GamepadButtons power_beam = GamepadButtons::DpadDown;
    GamepadButtons menu = GamepadButtons::Start;
};

struct GamepadState {
    bool connected = false;
    GamepadButtons buttons = GamepadButtons::None;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    std::int16_t left_x = 0;
    std::int16_t left_y = 0;
    std::int16_t right_x = 0;
    std::int16_t right_y = 0;
    std::string name;

    [[nodiscard]] bool down(GamepadButtons button) const noexcept;
};

struct GamepadConfig {
    float dead_zone = 0.2F;
    float walk_threshold = 0.5F;
    GamepadBindings bindings{};
};

class State {
public:
    void clear() noexcept;
    void set(Action action, bool held) noexcept;
    void clear_gamepad() noexcept;
    void set_gamepad(Action action, bool held) noexcept;
    void set_aim(net::Vec3 aim) noexcept;
    // PlayerEntityChatHud.ModForgetInputDeltas invalidates the next native
    // relative-mouse sample as well as the managed snapshots.
    void invalidate_mouse_delta() noexcept;
    [[nodiscard]] bool consume_mouse_delta_invalidation() noexcept;

    [[nodiscard]] net::IntentButtons buttons() const noexcept {
        return buttons_;
    }
    [[nodiscard]] net::Vec3 aim() const noexcept {
        return aim_;
    }
    [[nodiscard]] bool scan() const noexcept {
        return scan_keyboard_ || scan_gamepad_;
    }

private:
    void rebuild_buttons() noexcept;

    net::IntentButtons buttons_ = net::IntentButtons::None;
    net::IntentButtons keyboard_buttons_ = net::IntentButtons::None;
    net::IntentButtons gamepad_buttons_ = net::IntentButtons::None;
    net::Vec3 aim_{0.0F, 0.0F, 1.0F};
    bool scan_keyboard_ = false;
    bool scan_gamepad_ = false;
    bool mouse_delta_invalid_ = false;
};

// Translate a platform-neutral XInput sample into logical actions. This is
// kept separate from the DLL loader so the mapping is deterministic and
// unit-testable without a controller attached.
void apply_gamepad(State& state, const GamepadState& gamepad,
                   GamepadConfig config = {}) noexcept;

class Gamepad {
public:
    Gamepad();
    Gamepad(const Gamepad&) = delete;
    Gamepad& operator=(const Gamepad&) = delete;
    ~Gamepad();

    [[nodiscard]] bool poll(GamepadState& state) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fruityprime::input
