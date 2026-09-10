#include "Mods/window_mode.hpp"

#include <cctype>

namespace fruityprime::window {
namespace {

[[nodiscard]] std::string trim_copy(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)));
    }
    return value;
}

} // namespace

void State::apply_startup() noexcept {
    if (startup == StartMode::BorderlessFullscreen && !fullscreen) {
        enter();
    }
}

void State::toggle() noexcept {
    if (fullscreen) {
        leave();
    } else {
        enter();
    }
}

void State::enter() noexcept {
    fullscreen = true;
    topmost = true;
}

void State::leave() noexcept {
    fullscreen = false;
    topmost = false;
}

StartMode parse_start_mode(std::string_view value, StartMode fallback) noexcept {
    const std::string normalized = lower_ascii(trim_copy(std::string(value)));
    if (normalized == "borderless" || normalized == "fullscreen"
        || normalized == "borderless fullscreen" || normalized == "1"
        || normalized == "true") {
        return StartMode::BorderlessFullscreen;
    }
    if (normalized == "windowed" || normalized == "window"
        || normalized == "0" || normalized == "false") {
        return StartMode::Windowed;
    }
    return fallback;
}

std::string start_mode_name(StartMode mode) {
    return mode == StartMode::BorderlessFullscreen ? "borderless" : "windowed";
}

StartMode& startup_mode() noexcept {
    static StartMode mode = StartMode::Windowed;
    return mode;
}

} // namespace fruityprime::window
