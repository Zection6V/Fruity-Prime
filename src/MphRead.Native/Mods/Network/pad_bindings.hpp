#pragma once

#include "Mods/Input/input.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace fruityprime::input::pad_bindings {

inline constexpr std::size_t kActionCount = 12;

// The order is the order used by the managed settings screen, rather than
// PadAction's declaration order. Keeping it as data makes the native and
// managed lists auditable side by side.
const std::array<PadAction, kActionCount>& actions() noexcept;

[[nodiscard]] GamepadButtons get(PadAction action) noexcept;
void set(PadAction action, GamepadButtons buttons) noexcept;
[[nodiscard]] GamepadButtons default_binding(PadAction action) noexcept;
void reset() noexcept;

[[nodiscard]] std::string name(PadAction action);
[[nodiscard]] std::string describe(GamepadButtons buttons);
[[nodiscard]] std::string button_name(GamepadButtons button);
[[nodiscard]] std::string setting_key(PadAction action);

// Read the managed controls.txt representation: pad_<Action>=<flags>.
[[nodiscard]] bool try_load(std::string_view key, std::string_view value);

} // namespace fruityprime::input::pad_bindings
