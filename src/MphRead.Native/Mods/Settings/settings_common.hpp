#pragma once

#include "Mods/settings.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace fruityprime::settings::detail {

using BindingMember = input::GamepadButtons input::GamepadBindings::*;

struct BindingEntry {
    const char* key;
    BindingMember member;
};

const std::array<BindingEntry, 12>& binding_entries() noexcept;

bool parse_bool(std::string_view value, bool fallback);
bool parse_float(std::string_view value, float& result);
void trim(std::string& value);
bool parse_button_name(std::string_view name, input::GamepadButtons& result);
bool parse_buttons(std::string value, input::GamepadButtons& result);
std::string format_buttons(input::GamepadButtons buttons);
void clamp(InputConfig& config);

std::optional<std::string> json_string(std::string_view source,
                                        std::string_view key);
std::optional<std::string_view> json_value(std::string_view source,
                                           std::string_view key);
std::optional<long long> parse_integer(std::string_view value);
std::optional<float> parse_match_time(std::string_view value);
std::optional<bool> parse_match_bool(std::string value);
std::optional<std::uint8_t> parse_mode(std::string value);

std::string read_settings_json(const std::filesystem::path& directory);
void read_menu_string(std::string_view source, std::string_view key,
                      std::string& target);
std::string json_escape(std::string_view value);
void write_menu_string(std::ostream& output, std::string_view key,
                       std::string_view value, bool& first);

} // namespace fruityprime::settings::detail
