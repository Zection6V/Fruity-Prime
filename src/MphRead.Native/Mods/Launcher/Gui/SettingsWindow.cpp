#include "Mods/Launcher/Gui/settings_window.hpp"

#include <cmath>
#include <utility>

namespace fruityprime::launcher::gui {

SettingsWindow::SettingsWindow(settings::MenuSettings settings, bool in_game,
                               std::string product_name)
    : SettingsWindow(SettingsView(std::move(settings), in_game,
                                  std::move(product_name))) {}

SettingsWindow::SettingsWindow(SettingsView view)
    : title_(view.window_title()), view_(std::move(view)) {
    connect_view();
}

void SettingsWindow::connect_view() {
    view_.set_closed_handler([this] {
        if (!open_) {
            return;
        }
        open_ = false;
        if (closed_handler_) {
            closed_handler_();
        }
    });
}

bool SettingsWindow::show(mods::pause::WindowRect game_window,
                          double screen_scaling) {
    placement_ = in_game()
        ? cover_game_window(game_window, screen_scaling)
        : SettingsWindowPlacement{true, 0, 0, Width, Height};
    open_ = true;
    return true;
}

void SettingsWindow::close() {
    if (!open_) {
        return;
    }
    view_.close();
    // A view may already have been closed by a platform event; preserve the
    // window's close edge even if its callback was not connected by a host.
    open_ = false;
}

bool SettingsWindow::handle_escape() {
    if (!open_) {
        return false;
    }
    view_.close();
    open_ = false;
    return true;
}

void SettingsWindow::set_closed_handler(ClosedHandler handler) {
    closed_handler_ = std::move(handler);
}

SettingsWindowPlacement SettingsWindow::cover_game_window(
    mods::pause::WindowRect game_window, double screen_scaling) noexcept {
    if (game_window.width <= 0 || game_window.height <= 0) {
        return SettingsWindowPlacement{true, 0, 0, 0.0, 0.0};
    }
    if (screen_scaling <= 0.0 || !std::isfinite(screen_scaling)) {
        screen_scaling = 1.0;
    }
    return SettingsWindowPlacement{
        false,
        game_window.x,
        game_window.y,
        static_cast<double>(game_window.width) / screen_scaling,
        static_cast<double>(game_window.height) / screen_scaling};
}

} // namespace fruityprime::launcher::gui
