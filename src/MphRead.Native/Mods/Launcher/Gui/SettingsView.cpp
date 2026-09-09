#include "Mods/Launcher/Gui/settings_view.hpp"

#include "Mods/render_options.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace fruityprime::launcher::gui {
namespace {

constexpr std::array<std::string_view, 6> kSectionNames{
    "Display", "Audio", "Controls", "Match rules", "Profile", "Credits"};

constexpr std::array<std::string_view, 6> kLanguageNames{
    "English", "Japanese", "French", "Spanish", "German", "Italian"};

[[nodiscard]] std::string trim_copy(std::string_view value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

[[nodiscard]] bool ascii_equal(std::string_view left,
                               std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        char a = left[index];
        char b = right[index];
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] double round_to_even(double value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    const double lower = std::floor(value);
    const double fraction = value - lower;
    if (fraction < 0.5) {
        return lower;
    }
    if (fraction > 0.5) {
        return lower + 1.0;
    }
    const auto integer = static_cast<long long>(lower);
    return (integer % 2 == 0) ? lower : lower + 1.0;
}

[[nodiscard]] float parse_float(std::string_view value,
                                float fallback) noexcept {
    try {
        const std::string text = trim_copy(value);
        std::size_t consumed = 0;
        const float parsed = std::stof(text, &consumed);
        if (consumed != text.size() || !std::isfinite(parsed)) {
            return fallback;
        }
        return parsed;
    } catch (...) {
        return fallback;
    }
}

[[nodiscard]] std::string format_float(float value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(2) << value;
    std::string result = output.str();
    while (result.size() > 1 && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

[[nodiscard]] std::string on_off(bool value) {
    return value ? "on" : "off";
}

[[nodiscard]] std::string endpoint(std::string_view host,
                                   std::uint16_t port) {
    return std::string(host) + ":" + std::to_string(port);
}

[[nodiscard]] std::string update_pro_hud_json(std::string raw, bool value) {
    const std::string key = "\"ProHud\"";
    const std::size_t key_position = raw.find(key);
    const std::string replacement = value ? "true" : "false";
    if (key_position != std::string::npos) {
        std::size_t value_position = raw.find(':', key_position + key.size());
        if (value_position != std::string::npos) {
            ++value_position;
            while (value_position < raw.size()
                   && std::isspace(static_cast<unsigned char>(
                       raw[value_position]))) {
                ++value_position;
            }
            const std::size_t value_end = raw.compare(value_position, 4,
                                                       "true") == 0
                ? value_position + 4
                : raw.compare(value_position, 5, "false") == 0
                ? value_position + 5
                : value_position;
            if (value_end != value_position) {
                raw.replace(value_position, value_end - value_position,
                            replacement);
                return raw;
            }
        }
    }

    const std::size_t close = raw.find_last_of('}');
    if (close == std::string::npos) {
        return "{\"ProHud\": " + replacement + "}";
    }
    const std::size_t open = raw.find_first_not_of(" \t\r\n");
    if (open == std::string::npos || raw[open] != '{') {
        return "{\"ProHud\": " + replacement + "}";
    }
    const bool has_member = raw.find_first_not_of(" \t\r\n", open + 1)
        != close;
    raw.insert(close, (has_member ? ", " : "") + key + ": " + replacement);
    return raw;
}

} // namespace

SettingsView::SettingsView(settings::MenuSettings settings, bool in_game,
                           std::string product_name)
    : SettingsView(SettingsSources{std::move(settings), {}, {}, {}, {}},
                   in_game, std::move(product_name)) {}

SettingsView::SettingsView(SettingsSources sources, bool in_game,
                           std::string product_name)
    : sources_(std::move(sources)),
      window_title_(std::move(product_name) + " settings"),
      in_game_(in_game) {
    initialize_form();
}

void SettingsView::initialize_form() {
    form_.borderless_fullscreen = sources_.preferences.window_mode
        == WindowStartMode::BorderlessFullscreen;
    form_.resolution_scale = std::clamp(sources_.render.resolution_scale, 25,
                                         100);
    form_.lighting = sources_.render.lighting;
    form_.fog = sources_.render.fog;
    form_.texture_filtering = sources_.render.texture_filtering;
    form_.show_fps = sources_.render.show_fps;
    form_.cel_shading = sources_.render.cel_shading;
    form_.pro_hud = sources_.features.pro_hud;

    form_.sfx_volume = percent_from_text(sources_.menu.sfx_volume, 35);
    form_.music_volume = percent_from_text(sources_.menu.music_volume, 50);
    form_.language = "English";
    for (const std::string_view language : kLanguageNames) {
        if (sources_.menu.language == language) {
            form_.language = std::string(language);
            break;
        }
    }

    form_.sensitivity_slider = sensitivity_to_slider(
        sources_.input.mouse_sensitivity);
    form_.invert_mouse_y = sources_.input.invert_mouse_y;
    form_.invert_mouse_x = sources_.input.invert_mouse_x;
    form_.scroll_all_weapons = sources_.input.scroll_all_weapons;
    form_.gamepad_look_slider = look_to_slider(
        sources_.input.gamepad_look_sensitivity);
    form_.gamepad_dead_zone_slider = dead_zone_to_slider(
        sources_.input.gamepad_dead_zone);
    form_.gamepad_invert_y = sources_.input.gamepad_invert_y;

    form_.point_goal = sources_.menu.point_goal;
    form_.time_limit = sources_.menu.time_limit;
    form_.damage = sources_.menu.damage_level;
    form_.team_play = sources_.menu.team_play == "on";
    form_.friendly_fire = sources_.menu.friendly_fire == "on";
    form_.hunter_radar = sources_.menu.hunter_radar == "on";
    form_.affinity_weapons = sources_.menu.affinity_weapons == "on";

    form_.player_name = sources_.preferences.player_name;
    form_.hunter = sources_.preferences.last_hunter;
    form_.server_endpoint = endpoint(sources_.preferences.server_address,
                                      sources_.preferences.server_port);
    form_.master_endpoint = endpoint(sources_.preferences.master_host,
                                      sources_.preferences.master_port);
    form_.auto_update = sources_.preferences.auto_update;
}

void SettingsView::set_width(double width) noexcept {
    layout_.width = width;
    layout_.narrow = width < NarrowWidth;
    layout_.rail_width = layout_.narrow ? 0.0 : RailWidth;
    layout_.rail_padding_left = layout_.narrow ? 12.0 : 18.0;
    layout_.rail_padding_top = layout_.narrow ? 8.0 : 20.0;
}

bool SettingsView::show_section(std::string_view name) noexcept {
    for (std::size_t index = 0; index < kSectionNames.size(); ++index) {
        if (ascii_equal(name, kSectionNames[index])) {
            section_ = static_cast<Section>(index);
            return true;
        }
    }
    return false;
}

bool SettingsView::select_section(Section section) noexcept {
    const auto index = static_cast<std::size_t>(section);
    if (index >= kSectionNames.size()) {
        return false;
    }
    section_ = section;
    return true;
}

void SettingsView::set_closed_handler(ClosedHandler handler) {
    closed_handler_ = std::move(handler);
}

void SettingsView::set_game_files_handler(GameFilesHandler handler) {
    game_files_handler_ = std::move(handler);
}

void SettingsView::close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    if (closed_handler_) {
        closed_handler_();
    }
}

bool SettingsView::handle_escape() {
    if (closed_) {
        return false;
    }
    close();
    return true;
}

bool SettingsView::request_game_files() {
    if (closed_) {
        return false;
    }
    game_files_requested_ = true;
    if (game_files_handler_) {
        game_files_handler_();
    }
    close();
    return true;
}

bool SettingsView::commit() {
    if (closed_) {
        return false;
    }
    apply_form();
    saved_ = true;
    close();
    return true;
}

bool SettingsView::commit(const std::filesystem::path& directory) {
    if (closed_) {
        return false;
    }
    apply_form();
    if (!settings::save_menu(directory, sources_.menu)
        || !settings::save_input(directory, sources_.input)
        || !::fruityprime::launcher::save_preferences(
            directory, sources_.preferences)) {
        return false;
    }
    saved_ = true;
    close();
    return true;
}

void SettingsView::apply_form() {
    sources_.preferences.window_mode = form_.borderless_fullscreen
        ? WindowStartMode::BorderlessFullscreen
        : WindowStartMode::Windowed;
    sources_.render.resolution_scale = std::clamp(form_.resolution_scale, 25,
                                                  100);
    sources_.render.lighting = form_.lighting;
    sources_.render.fog = form_.fog;
    sources_.render.texture_filtering = form_.texture_filtering;
    sources_.render.show_fps = form_.show_fps;
    sources_.render.cel_shading = form_.cel_shading;
    sources_.render.cel_bands = 8;
    sources_.render.cel_edge = 0.5F;
    sources_.menu.resolution_scale = std::to_string(
        sources_.render.resolution_scale);
    sources_.menu.lighting = on_off(form_.lighting);
    sources_.menu.fog = on_off(form_.fog);
    sources_.menu.texture_filtering = on_off(form_.texture_filtering);
    sources_.menu.show_fps = on_off(form_.show_fps);
    sources_.menu.cel_shading = on_off(form_.cel_shading);
    sources_.menu.cel_bands = "8";
    sources_.menu.cel_edge = "50";

    sources_.menu.sfx_volume = format_float(
        static_cast<float>(std::clamp(form_.sfx_volume, 0, 100)) / 100.0F);
    sources_.menu.music_volume = format_float(
        static_cast<float>(std::clamp(form_.music_volume, 0, 100)) / 100.0F);
    sources_.menu.language = form_.language;

    sources_.input.mouse_sensitivity = slider_to_sensitivity(
        form_.sensitivity_slider);
    sources_.input.invert_mouse_y = form_.invert_mouse_y;
    sources_.input.invert_mouse_x = form_.invert_mouse_x;
    sources_.input.scroll_all_weapons = form_.scroll_all_weapons;
    sources_.input.gamepad_look_sensitivity = slider_to_look(
        form_.gamepad_look_slider);
    sources_.input.gamepad_dead_zone = slider_to_dead_zone(
        form_.gamepad_dead_zone_slider);
    sources_.input.gamepad_invert_y = form_.gamepad_invert_y;

    sources_.menu.point_goal = form_.point_goal;
    sources_.menu.time_limit = form_.time_limit;
    sources_.menu.damage_level = form_.damage;
    sources_.menu.team_play = on_off(form_.team_play);
    sources_.menu.friendly_fire = on_off(form_.friendly_fire);
    sources_.menu.hunter_radar = on_off(form_.hunter_radar);
    sources_.menu.affinity_weapons = on_off(form_.affinity_weapons);

    const std::string player_name = trim_copy(form_.player_name);
    if (!player_name.empty()) {
        sources_.preferences.player_name = player_name;
    }
    sources_.preferences.last_hunter = form_.hunter;
    (void)parse_endpoint(form_.server_endpoint,
                         sources_.preferences.server_address,
                         sources_.preferences.server_port);
    (void)parse_endpoint(form_.master_endpoint,
                         sources_.preferences.master_host,
                         sources_.preferences.master_port);
    sources_.preferences.auto_update = form_.auto_update;
    sources_.preferences.friendly_fire = form_.friendly_fire;

    sources_.features.pro_hud = form_.pro_hud;
    sources_.menu.features_json = update_pro_hud_json(
        sources_.menu.features_json, form_.pro_hud);

    mods::render::Options& render_options = mods::render::options();
    render_options.set_resolution_scale(sources_.render.resolution_scale);
    render_options.set_lighting(sources_.render.lighting);
    render_options.set_fog(sources_.render.fog);
    render_options.set_texture_filtering(sources_.render.texture_filtering);
    render_options.set_show_fps(sources_.render.show_fps);
    render_options.set_cel_shading(sources_.render.cel_shading);
    render_options.set_cel_bands(sources_.render.cel_bands);
    render_options.set_cel_edge(sources_.render.cel_edge);
}

std::span<const std::string_view> SettingsView::section_names() noexcept {
    return kSectionNames;
}

std::string_view SettingsView::section_name(Section section) noexcept {
    const auto index = static_cast<std::size_t>(section);
    return index < kSectionNames.size() ? kSectionNames[index]
                                        : std::string_view{};
}

int SettingsView::percent_from_text(std::string_view value,
                                    int fallback) noexcept {
    const float parsed = parse_float(value, -1.0F);
    if (parsed < 0.0F) {
        return fallback;
    }
    return std::clamp(static_cast<int>(round_to_even(parsed * 100.0)), 0,
                      100);
}

int SettingsView::sensitivity_to_slider(float sensitivity) noexcept {
    return std::clamp(static_cast<int>(round_to_even(
                           (static_cast<double>(sensitivity) - 0.1) / 2.9
                           * 100.0)),
                       0, 100);
}

float SettingsView::slider_to_sensitivity(int value) noexcept {
    return 0.1F + std::clamp(value, 0, 100) / 100.0F * 2.9F;
}

int SettingsView::look_to_slider(float look) noexcept {
    return std::clamp(static_cast<int>(round_to_even(
                           (static_cast<double>(look) - 0.25) / 2.75
                           * 100.0)),
                       0, 100);
}

float SettingsView::slider_to_look(int value) noexcept {
    return 0.25F + std::clamp(value, 0, 100) / 100.0F * 2.75F;
}

int SettingsView::dead_zone_to_slider(float dead_zone) noexcept {
    return std::clamp(static_cast<int>(round_to_even(
                           static_cast<double>(dead_zone) / 0.5 * 100.0)),
                       0, 100);
}

float SettingsView::slider_to_dead_zone(int value) noexcept {
    return std::clamp(value, 0, 100) / 100.0F * 0.5F;
}

bool SettingsView::parse_endpoint(std::string_view value, std::string& host,
                                  std::uint16_t& port) {
    const std::string text = trim_copy(value);
    if (text.empty()) {
        return false;
    }
    const std::size_t colon = text.find_last_of(':');
    if (colon == std::string::npos || colon == 0) {
        host = text;
        return true;
    }
    int parsed = 0;
    const std::string_view port_text(text.data() + colon + 1,
                                     text.size() - colon - 1);
    const auto result = std::from_chars(port_text.data(),
                                         port_text.data() + port_text.size(),
                                         parsed);
    if (result.ec != std::errc{} || result.ptr != port_text.data()
        + port_text.size() || parsed < 1 || parsed > 65'535) {
        return false;
    }
    host.assign(text.data(), colon);
    port = static_cast<std::uint16_t>(parsed);
    return true;
}

} // namespace fruityprime::launcher::gui
