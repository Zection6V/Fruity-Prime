#include "Mods/Launcher/Gui/pause_menu_window.hpp"

#include <cmath>
#include <utility>

namespace fruityprime::launcher::gui {

PauseMenuWindow::PauseMenuWindow(PauseMenuView::Options options,
                                 std::string product_name)
    : title_(std::move(product_name) + " - paused"), menu_(options) {}

bool PauseMenuWindow::show(mods::pause::WindowRect game_window,
                           double screen_scaling) {
    placement_ = cover_game_window(game_window, screen_scaling);
    open_ = true;
    settings_open_ = false;
    topmost_ = true;
    menu_.focus_resume();
    return true;
}

void PauseMenuWindow::close() {
    if (!open_) {
        return;
    }
    open_ = false;
    settings_open_ = false;
    topmost_ = true;
    if (closed_handler_) {
        closed_handler_();
    }
}

void PauseMenuWindow::close_if_open() {
    close();
}

bool PauseMenuWindow::handle_escape() {
    if (!open_) {
        return false;
    }
    close();
    return true;
}

bool PauseMenuWindow::follow_game_window(
    mods::pause::WindowRect game_window, double screen_scaling) noexcept {
    const PauseWindowPlacement next = cover_game_window(
        game_window, screen_scaling);
    if (!open_ || placement_ == next) {
        return false;
    }
    placement_ = next;
    return true;
}

bool PauseMenuWindow::open_settings() noexcept {
    if (!open_ || settings_open_) {
        return false;
    }
    settings_open_ = true;
    topmost_ = false;
    return true;
}

void PauseMenuWindow::close_settings() noexcept {
    if (!settings_open_) {
        return;
    }
    settings_open_ = false;
    topmost_ = true;
}

void PauseMenuWindow::set_closed_handler(ClosedHandler handler) {
    closed_handler_ = std::move(handler);
}

PauseWindowPlacement PauseMenuWindow::cover_game_window(
    mods::pause::WindowRect game_window, double screen_scaling) noexcept {
    if (game_window.width <= 0 || game_window.height <= 0) {
        return PauseWindowPlacement{true, 0, 0, 0.0, 0.0};
    }
    if (screen_scaling <= 0.0) {
        screen_scaling = 1.0;
    }
    return PauseWindowPlacement{
        false,
        game_window.x,
        game_window.y,
        static_cast<double>(game_window.width) / screen_scaling,
        static_cast<double>(game_window.height) / screen_scaling};
}

} // namespace fruityprime::launcher::gui
