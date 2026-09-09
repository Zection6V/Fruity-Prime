#include "Mods/Input/input.hpp"

#include <cstdint>

static_assert(sizeof(fruityprime::input::GamepadButtons) == sizeof(std::uint16_t));
static_assert(static_cast<std::uint16_t>(
                  fruityprime::input::GamepadButtons::A) == (1u << 0));
static_assert(static_cast<std::uint16_t>(
                  fruityprime::input::GamepadButtons::RightTrigger) == (1u << 15));

namespace fruityprime::input {

bool GamepadState::down(GamepadButtons button) const noexcept {
    return (buttons & button) != GamepadButtons::None;
}

} // namespace fruityprime::input
