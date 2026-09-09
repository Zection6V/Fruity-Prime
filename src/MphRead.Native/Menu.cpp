#include "Menu.hpp"

#include "MenuModelMetadata.hpp"

#include "Mods/branding.hpp"
#include "Metadata/metadata_lookup.hpp"
#include "Metadata/Rooms.hpp"
#include "Utility/rng.hpp"
#include "Metadata/room_metadata.hpp"
#include "SceneSetup.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace fruityprime::menu {
namespace {

constexpr std::array<std::string_view, 8> MenuHunters{{
    "Samus", "Kanden", "Spire", "Trace",
    "Noxus", "Sylux", "Weavel", "Guardian"
}};

// Menu.cs has a separate display/selection list above, but Enum.TryParse on
// a numeric Hunter value uses the enum declaration order.  Keep that order
// explicit instead of accidentally interpreting numeric input through the
// menu's local selection order.
constexpr std::array<std::string_view, 8> EnumHunters{{
    "Samus", "Kanden", "Trace", "Sylux",
    "Noxus", "Spire", "Weavel", "Guardian"
}};

constexpr std::array<std::string_view, 14> Modes{{
    "auto-select", "Adventure", "Battle", "Battle Teams",
    "Survival", "Survival Teams", "Capture", "Bounty",
    "Bounty Teams", "Nodes", "Nodes Teams", "Defender",
    "Defender Teams", "Prime Hunter"
}};

constexpr std::array<std::string_view, 8> MphVersions{{
    "A76E0", "AMHE0", "AMHE1", "AMHP0",
    "AMHP1", "AMHJ0", "AMHJ1", "AMHK0"
}};

constexpr std::array<std::string_view, 2> FhVersions{{"AMFE0", "AMFP0"}};

constexpr std::array<double, 15> BattlePoints{{
    1, 5, 7, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80, 90, 100
}};
constexpr std::array<double, 13> OctolithPoints{{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 25
}};
constexpr std::array<double, 14> NodePoints{{
    40, 50, 60, 70, 80, 90, 100, 120, 140, 160, 180, 190, 200, 250
}};
constexpr std::array<double, 11> ExtraLives{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
constexpr std::array<double, 14> TimeGoals{{
    60, 90, 120, 150, 180, 210, 240, 270, 300, 360, 420, 480, 540, 600
}};
constexpr std::array<double, 15> TimeLimits{{
    180, 300, 420, 540, 600, 900, 1200, 1500,
    1800, 2100, 2400, 2700, 3000, 3300, 3600
}};

[[nodiscard]] bool parse_integer(std::string_view value, int& result) noexcept {
    if (value.empty()) {
        return false;
    }
    const char* first = value.data();
    const char* last = first + value.size();
    while (first != last
           && std::isspace(static_cast<unsigned char>(*first)) != 0) {
        ++first;
    }
    while (first != last
           && std::isspace(static_cast<unsigned char>(*(last - 1))) != 0) {
        --last;
    }
    if (first == last) {
        return false;
    }
    // Int32.TryParse accepts an explicit plus sign; from_chars does not.
    // Keep the range/error behavior of from_chars for the actual digits.
    if (*first == '+') {
        ++first;
        if (first == last) {
            return false;
        }
    }
    const auto parsed = std::from_chars(first, last, result);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

[[nodiscard]] std::string trim_impl(std::string_view value);
[[nodiscard]] std::string lower_ascii_impl(std::string_view value);

[[nodiscard]] bool parse_number(std::string_view value, double& result) {
    const std::string copy(trim_impl(value));
    if (copy.empty()) {
        return false;
    }
    // Decimal.TryParse(string, out decimal) uses NumberStyles.Number.  The
    // menu values are decimal numbers, not C/C++ floating-point literals:
    // exponent notation, hexadecimal notation, NaN and Infinity are not
    // accepted by the managed parser.  Validate the ordinary decimal form
    // before handing it to strtod for the numeric conversion.
    std::size_t cursor = 0;
    if (copy[cursor] == '+' || copy[cursor] == '-') {
        ++cursor;
    }
    bool digit = false;
    bool integer_digit = false;
    bool fraction = false;
    bool decimal_point = false;
    std::string normalized;
    normalized.reserve(copy.size());
    if (cursor > 0) {
        normalized.push_back(copy.front());
    }
    while (cursor < copy.size()) {
        const char character = copy[cursor];
        if (character >= '0' && character <= '9') {
            digit = true;
            if (!fraction) {
                integer_digit = true;
            }
            normalized.push_back(character);
            ++cursor;
            continue;
        }
        if (character == ',') {
            // Decimal.TryParse(NumberStyles.Number) ignores group separators
            // without validating their grouping (for example, "1,00" is
            // accepted as 100). A leading comma is still invalid.
            if (fraction || !integer_digit) {
                return false;
            }
            ++cursor;
            continue;
        }
        if (character == '.' && !decimal_point) {
            decimal_point = true;
            fraction = true;
            normalized.push_back(character);
            ++cursor;
            continue;
        }
        return false;
    }
    if (!digit) {
        return false;
    }
    char* end = nullptr;
    result = std::strtod(normalized.c_str(), &end);
    return end != normalized.c_str() && end != nullptr && *end == '\0'
        && std::isfinite(result);
}

[[nodiscard]] bool parse_single_setting(std::string_view value,
                                         float& result) {
    const std::string copy(trim_impl(value));
    if (copy.empty()) {
        return false;
    }
    const std::string lower = lower_ascii_impl(copy);
    if (lower == "nan" || lower == "+nan" || lower == "-nan") {
        result = std::numeric_limits<float>::quiet_NaN();
        return true;
    }
    if (lower == "infinity" || lower == "+infinity") {
        result = std::numeric_limits<float>::infinity();
        return true;
    }
    if (lower == "-infinity") {
        result = -std::numeric_limits<float>::infinity();
        return true;
    }

    // Single.TryParse(value, InvariantCulture) accepts Float plus thousands:
    // signs, a decimal point, an exponent, and group separators in the
    // integer part. It does not accept hexadecimal floating-point syntax.
    std::size_t cursor = 0;
    if (copy[cursor] == '+' || copy[cursor] == '-') {
        ++cursor;
    }
    bool digit = false;
    bool integer_digit = false;
    bool fraction = false;
    bool decimal_point = false;
    bool exponent = false;
    std::string normalized;
    normalized.reserve(copy.size());
    if (cursor > 0) {
        normalized.push_back(copy.front());
    }
    while (cursor < copy.size()) {
        const char character = copy[cursor];
        if (character >= '0' && character <= '9') {
            digit = true;
            if (!fraction && !exponent) {
                integer_digit = true;
            }
            normalized.push_back(character);
            ++cursor;
            continue;
        }
        if (character == ',' && !fraction && !exponent) {
            if (!integer_digit) {
                return false;
            }
            ++cursor;
            continue;
        }
        if (character == '.' && !decimal_point && !exponent) {
            decimal_point = true;
            fraction = true;
            normalized.push_back(character);
            ++cursor;
            continue;
        }
        if ((character == 'e' || character == 'E') && !exponent && digit) {
            exponent = true;
            normalized.push_back('e');
            ++cursor;
            if (cursor < copy.size()
                && (copy[cursor] == '+' || copy[cursor] == '-')) {
                normalized.push_back(copy[cursor++]);
            }
            const std::size_t exponent_start = cursor;
            while (cursor < copy.size()
                   && copy[cursor] >= '0' && copy[cursor] <= '9') {
                normalized.push_back(copy[cursor++]);
            }
            if (cursor == exponent_start) {
                return false;
            }
            continue;
        }
        return false;
    }
    if (!digit) {
        return false;
    }
    char* end = nullptr;
    result = std::strtof(normalized.c_str(), &end);
    return end != normalized.c_str() && end != nullptr && *end == '\0'
        && (std::isfinite(result) || std::isnan(result)
            || std::isinf(result));
}

[[nodiscard]] std::string lower_ascii_impl(std::string_view value) {
    std::string result(value);
    for (char& character : result) {
        character = static_cast<char>(std::tolower(
            static_cast<unsigned char>(character)));
    }
    return result;
}

[[nodiscard]] std::string trim_impl(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

// Decimal.TryParse followed by decimal.ToString() preserves the parsed
// scale ("0.50" remains "0.50") while discarding surrounding whitespace and
// redundant integer zeroes. Keep that small part of Decimal's behavior for
// the two volume fields without making the entire menu depend on a binary
// floating-point formatter.
[[nodiscard]] std::string normalize_decimal_text(std::string_view value) {
    const std::string trimmed = trim_impl(value);
    if (trimmed.empty()) {
        return {};
    }
    bool negative = trimmed.front() == '-';
    const std::size_t begin = (trimmed.front() == '+' || negative) ? 1 : 0;
    const std::size_t dot = trimmed.find('.', begin);
    const std::size_t integer_end = dot == std::string::npos
        ? trimmed.size() : dot;
    if (begin == integer_end && (dot == std::string::npos
                                 || dot + 1 == trimmed.size())) {
        return {};
    }
    std::string integer(trimmed.substr(begin, integer_end - begin));
    std::string fraction = dot == std::string::npos
        ? std::string{} : std::string(trimmed.substr(dot + 1));
    const auto digits = [](std::string_view part) {
        return std::all_of(part.begin(), part.end(), [](unsigned char value) {
            return std::isdigit(value) != 0;
        });
    };
    if (!digits(fraction)) {
        return {};
    }
    // Decimal.TryParse(NumberStyles.Number) discards group separators in the
    // integer part without requiring a three-digit grouping. Preserve the
    // fractional scale after removing them for Decimal.ToString().
    if (integer.find(',') != std::string::npos) {
        std::string ungrouped;
        ungrouped.reserve(integer.size());
        for (const char character : integer) {
            if (character != ','
                && (character < '0' || character > '9')) {
                return {};
            }
            if (character != ',') {
                ungrouped.push_back(character);
            }
        }
        integer = std::move(ungrouped);
    } else if (!digits(integer)) {
        return {};
    }
    const std::size_t first_nonzero = integer.find_first_not_of('0');
    integer = first_nonzero == std::string::npos
        ? "0" : integer.substr(first_nonzero);
    if (integer == "0"
        && fraction.find_first_not_of('0') == std::string::npos) {
        // Decimal does not retain a negative sign for a zero value, while it
        // does retain the fractional scale ("-0.00" becomes "0.00").
        negative = false;
    }
    std::string result = (negative ? "-" : "") + integer;
    if (dot != std::string::npos) {
        result += '.';
        result += fraction;
    }
    return result;
}

[[nodiscard]] std::vector<std::string> split_csv_impl(std::string_view value) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t end = value.find(',', start);
        const auto part = value.substr(
            start, end == std::string_view::npos
                ? std::string_view::npos : end - start);
        if (!trim_impl(part).empty()) {
            result.push_back(trim_impl(part));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return result;
}

[[nodiscard]] std::vector<std::string> split_literal_spaces(
    std::string_view value) {
    // String.Split(' ', RemoveEmptyEntries | TrimEntries) in Menu.cs uses
    // only the literal U+0020 separator.  TrimEntries still removes other
    // whitespace around each resulting token, but a tab inside a token must
    // not become an additional field.
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t end = value.find(' ', start);
        const auto part = value.substr(
            start, end == std::string_view::npos
                ? std::string_view::npos : end - start);
        const std::string trimmed = trim_impl(part);
        if (!trimmed.empty()) {
            result.push_back(trimmed);
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return result;
}

[[nodiscard]] std::string join(const std::vector<std::string>& values,
                               std::string_view separator) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) {
            result += separator;
        }
        result += value;
    }
    return result;
}

[[nodiscard]] std::string join_views(
    const std::array<std::string_view, 5>& values) {
    std::vector<std::string> nonempty;
    for (const auto value : values) {
        if (!value.empty()) {
            nonempty.emplace_back(value);
        }
    }
    return join(nonempty, ",");
}

[[nodiscard]] bool begins(std::string_view value,
                          std::string_view prefix) noexcept {
    return value.size() >= prefix.size()
        && value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool ends(std::string_view value,
                        std::string_view suffix) noexcept {
    return value.size() >= suffix.size()
        && value.substr(value.size() - suffix.size()) == suffix;
}

[[nodiscard]] std::optional<formats::MetaDir> parse_meta_dir(
    std::string_view value) noexcept {
    constexpr std::array<std::pair<std::string_view, formats::MetaDir>, 24>
        values{{
            {"Models", formats::MetaDir::Models},
            {"Hud", formats::MetaDir::Hud},
            {"Stage", formats::MetaDir::Stage},
            {"MainMenu", formats::MetaDir::MainMenu},
            {"Logo", formats::MetaDir::Logo},
            {"CharSelect", formats::MetaDir::CharSelect},
            {"CreateJoin", formats::MetaDir::CreateJoin},
            {"GameOption", formats::MetaDir::GameOption},
            {"GamersCard", formats::MetaDir::GamersCard},
            {"Keyboard", formats::MetaDir::Keyboard},
            {"Keypad", formats::MetaDir::Keypad},
            {"MoviePlayer", formats::MetaDir::MoviePlayer},
            {"MultiMaster", formats::MetaDir::MultiMaster},
            {"Multiplayer", formats::MetaDir::Multiplayer},
            {"PaxControls", formats::MetaDir::PaxControls},
            {"Popup", formats::MetaDir::Popup},
            {"Results", formats::MetaDir::Results},
            {"ScStartGame", formats::MetaDir::ScStartGame},
            {"StartGame", formats::MetaDir::StartGame},
            {"ToStart", formats::MetaDir::ToStart},
            {"TouchToStart", formats::MetaDir::TouchToStart},
            {"TouchToStart2", formats::MetaDir::TouchToStart2},
            {"WifiCreate", formats::MetaDir::WifiCreate},
            {"WifiGames", formats::MetaDir::WifiGames}
        }};
    for (const auto& [name, directory] : values) {
        if (name == value) {
            return directory;
        }
    }
    // Enum.TryParse<MetaDir> also accepts an underlying signed integer,
    // including values which are not named enum members.  Metadata's lookup
    // treats every value other than Models/Hud/Logo/Multiplayer/TouchToStart
    // as a frontend directory, so preserve that result instead of treating a
    // numeric token as a failed parse.
    int numeric = 0;
    if (parse_integer(value, numeric)) {
        return static_cast<formats::MetaDir>(numeric);
    }
    return std::nullopt;
}

[[nodiscard]] ModelMetadataSet model_metadata_set(
    formats::MetaDir directory) noexcept {
    switch (directory) {
    case formats::MetaDir::Models: return ModelMetadataSet::Models;
    case formats::MetaDir::Hud: return ModelMetadataSet::Hud;
    case formats::MetaDir::TouchToStart: return ModelMetadataSet::TouchToStart;
    case formats::MetaDir::Multiplayer: return ModelMetadataSet::Multiplayer;
    case formats::MetaDir::Logo: return ModelMetadataSet::Logo;
    default: return ModelMetadataSet::Frontend;
    }
}

[[nodiscard]] const MenuModelMetadata* find_model_metadata(
    std::string_view name, bool first_hunt,
    formats::MetaDir directory) noexcept {
    const ModelMetadataSet set = first_hunt
        ? ModelMetadataSet::FirstHunt : model_metadata_set(directory);
    for (const auto& metadata : menu_model_metadata()) {
        if (!first_hunt && metadata.set == ModelMetadataSet::Special
            && (name == "doubleDamage_img" || name == "ad2_dm2")) {
            if (metadata.name == name) {
                return &metadata;
            }
        } else if (metadata.set == set && metadata.name == name) {
            return &metadata;
        }
    }
    return nullptr;
}

[[nodiscard]] std::string json_value(std::string_view object,
                                      std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const std::size_t key_pos = object.find(needle);
    if (key_pos == std::string_view::npos) {
        return {};
    }
    std::size_t cursor = object.find(':', key_pos + needle.size());
    if (cursor == std::string_view::npos) {
        return {};
    }
    ++cursor;
    while (cursor < object.size()
           && std::isspace(static_cast<unsigned char>(object[cursor]))) {
        ++cursor;
    }
    if (cursor >= object.size()) {
        return {};
    }
    if (object[cursor] == '"') {
        ++cursor;
        std::string value;
        bool escaped = false;
        while (cursor < object.size()) {
            const char character = object[cursor++];
            if (character == '"' && !escaped) {
                return value;
            }
            if (character == '\\' && !escaped) {
                escaped = true;
                continue;
            }
            value.push_back(character);
            escaped = false;
        }
        return {};
    }
    const std::size_t end = object.find_first_of(",}", cursor);
    return trim_impl(object.substr(
        cursor, end == std::string_view::npos
            ? std::string_view::npos : end - cursor));
}

[[nodiscard]] formats::SaveWhen save_when_from_string(
    std::string_view value, formats::SaveWhen fallback) noexcept {
    if (value.size() < 2) {
        return fallback;
    }
    std::string normalized = lower_ascii_impl(value);
    normalized[0] = static_cast<char>(std::toupper(
        static_cast<unsigned char>(normalized[0])));
    if (normalized == "Never") {
        return formats::SaveWhen::Never;
    }
    if (normalized == "Always") {
        return formats::SaveWhen::Always;
    }
    if (normalized == "Prompt") {
        return formats::SaveWhen::Prompt;
    }
    int numeric = 0;
    if (parse_integer(normalized, numeric)) {
        // Menu.cs uses Enum.TryParse without Enum.IsDefined here, so an
        // underlying numeric value is accepted even when it has no named
        // SaveWhen member.
        return static_cast<formats::SaveWhen>(numeric);
    }
    return fallback;
}

[[nodiscard]] std::string save_when_name(formats::SaveWhen value) {
    switch (value) {
    case formats::SaveWhen::Always: return "always";
    case formats::SaveWhen::Prompt: return "prompt";
    case formats::SaveWhen::Never: return "never";
    default: return std::to_string(static_cast<int>(value));
    }
}

[[nodiscard]] settings::MenuSettings to_native(
    const MenuSettings& value, std::string features) {
    settings::MenuSettings result;
    result.features_json = std::move(features);
    result.room_key = value.RoomKey;
    result.mode = value.Mode;
    result.player1 = value.Player1;
    result.player2 = value.Player2;
    result.player3 = value.Player3;
    result.player4 = value.Player4;
    result.models = value.Models;
    result.mph_version = value.MphVersion;
    result.fh_version = value.FhVersion;
    result.language = value.Language;
    result.sfx_volume = value.SfxVolume;
    result.music_volume = value.MusicVolume;
    result.resolution_scale = value.ResolutionScale;
    result.lighting = value.Lighting;
    result.fog = value.Fog;
    result.texture_filtering = value.TextureFiltering;
    result.show_fps = value.ShowFps;
    result.cel_shading = value.CelShading;
    result.cel_bands = value.CelBands;
    result.cel_edge = value.CelEdge;
    result.point_goal = value.PointGoal;
    result.time_limit = value.TimeLimit;
    result.time_goal = value.TimeGoal;
    result.auto_reset = value.AutoReset;
    result.team_play = value.TeamPlay;
    result.hunter_radar = value.HunterRadar;
    result.damage_level = value.DamageLevel;
    result.friendly_fire = value.FriendlyFire;
    result.affinity_weapons = value.AffinityWeapons;
    result.save_slot = value.SaveSlot;
    result.save_from_exit = value.SaveFromExit;
    result.save_from_ship = value.SaveFromShip;
    result.planets = value.Planets;
    result.alinos1_state = value.Alinos1State;
    result.alinos2_state = value.Alinos2State;
    result.ca1_state = value.Ca1State;
    result.ca2_state = value.Ca2State;
    result.vdo1_state = value.Vdo1State;
    result.vdo2_state = value.Vdo2State;
    result.arcterra1_state = value.Arcterra1State;
    result.arcterra2_state = value.Arcterra2State;
    result.checkpoint_id = value.CheckpointId;
    result.health_max = value.HealthMax;
    result.missile_max = value.MissileMax;
    result.ua_max = value.UaMax;
    result.weapons = value.Weapons;
    result.octoliths = value.Octoliths;
    return result;
}

[[nodiscard]] MenuSettings from_native(const settings::MenuSettings& value) {
    MenuSettings result;
    result.RoomKey = value.room_key;
    result.Mode = value.mode;
    result.Player1 = value.player1;
    result.Player2 = value.player2;
    result.Player3 = value.player3;
    result.Player4 = value.player4;
    result.Models = value.models;
    result.MphVersion = value.mph_version;
    result.FhVersion = value.fh_version;
    result.Language = value.language;
    result.SfxVolume = value.sfx_volume;
    result.MusicVolume = value.music_volume;
    result.ResolutionScale = value.resolution_scale;
    result.Lighting = value.lighting;
    result.Fog = value.fog;
    result.TextureFiltering = value.texture_filtering;
    result.ShowFps = value.show_fps;
    result.CelShading = value.cel_shading;
    result.CelBands = value.cel_bands;
    result.CelEdge = value.cel_edge;
    result.PointGoal = value.point_goal;
    result.TimeLimit = value.time_limit;
    result.TimeGoal = value.time_goal;
    result.AutoReset = value.auto_reset;
    result.TeamPlay = value.team_play;
    result.HunterRadar = value.hunter_radar;
    result.DamageLevel = value.damage_level;
    result.FriendlyFire = value.friendly_fire;
    result.AffinityWeapons = value.affinity_weapons;
    result.SaveSlot = value.save_slot;
    result.SaveFromExit = value.save_from_exit;
    result.SaveFromShip = value.save_from_ship;
    result.Planets = value.planets;
    result.Alinos1State = value.alinos1_state;
    result.Alinos2State = value.alinos2_state;
    result.Ca1State = value.ca1_state;
    result.Ca2State = value.ca2_state;
    result.Vdo1State = value.vdo1_state;
    result.Vdo2State = value.vdo2_state;
    result.Arcterra1State = value.arcterra1_state;
    result.Arcterra2State = value.arcterra2_state;
    result.CheckpointId = value.checkpoint_id;
    result.HealthMax = value.health_max;
    result.MissileMax = value.missile_max;
    result.UaMax = value.ua_max;
    result.Weapons = value.weapons;
    result.Octoliths = value.octoliths;
    return result;
}

[[nodiscard]] bool parse_language(std::string_view value,
                                  int& result) noexcept {
    constexpr std::array<std::pair<std::string_view, int>, 6>
        languages{{
            {"English", 0}, {"Japanese", 1}, {"French", 2},
            {"Spanish", 3}, {"German", 4}, {"Italian", 5}
        }};

    // Enum.TryParse<TEnum> trims each comma-separated enum name, but keeps
    // name matching case-sensitive. Numeric enum values use Int32, not the
    // byte-sized representation used by the cartridge language table.
    const std::string text = trim_impl(value);
    if (text.empty()) {
        return false;
    }
    int combined = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find(',', start);
        const std::string part = trim_impl(text.substr(
            start, end == std::string::npos
                ? std::string_view::npos : end - start));
        if (part.empty()) {
            return false;
        }
        bool found = false;
        for (const auto& [name, language] : languages) {
            if (name == part) {
                combined |= language;
                found = true;
                break;
            }
        }
        if (!found) {
            int numeric = 0;
            if (end != std::string::npos || !parse_integer(part, numeric)) {
                return false;
            }
            combined = numeric;
        }
        if (end == std::string::npos) {
            result = combined;
            return true;
        }
        start = end + 1;
    }
    return false;
}

} // namespace

Configuration::Configuration() {
    // Formats.Paths.AllPaths is lazy in the managed implementation and reads
    // paths.txt from the current directory. Keep the native value object on
    // the same first-use contract when no explicit root was given.
    paths_.update(std::filesystem::current_path());
    set_defaults();
    static_cast<void>(read_room(room_key_));
}

Configuration::Configuration(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {
    // Paths.AllPaths is lazy in Menu.cs and resolves a missing explicit root
    // against the process directory. The settings-taking constructor reaches
    // this one too, so it must have the same behavior as the default ctor.
    paths_.update(root_directory_.empty()
                      ? std::filesystem::current_path() : root_directory_);
    set_defaults();
    static_cast<void>(read_room(room_key_));
}

Configuration::Configuration(const MenuSettings& settings,
                             std::filesystem::path root_directory)
    : Configuration(std::move(root_directory)) {
    load(settings);
    update_settings();
}

void Configuration::set_defaults() {
    mode_ = "auto-select";
    language_value_ = 0;
    sfx_volume_ = 0.35;
    music_volume_ = 0.50;
    sfx_volume_text_ = "0.35";
    music_volume_text_ = "0.50";
    movie_id_ = -1;
    room_ = {};
    room_key_ = "MP3 PROVING GROUND";
    room_id_ = -1;
    fh_room_ = false;
    players_ = {{
        {"Samus", "orange", "0"},
        {"none", "green", "0"},
        {"none", "orange", "0"},
        {"none", "green", "0"}
    }};
    player_ids_ = {{0, -1, -1, -1}};
    models_.clear();
    apply_settings_ = false;
    teams_ = false;
    point_goal_ = 0;
    time_goal_ = 0;
    time_limit_ = 0;
    octolith_reset_ = true;
    radar_players_ = false;
    damage_level_ = 1;
    friendly_fire_ = false;
    affinity_weapons_ = false;
    goal_type_.clear();
    planets_ = {{1, 0, 0, 0, 0}};
    boss_states_.fill(0);
    checkpoint_id_ = -1;
    health_max_ = 99;
    missile_max_ = 50;
    ua_max_ = 400;
    weapons_ = {{1, 0, 1, 0, 0, 0, 0, 0, 0}};
    octoliths_.fill(0);
    SaveSlot = 0;
    PreviousSaveSlot = -1;
    SaveFromExit = formats::SaveWhen::Never;
    SaveFromShip = formats::SaveWhen::Prompt;
    NeededSave = formats::SaveWhen::Never;
    feature_registry_ = features::Registry{};
}

std::string Configuration::lower_ascii(std::string_view value) {
    return lower_ascii_impl(value);
}

std::string Configuration::trim(std::string_view value) {
    return trim_impl(value);
}

std::vector<std::string> Configuration::split_csv(std::string_view value) {
    return split_csv_impl(value);
}

bool Configuration::is_known_mph_version(std::string_view value) {
    return std::find(MphVersions.begin(), MphVersions.end(), value)
        != MphVersions.end();
}

bool Configuration::is_known_fh_version(std::string_view value) {
    return std::find(FhVersions.begin(), FhVersions.end(), value)
        != FhVersions.end();
}

std::string Configuration::format_volume(double value) {
    if (!std::isfinite(value)) {
        value = 0;
    }
    // decimal.ToString() does not retain insignificant zeroes (0.50 becomes
    // "0.5"). Loaded values may contain more decimal places than the .05
    // interactive step, so do not hard-code a scale of two here.
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(15) << value;
    std::string result = output.str();
    while (result.size() > 1 && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

std::string Configuration::language_name(int value) {
    switch (value) {
    case 0: return "English";
    case 1: return "Japanese";
    case 2: return "French";
    case 3: return "Spanish";
    case 4: return "German";
    case 5: return "Italian";
    default: return std::to_string(value);
    }
}

int Configuration::parse_state(std::string_view value) noexcept {
    return value == "escape" ? 1 : value == "done" ? 2 : 0;
}

std::string Configuration::format_state(int state) const {
    return state == 1 ? "escape" : state == 2 ? "done" : "none";
}

formats::SaveWhen Configuration::parse_save_when(
    std::string_view value, formats::SaveWhen fallback) const noexcept {
    return save_when_from_string(value, fallback);
}

void Configuration::load(const MenuSettings& settings_value) {
    static_cast<void>(read_room(settings_value.RoomKey));
    static_cast<void>(read_mode(settings_value.Mode));
    static_cast<void>(read_player(settings_value.Player1, 0));
    static_cast<void>(read_player(settings_value.Player2, 1));
    static_cast<void>(read_player(settings_value.Player3, 2));
    static_cast<void>(read_player(settings_value.Player4, 3));
    static_cast<void>(read_models(settings_value.Models));
    static_cast<void>(read_mph_version(settings_value.MphVersion));
    // This intentionally mirrors Menu.cs: the original call passes
    // MphVersion instead of FhVersion.
    static_cast<void>(read_fh_version(settings_value.MphVersion));
    static_cast<void>(read_time_limit(settings_value.TimeLimit));
    static_cast<void>(read_time_goal(settings_value.TimeGoal));

    int parsed_language = 0;
    if (parse_language(settings_value.Language, parsed_language)) {
        language_value_ = parsed_language;
    }

    double parsed = 0;
    if (parse_number(settings_value.SfxVolume, parsed)) {
        sfx_volume_ = parsed;
        sfx_volume_text_ = normalize_decimal_text(settings_value.SfxVolume);
        if (sfx_volume_text_.empty()) {
            sfx_volume_text_ = format_volume(sfx_volume_);
        }
    }
    if (parse_number(settings_value.MusicVolume, parsed)) {
        music_volume_ = parsed;
        music_volume_text_ = normalize_decimal_text(settings_value.MusicVolume);
        if (music_volume_text_.empty()) {
            music_volume_text_ = format_volume(music_volume_);
        }
    }

    int integer = 0;
    if (parse_integer(settings_value.PointGoal, integer)) {
        point_goal_ = integer;
    }
    octolith_reset_ = settings_value.PointGoal != "off";
    teams_ = settings_value.TeamPlay != "off";
    radar_players_ = settings_value.HunterRadar != "off";
    if (settings_value.DamageLevel == "low") {
        damage_level_ = 0;
    } else if (settings_value.DamageLevel == "medium") {
        damage_level_ = 1;
    } else if (settings_value.DamageLevel == "high") {
        damage_level_ = 2;
    }
    friendly_fire_ = settings_value.FriendlyFire != "off";
    affinity_weapons_ = settings_value.AffinityWeapons != "off";

    if (settings_value.SaveSlot == "none") {
        SaveSlot = 0;
    } else if (parse_integer(settings_value.SaveSlot, integer)
               && integer >= 0 && integer <= 255) {
        SaveSlot = static_cast<std::uint8_t>(integer);
    }
    SaveFromExit = parse_save_when(settings_value.SaveFromExit,
                                   formats::SaveWhen::Never);
    SaveFromShip = parse_save_when(settings_value.SaveFromShip,
                                   formats::SaveWhen::Prompt);

    const auto planet_values = split_csv(settings_value.Planets);
    std::vector<std::string> planets;
    planets.reserve(planet_values.size());
    for (const auto& value : planet_values) {
        planets.push_back(lower_ascii_impl(value));
    }
    if (!planets.empty()) {
        planets_[0] = std::find(planets.begin(), planets.end(), "ca")
                      != planets.end();
        planets_[1] = std::find(planets.begin(), planets.end(), "alinos")
                      != planets.end();
        planets_[2] = std::find(planets.begin(), planets.end(), "vdo")
                      != planets.end();
        planets_[3] = std::find(planets.begin(), planets.end(), "arcterra")
                      != planets.end();
        planets_[4] = std::find(planets.begin(), planets.end(), "oubliette")
                      != planets.end();
    }

    boss_states_[0] = parse_state(settings_value.Ca1State);
    boss_states_[1] = parse_state(settings_value.Ca2State);
    boss_states_[2] = parse_state(settings_value.Alinos1State);
    boss_states_[3] = parse_state(settings_value.Alinos2State);
    boss_states_[4] = parse_state(settings_value.Vdo1State);
    boss_states_[5] = parse_state(settings_value.Vdo2State);
    boss_states_[6] = parse_state(settings_value.Arcterra1State);
    boss_states_[7] = parse_state(settings_value.Arcterra2State);

    if (settings_value.CheckpointId == "none") {
        checkpoint_id_ = -1;
    } else if (parse_integer(settings_value.CheckpointId, integer)
               && integer >= 0) {
        checkpoint_id_ = integer;
    }
    if (parse_integer(settings_value.HealthMax, integer)) {
        health_max_ = std::max(integer, 1);
    }
    if (parse_integer(settings_value.MissileMax, integer)) {
        missile_max_ = std::max(integer, 0);
    }
    if (parse_integer(settings_value.UaMax, integer)) {
        ua_max_ = std::max(integer, 0);
    }

    const auto weapons = split_csv(settings_value.Weapons);
    if (!weapons.empty()) {
        const auto lower = [&weapons] {
            std::vector<std::string> result;
            result.reserve(weapons.size());
            for (const auto& value : weapons) {
                result.push_back(lower_ascii_impl(value));
            }
            return result;
        }();
        const auto has = [&lower](std::string_view value) {
            return std::find(lower.begin(), lower.end(), value) != lower.end();
        };
        weapons_.fill(0);
        weapons_[0] = has("power beam");
        weapons_[2] = has("missiles");
        weapons_[1] = has("volt driver");
        weapons_[3] = has("battlehammer");
        weapons_[4] = has("imperialist");
        weapons_[5] = has("judicator");
        weapons_[6] = has("magmaul");
        weapons_[7] = has("shock coil");
        weapons_[8] = has("omega cannon");
    }

    const auto octoliths = split_csv(settings_value.Octoliths);
    if (!octoliths.empty()) {
        const auto lower = [&octoliths] {
            std::vector<std::string> result;
            result.reserve(octoliths.size());
            for (const auto& value : octoliths) {
                result.push_back(lower_ascii_impl(value));
            }
            return result;
        }();
        const auto has = [&lower](std::string_view value) {
            return std::find(lower.begin(), lower.end(), value) != lower.end();
        };
        octoliths_.fill(0);
        octoliths_[0] = has("ca1");
        octoliths_[1] = has("ca2");
        octoliths_[2] = has("alinos1");
        octoliths_[3] = has("alinos2");
        octoliths_[4] = has("vdo1");
        octoliths_[5] = has("vdo2");
        octoliths_[6] = has("arcterra1");
        octoliths_[7] = has("arcterra2");
    }
}

void Configuration::load_native(const settings::MenuSettings& settings_value) {
    load_features_json(settings_value.features_json);
    MenuSettings value = from_native(settings_value);
    load(value);
}

std::string Configuration::format_time(double value) {
    if (!std::isfinite(value)) {
        value = 0;
    }
    // Menu.cs passes a float to TimeSpan.FromSeconds.  FromSeconds rounds to
    // the nearest millisecond and the h/m/ss format strings use the
    // TimeSpan components (hours wrap at 24), not a total-hour count.
    const double float_seconds = static_cast<double>(
        static_cast<float>(value));
    const long long milliseconds = std::llround(float_seconds * 1000.0);
    const long long total = milliseconds / 1000;
    const long long hours = (total / 3600) % 24;
    const long long minutes = (total / 60) % 60;
    const long long seconds = total % 60;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    if (hours > 0) {
        output << hours << ':' << std::setw(2) << std::setfill('0')
               << minutes << ':' << std::setw(2) << seconds;
    } else if (minutes > 0) {
        output << minutes << ':' << std::setw(2) << std::setfill('0')
               << seconds;
    } else {
        output << "0:" << std::setw(2) << std::setfill('0') << seconds;
    }
    return output.str();
}

bool Configuration::parse_time(std::string_view input,
                               double& result) noexcept {
    result = 0;
    const std::string value = trim_impl(input);
    std::size_t start = 0;
    int parts[3]{};
    int count = 0;
    while (start <= value.size()) {
        if (count == 3) {
            return false;
        }
        const std::size_t end = value.find(':', start);
        const std::string_view part(value.data() + start,
                                    end == std::string::npos
                                        ? value.size() - start
                                        : end - start);
        if (!parse_integer(part, parts[count])) {
            return false;
        }
        ++count;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    if (count == 1) {
        result = parts[0] * 60.0;
        return true;
    }
    if (count == 2) {
        result = parts[0] * 60.0 + parts[1];
        return true;
    }
    if (count == 3) {
        result = parts[0] * 3600.0 + parts[1] * 60.0 + parts[2];
        return true;
    }
    return false;
}

bool Configuration::read_room(std::string_view input) {
    const std::string value = lower_ascii(trim_impl(input));
    if (value.empty()) {
        return false;
    }
    const metadata::RoomMetadata* found = nullptr;
    int id = -1;
    int parsed_id = 0;
    if (parse_integer(value, parsed_id)) {
        found = metadata::get_room_by_id(parsed_id, true);
        id = parsed_id;
    } else {
        for (const auto& room : metadata::RoomList) {
            if (lower_ascii_impl(room.name) == value) {
                found = &room;
                break;
            }
        }
        if (found == nullptr) {
            const bool multiplayer = mode_ != "Adventure";
            for (const auto& room : metadata::RoomList) {
                if (!room.in_game_name.empty()
                    && lower_ascii_impl(room.in_game_name) == value
                    && room.multiplayer == multiplayer) {
                    found = &room;
                    break;
                }
            }
        }
        if (found == nullptr) {
            for (const auto& room : metadata::RoomList) {
                if (!room.in_game_name.empty()
                    && lower_ascii_impl(room.in_game_name) == value) {
                    found = &room;
                    break;
                }
            }
        }
        if (found != nullptr) {
            id = found->id;
        }
    }
    if (found == nullptr) {
        return false;
    }
    room_id_ = id;
    room_ = found->in_game_name.empty()
        ? std::string(found->name) : std::string(found->in_game_name);
    room_key_ = std::string(found->name);
    fh_room_ = found->first_hunt;
    return true;
}

bool Configuration::read_mode(std::string_view input) {
    const std::string value = lower_ascii_impl(
        trim_impl(input));
    if (value.empty()) {
        return false;
    }
    std::string compact;
    compact.reserve(value.size());
    for (const char character : value) {
        if (character != ' ') {
            compact.push_back(character);
        }
    }
    std::string selected = "auto-select";
    if (compact == "adventure" || compact == "story" || compact == "1p") {
        selected = "Adventure";
    } else if (compact == "battle") {
        selected = "Battle";
    } else if (compact == "battleteams") {
        selected = "Battle Teams";
    } else if (compact == "survival") {
        selected = "Survival";
    } else if (compact == "survivalteams") {
        selected = "Survival Teams";
    } else if (compact == "capture") {
        selected = "Capture";
    } else if (compact == "bounty") {
        selected = "Bounty";
    } else if (compact == "bountyteams") {
        selected = "Bounty Teams";
    } else if (compact == "nodes") {
        selected = "Nodes";
    } else if (compact == "nodesteams") {
        selected = "Nodes Teams";
    } else if (compact == "defender") {
        selected = "Defender";
    } else if (compact == "defenderteams") {
        selected = "Defender Teams";
    } else if (compact == "primehunter") {
        selected = "Prime Hunter";
    } else if (!compact.empty()) {
        mode_ = "auto-select";
        return false;
    }
    mode_ = selected;
    return true;
}

bool Configuration::read_player(std::string_view input, std::size_t index) {
    if (index >= players_.size()) {
        return false;
    }
    const std::string value = lower_ascii(trim_impl(input));
    if (value.empty()) {
        return false;
    }
    // Menu.cs uses Split(' '), not a whitespace tokenizer. In particular,
    // two spaces mean that the optional second field is empty; preserving
    // that detail prevents a malformed persisted value from being silently
    // reinterpreted as a recolor/team number.
    const std::size_t first_space = value.find(' ');
    const std::string name = value.substr(0, first_space);
    const std::size_t second_space = first_space == std::string::npos
        ? std::string::npos : value.find(' ', first_space + 1);
    const std::string second = first_space == std::string::npos
        ? std::string{}
        : value.substr(first_space + 1,
                       second_space == std::string::npos
                           ? std::string::npos
                           : second_space - first_space - 1);
    std::string player = "none";
    std::string normalized_name = name;
    if (!normalized_name.empty()) {
        normalized_name[0] = static_cast<char>(std::toupper(
            static_cast<unsigned char>(normalized_name[0])));
        for (std::size_t character = 1;
             character < normalized_name.size(); ++character) {
            normalized_name[character] = static_cast<char>(std::tolower(
                static_cast<unsigned char>(normalized_name[character])));
        }
        for (const auto candidate : MenuHunters) {
            if (normalized_name == candidate) {
                player = std::string(candidate);
                break;
            }
        }
        if (player == "none") {
            int numeric_hunter = 0;
            if (parse_integer(normalized_name, numeric_hunter)
                && numeric_hunter >= 0 && numeric_hunter < 8) {
                player = std::string(EnumHunters[static_cast<std::size_t>(
                    numeric_hunter)]);
            }
        }
    }
    std::string team = players_[index].Team;
    std::string recolor = players_[index].Recolor;
    if (!second.empty()) {
        if (teams_) {
            const std::string lower = lower_ascii_impl(second);
            if (lower == "orange" || lower == "red") {
                team = "orange";
            } else if (lower == "green") {
                team = "green";
            } else {
                int team_index = 0;
                if (parse_integer(second, team_index)) {
                    team = std::clamp(team_index, 0, 1) == 0
                    ? "orange" : "green";
                }
            }
        } else {
            int value_int = 0;
            if (parse_integer(second, value_int)) {
                recolor = std::to_string(std::clamp(value_int, 0, 5));
            }
        }
    }
    players_[index] = {player, team, recolor};
    player_ids_[index] = -1;
    for (std::size_t hunter = 0; hunter < MenuHunters.size(); ++hunter) {
        if (MenuHunters[hunter] == player) {
            player_ids_[index] = static_cast<int>(hunter);
            break;
        }
    }
    return true;
}

bool Configuration::read_models(std::string_view input) {
    const std::string value = trim_impl(input);
    if (value.empty()) {
        return false;
    }
    models_.clear();
    for (const auto& entry : split_csv_impl(value)) {
        const std::vector<std::string> parts = split_literal_spaces(entry);
        if (parts.empty()) {
            continue;
        }
        const std::string& name = parts.front();
        if (lower_ascii_impl(name) == "none") {
            continue;
        }
        int recolor = 0;
        bool first_hunt = false;
        formats::MetaDir directory = formats::MetaDir::Models;
        for (std::size_t part_index = 1; part_index < parts.size();
             ++part_index) {
            const std::string& part = parts[part_index];
            if (lower_ascii_impl(part) == "fh") {
                first_hunt = true;
            } else {
                int parsed = 0;
                if (parse_integer(part, parsed)) {
                    recolor = parsed;
                } else if (const auto parsed_dir = parse_meta_dir(part)) {
                    // Int32.TryParse(part, out recolor) assigns zero on
                    // failure before Enum.TryParse is attempted in Menu.cs.
                    recolor = 0;
                    directory = *parsed_dir;
                } else {
                    // The out parameter of Enum.TryParse receives the enum's
                    // default value on failure.  MetaDir.Models is zero.
                    recolor = 0;
                    directory = formats::MetaDir::Models;
                }
            }
        }
        // Menu.cs asks Metadata for the model before adding it. The generated
        // table is only the part of that lookup this method needs, but the
        // acceptance and recolor clamp are the same: unknown names are
        // dropped and every model uses its own recolor count.
        const MenuModelMetadata* metadata = find_model_metadata(
            name, first_hunt, directory);
        if (metadata == nullptr) {
            continue;
        }
        recolor = std::clamp(recolor, 0,
                             static_cast<int>(metadata->recolor_count) - 1);
        models_.push_back({std::string(metadata->name), recolor,
                           first_hunt, directory});
    }
    return true;
}

bool Configuration::read_mph_version(std::string_view input) {
    std::string value = trim_impl(input);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    if (!is_known_mph_version(value) || paths_.path(value).empty()) {
        return false;
    }
    paths_.mph_key = value;
    set_default_language();
    return true;
}

bool Configuration::read_fh_version(std::string_view input) {
    std::string value = trim_impl(input);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    if (!is_known_fh_version(value) || paths_.path(value).empty()) {
        return false;
    }
    paths_.fh_key = value;
    set_default_language();
    return true;
}

void Configuration::set_default_language() {
    language_value_ = paths_.is_mph_japan() || paths_.is_mph_korea() ? 1 : 0;
}

bool Configuration::read_time_goal(std::string_view input) {
    const std::string value = trim_impl(input);
    if (value.empty()) {
        return false;
    }
    double parsed_time = 0;
    if (goal_type_ == "Time Goal") {
        if (parse_time(value, parsed_time)) {
            time_goal_ = parsed_time;
            return true;
        }
        return false;
    }
    double parsed = 0;
    if (!parse_number(value, parsed)) {
        return false;
    }
    const int minimum = goal_type_ == "Extra Lives" ? 0 : 1;
    point_goal_ = std::clamp(static_cast<int>(parsed), minimum, 99999);
    return true;
}

bool Configuration::read_time_limit(std::string_view input) {
    const std::string value = trim_impl(input);
    double parsed = 0;
    if (!parse_time(value, parsed)) {
        return false;
    }
    time_limit_ = parsed;
    return true;
}

void Configuration::reset_goal() {
    if (mode_ == "auto-select" || begins(mode_, "Battle")) {
        point_goal_ = 7;
    } else if (begins(mode_, "Survival")) {
        point_goal_ = 2;
    } else if (mode_ == "Capture") {
        point_goal_ = 5;
    } else if (begins(mode_, "Bounty")) {
        point_goal_ = 3;
    } else if (begins(mode_, "Nodes")) {
        point_goal_ = 70;
    } else if (begins(mode_, "Defender") || mode_ == "Prime Hunter") {
        time_goal_ = 90;
    }
}

void Configuration::reset_time_limit() {
    time_limit_ = mode_ == "auto-select" || begins(mode_, "Battle")
        ? 420 : 900;
}

void Configuration::reset_adventure_settings() {
    // ShowStoryModePrompts uses this exact block for its "Reset Adventure
    // Settings" item.  Keep save-slot selection and save policies intact;
    // those are separate menu fields in Menu.cs.
    planets_ = {{1, 0, 0, 0, 0}};
    boss_states_.fill(0);
    checkpoint_id_ = -1;
    health_max_ = 99;
    missile_max_ = 50;
    ua_max_ = 400;
    weapons_ = {{1, 0, 1, 0, 0, 0, 0, 0, 0}};
    octoliths_.fill(0);
    update_settings();
}

void Configuration::reset_match_settings() {
    // This is the exact body behind Menu.cs's reset item.  UpdateSettings
    // then recomputes teams, goal type, goal, and time limit from the current
    // mode rather than treating those values as independent defaults.
    radar_players_ = false;
    damage_level_ = 1;
    friendly_fire_ = false;
    affinity_weapons_ = false;
    update_settings();
}

void Configuration::update_settings() {
    teams_ = false;
    point_goal_ = 0;
    time_goal_ = 0;
    time_limit_ = 0;
    octolith_reset_ = true;
    reset_goal();
    reset_time_limit();
    if (mode_ == "auto-select" || begins(mode_, "Battle")
        || begins(mode_, "Nodes")) {
        goal_type_ = "Point Goal";
    } else if (begins(mode_, "Survival")) {
        goal_type_ = "Extra Lives";
    } else if (mode_ == "Capture" || begins(mode_, "Bounty")) {
        goal_type_ = "Octolith Goal";
    } else if (begins(mode_, "Defender") || mode_ == "Prime Hunter") {
        goal_type_ = "Time Goal";
    }
    if (mode_ == "Capture" || ends(mode_, "Teams")) {
        teams_ = true;
    }
}

game::Mode Configuration::game_mode() const noexcept {
    if (mode_ == "Adventure") return game::Mode::Story;
    if (mode_ == "Battle") return game::Mode::Battle;
    if (mode_ == "Battle Teams") return game::Mode::BattleTeams;
    if (mode_ == "Survival") return game::Mode::Survival;
    if (mode_ == "Survival Teams") return game::Mode::SurvivalTeams;
    if (mode_ == "Capture") return game::Mode::Capture;
    if (mode_ == "Bounty") return game::Mode::Bounty;
    if (mode_ == "Bounty Teams") return game::Mode::BountyTeams;
    if (mode_ == "Nodes") return game::Mode::Nodes;
    if (mode_ == "Nodes Teams") return game::Mode::NodesTeams;
    if (mode_ == "Defender") return game::Mode::Defender;
    if (mode_ == "Defender Teams") return game::Mode::DefenderTeams;
    if (mode_ == "Prime Hunter") return game::Mode::PrimeHunter;
    return game::Mode::None;
}

void Configuration::set_sfx_volume(double value) {
    sfx_volume_ = value;
    sfx_volume_text_ = format_volume(value);
}

void Configuration::set_music_volume(double value) {
    music_volume_ = value;
    music_volume_text_ = format_volume(value);
}

MenuSettings Configuration::commit() const {
    MenuSettings result;
    result.RoomKey = room_key_;
    result.Mode = mode_;
    const auto format_hunter = [this](std::size_t index) {
        const int id = player_ids_[index];
        if (id < 0 || static_cast<std::size_t>(id) >= MenuHunters.size()) {
            return std::string("none");
        }
        return std::string(MenuHunters[static_cast<std::size_t>(id)]) + " "
            + (teams_ ? players_[index].Team : players_[index].Recolor);
    };
    result.Player1 = format_hunter(0);
    result.Player2 = format_hunter(1);
    result.Player3 = format_hunter(2);
    result.Player4 = format_hunter(3);

    std::vector<std::string> model_values;
    for (const auto& model : models_) {
        model_values.push_back(model.Name + " " + std::to_string(model.Recolor));
    }
    result.Models = join(model_values, ", ");
    result.MphVersion = paths_.mph_key;
    result.FhVersion = paths_.fh_key;
    result.Language = language_name(language_value_);
    result.SfxVolume = sfx_volume_text_;
    result.MusicVolume = music_volume_text_;
    result.PointGoal = std::to_string(static_cast<int>(point_goal_));
    result.TimeLimit = format_time(time_limit_);
    result.TimeGoal = format_time(time_goal_);
    result.AutoReset = octolith_reset_ ? "on" : "off";
    result.TeamPlay = teams_ ? "on" : "off";
    result.HunterRadar = radar_players_ ? "on" : "off";
    result.DamageLevel = damage_level_ == 0 ? "low"
        : damage_level_ == 2 ? "high" : "medium";
    result.FriendlyFire = friendly_fire_ ? "on" : "off";
    result.AffinityWeapons = affinity_weapons_ ? "on" : "off";
    result.SaveSlot = SaveSlot == 0 ? "none" : std::to_string(SaveSlot);
    result.SaveFromExit = save_when_name(SaveFromExit);
    result.SaveFromShip = save_when_name(SaveFromShip);
    result.Planets = join_views({{
        planets_[0] ? "CA" : "",
        planets_[1] ? "Alinos" : "",
        planets_[2] ? "VDO" : "",
        planets_[3] ? "Arcterra" : "",
        planets_[4] ? "Oubliette" : ""
    }});
    result.Alinos1State = format_state(boss_states_[2]);
    result.Alinos2State = format_state(boss_states_[3]);
    result.Ca1State = format_state(boss_states_[0]);
    result.Ca2State = format_state(boss_states_[1]);
    result.Vdo1State = format_state(boss_states_[4]);
    result.Vdo2State = format_state(boss_states_[5]);
    result.Arcterra1State = format_state(boss_states_[6]);
    result.Arcterra2State = format_state(boss_states_[7]);
    result.CheckpointId = checkpoint_id_ == -1
        ? "none" : std::to_string(checkpoint_id_);
    result.HealthMax = std::to_string(health_max_);
    result.MissileMax = std::to_string(missile_max_);
    result.UaMax = std::to_string(ua_max_);

    std::vector<std::string> weapon_values;
    const std::array<std::pair<std::size_t, std::string_view>, 9> weapon_names{{
        {0, "Power Beam"}, {2, "Missiles"}, {1, "Volt Driver"},
        {3, "Battlehammer"}, {4, "Imperialist"}, {5, "Judicator"},
        {6, "Magmaul"}, {7, "Shock Coil"}, {8, "Omega Cannon"}
    }};
    for (const auto& [index, name] : weapon_names) {
        if (weapons_[index] != 0) {
            weapon_values.emplace_back(name);
        }
    }
    result.Weapons = join(weapon_values, ",");

    const std::array<std::pair<std::size_t, std::string_view>, 8> octolith_names{{
        {0, "CA1"}, {1, "CA2"}, {2, "Alinos1"}, {3, "Alinos2"},
        {4, "VDO1"}, {5, "VDO2"}, {6, "Arcterra1"}, {7, "Arcterra2"}
    }};
    std::vector<std::string> octolith_values;
    for (const auto& [index, name] : octolith_names) {
        if (octoliths_[index] != 0) {
            octolith_values.emplace_back(name);
        }
    }
    result.Octoliths = join(octolith_values, ",");
    return result;
}

settings::MenuSettings Configuration::native_settings() const {
    return to_native(commit(), features_json(feature_registry_));
}

bool Configuration::save(const std::filesystem::path& directory) const {
    return settings::save_menu(directory, native_settings());
}

void Configuration::set_mode(std::string_view value) {
    static_cast<void>(read_mode(value));
}

void Configuration::set_planet(std::size_t index, int value) noexcept {
    if (index < planets_.size()) {
        planets_[index] = value == 0 ? 0 : 1;
    }
}

void Configuration::set_boss_state(std::size_t index, int value) noexcept {
    if (index < boss_states_.size()) {
        boss_states_[index] = std::clamp(value, 0, 2);
    }
}

void Configuration::set_weapon(std::size_t index, int value) noexcept {
    if (index < weapons_.size()) {
        weapons_[index] = value == 0 ? 0 : 1;
    }
}

void Configuration::set_octolith(std::size_t index, int value) noexcept {
    if (index < octoliths_.size()) {
        octoliths_[index] = value == 0 ? 0 : 1;
    }
}

std::string Configuration::features_json(const features::Registry& registry) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{\"ReticleOpacity\":\"" << std::setprecision(9)
           << registry.features.reticle_opacity << "\",\"ProHud\":\""
           << (registry.features.pro_hud ? "true" : "false") << "\"}";
    return output.str();
}

void Configuration::load_features_json(std::string_view value) {
    const std::string reticle = json_value(value, "ReticleOpacity");
    float parsed = 0.0F;
    if (!reticle.empty() && parse_single_setting(reticle, parsed)) {
        feature_registry_.features.reticle_opacity = parsed;
    }
    const std::string pro_hud = json_value(value, "ProHud");
    const std::string normalized_pro_hud = lower_ascii_impl(pro_hud);
    if (normalized_pro_hud == "true") {
        feature_registry_.features.pro_hud = true;
    } else if (normalized_pro_hud == "false") {
        feature_registry_.features.pro_hud = false;
    }
}

void Configuration::reset_features() {
    // Keep this list in the same order and scope as Menu.ResetFeatures().
    // ProHud and the hidden HUD backing fields are deliberately not touched:
    // the managed reset method predates that launcher-only switch and does
    // not reset it.
    auto& features = feature_registry_.features;
    features.no_repeat_encounters = false;
    features.allow_invalid_teams = true;
    features.top_screen_target_info = true;
    features.helmet_opacity = 1.0F;
    features.visor_opacity = 0.5F;
    features.hud_opacity = 1.0F;
    features.reticle_opacity = 1.0F;
    features.hud_sway = true;
    features.target_info_sway = false;
    features.delayed_idle_sway = true;
    features.no_idle_sway = false;
    features.no_map_centering = false;
    features.max_room_detail = false;
    features.max_player_detail = true;
    features.log_spatial_audio = false;
    features.half_second_alarm = false;
    features.full_boost_charge = false;
    features.boost_opens_doors = false;
    features.alternate_hunters_1p = true;

    auto& cheats = feature_registry_.cheats;
    cheats.free_weapon_select = false;
    cheats.unlimited_jumps = false;
    cheats.no_random_encounters = false;
    cheats.unlock_all_doors = false;
    cheats.continue_from_current_room = false;
    cheats.skip_planet_intros = false;
    cheats.start_with_all_upgrades = false;
    cheats.start_with_all_octoliths = false;
    cheats.walk_through_walls = false;
    cheats.always_fight_gorea2 = false;
    cheats.quadruple_damage = false;

    auto& bugfixes = feature_registry_.bugfixes;
    bugfixes.smooth_cam_seq_handoff = false;
    bugfixes.better_cam_seq_node_ref = true;
    bugfixes.no_stray_respawn_text = false;
    bugfixes.correct_bounty_sfx = true;
    bugfixes.no_double_enemy_death = true;
    bugfixes.no_slench_roll_timer_underflow = true;
}

Configuration::LogbookSummary Configuration::logbook_summary(
    const game::StorySave& save) noexcept {
    const auto category = [](char value) {
        return std::span<const char>(&value, 1);
    };
    return {
        save.scan_count,
        save.max_scan_count(),
        save.logbook_count(false, category('L')),
        save.logbook_count(false, category('B')),
        save.logbook_count(false, category('O')),
        save.logbook_count(false, category('E'))
    };
}

Configuration::LogbookLists Configuration::logbook_entries(
    const game::StorySave& save,
    std::span<const strings::TableEntry> entries) {
    LogbookLists result;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        std::size_t list = 4;
        switch (entry.category) {
        case 'L': list = 0; break;
        case 'B': list = 1; break;
        case 'O': list = 2; break;
        case 'E': list = 3; break;
        default: break;
        }
        if (list == 4) {
            continue;
        }
        if (save.logbook_found(static_cast<std::int32_t>(index))) {
            result[list].push_back({entry.value1, entry.value2});
        } else {
            result[list].push_back({"-", "-"});
        }
    }
    return result;
}

std::array<std::string, 10> Configuration::save_info(
    game::State& state, game::StorySave& save) const {
    // The managed overload receives an explicit save and therefore does not
    // clear CompletedRandomEncounterRooms. It does consume Rng2 while
    // assigning random hunters, then restores that stream immediately after.
    auto& rng = utility::global_rng();
    const std::uint32_t rng2 = rng.rng2();
    scene_setup::update_area_hunters(state, save, rng);
    const auto result = save_info(static_cast<const game::StorySave&>(save));
    rng.set_rng2(rng2);
    return result;
}

std::array<std::string, 10> Configuration::save_info(
    const game::StorySave& save) const {
    std::array<std::string, 10> result{{
        "Alinos   : locked  : ",
        "CA       : locked  : ",
        "VDO      : locked  : ",
        "Arcterra : locked  : ",
        "Oubliette: locked",
        "Artifacts: ",
        "Octoliths: ",
        "Expansion: ",
        "Weapons  : ",
        "Complete : "
    }};
    const auto artifact = [&save](int area, int id) {
        return save.found_artifact(id, area) ? "1" : "0";
    };
    const auto octolith = [&save](int area) {
        return save.found_octolith(area) ? "1" : "0";
    };
    result[0] = "Alinos   : "
        + std::string((save.areas & 1) == 0 ? "locked  " : "unlocked") + ": ";
    result[1] = "CA       : "
        + std::string((save.areas & 0x4) == 0 ? "locked  " : "unlocked") + ": ";
    result[2] = "VDO      : "
        + std::string((save.areas & 0x10) == 0 ? "locked  " : "unlocked") + ": ";
    result[3] = "Arcterra : "
        + std::string((save.areas & 0x40) == 0 ? "locked  " : "unlocked") + ": ";
    result[4] = "Oubliette: "
        + std::string((save.areas & 0x100) == 0 ? "locked" : "unlocked");

    result[5] = std::string("Artifacts: CA ") + artifact(2, 0) + "/" + artifact(2, 1)
        + "/" + artifact(2, 2) + " " + artifact(3, 0) + "/"
        + artifact(3, 1) + "/" + artifact(3, 2)
        + ", Alinos " + artifact(0, 0) + "/" + artifact(0, 1) + "/"
        + artifact(0, 2) + " " + artifact(1, 0) + "/" + artifact(1, 1)
        + "/" + artifact(1, 2)
        + ", VDO " + artifact(4, 0) + "/" + artifact(4, 1) + "/"
        + artifact(4, 2) + " " + artifact(5, 0) + "/" + artifact(5, 1)
        + "/" + artifact(5, 2)
        + ", Arcterra " + artifact(6, 0) + "/" + artifact(6, 1) + "/"
        + artifact(6, 2) + " " + artifact(7, 0) + "/" + artifact(7, 1)
        + "/" + artifact(7, 2);
    result[6] = std::string("Octoliths: CA ") + octolith(2) + "     " + octolith(3)
        + ",     Alinos " + octolith(0) + "     " + octolith(1)
        + ",     VDO " + octolith(4) + "     " + octolith(5)
        + ",     Arcterra " + octolith(6) + "     " + octolith(7);
    result[7] = "Expansion: Health " + std::to_string(save.health_max)
        + ", Missiles " + std::to_string(save.ammo_max[1] / 10)
        + ", UA " + std::to_string(save.ammo_max[0] / 10);
    const auto weapon = [&save, &result](int bit, std::string_view name) {
        if ((save.weapons & (1u << bit)) == 0) {
            return;
        }
        if (!result[8].ends_with(' ')) {
            result[8] += ", ";
        }
        result[8] += name;
    };
    weapon(3, "Battlehammer");
    weapon(5, "Judicator");
    weapon(1, "Volt Driver");
    weapon(6, "Magmaul");
    weapon(7, "Shock Coil");
    weapon(4, "Imperialist");
    weapon(8, "Omega Cannon");
    result[9] = "Complete : "
        + std::to_string(save.completion_percentage()) + "%";

    for (std::size_t area = 0; area < 4; ++area) {
        for (int hunter = 1; hunter < 7; ++hunter) {
            if ((save.area_hunters[area] & (1u << hunter)) == 0) {
                continue;
            }
            if (!result[area].ends_with(' ')) {
                result[area] += ", ";
            }
            const auto name = static_cast<formats::Hunter>(hunter);
            const std::array<std::string_view, 8> names{{
                "Samus", "Kanden", "Trace", "Sylux",
                "Noxus", "Spire", "Weavel", "Guardian"
            }};
            result[area] += names[static_cast<std::size_t>(name)];
            int dropped = 0;
            for (int index = 0; index < 8; ++index) {
                if (((save.lost_octoliths >> (4 * index)) & 15u)
                    == static_cast<unsigned>(hunter)) {
                    ++dropped;
                }
            }
            if (dropped > 0) {
                result[area] += " (x" + std::to_string(dropped) + ")";
            }
        }
    }
    return result;
}

void Configuration::apply_multiplayer_settings(game::State& state) const noexcept {
    if (!apply_settings_) {
        return;
    }
    state.teams = teams_;
    state.point_goal = static_cast<std::uint16_t>(point_goal_);
    state.time_goal = static_cast<float>(time_goal_);
    state.match_time = static_cast<float>(time_limit_);
    state.octolith_reset = octolith_reset_;
    state.radar_players = radar_players_;
    state.damage_level = damage_level_;
    state.friendly_fire = friendly_fire_;
    state.affinity_weapons = affinity_weapons_;
}

void Configuration::apply_adventure_settings(game::State& state) const noexcept {
    if (!apply_settings_ || SaveSlot != 0) {
        return;
    }
    int areas = (planets_[0] == 0 ? 0 : 0xC)
        | (planets_[1] == 0 ? 0 : 0x3)
        | (planets_[2] == 0 ? 0 : 0x30)
        | (planets_[3] == 0 ? 0 : 0xC0)
        | (planets_[4] == 0 ? 0 : 0x100);
    state.story_save.areas = static_cast<std::uint16_t>(areas);

    const auto flag_for = [](int state_value, game::BossFlags kill,
                             game::BossFlags done) {
        switch (std::clamp(state_value, 0, 2)) {
        case 1: return static_cast<std::uint32_t>(kill);
        case 2: return static_cast<std::uint32_t>(done);
        default: return std::uint32_t{0};
        }
    };
    const std::array<std::pair<game::BossFlags, game::BossFlags>, 8> flags{{
        {game::BossFlags::Unit2B1Kill, game::BossFlags::Unit2B1Done},
        {game::BossFlags::Unit2B2Kill, game::BossFlags::Unit2B2Done},
        {game::BossFlags::Unit1B1Kill, game::BossFlags::Unit1B1Done},
        {game::BossFlags::Unit1B2Kill, game::BossFlags::Unit1B2Done},
        {game::BossFlags::Unit3B1Kill, game::BossFlags::Unit3B1Done},
        {game::BossFlags::Unit3B2Kill, game::BossFlags::Unit3B2Done},
        {game::BossFlags::Unit4B1Kill, game::BossFlags::Unit4B1Done},
        {game::BossFlags::Unit4B2Kill, game::BossFlags::Unit4B2Done}
    }};
    std::uint32_t boss_flags = 0;
    for (std::size_t index = 0; index < flags.size(); ++index) {
        boss_flags |= flag_for(boss_states_[index], flags[index].first,
                               flags[index].second);
    }
    state.story_save.boss_flags = boss_flags;
    state.story_save.checkpoint_entity_id = checkpoint_id_;
    state.story_save.health_max = health_max_;
    state.story_save.health = health_max_;
    state.story_save.ammo_max[1] = missile_max_;
    state.story_save.ammo[1] = missile_max_;
    state.story_save.ammo_max[0] = ua_max_;
    state.story_save.ammo[0] = ua_max_;
    std::uint16_t weapons = 0;
    for (std::size_t index = 0; index < weapons_.size(); ++index) {
        if (weapons_[index] != 0) {
            weapons |= static_cast<std::uint16_t>(1u << index);
        }
    }
    state.story_save.weapons = weapons;
    constexpr std::array<int, 8> octolith_bits{{2, 3, 0, 1, 4, 5, 6, 7}};
    std::uint16_t octoliths = 0;
    for (std::size_t index = 0; index < octoliths_.size(); ++index) {
        if (octoliths_[index] != 0) {
            octoliths |= static_cast<std::uint16_t>(
                1u << octolith_bits[index]);
        }
    }
    state.story_save.current_octoliths = octoliths;
    state.story_save.found_octoliths = octoliths;
}

Model::Model() {
    rebuild_items();
}

void Model::open(Page page) {
    page_ = page;
    selected_ = 0;
    rebuild_items();
}

void Model::back() {
    if (page_ != Page::Home) {
        open(Page::Home);
    }
}

void Model::move_selection(int direction) noexcept {
    if (items_.empty() || direction == 0) {
        return;
    }
    const auto count = items_.size();
    const auto current = static_cast<long long>(selected_);
    const auto next = (current + direction % static_cast<int>(count)
                       + static_cast<long long>(count))
        % static_cast<long long>(count);
    selected_ = static_cast<std::size_t>(next);
}

Action Model::activate() const noexcept {
    return items_.empty() || !items_[selected_].enabled
        ? Action::None
        : items_[selected_].action;
}

void Model::rebuild_items() {
    items_.clear();
    switch (page_) {
    case Page::Home:
        items_ = {{"host", Action::Host, game_files_ready_},
                  {"join", Action::Join, game_files_ready_},
                  {"settings", Action::OpenSettings, true},
                  {"game_files", Action::OpenGameFiles, true},
                  {"demos", Action::OpenDemos, game_files_ready_},
                  {"quit", Action::Quit, true}};
        break;
    case Page::GameFiles:
        items_ = {{"back", Action::Back, true}};
        break;
    case Page::Settings:
        items_ = {{"back", Action::Back, true}};
        break;
    case Page::Join:
        items_ = {{"back", Action::Back, true}};
        break;
    case Page::Demos:
        items_ = {{"back", Action::Back, true}};
        break;
    }
}

Configuration& current() noexcept {
    static Configuration value;
    return value;
}

void ApplyMultiplayerSettings(game::State& state) noexcept {
    current().apply_multiplayer_settings(state);
}

void ApplyAdventureSettings(game::State& state) noexcept {
    current().apply_adventure_settings(state);
}

void ResetFeatures() {
    current().reset_features();
}

std::string format_time(double seconds) {
    return Configuration::format_time(seconds);
}

bool parse_time(std::string_view input, double& seconds) noexcept {
    return Configuration::parse_time(input, seconds);
}

void print_sound_info(SoundCapability capability, std::ostream& output) {
    if (capability == SoundCapability::None) {
        output << "\nWARNING: Audio system could not be loaded. "
                  "Sound effects will not be played.\n"
                  "You may need to install OpenAL Soft on your system.\n"
                  "Music and video playback will not be affected.\n";
    } else if (capability == SoundCapability::Unsupported) {
        output << "\nWARNING: Audio system was loaded, but an unsupported "
                  "version of OpenAL was used.\n"
                  "You may need to install OpenAL Soft on your system for "
                  "sounds to play correctly.\n"
                  "Music and video playback will not be affected.\n";
    }
}

void show_menu_prompts(const std::filesystem::path& directory,
                       std::istream& input, std::ostream& output) {
    Configuration configuration(directory);
    configuration.load_native(settings::load_menu(directory));
    configuration.update_settings();

    output << branding::name_and_version()
           << " native Menu.cs compatibility console\n";
    output << "Commands: room mode player1 player2 player3 player4 models "
               "mph fh match story features reset launch quit\n";
    std::string line;
    while (output && std::getline(input, line)) {
        const std::string command = lower_ascii_impl(trim_impl(line));
        if (command == "quit" || command == "q" || command == "escape") {
            return;
        }
        if (command == "launch" || command == "l" || command.empty()) {
            configuration.set_apply_settings(true);
            static_cast<void>(configuration.save(directory));
            output << "Loading...\n";
            return;
        }
        if (command == "room") {
            std::getline(input, line);
            static_cast<void>(configuration.read_room(line));
        } else if (command == "mode") {
            std::getline(input, line);
            if (configuration.read_mode(line)) {
                configuration.update_settings();
            }
        } else if (begins(command, "player")) {
            int index = 0;
            if (parse_integer(command.substr(6), index) && index >= 1
                && index <= 4) {
                std::getline(input, line);
                static_cast<void>(configuration.read_player(line,
                                                            static_cast<std::size_t>(index - 1)));
            }
        } else if (command == "models") {
            std::getline(input, line);
            static_cast<void>(configuration.read_models(line));
        } else if (command == "mph") {
            std::getline(input, line);
            static_cast<void>(configuration.read_mph_version(line));
        } else if (command == "fh") {
            std::getline(input, line);
            static_cast<void>(configuration.read_fh_version(line));
        } else if (command == "match") {
            output << "mode: ";
            std::getline(input, line);
            if (configuration.read_mode(line)) {
                configuration.update_settings();
            }
            output << "point/time goal: ";
            std::getline(input, line);
            static_cast<void>(configuration.read_time_goal(line));
            output << "time limit: ";
            std::getline(input, line);
            static_cast<void>(configuration.read_time_limit(line));
            output << "teams (on/off): ";
            std::getline(input, line);
            configuration.set_teams(lower_ascii_impl(trim_impl(line)) == "on");
        } else if (command == "story") {
            output << "save slot (0-255): ";
            std::getline(input, line);
            int slot = 0;
            if (parse_integer(trim_impl(line), slot) && slot >= 0 && slot <= 255) {
                configuration.SaveSlot = static_cast<std::uint8_t>(slot);
            }
            output << "checkpoint id (-1 for none): ";
            std::getline(input, line);
            if (parse_integer(trim_impl(line), slot)) {
                configuration.set_checkpoint_id(slot);
            }
        } else if (command == "reset") {
            configuration.reset_features();
            configuration.load(MenuSettings{});
            configuration.update_settings();
        } else if (command == "features") {
            output << "Features are edited through the native feature registry; "
                      "enter reset to restore Menu.cs defaults.\n";
        }
        output << "menu> ";
    }
}

void ShowMenuPrompts() {
    show_menu_prompts(std::filesystem::current_path(), std::cin, std::cout);
}

} // namespace fruityprime::menu
