#include "Mods/render_options.hpp"
#include "Settings/settings_common.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <system_error>

namespace fruityprime::mods::render {
namespace {

[[nodiscard]] std::string_view trim(std::string_view value) noexcept {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] std::string_view trim_percent(std::string_view value) noexcept {
    value = trim(value);
    while (!value.empty() && value.back() == '%') {
        value.remove_suffix(1);
    }
    return value;
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

[[nodiscard]] int parse_number(std::string_view value, int fallback) noexcept {
    value = trim_percent(value);
    if (!value.empty() && value.front() == '+') {
        value.remove_prefix(1);
    }
    if (value.empty()) {
        return fallback;
    }
    int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                        result);
    if (parsed.ec != std::errc{}
        || parsed.ptr != value.data() + value.size()) {
        return fallback;
    }
    return result;
}

} // namespace

void Options::set_resolution_scale(int value) noexcept {
    resolution_scale_ = std::clamp(value, MinScale, 100);
}

void Options::set_cel_bands(int value) noexcept {
    cel_bands_ = std::clamp(value, 2, 8);
}

void Options::set_cel_edge(float value) noexcept {
    // Math.Clamp preserves NaN because neither bound comparison succeeds.
    cel_edge_ = std::clamp(value, 0.0F, 1.0F);
}

int Options::scaled(int pixels) const noexcept {
    if (resolution_scale_ >= 100) {
        return pixels;
    }
    return std::max(1, pixels * resolution_scale_ / 100);
}

bool Options::parse_on_off(std::string_view value, bool fallback) noexcept {
    value = trim(value);
    if (ascii_equal(value, "on") || ascii_equal(value, "true")
        || ascii_equal(value, "yes")) {
        return true;
    }
    if (ascii_equal(value, "off") || ascii_equal(value, "false")
        || ascii_equal(value, "no")) {
        return false;
    }
    return fallback;
}

std::string Options::on_off(bool value) {
    return value ? "on" : "off";
}

int Options::parse_scale(std::string_view value, int fallback) noexcept {
    return std::clamp(parse_number(value, fallback), MinScale, 100);
}

int Options::parse_int(std::string_view value, int fallback) noexcept {
    return parse_number(value, fallback);
}

Options& options() noexcept {
    static Options instance;
    return instance;
}

} // namespace fruityprime::mods::render

namespace fruityprime::mods {

int RenderOptions::ResolutionScale() noexcept {
    return render::options().resolution_scale();
}

void RenderOptions::SetResolutionScale(int value) noexcept {
    render::options().set_resolution_scale(value);
}

bool RenderOptions::Lighting() noexcept { return render::options().lighting(); }
void RenderOptions::SetLighting(bool value) noexcept {
    render::options().set_lighting(value);
}
bool RenderOptions::CelShading() noexcept {
    return render::options().cel_shading();
}
void RenderOptions::SetCelShading(bool value) noexcept {
    render::options().set_cel_shading(value);
}
bool RenderOptions::ShowFps() noexcept { return render::options().show_fps(); }
void RenderOptions::SetShowFps(bool value) noexcept {
    render::options().set_show_fps(value);
}
int RenderOptions::CelBands() noexcept { return render::options().cel_bands(); }
void RenderOptions::SetCelBands(int value) noexcept {
    render::options().set_cel_bands(value);
}
float RenderOptions::CelEdge() noexcept { return render::options().cel_edge(); }
void RenderOptions::SetCelEdge(float value) noexcept {
    render::options().set_cel_edge(value);
}
bool RenderOptions::Fog() noexcept { return render::options().fog(); }
void RenderOptions::SetFog(bool value) noexcept {
    render::options().set_fog(value);
}
bool RenderOptions::TextureFiltering() noexcept {
    return render::options().texture_filtering();
}
void RenderOptions::SetTextureFiltering(bool value) noexcept {
    render::options().set_texture_filtering(value);
}
int RenderOptions::Scaled(int pixels) noexcept {
    return render::options().scaled(pixels);
}
bool RenderOptions::ParseOnOff(std::string_view value,
                               bool fallback) noexcept {
    return render::Options::parse_on_off(value, fallback);
}
std::string RenderOptions::OnOff(bool value) {
    return render::Options::on_off(value);
}
int RenderOptions::ParseScale(std::string_view value, int fallback) noexcept {
    return render::Options::parse_scale(value, fallback);
}
int RenderOptions::ParseInt(std::string_view value, int fallback) noexcept {
    return render::Options::parse_int(value, fallback);
}

} // namespace fruityprime::mods



namespace fruityprime::settings {

RenderConfig load_render(const std::filesystem::path& directory) {
    RenderConfig config;
    const std::string source = detail::read_settings_json(directory);
    if (source.empty()) {
        return config;
    }
    try {
        const auto read_bool = [&source](std::string_view key, bool& target) {
            if (const auto value = detail::json_string(source, key); value) {
                if (const auto parsed = detail::parse_match_bool(*value); parsed) {
                    target = *parsed;
                }
            }
        };
        const auto read_integer = [&source](std::string_view key, int& target,
                                            int minimum, int maximum) {
            if (const auto value = detail::json_string(source, key); value) {
                if (const auto parsed = detail::parse_integer(*value); parsed
                    && *parsed >= minimum && *parsed <= maximum) {
                    target = static_cast<int>(*parsed);
                }
            }
        };
        read_integer("ResolutionScale", config.resolution_scale, 25, 100);
        read_bool("Lighting", config.lighting);
        read_bool("Fog", config.fog);
        read_bool("TextureFiltering", config.texture_filtering);
        read_bool("ShowFps", config.show_fps);
        read_bool("CelShading", config.cel_shading);
        read_integer("CelBands", config.cel_bands, 2, 8);
        if (const auto value = detail::json_string(source, "CelEdge"); value) {
            if (const auto parsed = detail::parse_integer(*value); parsed
                && *parsed >= 0 && *parsed <= 100) {
                config.cel_edge = static_cast<float>(*parsed) / 100.0F;
            }
        }
    } catch (...) {
        // A malformed settings file must never prevent a native game launch.
    }
    return config;
}

} // namespace fruityprime::settings
