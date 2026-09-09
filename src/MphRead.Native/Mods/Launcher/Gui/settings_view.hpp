#pragma once

#include "Features.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"
#include "Mods/settings.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace fruityprime::launcher::gui {

// The portable settings model keeps the row values independent from
// Avalonia/Win32/Android controls.  The platform head paints these values and
// translates its own events into form() updates.
struct SettingsForm final {
    bool borderless_fullscreen = false;
    int resolution_scale = 100;
    bool lighting = true;
    bool fog = true;
    bool texture_filtering = false;
    bool show_fps = false;
    bool cel_shading = false;
    bool pro_hud = false;

    int sfx_volume = 35;
    int music_volume = 50;
    std::string language = "English";

    int sensitivity_slider = 31;
    bool invert_mouse_y = false;
    bool invert_mouse_x = false;
    bool scroll_all_weapons = true;
    int gamepad_look_slider = 27;
    int gamepad_dead_zone_slider = 40;
    bool gamepad_invert_y = false;

    std::string point_goal = "7";
    std::string time_limit = "7:00";
    std::string damage = "medium";
    bool team_play = false;
    bool friendly_fire = false;
    bool hunter_radar = false;
    bool affinity_weapons = false;

    std::string player_name = "Player";
    metadata::Hunter hunter = metadata::Hunter::Samus;
    std::string server_endpoint;
    std::string master_endpoint;
    bool auto_update = true;
};

struct SettingsSources final {
    settings::MenuSettings menu{};
    ::fruityprime::launcher::Preferences preferences{};
    settings::InputConfig input{};
    settings::RenderConfig render{};
    features::FeatureSettings features{};
};

struct SettingsLayout final {
    bool narrow = false;
    double width = 0.0;
    double rail_width = 216.0;
    double page_margin = 26.0;
    double rail_padding_left = 18.0;
    double rail_padding_top = 20.0;

    [[nodiscard]] bool operator==(const SettingsLayout&) const noexcept =
        default;
};

class SettingsView final {
public:
    enum class Section : std::uint8_t {
        Display,
        Audio,
        Controls,
        MatchRules,
        Profile,
        Credits,
    };

    static constexpr double NarrowWidth = 720.0;
    static constexpr double RailWidth = 216.0;
    static constexpr double PageMargin = 26.0;

    using ClosedHandler = std::function<void()>;
    using GameFilesHandler = std::function<void()>;

    explicit SettingsView(settings::MenuSettings settings,
                           bool in_game = false,
                           std::string product_name = "Fruity Prime");
    SettingsView(SettingsSources sources, bool in_game = false,
                 std::string product_name = "Fruity Prime");

    [[nodiscard]] const std::string& window_title() const noexcept {
        return window_title_;
    }
    [[nodiscard]] bool in_game() const noexcept { return in_game_; }
    [[nodiscard]] bool saved() const noexcept { return saved_; }
    [[nodiscard]] bool closed() const noexcept { return closed_; }
    [[nodiscard]] bool game_files_requested() const noexcept {
        return game_files_requested_;
    }
    [[nodiscard]] Section section() const noexcept { return section_; }
    [[nodiscard]] const SettingsLayout& layout() const noexcept {
        return layout_;
    }
    [[nodiscard]] const SettingsForm& form() const noexcept { return form_; }
    [[nodiscard]] SettingsForm& form() noexcept { return form_; }

    [[nodiscard]] const settings::MenuSettings& menu_settings() const noexcept {
        return sources_.menu;
    }
    [[nodiscard]] const ::fruityprime::launcher::Preferences& preferences()
        const noexcept {
        return sources_.preferences;
    }
    [[nodiscard]] const settings::InputConfig& input_config() const noexcept {
        return sources_.input;
    }
    [[nodiscard]] const settings::RenderConfig& render_config() const noexcept {
        return sources_.render;
    }
    [[nodiscard]] const features::FeatureSettings& feature_settings()
        const noexcept {
        return sources_.features;
    }

    void set_width(double width) noexcept;
    [[nodiscard]] bool show_section(std::string_view name) noexcept;
    [[nodiscard]] bool select_section(Section section) noexcept;

    void set_closed_handler(ClosedHandler handler);
    void set_game_files_handler(GameFilesHandler handler);
    void close();
    [[nodiscard]] bool handle_escape();
    [[nodiscard]] bool request_game_files();

    // Apply is the in-memory counterpart of SettingsView.Commit. It is used
    // by a game opened from a pause menu, where the host owns persistence.
    [[nodiscard]] bool commit();
    // Persist the managed-compatible settings.json, controls.txt and
    // launcher.txt files. A failed write leaves the view open and unsaved.
    [[nodiscard]] bool commit(const std::filesystem::path& directory);

    [[nodiscard]] static std::span<const std::string_view> section_names()
        noexcept;
    [[nodiscard]] static std::string_view section_name(Section section) noexcept;
    [[nodiscard]] static int percent_from_text(std::string_view value,
                                                int fallback) noexcept;
    [[nodiscard]] static int sensitivity_to_slider(float sensitivity) noexcept;
    [[nodiscard]] static float slider_to_sensitivity(int value) noexcept;
    [[nodiscard]] static int look_to_slider(float look) noexcept;
    [[nodiscard]] static float slider_to_look(int value) noexcept;
    [[nodiscard]] static int dead_zone_to_slider(float dead_zone) noexcept;
    [[nodiscard]] static float slider_to_dead_zone(int value) noexcept;
    [[nodiscard]] static bool parse_endpoint(std::string_view text,
                                              std::string& host,
                                              std::uint16_t& port);

private:
    void initialize_form();
    void apply_form();

    SettingsSources sources_;
    SettingsForm form_;
    std::string window_title_;
    SettingsLayout layout_{};
    Section section_ = Section::Display;
    ClosedHandler closed_handler_;
    GameFilesHandler game_files_handler_;
    bool in_game_ = false;
    bool saved_ = false;
    bool closed_ = false;
    bool game_files_requested_ = false;
};

} // namespace fruityprime::launcher::gui
