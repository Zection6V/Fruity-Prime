#pragma once

#include "Mods/Launcher/Portable/adventure_save.hpp"
#include "Mods/Launcher/Portable/launch_plan.hpp"
#include "GameState.hpp"
#include "Mods/settings.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::launcher::gui {

enum class HomeCard : std::uint8_t {
    Setup,
    Home,
    Online,
    Host,
    Browse,
};

struct HomeLayout final {
    bool narrow = false;
    double width = 0.0;
    double splash_height = 0.0;
    // Zero means the platform head should use the remaining width, matching
    // Avalonia's Double.NaN width on the narrow layout.
    double panel_width = 400.0;

    [[nodiscard]] bool operator==(const HomeLayout&) const noexcept = default;
};

struct HomeMode final {
    std::string_view label;
    game::Mode mode = game::Mode::Battle;
};

struct HomeMatchChoice final {
    LaunchKind kind = LaunchKind::Offline;
    std::string room_key;
    game::Mode mode = game::Mode::Battle;
    metadata::Hunter hunter = metadata::Hunter::Samus;
    int bots = 0;
    int bot_level = 1;
    int port = 27888;
    bool on_master = true;
    bool listed = true;
};

class HomeView final {
public:
    static constexpr double NarrowWidth = 720.0;
    static constexpr double BrowsePanelWidth = 600.0;
    static constexpr double DefaultPanelWidth = 400.0;
    static constexpr double NarrowSplashHeight = 150.0;

    using DoneHandler = std::function<void(const LaunchPlan&)>;
    using Overlay = std::uint8_t;
    static constexpr Overlay SettingsOverlay = 1;
    static constexpr Overlay MapPickerOverlay = 2;
    static constexpr Overlay DemoPickerOverlay = 3;

    explicit HomeView(settings::MenuSettings settings,
                      std::vector<std::string> rooms = {},
                      bool game_files_ready = true);

    [[nodiscard]] const settings::MenuSettings& settings() const noexcept {
        return settings_;
    }
    [[nodiscard]] settings::MenuSettings& settings() noexcept {
        return settings_;
    }
    [[nodiscard]] const std::vector<std::string>& rooms() const noexcept {
        return rooms_;
    }
    [[nodiscard]] HomeCard card() const noexcept { return card_; }
    [[nodiscard]] const HomeLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] bool game_files_ready() const noexcept {
        return game_files_ready_;
    }
    [[nodiscard]] bool finished() const noexcept { return finished_; }
    [[nodiscard]] const LaunchPlan& plan() const noexcept { return plan_; }
    [[nodiscard]] Overlay overlay() const noexcept { return overlay_; }
    [[nodiscard]] bool status_polling() const noexcept {
        return status_polling_;
    }
    [[nodiscard]] bool version_visible() const noexcept {
        return card_ == HomeCard::Home;
    }
    [[nodiscard]] bool debug_visible() const noexcept {
        return card_ == HomeCard::Home;
    }
    [[nodiscard]] bool match_entries_enabled() const noexcept {
        return game_files_ready_;
    }

    void set_done_handler(DoneHandler handler);
    void set_width(double width) noexcept;
    void set_game_files_ready(bool ready, std::string problem = {});
    [[nodiscard]] const std::string& game_files_problem() const noexcept {
        return game_files_problem_;
    }
    void set_rooms(std::vector<std::string> rooms);

    [[nodiscard]] bool show_card(HomeCard card) noexcept;
    [[nodiscard]] bool go_back();
    [[nodiscard]] bool handle_escape();
    void close();
    void reset();

    [[nodiscard]] bool open_overlay(Overlay overlay) noexcept;
    void close_overlay() noexcept { overlay_ = 0; }

    [[nodiscard]] bool start_adventure(int slot, metadata::Hunter hunter,
                                       bool new_game, bool slot_used,
                                       std::string player_name = {});
    [[nodiscard]] bool start_match(HomeMatchChoice choice,
                                   std::string player_name = {});
    [[nodiscard]] bool join(std::string host, int port,
                            metadata::Hunter hunter,
                            std::string player_name = {});
    [[nodiscard]] bool play_demo(std::string demo_path);

    [[nodiscard]] static std::span<const HomeMode> mode_options() noexcept;
    [[nodiscard]] static std::span<const std::string_view>
    hunter_options() noexcept;
    [[nodiscard]] static std::string player_name_or_default(
        std::string_view name);

private:
    void finish(LaunchPlan plan);
    void refresh_layout_panel_width() noexcept;

    settings::MenuSettings settings_;
    std::vector<std::string> rooms_;
    std::string game_files_problem_;
    LaunchPlan plan_{};
    DoneHandler done_handler_;
    HomeLayout layout_{};
    HomeCard card_ = HomeCard::Home;
    Overlay overlay_ = 0;
    bool game_files_ready_ = true;
    bool status_polling_ = false;
    bool finished_ = false;
};

} // namespace fruityprime::launcher::gui
