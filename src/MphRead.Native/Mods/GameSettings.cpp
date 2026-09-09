#include "Mods/settings.hpp"
#include "Mods/GameSettings.hpp"
#include "Mods/render_options.hpp"
#include "Settings/settings_common.hpp"
#include "GameState.hpp"
#include "Sound/music.hpp"
#include "Sound/sfx_runtime.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace fruityprime::settings {

MenuSettings load_menu(const std::filesystem::path& directory) {
    MenuSettings settings;
    const std::string source = detail::read_settings_json(directory);
    if (source.empty()) {
        return settings;
    }
    try {
        if (const auto value = detail::json_value(source, "Features"); value
            && value->size() >= 2 && value->front() == '{'
            && value->back() == '}') {
            settings.features_json = std::string(*value);
        }
        detail::read_menu_string(source, "RoomKey", settings.room_key);
        detail::read_menu_string(source, "Mode", settings.mode);
        detail::read_menu_string(source, "Player1", settings.player1);
        detail::read_menu_string(source, "Player2", settings.player2);
        detail::read_menu_string(source, "Player3", settings.player3);
        detail::read_menu_string(source, "Player4", settings.player4);
        detail::read_menu_string(source, "Models", settings.models);
        detail::read_menu_string(source, "MphVersion", settings.mph_version);
        detail::read_menu_string(source, "FhVersion", settings.fh_version);
        detail::read_menu_string(source, "Language", settings.language);
        detail::read_menu_string(source, "SfxVolume", settings.sfx_volume);
        detail::read_menu_string(source, "MusicVolume", settings.music_volume);
        detail::read_menu_string(source, "ResolutionScale", settings.resolution_scale);
        detail::read_menu_string(source, "Lighting", settings.lighting);
        detail::read_menu_string(source, "Fog", settings.fog);
        detail::read_menu_string(source, "TextureFiltering", settings.texture_filtering);
        detail::read_menu_string(source, "ShowFps", settings.show_fps);
        detail::read_menu_string(source, "CelShading", settings.cel_shading);
        detail::read_menu_string(source, "CelBands", settings.cel_bands);
        detail::read_menu_string(source, "CelEdge", settings.cel_edge);
        detail::read_menu_string(source, "PointGoal", settings.point_goal);
        detail::read_menu_string(source, "TimeLimit", settings.time_limit);
        detail::read_menu_string(source, "TimeGoal", settings.time_goal);
        detail::read_menu_string(source, "AutoReset", settings.auto_reset);
        detail::read_menu_string(source, "TeamPlay", settings.team_play);
        detail::read_menu_string(source, "HunterRadar", settings.hunter_radar);
        detail::read_menu_string(source, "DamageLevel", settings.damage_level);
        detail::read_menu_string(source, "FriendlyFire", settings.friendly_fire);
        detail::read_menu_string(source, "AffinityWeapons", settings.affinity_weapons);
        detail::read_menu_string(source, "SaveSlot", settings.save_slot);
        detail::read_menu_string(source, "SaveFromExit", settings.save_from_exit);
        detail::read_menu_string(source, "SaveFromShip", settings.save_from_ship);
        detail::read_menu_string(source, "Planets", settings.planets);
        detail::read_menu_string(source, "Alinos1State", settings.alinos1_state);
        detail::read_menu_string(source, "Alinos2State", settings.alinos2_state);
        detail::read_menu_string(source, "Ca1State", settings.ca1_state);
        detail::read_menu_string(source, "Ca2State", settings.ca2_state);
        detail::read_menu_string(source, "Vdo1State", settings.vdo1_state);
        detail::read_menu_string(source, "Vdo2State", settings.vdo2_state);
        detail::read_menu_string(source, "Arcterra1State", settings.arcterra1_state);
        detail::read_menu_string(source, "Arcterra2State", settings.arcterra2_state);
        detail::read_menu_string(source, "CheckpointId", settings.checkpoint_id);
        detail::read_menu_string(source, "HealthMax", settings.health_max);
        detail::read_menu_string(source, "MissileMax", settings.missile_max);
        detail::read_menu_string(source, "UaMax", settings.ua_max);
        detail::read_menu_string(source, "Weapons", settings.weapons);
        detail::read_menu_string(source, "Octoliths", settings.octoliths);
    } catch (...) {
        // A malformed settings file must never prevent a native game launch.
    }
    return settings;
}

bool save_menu(const std::filesystem::path& directory,
               const MenuSettings& settings) {
    std::error_code error;
    const auto save_directory = directory / "Savedata";
    std::filesystem::create_directories(save_directory, error);
    if (error) {
        return false;
    }
    std::ofstream output(save_directory / "settings.json", std::ios::trunc);
    if (!output) {
        return false;
    }
    std::string features = settings.features_json;
    detail::trim(features);
    if (features.size() < 2 || features.front() != '{'
        || features.back() != '}') {
        features = "{}";
    }
    output << "{\n  \"Features\": " << features
           << ",\n  \"MenuSettings\": {\n";
    bool first = true;
    detail::write_menu_string(output, "RoomKey", settings.room_key, first);
    detail::write_menu_string(output, "Mode", settings.mode, first);
    detail::write_menu_string(output, "Player1", settings.player1, first);
    detail::write_menu_string(output, "Player2", settings.player2, first);
    detail::write_menu_string(output, "Player3", settings.player3, first);
    detail::write_menu_string(output, "Player4", settings.player4, first);
    detail::write_menu_string(output, "Models", settings.models, first);
    detail::write_menu_string(output, "MphVersion", settings.mph_version, first);
    detail::write_menu_string(output, "FhVersion", settings.fh_version, first);
    detail::write_menu_string(output, "Language", settings.language, first);
    detail::write_menu_string(output, "SfxVolume", settings.sfx_volume, first);
    detail::write_menu_string(output, "MusicVolume", settings.music_volume, first);
    detail::write_menu_string(output, "ResolutionScale", settings.resolution_scale, first);
    detail::write_menu_string(output, "Lighting", settings.lighting, first);
    detail::write_menu_string(output, "Fog", settings.fog, first);
    detail::write_menu_string(output, "TextureFiltering", settings.texture_filtering, first);
    detail::write_menu_string(output, "ShowFps", settings.show_fps, first);
    detail::write_menu_string(output, "CelShading", settings.cel_shading, first);
    detail::write_menu_string(output, "CelBands", settings.cel_bands, first);
    detail::write_menu_string(output, "CelEdge", settings.cel_edge, first);
    detail::write_menu_string(output, "PointGoal", settings.point_goal, first);
    detail::write_menu_string(output, "TimeLimit", settings.time_limit, first);
    detail::write_menu_string(output, "TimeGoal", settings.time_goal, first);
    detail::write_menu_string(output, "AutoReset", settings.auto_reset, first);
    detail::write_menu_string(output, "TeamPlay", settings.team_play, first);
    detail::write_menu_string(output, "HunterRadar", settings.hunter_radar, first);
    detail::write_menu_string(output, "DamageLevel", settings.damage_level, first);
    detail::write_menu_string(output, "FriendlyFire", settings.friendly_fire, first);
    detail::write_menu_string(output, "AffinityWeapons", settings.affinity_weapons, first);
    detail::write_menu_string(output, "SaveSlot", settings.save_slot, first);
    detail::write_menu_string(output, "SaveFromExit", settings.save_from_exit, first);
    detail::write_menu_string(output, "SaveFromShip", settings.save_from_ship, first);
    detail::write_menu_string(output, "Planets", settings.planets, first);
    detail::write_menu_string(output, "Alinos1State", settings.alinos1_state, first);
    detail::write_menu_string(output, "Alinos2State", settings.alinos2_state, first);
    detail::write_menu_string(output, "Ca1State", settings.ca1_state, first);
    detail::write_menu_string(output, "Ca2State", settings.ca2_state, first);
    detail::write_menu_string(output, "Vdo1State", settings.vdo1_state, first);
    detail::write_menu_string(output, "Vdo2State", settings.vdo2_state, first);
    detail::write_menu_string(output, "Arcterra1State", settings.arcterra1_state, first);
    detail::write_menu_string(output, "Arcterra2State", settings.arcterra2_state, first);
    detail::write_menu_string(output, "CheckpointId", settings.checkpoint_id, first);
    detail::write_menu_string(output, "HealthMax", settings.health_max, first);
    detail::write_menu_string(output, "MissileMax", settings.missile_max, first);
    detail::write_menu_string(output, "UaMax", settings.ua_max, first);
    detail::write_menu_string(output, "Weapons", settings.weapons, first);
    detail::write_menu_string(output, "Octoliths", settings.octoliths, first);
    output << "\n  }\n}\n";
    return static_cast<bool>(output);
}

MatchConfig load_match(const std::filesystem::path& directory) {
    MatchConfig config;
    const std::string source = detail::read_settings_json(directory);
    if (source.empty()) {
        return config;
    }
    try {
        if (const auto value = detail::json_string(source, "Mode"); value) {
            if (const auto mode = detail::parse_mode(*value); mode) {
                config.mode = *mode;
                config.mode_set = true;
            }
        }
        if (const auto value = detail::json_string(source, "TimeLimit"); value) {
            if (const auto seconds = detail::parse_match_time(*value);
                seconds && *seconds >= 0.0F) {
                config.time_limit_seconds = *seconds;
                config.time_limit_set = true;
            }
        }
        if (const auto value = detail::json_string(source, "PointGoal"); value) {
            if (const auto goal = detail::parse_integer(*value);
                goal && *goal >= 0 && *goal <= 65'535) {
                config.point_goal = static_cast<std::uint16_t>(*goal);
                config.point_goal_set = true;
            }
        }
        if (const auto value = detail::json_string(source, "TeamPlay"); value) {
            if (const auto enabled = detail::parse_match_bool(*value); enabled) {
                config.team_play = *enabled;
                config.team_play_set = true;
            }
        }
        if (const auto value = detail::json_string(source, "FriendlyFire"); value) {
            if (const auto enabled = detail::parse_match_bool(*value); enabled) {
                config.friendly_fire = *enabled;
                config.friendly_fire_set = true;
            }
        }
    } catch (...) {
        // A malformed settings file must never prevent a native game launch.
    }
    return config;
}

} // namespace fruityprime::settings

namespace fruityprime::mods {
namespace {

std::optional<settings::MenuSettings> current_settings;
game::State* game_state = nullptr;
::fruityprime::sound::SfxRuntime* sfx_runtime = nullptr;
::fruityprime::sound::MusicController* music_controller = nullptr;
std::string current_mph_key;

[[nodiscard]] std::string_view trim(std::string_view value) noexcept {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] bool parse_float_invariant(std::string_view value,
                                         float& result) noexcept {
    value = trim(value);
    if (value.empty()) {
        return false;
    }
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result,
        std::chars_format::general);
    return parsed.ec == std::errc{}
        && parsed.ptr == value.data() + value.size();
}

[[nodiscard]] bool try_volume(std::string_view value,
                              float& volume) noexcept {
    float parsed = 0.0F;
    if (!parse_float_invariant(value, parsed)) {
        // The managed console path may write a CurrentCulture decimal comma.
        std::string localized(trim(value));
        if (localized.empty() || localized.find('.') != std::string::npos
            || localized.find(',') == std::string::npos
            || localized.find(',', localized.find(',') + 1)
                != std::string::npos) {
            return false;
        }
        localized[localized.find(',')] = '.';
        if (!parse_float_invariant(localized, parsed)) {
            return false;
        }
    }
    volume = std::clamp(parsed, 0.0F, 1.0F);
    return true;
}

[[nodiscard]] std::optional<std::uint8_t> parse_language(
    std::string_view value) noexcept {
    if (value == "English") return 0;
    if (value == "Japanese") return 1;
    if (value == "French") return 2;
    if (value == "Spanish") return 3;
    if (value == "German") return 4;
    if (value == "Italian") return 5;

    value = trim(value);
    unsigned int numeric = 0;
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), numeric);
    if (parsed.ec == std::errc{}
        && parsed.ptr == value.data() + value.size()
        && numeric <= std::numeric_limits<std::uint8_t>::max()) {
        // Enum.TryParse accepts a numeric value even when no named enum member
        // has that value.
        return static_cast<std::uint8_t>(numeric);
    }
    return std::nullopt;
}

void apply_immediate_values(const settings::MenuSettings& value) noexcept {
    float volume = 0.0F;
    if (sfx_runtime != nullptr
        && try_volume(value.sfx_volume, volume)) {
        sfx_runtime->set_volume(volume);
    }
    if (try_volume(value.music_volume, volume)) {
        ::fruityprime::sound::Music::SetUserVolume(volume);
    }
    if (game_state != nullptr) {
        if (const auto language = parse_language(value.language); language) {
            game_state->language = current_mph_key == "AMHK0"
                ? 1 : *language;
        }
    }

    auto& options = render::options();
    options.set_resolution_scale(render::Options::parse_scale(
        value.resolution_scale, options.resolution_scale()));
    options.set_lighting(render::Options::parse_on_off(
        value.lighting, options.lighting()));
    options.set_fog(render::Options::parse_on_off(
        value.fog, options.fog()));
    options.set_texture_filtering(render::Options::parse_on_off(
        value.texture_filtering, options.texture_filtering()));
    options.set_show_fps(render::Options::parse_on_off(
        value.show_fps, options.show_fps()));
    options.set_cel_shading(render::Options::parse_on_off(
        value.cel_shading, options.cel_shading()));
    // These values are deliberately locked in managed GameSettings.Apply.
    options.set_cel_bands(8);
    options.set_cel_edge(0.5F);
}

} // namespace

const std::optional<settings::MenuSettings>& GameSettings::Current() noexcept {
    return current_settings;
}

void GameSettings::Apply(const settings::MenuSettings& value) noexcept {
    current_settings = value;
    apply_immediate_values(*current_settings);
}

void GameSettings::ApplyMatchRules() noexcept {
    if (!current_settings.has_value() || game_state == nullptr
        || !game_state->multiplayer()) {
        return;
    }
    const auto& value = *current_settings;
    float seconds = 0.0F;
    if (TryTime(value.time_limit, seconds) && seconds > 0.0F) {
        game_state->match_time = seconds;
    }
    if (TryTime(value.time_goal, seconds) && seconds > 0.0F) {
        game_state->time_goal = seconds;
    }

    const std::string_view goal_text = trim(value.point_goal);
    int point_goal = 0;
    const auto goal = std::from_chars(
        goal_text.data(), goal_text.data() + goal_text.size(), point_goal);
    if (goal.ec == std::errc{}
        && goal.ptr == goal_text.data() + goal_text.size()
        && point_goal > 0) {
        // State currently mirrors the 16-bit match protocol. Values beyond
        // that boundary cannot be represented by a native match packet.
        game_state->point_goal = static_cast<std::uint16_t>(std::min(
            point_goal,
            static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    }

    if (value.damage_level == "low") {
        game_state->damage_level = 0;
    } else if (value.damage_level == "high") {
        game_state->damage_level = 2;
    } else if (value.damage_level == "medium") {
        game_state->damage_level = 1;
    }
    game_state->friendly_fire = value.friendly_fire == "on";
    game_state->radar_players = value.hunter_radar == "on";
    game_state->affinity_weapons = value.affinity_weapons == "on";
    game_state->octolith_reset = value.point_goal != "off";
    // Teams intentionally remains whatever GameState.Setup selected.
}

void GameSettings::BindRuntime(
                               game::State* state,
                               ::fruityprime::sound::SfxRuntime* sfx,
                               ::fruityprime::sound::MusicController* music,
                               std::string_view mph_key) noexcept {
    game_state = state;
    sfx_runtime = sfx;
    music_controller = music;
    ::fruityprime::sound::Music::BindRuntime(music, state);
    current_mph_key.assign(mph_key);
    if (current_settings.has_value()) {
        apply_immediate_values(*current_settings);
    }
}

bool GameSettings::TryVolume(std::string_view value,
                             float& volume) noexcept {
    return try_volume(value, volume);
}

bool GameSettings::TryTime(std::string_view value,
                           float& seconds) noexcept {
    value = trim(value);
    if (value.empty()) {
        return false;
    }
    float total = 0.0F;
    std::size_t part_count = 0;
    while (true) {
        const auto separator = value.find(':');
        const std::string_view part = value.substr(0, separator);
        int number = 0;
        const auto parsed = std::from_chars(
            part.data(), part.data() + part.size(), number);
        if (part.empty() || parsed.ec != std::errc{}
            || parsed.ptr != part.data() + part.size() || number < 0) {
            return false;
        }
        total = total * 60.0F + static_cast<float>(number);
        ++part_count;
        if (separator == std::string_view::npos) {
            break;
        }
        if (part_count == 3) {
            return false;
        }
        value.remove_prefix(separator + 1);
    }
    seconds = total;
    return true;
}

} // namespace fruityprime::mods
