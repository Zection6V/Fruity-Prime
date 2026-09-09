#pragma once

#include "Formats/launcher_theme.hpp"
#include "Mods/Network/net_status.hpp"
#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::launcher::gui {

struct ServerColumns final {
    double name_x = 0.0;
    double name_width = 0.0;
    double map_x = 0.0;
    double map_width = 0.0;
    double mode_x = 0.0;
    double mode_width = 0.0;
    double players_right = 0.0;
    double players_width = 0.0;
    double ping_right = 0.0;
    double ping_width = 0.0;

    explicit ServerColumns(double width) noexcept;
};

class ServerRow final {
public:
    static constexpr double Height = 30.0;

    using ClickedHandler = std::function<void()>;

    ServerRow(std::string name, std::string endpoint);

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& endpoint() const noexcept {
        return endpoint_;
    }
    [[nodiscard]] const std::string& map() const noexcept { return map_; }
    [[nodiscard]] const std::string& mode() const noexcept { return mode_; }
    [[nodiscard]] const std::string& players() const noexcept {
        return players_;
    }
    [[nodiscard]] const std::string& ping() const noexcept { return ping_; }
    [[nodiscard]] bool answered() const noexcept { return answered_; }
    [[nodiscard]] bool hot() const noexcept { return hot_; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }
    [[nodiscard]] Color name_color() const noexcept;
    [[nodiscard]] Color ping_color() const noexcept { return ping_color_; }

    void set_status(const net::ServerStatus& status);
    void set_clicked_handler(ClickedHandler handler);
    void pointer_enter() noexcept { hot_ = true; }
    void pointer_exit() noexcept { hot_ = false; }
    void focus() noexcept { focused_ = true; }
    void blur() noexcept { focused_ = false; }
    [[nodiscard]] bool pointer_press();
    [[nodiscard]] bool key_press(WidgetKey key);

    [[nodiscard]] static ServerColumns columns(double width) noexcept {
        return ServerColumns(width);
    }

private:
    std::string name_;
    std::string endpoint_;
    std::string map_ = "asking...";
    std::string mode_;
    std::string players_;
    std::string ping_;
    Color ping_color_{};
    bool answered_ = false;
    bool hot_ = false;
    bool focused_ = false;
    ClickedHandler clicked_handler_;
};

struct ServerHeaderCell final {
    std::string_view label;
    double x = 0.0;
    double width = 0.0;
    bool right_aligned = false;
};

class ServerHeader final {
public:
    static constexpr double Height = 22.0;

    [[nodiscard]] static std::vector<ServerHeaderCell> cells(double width);
};

} // namespace fruityprime::launcher::gui
