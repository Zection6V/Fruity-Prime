#pragma once

namespace fruityprime::mods::pause {

// Window coordinates are kept independent from Win32/OpenTK so the same
// pause state can be driven by the native desktop window and a future
// frontend.  Coordinates are client coordinates in screen space, matching
// NativeWindow.ClientLocation/ClientSize in the managed menu.
struct WindowRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool operator==(const WindowRect&) const noexcept = default;
};

// Native counterpart of Mods/PauseMenu.cs.  The managed implementation
// shares only requests between the game thread and the menu thread; this
// value type keeps that contract without putting a GUI toolkit in gameplay.
class Controller final {
public:
    [[nodiscard]] bool open() const noexcept { return open_; }
    [[nodiscard]] bool left_match() const noexcept { return left_match_; }
    [[nodiscard]] bool quit_program() const noexcept { return quit_program_; }
    [[nodiscard]] bool window_moved() const noexcept { return window_moved_; }
    [[nodiscard]] bool refocus_requested() const noexcept {
        return refocus_;
    }
    [[nodiscard]] const WindowRect& window_rect() const noexcept {
        return window_rect_;
    }

    // Escape is consumed by the pause menu whenever a native menu can be
    // shown.  The caller then pauses/resumes the simulation from open().
    [[nodiscard]] bool handle_escape(WindowRect rect) noexcept;

    // Called between frames, before a frontend lays out its menu over the
    // game window.  It preserves the managed "changed since last layout"
    // edge rather than forcing a toolkit call every frame.
    void update_window_rect(WindowRect rect) noexcept;
    void clear_window_moved() noexcept { window_moved_ = false; }

    void request_leave() noexcept { leave_request_ = true; }
    void request_quit() noexcept { quit_request_ = true; }
    void request_fullscreen_toggle() noexcept {
        fullscreen_request_ = true;
    }

    // Applies requests emitted by the menu.  The requests remain observable
    // through the public result flags just like the managed properties.
    void poll() noexcept;

    void mark_closed() noexcept;
    void clear_refocus_request() noexcept { refocus_ = false; }
    void reset() noexcept;

private:
    void close() noexcept;

    bool open_ = false;
    bool leave_request_ = false;
    bool quit_request_ = false;
    bool fullscreen_request_ = false;
    bool refocus_ = false;
    bool left_match_ = false;
    bool quit_program_ = false;
    bool window_moved_ = false;
    WindowRect window_rect_{};
};

} // namespace fruityprime::mods::pause
