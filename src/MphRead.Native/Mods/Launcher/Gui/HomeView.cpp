#include "Mods/Launcher/Gui/home_view.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace fruityprime::launcher::gui {
namespace {

constexpr std::array<HomeMode, 12> kModes{{
    {"Battle", game::Mode::Battle},
    {"Battle teams", game::Mode::BattleTeams},
    {"Survival", game::Mode::Survival},
    {"Survival teams", game::Mode::SurvivalTeams},
    {"Capture", game::Mode::Capture},
    {"Bounty", game::Mode::Bounty},
    {"Bounty teams", game::Mode::BountyTeams},
    {"Defender", game::Mode::Defender},
    {"Defender teams", game::Mode::DefenderTeams},
    {"Nodes", game::Mode::Nodes},
    {"Nodes teams", game::Mode::NodesTeams},
    {"Prime hunter", game::Mode::PrimeHunter},
}};

constexpr std::array<std::string_view, 8> kHunters{{
    "Samus", "Kanden", "Trace", "Sylux", "Noxus", "Spire", "Weavel",
    "Random",
}};

[[nodiscard]] std::string trim_copy(std::string_view value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

} // namespace

HomeView::HomeView(settings::MenuSettings settings,
                   std::vector<std::string> rooms,
                   bool game_files_ready)
    : settings_(std::move(settings)), rooms_(std::move(rooms)),
      card_(game_files_ready ? HomeCard::Home : HomeCard::Setup),
      game_files_ready_(game_files_ready) {
    refresh_layout_panel_width();
}

void HomeView::set_done_handler(DoneHandler handler) {
    done_handler_ = std::move(handler);
}

void HomeView::set_width(double width) noexcept {
    layout_.width = width;
    layout_.narrow = width < NarrowWidth;
    layout_.splash_height = layout_.narrow ? NarrowSplashHeight : 0.0;
    refresh_layout_panel_width();
}

void HomeView::set_game_files_ready(bool ready, std::string problem) {
    game_files_ready_ = ready;
    game_files_problem_ = std::move(problem);
    if (ready && card_ == HomeCard::Setup) {
        (void)show_card(HomeCard::Home);
    } else if (!ready) {
        status_polling_ = false;
        card_ = HomeCard::Setup;
        refresh_layout_panel_width();
    }
}

void HomeView::set_rooms(std::vector<std::string> rooms) {
    rooms_ = std::move(rooms);
}

bool HomeView::show_card(HomeCard card) noexcept {
    if (!game_files_ready_ && card != HomeCard::Setup) {
        return false;
    }
    card_ = card;
    status_polling_ = card == HomeCard::Online;
    refresh_layout_panel_width();
    return true;
}

bool HomeView::go_back() {
    if (overlay_ != 0) {
        close_overlay();
        return true;
    }
    if (card_ != HomeCard::Home && game_files_ready_) {
        return show_card(HomeCard::Home);
    }
    return false;
}

bool HomeView::handle_escape() {
    if (go_back()) {
        return true;
    }
    finish({});
    return true;
}

void HomeView::close() {
    finish({});
}

void HomeView::reset() {
    finished_ = false;
    plan_ = {};
    overlay_ = 0;
    status_polling_ = false;
    card_ = game_files_ready_ ? HomeCard::Home : HomeCard::Setup;
    refresh_layout_panel_width();
}

bool HomeView::open_overlay(Overlay overlay) noexcept {
    if (overlay == 0 || !game_files_ready_) {
        return false;
    }
    overlay_ = overlay;
    status_polling_ = false;
    return true;
}

bool HomeView::start_adventure(int slot, metadata::Hunter hunter,
                               bool new_game, bool slot_used,
                               std::string player_name) {
    if (finished_ || !game_files_ready_) {
        return false;
    }
    slot = std::clamp(slot, 1, static_cast<int>(launcher::SlotCount));
    if (!slot_used) {
        new_game = true;
    }
    LaunchPlan plan;
    plan.kind = LaunchKind::Adventure;
    plan.hunter = hunter;
    plan.player_name = player_name_or_default(player_name);
    plan.save_slot = static_cast<std::uint8_t>(slot);
    plan.new_game = new_game;
    finish(std::move(plan));
    return true;
}

bool HomeView::start_match(HomeMatchChoice choice, std::string player_name) {
    if (finished_ || !game_files_ready_ || choice.room_key.empty()) {
        return false;
    }
    if (choice.kind != LaunchKind::Offline
        && choice.kind != LaunchKind::Host) {
        return false;
    }
    if (choice.kind == LaunchKind::Host
        && (choice.port < 1 || choice.port > 65'535)) {
        return false;
    }
    settings_.room_key = choice.room_key;
    LaunchPlan plan;
    plan.kind = choice.kind;
    plan.hunter = choice.hunter;
    plan.player_name = player_name_or_default(player_name);
    plan.room_key = std::move(choice.room_key);
    plan.mode = static_cast<std::uint8_t>(choice.mode);
    plan.bots = std::clamp(choice.bots, 0, 7);
    plan.bot_level = std::clamp(choice.bot_level, 0, 2);
    plan.port = choice.port;
    finish(std::move(plan));
    return true;
}

bool HomeView::join(std::string host, int port, metadata::Hunter hunter,
                    std::string player_name) {
    if (finished_ || !game_files_ready_ || host.empty() || port < 1
        || port > 65'535) {
        return false;
    }
    LaunchPlan plan;
    plan.kind = LaunchKind::Online;
    plan.hunter = hunter;
    plan.player_name = player_name_or_default(player_name);
    plan.port = port;
    plan.server_address = std::move(host);
    plan.mode = static_cast<std::uint8_t>(game::Mode::Battle);
    finish(std::move(plan));
    return true;
}

bool HomeView::play_demo(std::string demo_path) {
    if (finished_ || demo_path.empty()) {
        return false;
    }
    LaunchPlan plan;
    plan.kind = LaunchKind::Demo;
    plan.demo_path = std::move(demo_path);
    finish(std::move(plan));
    return true;
}

std::span<const HomeMode> HomeView::mode_options() noexcept {
    return kModes;
}

std::span<const std::string_view> HomeView::hunter_options() noexcept {
    return kHunters;
}

std::string HomeView::player_name_or_default(std::string_view name) {
    std::string result = trim_copy(name);
    return result.empty() ? "Player" : result;
}

void HomeView::finish(LaunchPlan plan) {
    if (finished_) {
        return;
    }
    finished_ = true;
    status_polling_ = false;
    plan_ = std::move(plan);
    if (done_handler_) {
        done_handler_(plan_);
    }
}

void HomeView::refresh_layout_panel_width() noexcept {
    if (layout_.narrow) {
        layout_.panel_width = 0.0;
        return;
    }
    layout_.panel_width = card_ == HomeCard::Browse
        ? BrowsePanelWidth : DefaultPanelWidth;
}

} // namespace fruityprime::launcher::gui
