#pragma once

#include "Mods/Input/input.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace fruityprime::settings {

// The native controls file intentionally uses the same key names as the
// managed InputSettings file. That lets a user's existing controls.txt move
// with the rest of a portable installation during the C++ migration.
struct InputConfig {
    float mouse_sensitivity = 1.0F;
    bool invert_mouse_y = false;
    bool invert_mouse_x = false;
    bool scroll_all_weapons = true;
    float gamepad_dead_zone = 0.2F;
    float gamepad_look_sensitivity = 1.0F;
    bool gamepad_invert_y = false;
    input::GamepadBindings pad_bindings{};
};

// The managed launcher stores match values as strings inside
// Savedata/settings.json. Keep this small value object independent from the
// renderer so the native game and future frontends can share it.
struct MatchConfig {
    std::uint8_t mode = 3; // GameMode.Battle
    float time_limit_seconds = 7.0F * 60.0F;
    std::uint16_t point_goal = 7;
    bool team_play = false;
    bool friendly_fire = false;
    // Presence is tracked so a selected mode can supply its own managed
    // defaults without confusing an absent JSON key with an explicit 0.
    bool mode_set = false;
    bool time_limit_set = false;
    bool point_goal_set = false;
    bool team_play_set = false;
    bool friendly_fire_set = false;
};

// RenderOptions.cs stores these values as strings in the same MenuSettings
// object. Keep the native settings boundary independent from any renderer so
// the Win32 host and future frontends restore the same values at startup.
struct RenderConfig {
    int resolution_scale = 100;
    bool lighting = true;
    bool fog = true;
    bool texture_filtering = false;
    bool show_fps = false;
    bool cel_shading = false;
    int cel_bands = 8;
    float cel_edge = 0.5F;
};

// Native counterpart of MenuSettings.  The managed launcher serializes these
// values as strings because the console menu accepts both symbolic and
// numeric forms.  Keep the wire/storage shape intact so a native launcher can
// read an existing installation without silently dropping settings that are
// not yet consumed by the renderer.
struct MenuSettings {
    // Keep the managed Features object as raw JSON. The native side does not
    // interpret every feature flag yet, but saving settings must not erase
    // flags written by the managed launcher.
    std::string features_json = "{}";
    std::string room_key = "MP3 PROVING GROUND";
    std::string mode = "auto-select";
    std::string player1 = "Samus 0";
    std::string player2 = "none 0";
    std::string player3 = "none 0";
    std::string player4 = "none 0";
    std::string models = "none";
    std::string mph_version = "AMHE1";
    std::string fh_version = "AMFE0";
    std::string language = "English";
    std::string sfx_volume = "0.35";
    std::string music_volume = "0.50";
    std::string resolution_scale = "100";
    std::string lighting = "on";
    std::string fog = "on";
    std::string texture_filtering = "off";
    std::string show_fps = "off";
    std::string cel_shading = "off";
    std::string cel_bands = "8";
    std::string cel_edge = "50";
    std::string point_goal = "7";
    std::string time_limit = "7:00";
    std::string time_goal = "1:30";
    std::string auto_reset = "on";
    std::string team_play = "off";
    std::string hunter_radar = "off";
    std::string damage_level = "medium";
    std::string friendly_fire = "off";
    std::string affinity_weapons = "off";
    std::string save_slot = "none";
    std::string save_from_exit = "never";
    std::string save_from_ship = "prompt";
    std::string planets = "CA";
    std::string alinos1_state = "none";
    std::string alinos2_state = "none";
    std::string ca1_state = "none";
    std::string ca2_state = "none";
    std::string vdo1_state = "none";
    std::string vdo2_state = "none";
    std::string arcterra1_state = "none";
    std::string arcterra2_state = "none";
    std::string checkpoint_id = "none";
    std::string health_max = "99";
    std::string missile_max = "50";
    std::string ua_max = "400";
    std::string weapons = "PB, MS";
    std::string octoliths = "none";
};

[[nodiscard]] MenuSettings load_menu(const std::filesystem::path& directory);

// Writes the managed-compatible settings shape under Savedata/. Native
// callers should load, modify, and save the value so the other known settings
// are retained across a partial update.
[[nodiscard]] bool save_menu(const std::filesystem::path& directory,
                             const MenuSettings& settings);

[[nodiscard]] InputConfig load_input(const std::filesystem::path& directory);

[[nodiscard]] MatchConfig load_match(const std::filesystem::path& directory);

[[nodiscard]] RenderConfig load_render(const std::filesystem::path& directory);

// Returns false when the controls file cannot be written. Settings are a
// convenience and must never prevent the game from starting.
[[nodiscard]] bool save_input(const std::filesystem::path& directory,
                              const InputConfig& config);

} // namespace fruityprime::settings
