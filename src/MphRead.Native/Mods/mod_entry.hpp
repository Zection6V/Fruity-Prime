#pragma once

#include <span>
#include <string>

namespace fruityprime::mods {

// The only two public dispatch boundaries owned by the managed
// Mods/ModEntry.cs. Program calls these in this order; command-specific
// routing must not be copied into Program.cpp or either platform entrypoint.
[[nodiscard]] bool try_handle_headless(
    std::span<const std::string> args);
[[nodiscard]] bool try_handle(std::span<const std::string> args);

} // namespace fruityprime::mods
