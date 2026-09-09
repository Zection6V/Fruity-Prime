#pragma once

#include "Features.hpp"
#include "GameState.hpp"
#include "Formats/enum_tables.hpp"
#include "Metadata/metadata.hpp"
#include "Formats/paths.hpp"
#include "Mods/settings.hpp"
#include "Strings.hpp"
#include "Formats/Types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::menu {

// Menu.cs::SoundCapability.  This is deliberately kept separate from the
// sound runtime's loaded flag: the managed menu distinguishes a missing
// backend from a backend which loaded but is not the OpenAL version it can
// use.
enum class SoundCapability {
    None = 0,
    Unsupported = 1,
    Supported = 2
};

// A source-compatible value object for Menu.cs::MenuSettings.  The native
// settings layer stores the same fields in snake_case; conversion is kept in
// Menu.cpp so the on-disk representation remains shared without changing the
// spelling used by this port of the managed class.
struct MenuSettings {
    std::string RoomKey = "MP3 PROVING GROUND";
    std::string Mode = "auto-select";
    std::string Player1 = "Samus 0";
    std::string Player2 = "none 0";
    std::string Player3 = "none 0";
    std::string Player4 = "none 0";
    std::string Models = "none";
    std::string MphVersion = "AMHE1";
    std::string FhVersion = "AMFE0";
    std::string Language = "English";
    std::string SfxVolume = "0.35";
    std::string MusicVolume = "0.50";
    std::string ResolutionScale = "100";
    std::string Lighting = "on";
    std::string Fog = "on";
    std::string TextureFiltering = "off";
    std::string ShowFps = "off";
    std::string CelShading = "off";
    std::string CelBands = "8";
    std::string CelEdge = "50";
    std::string PointGoal = "7";
    std::string TimeLimit = "7:00";
    std::string TimeGoal = "1:30";
    std::string AutoReset = "on";
    std::string TeamPlay = "off";
    std::string HunterRadar = "off";
    std::string DamageLevel = "medium";
    std::string FriendlyFire = "off";
    std::string AffinityWeapons = "off";
    std::string SaveSlot = "none";
    std::string SaveFromExit = "never";
    std::string SaveFromShip = "prompt";
    std::string Planets = "CA";
    std::string Alinos1State = "none";
    std::string Alinos2State = "none";
    std::string Ca1State = "none";
    std::string Ca2State = "none";
    std::string Vdo1State = "none";
    std::string Vdo2State = "none";
    std::string Arcterra1State = "none";
    std::string Arcterra2State = "none";
    std::string CheckpointId = "none";
    std::string HealthMax = "99";
    std::string MissileMax = "50";
    std::string UaMax = "400";
    std::string Weapons = "PB, MS";
    std::string Octoliths = "none";
};

struct PlayerSetting {
    std::string Hunter = "none";
    std::string Team = "orange";
    std::string Recolor = "0";
};

struct ModelSetting {
    std::string Name;
    int Recolor = 0;
    bool FirstHunt = false;
    formats::MetaDir Directory = formats::MetaDir::Models;
};

// The managed menu keeps these as decimal values.  The native port uses
// double only as an implementation representation; all persisted formatting
// is performed by format_time/format_volume to retain the managed strings.
class Configuration {
public:
    Configuration();
    explicit Configuration(std::filesystem::path root_directory);
    explicit Configuration(const MenuSettings& settings,
                           std::filesystem::path root_directory = {});

    // This is the direct counterpart of the local LoadSettings function in
    // Menu.ShowMenuPrompts.  It intentionally does not call update_settings;
    // the managed caller does that as a separate step after loading.
    void load(const MenuSettings& settings);
    void load_native(const settings::MenuSettings& settings);
    [[nodiscard]] MenuSettings commit() const;
    [[nodiscard]] settings::MenuSettings native_settings() const;
    [[nodiscard]] bool save(const std::filesystem::path& directory) const;

    void update_settings();
    // ShowSettingsPrompts' "Reset Match Settings" action.  It deliberately
    // preserves the selected mode and restores the values that page owns.
    void reset_match_settings();
    void reset_features();
    void reset_adventure_settings();
    void reset_goal();
    void reset_time_limit();

    [[nodiscard]] bool read_room(std::string_view input);
    [[nodiscard]] bool read_mode(std::string_view input);
    [[nodiscard]] bool read_player(std::string_view input, std::size_t index);
    [[nodiscard]] bool read_models(std::string_view input);
    [[nodiscard]] bool read_mph_version(std::string_view input);
    [[nodiscard]] bool read_fh_version(std::string_view input);
    [[nodiscard]] bool read_time_goal(std::string_view input);
    [[nodiscard]] bool read_time_limit(std::string_view input);

    [[nodiscard]] static bool parse_time(std::string_view input,
                                         double& seconds) noexcept;
    [[nodiscard]] static std::string format_time(double seconds);

    // These two methods are the stateful equivalents of the two public
    // Menu.cs methods.  They retain the _applySettings and SaveSlot guards.
    void apply_multiplayer_settings(game::State& state) const noexcept;
    void apply_adventure_settings(game::State& state) const noexcept;

    void set_apply_settings(bool value) noexcept { apply_settings_ = value; }
    [[nodiscard]] bool apply_settings() const noexcept { return apply_settings_; }

    [[nodiscard]] const std::string& mode() const noexcept { return mode_; }
    [[nodiscard]] game::Mode game_mode() const noexcept;
    [[nodiscard]] const std::string& room_key() const noexcept { return room_key_; }
    [[nodiscard]] int room_id() const noexcept { return room_id_; }
    [[nodiscard]] const std::string& room() const noexcept { return room_; }
    [[nodiscard]] bool first_hunt_room() const noexcept { return fh_room_; }
    [[nodiscard]] formats::Language language() const noexcept {
        return static_cast<formats::Language>(language_value_);
    }
    [[nodiscard]] double sfx_volume() const noexcept { return sfx_volume_; }
    [[nodiscard]] double music_volume() const noexcept { return music_volume_; }
    [[nodiscard]] int movie_id() const noexcept { return movie_id_; }
    [[nodiscard]] bool teams() const noexcept { return teams_; }
    [[nodiscard]] double point_goal() const noexcept { return point_goal_; }
    [[nodiscard]] double time_goal() const noexcept { return time_goal_; }
    [[nodiscard]] double time_limit() const noexcept { return time_limit_; }
    [[nodiscard]] bool octolith_reset() const noexcept { return octolith_reset_; }
    [[nodiscard]] bool radar_players() const noexcept { return radar_players_; }
    [[nodiscard]] int damage_level() const noexcept { return damage_level_; }
    [[nodiscard]] bool friendly_fire() const noexcept { return friendly_fire_; }
    [[nodiscard]] bool affinity_weapons() const noexcept { return affinity_weapons_; }
    [[nodiscard]] const std::string& goal_type() const noexcept { return goal_type_; }
    void set_mode(std::string_view value);
    void set_teams(bool value) noexcept { teams_ = value; }
    void set_point_goal(double value) noexcept { point_goal_ = value; }
    void set_time_goal(double value) noexcept { time_goal_ = value; }
    void set_time_limit(double value) noexcept { time_limit_ = value; }
    void set_octolith_reset(bool value) noexcept { octolith_reset_ = value; }
    void set_radar_players(bool value) noexcept { radar_players_ = value; }
    void set_damage_level(int value) noexcept { damage_level_ = std::clamp(value, 0, 2); }
    void set_friendly_fire(bool value) noexcept { friendly_fire_ = value; }
    void set_affinity_weapons(bool value) noexcept { affinity_weapons_ = value; }
    void set_movie_id(int value) noexcept { movie_id_ = value; }
    void set_language(formats::Language value) noexcept {
        language_value_ = static_cast<int>(value);
    }
    void set_sfx_volume(double value);
    void set_music_volume(double value);
    void set_planet(std::size_t index, int value) noexcept;
    void set_boss_state(std::size_t index, int value) noexcept;
    void set_checkpoint_id(int value) noexcept { checkpoint_id_ = value; }
    void set_health_max(int value) noexcept { health_max_ = std::max(value, 1); }
    void set_missile_max(int value) noexcept { missile_max_ = std::max(value, 0); }
    void set_ua_max(int value) noexcept { ua_max_ = std::max(value, 0); }
    void set_weapon(std::size_t index, int value) noexcept;
    void set_octolith(std::size_t index, int value) noexcept;
    [[nodiscard]] const std::array<PlayerSetting, 4>& players() const noexcept {
        return players_;
    }
    [[nodiscard]] const std::vector<ModelSetting>& models() const noexcept {
        return models_;
    }

    // C# public static Menu properties are represented as value state here so
    // tests and non-console frontends can use more than one menu session.
    std::uint8_t SaveSlot = 0;
    int PreviousSaveSlot = -1;
    formats::SaveWhen SaveFromExit = formats::SaveWhen::Never;
    formats::SaveWhen SaveFromShip = formats::SaveWhen::Prompt;
    formats::SaveWhen NeededSave = formats::SaveWhen::Never;

    [[nodiscard]] const std::array<int, 5>& planets() const noexcept {
        return planets_;
    }
    [[nodiscard]] const std::array<int, 8>& octoliths() const noexcept {
        return octoliths_;
    }
    [[nodiscard]] const std::array<int, 9>& weapons() const noexcept {
        return weapons_;
    }
    [[nodiscard]] int checkpoint_id() const noexcept { return checkpoint_id_; }
    [[nodiscard]] int health_max() const noexcept { return health_max_; }
    [[nodiscard]] int missile_max() const noexcept { return missile_max_; }
    [[nodiscard]] int ua_max() const noexcept { return ua_max_; }
    [[nodiscard]] const std::array<int, 8>& boss_states() const noexcept {
        return boss_states_;
    }

    // UpdateSaveInfo/ShowLogbook are private in C#, but exposing their
    // calculated value makes the native story menu and tests use exactly the
    // same save interpretation without duplicating it in a frontend.
    [[nodiscard]] std::array<std::string, 10> save_info(
        const game::StorySave& save) const;
    // This overload preserves the side effect performed by Menu.cs's
    // UpdateSaveInfo: when a live game state and mutable save are supplied,
    // rebuild the area-hunter masks before formatting the lines.
    [[nodiscard]] std::array<std::string, 10> save_info(
        game::State& state, game::StorySave& save) const;
    struct LogbookSummary {
        int scans = 0;
        int max_scans = 0;
        int lore = 0;
        int bioforms = 0;
        int objects = 0;
        int equipment = 0;
    };
    [[nodiscard]] static LogbookSummary logbook_summary(
        const game::StorySave& save) noexcept;
    struct LogbookEntry {
        std::string title;
        std::string log;
    };
    using LogbookLists = std::array<std::vector<LogbookEntry>, 4>;
    // This is the data-building half of Menu.cs::ShowLogbook.  The managed
    // method reads ScanLog, preserves its category order, and substitutes
    // ("-", "-") for an entry which is not unlocked.  A frontend can render
    // the returned lists without reimplementing that save interpretation.
    [[nodiscard]] static LogbookLists logbook_entries(
        const game::StorySave& save,
        std::span<const strings::TableEntry> entries);

    [[nodiscard]] const features::Registry& feature_registry() const noexcept {
        return feature_registry_;
    }
    features::Registry& feature_registry() noexcept { return feature_registry_; }

private:
    void set_defaults();
    void set_default_language();
    [[nodiscard]] formats::SaveWhen parse_save_when(
        std::string_view value, formats::SaveWhen fallback) const noexcept;
    [[nodiscard]] std::string format_state(int state) const;
    [[nodiscard]] static int parse_state(std::string_view value) noexcept;
    [[nodiscard]] static std::string lower_ascii(std::string_view value);
    [[nodiscard]] static std::string trim(std::string_view value);
    [[nodiscard]] static std::vector<std::string> split_csv(
        std::string_view value);
    [[nodiscard]] static std::string format_volume(double value);
    [[nodiscard]] static std::string language_name(int value);
    [[nodiscard]] static bool is_known_mph_version(std::string_view value);
    [[nodiscard]] static bool is_known_fh_version(std::string_view value);
    [[nodiscard]] static std::string features_json(
        const features::Registry& registry);
    void load_features_json(std::string_view value);

    std::filesystem::path root_directory_;
    formats::Paths paths_;
    features::Registry feature_registry_;

    std::string mode_ = "auto-select";
    // Language is an ordinary C# enum (underlying Int32), and Enum.TryParse
    // accepts undefined numeric values. Keep the raw value instead of
    // narrowing it to the native byte-sized presentation enum.
    int language_value_ = 0;
    double sfx_volume_ = 0.35;
    double music_volume_ = 0.50;
    std::string sfx_volume_text_ = "0.35";
    std::string music_volume_text_ = "0.50";
    int movie_id_ = -1;

    std::string room_;
    std::string room_key_ = "MP3 PROVING GROUND";
    int room_id_ = -1;
    bool fh_room_ = false;
    std::array<PlayerSetting, 4> players_{{
        {"Samus", "orange", "0"},
        {"none", "green", "0"},
        {"none", "orange", "0"},
        {"none", "green", "0"},
    }};
    std::array<int, 4> player_ids_{{0, -1, -1, -1}};
    std::vector<ModelSetting> models_;

    bool apply_settings_ = false;
    bool teams_ = false;
    double point_goal_ = 0;
    double time_goal_ = 0;
    double time_limit_ = 0;
    bool octolith_reset_ = true;
    bool radar_players_ = false;
    int damage_level_ = 1;
    bool friendly_fire_ = false;
    bool affinity_weapons_ = false;
    std::string goal_type_;

    std::array<int, 5> planets_{{1, 0, 0, 0, 0}};
    std::array<int, 8> boss_states_{{0, 0, 0, 0, 0, 0, 0, 0}};
    int checkpoint_id_ = -1;
    int health_max_ = 99;
    int missile_max_ = 50;
    int ua_max_ = 400;
    std::array<int, 9> weapons_{{1, 0, 1, 0, 0, 0, 0, 0, 0}};
    std::array<int, 8> octoliths_{{0, 0, 0, 0, 0, 0, 0, 0}};
};

enum class Page {
    Home,
    GameFiles,
    Settings,
    Join,
    Demos
};

enum class Action {
    None,
    Host,
    Join,
    OpenSettings,
    OpenGameFiles,
    OpenDemos,
    Back,
    Quit
};

struct Item {
    std::string label_key;
    Action action = Action::None;
    bool enabled = true;
};

class Model {
public:
    Model();
    void open(Page page);
    void back();
    void set_game_files_ready(bool ready) noexcept { game_files_ready_ = ready; }
    [[nodiscard]] Page page() const noexcept { return page_; }
    [[nodiscard]] std::size_t selected() const noexcept { return selected_; }
    [[nodiscard]] const std::vector<Item>& items() const noexcept {
        return items_;
    }
    void move_selection(int direction) noexcept;
    [[nodiscard]] Action activate() const noexcept;

private:
    void rebuild_items();
    Page page_ = Page::Home;
    std::size_t selected_ = 0;
    bool game_files_ready_ = false;
    std::vector<Item> items_;
};

// Menu.cs's entry point.  The overload is useful for deterministic tests and
// for the native text launcher; the no-argument form is the console-facing
// equivalent of the managed method.
void show_menu_prompts(const std::filesystem::path& directory,
                       std::istream& input, std::ostream& output);
void ShowMenuPrompts();

// Process-local compatibility state for callers which used Menu.cs's static
// settings.  New code should prefer Configuration directly.
Configuration& current() noexcept;
void ApplyMultiplayerSettings(game::State& state) noexcept;
void ApplyAdventureSettings(game::State& state) noexcept;
void ResetFeatures();
[[nodiscard]] std::string format_time(double seconds);
[[nodiscard]] bool parse_time(std::string_view input,
                              double& seconds) noexcept;
void print_sound_info(SoundCapability capability, std::ostream& output);

} // namespace fruityprime::menu



// Stable source-tree facade.  Menu's implementation header intentionally
// lives beside Menu.cpp so the managed/native pair is visible together.
namespace MphReadNative {
using MenuModel = ::fruityprime::menu::Model;
using MenuSettings = ::fruityprime::menu::MenuSettings;
using MenuConfiguration = ::fruityprime::menu::Configuration;
using SoundCapability = ::fruityprime::menu::SoundCapability;
namespace Menu = ::fruityprime::menu;
}
