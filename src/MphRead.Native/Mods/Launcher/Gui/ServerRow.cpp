#include "Mods/Launcher/Gui/server_row.hpp"

#include <algorithm>
#include <utility>

namespace fruityprime::launcher::gui {

ServerColumns::ServerColumns(double width) noexcept {
    constexpr double margin = 8.0;
    constexpr double gutter = 10.0;
    constexpr double max_ping = 34.0;
    constexpr double max_players = 52.0;
    constexpr double max_mode = 66.0;
    constexpr double name_share = 0.44;

    ping_right = width - margin;
    ping_width = max_ping;
    players_right = ping_right - max_ping - gutter;
    players_width = max_players;
    mode_width = std::min(max_mode, std::max(0.0, (width - 200.0) * 0.4));
    mode_x = players_right - max_players - gutter - mode_width;
    name_x = margin;
    const double rest = std::max(0.0, mode_x - gutter - margin);
    name_width = rest * name_share;
    map_x = name_x + name_width + gutter;
    map_width = std::max(0.0, rest - name_width - gutter);
}

ServerRow::ServerRow(std::string name, std::string endpoint)
    : name_(std::move(name)), endpoint_(std::move(endpoint)),
      ping_color_(palette().text_dim) {}

Color ServerRow::name_color() const noexcept {
    return answered_ ? palette().text : palette().text_dim;
}

void ServerRow::set_status(const net::ServerStatus& status) {
    answered_ = status.online;
    if (!status.online) {
        map_ = "did not answer";
        mode_.clear();
        players_.clear();
        ping_ = "--";
        ping_color_ = palette().bad;
        return;
    }
    map_ = status.room_key;
    mode_ = net::mode_name(status.mode);
    players_ = status.max_players > 0
        ? std::to_string(status.players) + "/"
            + std::to_string(status.max_players)
        : std::to_string(status.players);
    if (status.latency_ms >= 0) {
        ping_ = std::to_string(status.latency_ms);
        ping_color_ = status.latency_ms < 80 ? palette().good
            : status.latency_ms < 160 ? palette().warm : palette().bad;
    } else {
        ping_ = "--";
        ping_color_ = palette().text_dim;
    }
}

void ServerRow::set_clicked_handler(ClickedHandler handler) {
    clicked_handler_ = std::move(handler);
}

bool ServerRow::pointer_press() {
    focus();
    if (clicked_handler_) {
        clicked_handler_();
    }
    return true;
}

bool ServerRow::key_press(WidgetKey key) {
    if (key != WidgetKey::Enter && key != WidgetKey::Space) {
        return false;
    }
    if (clicked_handler_) {
        clicked_handler_();
    }
    return true;
}

std::vector<ServerHeaderCell> ServerHeader::cells(double width) {
    const ServerColumns layout(width);
    return {
        {"SERVER", layout.name_x, layout.name_width, false},
        {"MAP", layout.map_x, layout.map_width, false},
        {"TYPE", layout.mode_x, layout.mode_width, false},
        {"PLAYERS", layout.players_right, layout.players_width, true},
        {"PING", layout.ping_right, layout.ping_width, true},
    };
}

} // namespace fruityprime::launcher::gui
