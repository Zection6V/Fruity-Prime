#pragma once

#include "Mods/pause_menu.hpp"
#include "Mods/Launcher/Gui/settings_view.hpp"

#include <functional>
#include <string>

namespace fruityprime::launcher::gui {

struct SettingsWindowPlacement final {
    bool center_on_owner = false;
    int x = 0;
    int y = 0;
    double width = 0.0;
    double height = 0.0;

    [[nodiscard]] bool operator==(const SettingsWindowPlacement&) const
        noexcept = default;
};

// Toolkit-neutral frame around SettingsView.  The actual native window is
// owned by the platform head; this value carries the same title, fixed-size
// rules, game-window cover placement and close/save hand-off as the managed
// SettingsWindow.
class SettingsWindow final {
public:
    static constexpr double Width = 980.0;
    static constexpr double Height = 660.0;
    static constexpr double MinWidth = 720.0;
    static constexpr double MinHeight = 460.0;

    using ClosedHandler = std::function<void()>;

    SettingsWindow(settings::MenuSettings settings, bool in_game = false,
                   std::string product_name = "Fruity Prime");
    explicit SettingsWindow(SettingsView view);

    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    [[nodiscard]] const SettingsView& view() const noexcept { return view_; }
    [[nodiscard]] SettingsView& view() noexcept { return view_; }
    [[nodiscard]] bool in_game() const noexcept { return view_.in_game(); }
    [[nodiscard]] bool saved() const noexcept { return view_.saved(); }
    [[nodiscard]] bool open() const noexcept { return open_; }
    [[nodiscard]] bool can_resize() const noexcept { return !in_game(); }
    [[nodiscard]] bool decorated() const noexcept { return !in_game(); }
    [[nodiscard]] bool topmost() const noexcept { return in_game(); }
    [[nodiscard]] bool show_in_taskbar() const noexcept { return !in_game(); }
    [[nodiscard]] bool centered_on_owner() const noexcept { return !in_game(); }
    [[nodiscard]] const SettingsWindowPlacement& placement() const noexcept {
        return placement_;
    }

    [[nodiscard]] bool show(mods::pause::WindowRect game_window = {},
                            double screen_scaling = 1.0);
    void close();
    [[nodiscard]] bool handle_escape();
    void set_closed_handler(ClosedHandler handler);

    [[nodiscard]] static SettingsWindowPlacement cover_game_window(
        mods::pause::WindowRect game_window,
        double screen_scaling = 1.0) noexcept;

private:
    void connect_view();

    std::string title_;
    SettingsView view_;
    SettingsWindowPlacement placement_{};
    ClosedHandler closed_handler_;
    bool open_ = false;
};

} // namespace fruityprime::launcher::gui
