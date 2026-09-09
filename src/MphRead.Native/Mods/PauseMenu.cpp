#include "Mods/pause_menu.hpp"

namespace fruityprime::mods::pause {

bool Controller::handle_escape(WindowRect rect) noexcept {
    update_window_rect(rect);
    if (open_) {
        close();
        return true;
    }
    open_ = true;
    return true;
}

void Controller::update_window_rect(WindowRect rect) noexcept {
    if (rect == window_rect_) {
        return;
    }
    window_rect_ = rect;
    window_moved_ = true;
}

void Controller::poll() noexcept {
    if (fullscreen_request_) {
        fullscreen_request_ = false;
    }
    if (quit_request_) {
        quit_request_ = false;
        quit_program_ = true;
        close();
    } else if (leave_request_) {
        leave_request_ = false;
        left_match_ = true;
        close();
    }
}

void Controller::mark_closed() noexcept {
    open_ = false;
    // The menu is a separate native window in the managed build.  Closing it
    // does not reliably return keyboard focus, so the game consumes this edge
    // on its own thread after the menu teardown has completed.
    refocus_ = true;
}

void Controller::reset() noexcept {
    left_match_ = false;
    quit_program_ = false;
    leave_request_ = false;
    quit_request_ = false;
    fullscreen_request_ = false;
}

void Controller::close() noexcept {
    open_ = false;
    refocus_ = true;
}

} // namespace fruityprime::mods::pause
