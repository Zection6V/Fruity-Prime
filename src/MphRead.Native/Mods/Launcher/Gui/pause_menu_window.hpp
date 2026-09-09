#pragma once

#include "Mods/pause_menu.hpp"
#include "Mods/Launcher/Gui/pause_menu_view.hpp"

#include <functional>
#include <string>

namespace fruityprime::launcher::gui {

struct PauseWindowPlacement final {
    bool center_on_screen = false;
    int x = 0;
    int y = 0;
    double width = 0.0;
    double height = 0.0;

    [[nodiscard]] bool operator==(const PauseWindowPlacement&) const noexcept =
        default;
};

// Toolkit-neutral counterpart of PauseMenuWindow.  The platform head owns
// the actual native window; this object owns its cover geometry and the
// settings-overlay/topmost lifecycle shared by desktop and mobile hosts.
class PauseMenuWindow final {
public:
    using ClosedHandler = std::function<void()>;

    explicit PauseMenuWindow(PauseMenuView::Options options,
                             std::string product_name = "Fruity Prime");

    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] const PauseMenuView& menu() const noexcept { return menu_; }
    [[nodiscard]] PauseMenuView& menu() noexcept { return menu_; }
    [[nodiscard]] bool open() const noexcept { return open_; }
    [[nodiscard]] bool settings_open() const noexcept {
        return settings_open_;
    }
    [[nodiscard]] bool topmost() const noexcept { return topmost_; }
    [[nodiscard]] bool can_resize() const noexcept { return false; }
    [[nodiscard]] bool decorated() const noexcept { return false; }
    [[nodiscard]] bool show_in_taskbar() const noexcept { return false; }
    [[nodiscard]] const PauseWindowPlacement& placement() const noexcept {
        return placement_;
    }

    [[nodiscard]] bool show(mods::pause::WindowRect game_window,
                            double screen_scaling = 1.0);
    void close();
    void close_if_open();
    [[nodiscard]] bool handle_escape();
    [[nodiscard]] bool follow_game_window(
        mods::pause::WindowRect game_window,
        double screen_scaling = 1.0) noexcept;

    [[nodiscard]] bool open_settings() noexcept;
    void close_settings() noexcept;
    void set_closed_handler(ClosedHandler handler);

    [[nodiscard]] static PauseWindowPlacement cover_game_window(
        mods::pause::WindowRect game_window,
        double screen_scaling = 1.0) noexcept;

private:
    std::string title_;
    PauseMenuView menu_;
    PauseWindowPlacement placement_{};
    ClosedHandler closed_handler_;
    bool open_ = false;
    bool settings_open_ = false;
    bool topmost_ = true;
};

} // namespace fruityprime::launcher::gui
