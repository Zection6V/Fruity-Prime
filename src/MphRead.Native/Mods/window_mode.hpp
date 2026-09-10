#pragma once

#include <string>
#include <string_view>

namespace fruityprime::window {

enum class StartMode {
    Windowed,
    BorderlessFullscreen
};

class State {
public:
    StartMode startup = StartMode::Windowed;
    bool fullscreen = false;
    bool topmost = false;

    void apply_startup() noexcept;
    void toggle() noexcept;
    void enter() noexcept;
    void leave() noexcept;
    void set_topmost(bool value) noexcept { topmost = value; }
};

[[nodiscard]] StartMode parse_start_mode(
    std::string_view value, StartMode fallback = StartMode::Windowed) noexcept;
[[nodiscard]] std::string start_mode_name(StartMode mode);

// Process-wide counterpart of WindowMode.Startup. Platform window states can
// copy this value when they are created, while command-line policy stays in
// Mods/ModEntry.
[[nodiscard]] StartMode& startup_mode() noexcept;

} // namespace fruityprime::window
