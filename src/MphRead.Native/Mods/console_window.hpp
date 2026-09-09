#pragma once

namespace fruityprime::mods::console {

struct ShowResult {
    bool available = false;
    bool owns_console = false;
};

// Mirrors Mods.ConsoleWindow's Windows-only console policy. A redirected
// process already has usable streams and must not flash a new console window.
[[nodiscard]] bool owns_its_console() noexcept;
[[nodiscard]] ShowResult show() noexcept;

} // namespace fruityprime::mods::console
