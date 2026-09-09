#include "Mods/settings.hpp"
#include "Mods/GameSettings.hpp"
#include "Mods/render_options.hpp"
#include "GameState.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path()
        / ("fruity_prime_native_settings_test_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    try {
        std::filesystem::create_directories(directory);
        {
            std::ofstream output(directory / "controls.txt");
            output << "# managed-compatible input settings\n"
                   << "sensitivity=2.5\n"
                   << "invert_y=on\n"
                   << "invert_x=false\n"
                   << "scroll_all_weapons=0\n"
                   << "gamepad_deadzone=9\n"
                   << "gamepad_look=0.01\n"
                   << "gamepad_invert_y=true\n"
                   << "pad_Shoot=A\n"
                   << "pad_NextWeapon=RightBumper, DpadRight\n"
                   << "unknown=value\n";
        }
        auto config = fruityprime::settings::load_input(directory);
        require(config.mouse_sensitivity == 2.5F,
                "mouse sensitivity did not load");
        require(config.invert_mouse_y && !config.invert_mouse_x,
                "mouse inversion did not load");
        require(!config.scroll_all_weapons,
                "scroll weapon setting did not load");
        require(config.gamepad_dead_zone == 0.9F,
                "gamepad dead zone was not clamped");
        require(config.gamepad_look_sensitivity == 0.1F,
                "gamepad sensitivity was not clamped");
        require(config.gamepad_invert_y,
                "gamepad Y inversion did not load");
        require(config.pad_bindings.shoot
                    == fruityprime::input::GamepadButtons::A,
                "custom gamepad shoot binding did not load");
        require(config.pad_bindings.next_weapon
                    == (fruityprime::input::GamepadButtons::RightBumper
                        | fruityprime::input::GamepadButtons::DpadRight),
                "combined gamepad binding did not load");

        config.mouse_sensitivity = 0.0F;
        config.gamepad_dead_zone = -1.0F;
        require(fruityprime::settings::save_input(directory, config),
                "settings file could not be saved");
        const auto reloaded = fruityprime::settings::load_input(directory);
        require(reloaded.mouse_sensitivity == 0.05F
                    && reloaded.gamepad_dead_zone == 0.0F,
                "saved settings were not clamped on round trip");
        require(reloaded.pad_bindings.shoot
                    == fruityprime::input::GamepadButtons::A,
                "custom gamepad binding did not survive round trip");

        std::filesystem::create_directories(directory / "Savedata");
        {
            std::ofstream output(directory / "Savedata" / "settings.json");
            output << "{\n"
                   << "  \"Features\": {\"KeepNativeFlag\": true},\n"
                   << "  \"MenuSettings\": {\n"
                   << "    \"Mode\": \"Capture\",\n"
                   << "    \"TimeLimit\": \"1:30\",\n"
                   << "    \"PointGoal\": \"11\",\n"
                   << "    \"TeamPlay\": \"off\",\n"
                   << "    \"FriendlyFire\": \"on\",\n"
                   << "    \"ResolutionScale\": \"75\",\n"
                   << "    \"Lighting\": \"off\",\n"
                   << "    \"Fog\": \"false\",\n"
                   << "    \"TextureFiltering\": \"yes\",\n"
                   << "    \"ShowFps\": \"on\",\n"
                   << "    \"CelShading\": \"true\",\n"
                   << "    \"CelBands\": \"4\",\n"
                   << "    \"CelEdge\": \"25\"\n"
                   << "  }\n"
                   << "}\n";
        }
        auto menu = fruityprime::settings::load_menu(directory);
        require(menu.mode == "Capture" && menu.time_limit == "1:30",
                "menu settings did not load from JSON");
        require(menu.features_json == "{\"KeepNativeFlag\": true}",
                "feature settings were not retained while loading");
        menu.room_key = "Room \"A\"\\B";
        menu.features_json = "{\"KeepNativeFlag\": true,\"Other\":7}";
        require(fruityprime::settings::save_menu(directory, menu),
                "menu settings file could not be saved");
        const auto reloaded_menu = fruityprime::settings::load_menu(directory);
        require(reloaded_menu.room_key == "Room \"A\"\\B",
                "menu setting escaping failed on round trip");
        require(reloaded_menu.features_json
                    == "{\"KeepNativeFlag\": true,\"Other\":7}",
                "feature settings were not retained on round trip");
        const auto match = fruityprime::settings::load_match(directory);
        require(match.mode == 7, "match mode did not load from JSON");
        require(match.time_limit_seconds == 90.0F,
                "match time limit did not parse");
        require(match.point_goal == 11,
                "match point goal did not load from JSON");
        require(!match.team_play && match.friendly_fire,
                "match boolean settings did not load");
        require(match.mode_set && match.time_limit_set
                    && match.point_goal_set && match.team_play_set
                    && match.friendly_fire_set,
                "match setting presence was not tracked");

        const auto render = fruityprime::settings::load_render(directory);
        require(render.resolution_scale == 75 && !render.lighting
                    && !render.fog && render.texture_filtering
                    && render.show_fps && render.cel_shading,
                "render boolean/scale settings did not load");
        require(render.cel_bands == 4 && render.cel_edge == 0.25F,
                "render cel settings did not load");

        // Mods/GameSettings.cs is a process-wide static application layer,
        // distinct from merely deserializing settings.json.
        fruityprime::game::State game_state;
        fruityprime::mods::GameSettings::BindRuntime(
            &game_state, nullptr, nullptr, "AMHE1");
        menu.language = "French";
        menu.sfx_volume = "1.5";
        menu.music_volume = "0,25";
        menu.resolution_scale = "75";
        menu.lighting = "off";
        menu.fog = "off";
        menu.texture_filtering = "on";
        menu.show_fps = "on";
        menu.cel_shading = "on";
        menu.cel_bands = "2";
        menu.cel_edge = "0";
        menu.time_limit = "1:02:03";
        menu.time_goal = "7";
        menu.point_goal = "19";
        menu.damage_level = "high";
        menu.friendly_fire = "on";
        menu.hunter_radar = "on";
        menu.affinity_weapons = "on";
        fruityprime::mods::GameSettings::Apply(menu);
        const auto& applied = fruityprime::mods::GameSettings::Current();
        require(applied.has_value() && applied->language == "French",
                "GameSettings Current did not retain the applied settings");
        require(game_state.language == 2,
                "GameSettings did not apply the selected language");
        const auto& live_render = fruityprime::mods::render::options();
        require(live_render.resolution_scale() == 75
                    && !live_render.lighting() && !live_render.fog()
                    && live_render.texture_filtering()
                    && live_render.show_fps() && live_render.cel_shading(),
                "GameSettings did not apply live render settings");
        require(live_render.cel_bands() == 8
                    && live_render.cel_edge() == 0.5F,
                "GameSettings did not lock cel settings to managed values");

        game_state.single_player = false;
        game_state.match_time = 10.0F;
        game_state.time_goal = 10.0F;
        game_state.point_goal = 1;
        game_state.teams = true;
        fruityprime::mods::GameSettings::ApplyMatchRules();
        require(game_state.match_time == 3723.0F
                    && game_state.time_goal == 7.0F
                    && game_state.point_goal == 19,
                "GameSettings did not parse managed match times/goals");
        require(game_state.damage_level == 2 && game_state.friendly_fire
                    && game_state.radar_players
                    && game_state.affinity_weapons
                    && game_state.octolith_reset,
                "GameSettings did not apply multiplayer rules");
        require(game_state.teams,
                "GameSettings must not overwrite Setup's team mode");

        menu.point_goal = "off";
        menu.time_limit = "bad";
        menu.time_goal = "1:2:3:4";
        menu.damage_level = "unknown";
        menu.friendly_fire = "ON";
        menu.hunter_radar = "true";
        menu.affinity_weapons = "yes";
        fruityprime::mods::GameSettings::Apply(menu);
        game_state.match_time = 33.0F;
        game_state.time_goal = 44.0F;
        game_state.damage_level = 1;
        fruityprime::mods::GameSettings::ApplyMatchRules();
        require(game_state.match_time == 33.0F
                    && game_state.time_goal == 44.0F
                    && game_state.damage_level == 1,
                "invalid GameSettings values changed existing mode defaults");
        require(!game_state.friendly_fire && !game_state.radar_players
                    && !game_state.affinity_weapons
                    && !game_state.octolith_reset,
                "GameSettings on/off rules were not case-exact");

        menu.language = "German";
        fruityprime::mods::GameSettings::BindRuntime(
            &game_state, nullptr, nullptr, "AMHK0");
        fruityprime::mods::GameSettings::Apply(menu);
        require(game_state.language == 1,
                "Korean cartridge language was not forced to Japanese");

        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        require(!std::filesystem::exists(directory),
                "settings test directory could not be removed");
        std::cout << "native settings tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
