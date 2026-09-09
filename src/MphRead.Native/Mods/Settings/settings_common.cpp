#include "settings_common.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <locale>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace fruityprime::settings::detail {


[[nodiscard]] bool parse_bool(std::string_view value, bool fallback) {
    if (value == "true" || value == "1" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "off") {
        return false;
    }
    return fallback;
}

[[nodiscard]] bool parse_float(std::string_view value, float& result) {
    try {
        std::size_t consumed = 0;
        const std::string text(value);
        const float parsed = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        result = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

void trim(std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
}

constexpr std::array<BindingEntry, 12> kBindingEntries{{
    {"pad_Shoot", &input::GamepadBindings::shoot},
    {"pad_Zoom", &input::GamepadBindings::zoom},
    {"pad_Jump", &input::GamepadBindings::jump},
    {"pad_Morph", &input::GamepadBindings::morph},
    {"pad_Scan", &input::GamepadBindings::scan},
    {"pad_ScanVisor", &input::GamepadBindings::scan_visor},
    {"pad_Scoreboard", &input::GamepadBindings::scoreboard},
    {"pad_NextWeapon", &input::GamepadBindings::next_weapon},
    {"pad_PrevWeapon", &input::GamepadBindings::prev_weapon},
    {"pad_Missile", &input::GamepadBindings::missile},
    {"pad_PowerBeam", &input::GamepadBindings::power_beam},
    {"pad_Menu", &input::GamepadBindings::menu}
}};

const std::array<BindingEntry, 12>& binding_entries() noexcept {
    return kBindingEntries;
}

[[nodiscard]] bool parse_button_name(std::string_view name,
                                     input::GamepadButtons& result) {
    using input::GamepadButtons;
    const auto set = [&result](GamepadButtons button) {
        result |= button;
        return true;
    };
    if (name == "None") {
        return true;
    }
    if (name == "A") return set(GamepadButtons::A);
    if (name == "B") return set(GamepadButtons::B);
    if (name == "X") return set(GamepadButtons::X);
    if (name == "Y") return set(GamepadButtons::Y);
    if (name == "LeftBumper") return set(GamepadButtons::LeftBumper);
    if (name == "RightBumper") return set(GamepadButtons::RightBumper);
    if (name == "Back") return set(GamepadButtons::Back);
    if (name == "Start") return set(GamepadButtons::Start);
    if (name == "LeftThumb") return set(GamepadButtons::LeftThumb);
    if (name == "RightThumb") return set(GamepadButtons::RightThumb);
    if (name == "DpadUp") return set(GamepadButtons::DpadUp);
    if (name == "DpadRight") return set(GamepadButtons::DpadRight);
    if (name == "DpadDown") return set(GamepadButtons::DpadDown);
    if (name == "DpadLeft") return set(GamepadButtons::DpadLeft);
    if (name == "LeftTrigger") return set(GamepadButtons::LeftTrigger);
    if (name == "RightTrigger") return set(GamepadButtons::RightTrigger);
    return false;
}

[[nodiscard]] bool parse_buttons(std::string value,
                                 input::GamepadButtons& result) {
    trim(value);
    if (value.empty()) {
        return false;
    }
    result = input::GamepadButtons::None;
    std::size_t begin = 0;
    while (begin < value.size()) {
        const std::size_t end = value.find_first_of(",|", begin);
        std::string token = value.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        trim(token);
        if (token != "None" && !parse_button_name(token, result)) {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return true;
}

[[nodiscard]] std::string format_buttons(input::GamepadButtons buttons) {
    using input::GamepadButtons;
    std::string result;
    const auto append = [&result, buttons](GamepadButtons button,
                                            const char* name) {
        if ((buttons & button) == GamepadButtons::None) {
            return;
        }
        if (!result.empty()) {
            result += ", ";
        }
        result += name;
    };
    append(GamepadButtons::A, "A");
    append(GamepadButtons::B, "B");
    append(GamepadButtons::X, "X");
    append(GamepadButtons::Y, "Y");
    append(GamepadButtons::LeftBumper, "LeftBumper");
    append(GamepadButtons::RightBumper, "RightBumper");
    append(GamepadButtons::Back, "Back");
    append(GamepadButtons::Start, "Start");
    append(GamepadButtons::LeftThumb, "LeftThumb");
    append(GamepadButtons::RightThumb, "RightThumb");
    append(GamepadButtons::DpadUp, "DpadUp");
    append(GamepadButtons::DpadRight, "DpadRight");
    append(GamepadButtons::DpadDown, "DpadDown");
    append(GamepadButtons::DpadLeft, "DpadLeft");
    append(GamepadButtons::LeftTrigger, "LeftTrigger");
    append(GamepadButtons::RightTrigger, "RightTrigger");
    return result.empty() ? "None" : result;
}

void clamp(InputConfig& config) {
    config.mouse_sensitivity = std::clamp(
        config.mouse_sensitivity, 0.05F, 10.0F);
    config.gamepad_dead_zone = std::clamp(
        config.gamepad_dead_zone, 0.0F, 0.9F);
    config.gamepad_look_sensitivity = std::clamp(
        config.gamepad_look_sensitivity, 0.1F, 5.0F);
}

[[nodiscard]] std::optional<std::string> json_string(
    std::string_view source, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t search = 0;
    while (true) {
        const std::size_t key_start = source.find(needle, search);
        if (key_start == std::string_view::npos) {
            return std::nullopt;
        }
        std::size_t cursor = key_start + needle.size();
        while (cursor < source.size()
               && std::isspace(static_cast<unsigned char>(source[cursor]))) {
            ++cursor;
        }
        if (cursor >= source.size() || source[cursor] != ':') {
            search = key_start + 1;
            continue;
        }
        ++cursor;
        while (cursor < source.size()
               && std::isspace(static_cast<unsigned char>(source[cursor]))) {
            ++cursor;
        }
        if (cursor >= source.size() || source[cursor] != '"') {
            search = key_start + 1;
            continue;
        }
        ++cursor;
        std::string result;
        while (cursor < source.size()) {
            const char value = source[cursor++];
            if (value == '"') {
                return result;
            }
            if (value != '\\' || cursor >= source.size()) {
                result.push_back(value);
                continue;
            }
            const char escaped = source[cursor++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default:
                // Settings written by both launchers are ASCII. Preserve an
                // unknown escape as its payload instead of aborting startup.
                result.push_back(escaped);
                break;
            }
        }
        return std::nullopt;
    }
}

// Return the raw JSON value for the small number of managed objects that the
// native side does not interpret yet. Strings are returned with their quotes;
// containers are scanned while respecting quoted braces and brackets.
[[nodiscard]] std::optional<std::string_view> json_value(
    std::string_view source, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t search = 0;
    while (true) {
        const std::size_t key_start = source.find(needle, search);
        if (key_start == std::string_view::npos) {
            return std::nullopt;
        }
        std::size_t cursor = key_start + needle.size();
        while (cursor < source.size()
               && std::isspace(static_cast<unsigned char>(source[cursor]))) {
            ++cursor;
        }
        if (cursor >= source.size() || source[cursor] != ':') {
            search = key_start + 1;
            continue;
        }
        ++cursor;
        while (cursor < source.size()
               && std::isspace(static_cast<unsigned char>(source[cursor]))) {
            ++cursor;
        }
        if (cursor >= source.size()) {
            return std::nullopt;
        }
        const std::size_t value_start = cursor;
        if (source[cursor] == '"') {
            ++cursor;
            bool escaped = false;
            while (cursor < source.size()) {
                const char character = source[cursor++];
                if (escaped) {
                    escaped = false;
                } else if (character == '\\') {
                    escaped = true;
                } else if (character == '"') {
                    return source.substr(value_start, cursor - value_start);
                }
            }
            return std::nullopt;
        }
        if (source[cursor] == '{' || source[cursor] == '[') {
            int depth = 0;
            bool in_string = false;
            bool escaped = false;
            for (; cursor < source.size(); ++cursor) {
                const char character = source[cursor];
                if (in_string) {
                    if (escaped) {
                        escaped = false;
                    } else if (character == '\\') {
                        escaped = true;
                    } else if (character == '"') {
                        in_string = false;
                    }
                    continue;
                }
                if (character == '"') {
                    in_string = true;
                } else if (character == '{' || character == '[') {
                    ++depth;
                } else if (character == '}' || character == ']') {
                    --depth;
                    if (depth == 0) {
                        return source.substr(value_start,
                                             cursor - value_start + 1);
                    }
                }
            }
            return std::nullopt;
        }
        while (cursor < source.size() && source[cursor] != ','
               && source[cursor] != '}') {
            ++cursor;
        }
        if (cursor == value_start) {
            return std::nullopt;
        }
        const std::size_t value_end = source.find_last_not_of(
            " \t\r\n", cursor - 1);
        if (value_end == std::string_view::npos || value_end < value_start) {
            return std::nullopt;
        }
        return source.substr(value_start, value_end - value_start + 1);
    }
}

[[nodiscard]] std::optional<long long> parse_integer(std::string_view value) {
    try {
        std::size_t consumed = 0;
        const std::string text(value);
        const long long parsed = std::stoll(text, &consumed, 10);
        if (consumed != text.size()) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<float> parse_match_time(
    std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    float total = 0.0F;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t end = value.find(':', begin);
        const std::string part(value.substr(
            begin, end == std::string_view::npos
                ? std::string_view::npos : end - begin));
        const auto number = parse_integer(part);
        if (!number || *number < 0 || *number > 2'147'483'647) {
            return std::nullopt;
        }
        total = total * 60.0F + static_cast<float>(*number);
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return total;
}

[[nodiscard]] std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)));
    }
    return value;
}

[[nodiscard]] std::optional<bool> parse_match_bool(std::string value) {
    value = lower_ascii(std::move(value));
    if (value == "on" || value == "true" || value == "1"
        || value == "yes") {
        return true;
    }
    if (value == "off" || value == "false" || value == "0"
        || value == "no") {
        return false;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::uint8_t> parse_mode(std::string value) {
    value = lower_ascii(std::move(value));
    if (const auto number = parse_integer(value);
        number && *number >= 0 && *number <= 255) {
        return static_cast<std::uint8_t>(*number);
    }
    // These names match the mode choices exposed by the managed launcher.
    static constexpr std::array<std::pair<std::string_view, std::uint8_t>, 15>
        names{{
            {"none", 0}, {"adventure", 2}, {"story", 2}, {"1p", 2},
            {"battle", 3}, {"battleteams", 4}, {"survival", 5},
            {"survivalteams", 6}, {"capture", 7}, {"bounty", 8},
            {"bountyteams", 9}, {"nodes", 10}, {"nodesteams", 11},
            {"defender", 12}, {"defenderteams", 13}
        }};
    for (const auto& [name, mode] : names) {
        if (value == name) {
            return mode;
        }
    }
    if (value == "primehunter") {
        return 14;
    }
    return std::nullopt;
}

[[nodiscard]] std::string read_settings_json(
    const std::filesystem::path& directory) {
    const std::array paths{
        directory / "Savedata" / "settings.json",
        directory / "settings.json"
    };
    for (const auto& path : paths) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            continue;
        }
        std::ostringstream text;
        text << input.rdbuf();
        return text.str();
    }
    return {};
}

void read_menu_string(std::string_view source, std::string_view key,
                      std::string& target) {
    if (const auto value = json_string(source, key); value.has_value()) {
        target = *value;
    }
}

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(character);
            break;
        }
    }
    return result;
}

void write_menu_string(std::ostream& output, std::string_view key,
                       std::string_view value, bool& first) {
    if (!first) {
        output << ",\n";
    }
    first = false;
    output << "    \"" << key << "\": \""
           << json_escape(value) << '"';
}

} // namespace fruityprime::settings::detail
