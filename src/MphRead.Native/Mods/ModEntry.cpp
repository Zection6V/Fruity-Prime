#include "Mods/mod_entry.hpp"

#include "Metadata/metadata.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace fruityprime::mods {
namespace {

[[nodiscard]] std::string_view without_dashes(std::string_view value) noexcept {
    while (!value.empty() && value.front() == '-') {
        value.remove_prefix(1);
    }
    return value;
}

[[nodiscard]] bool equal_name(std::string_view left,
                              std::string_view right) noexcept {
    left = without_dashes(left);
    right = without_dashes(right);
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto lhs = static_cast<unsigned char>(left[i]);
        const auto rhs = static_cast<unsigned char>(right[i]);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool looks_like_option(std::string_view value) noexcept {
    return value.size() >= 2 && value.front() == '-'
        && std::isalpha(static_cast<unsigned char>(value[1])) != 0;
}

} // namespace

bool has_flag(int argc, char** argv, std::string_view name) noexcept {
    if (argv == nullptr || argc <= 1) {
        return false;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && equal_name(argv[i], name)) {
            return true;
        }
    }
    return false;
}

int index_of_flag(int argc, char** argv, std::string_view name) noexcept {
    if (argv == nullptr || argc <= 1) {
        return -1;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && equal_name(argv[i], name)) {
            return i;
        }
    }
    return -1;
}

std::string value_after(int argc, char** argv, std::string_view name) {
    if (argv == nullptr || argc <= 2) {
        return {};
    }
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] != nullptr && equal_name(argv[i], name)
            && argv[i + 1] != nullptr) {
            return argv[i + 1];
        }
    }
    return {};
}

std::vector<std::string> values_after(
    int argc, char** argv, std::string_view name) {
    std::vector<std::string> result;
    if (argv == nullptr || argc <= 2) {
        return result;
    }
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] != nullptr && equal_name(argv[i], name)
            && argv[i + 1] != nullptr) {
            result.emplace_back(argv[i + 1]);
        }
    }
    return result;
}

bool parse_port_range(std::string_view value, int& first, int& last) noexcept {
    const std::size_t dash = value.find('-');
    if (dash == std::string_view::npos || dash == 0
        || dash + 1 >= value.size()) {
        return false;
    }
    const auto parse = [](std::string_view part, int& output) noexcept {
        const std::string owned(part);
        char* end = nullptr;
        const long parsed = std::strtol(owned.c_str(), &end, 10);
        if (end == owned.c_str() || *end != '\0'
            || parsed < 1 || parsed > 65'535
            || parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        output = static_cast<int>(parsed);
        return true;
    };
    int parsed_first = 0;
    int parsed_last = 0;
    if (!parse(value.substr(0, dash), parsed_first)
        || !parse(value.substr(dash + 1), parsed_last)
        || parsed_last < parsed_first) {
        return false;
    }
    first = parsed_first;
    last = parsed_last;
    return true;
}

int integer_after(int argc, char** argv, std::string_view name,
                  int fallback) noexcept {
    const std::string value = value_after(argc, argv, name);
    if (value.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0'
        || parsed < static_cast<long>(-2'147'483'648)
        || parsed > static_cast<long>(2'147'483'647)) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

std::uint8_t hunter_after(int argc, char** argv, std::string_view name,
                          std::uint8_t fallback) noexcept {
    const std::string value = value_after(argc, argv, name);
    if (value.empty()) {
        return fallback;
    }
    return metadata::parse_hunter(value).value_or(fallback);
}

std::optional<double> seconds_value(std::string_view value) noexcept {
    if (value.empty() || looks_like_option(value)) {
        return std::nullopt;
    }
    const std::string owned(value);
    char* end = nullptr;
    const double parsed = std::strtod(owned.c_str(), &end);
    if (end == owned.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

SpectateArguments spectate_arguments(int argc, char** argv) noexcept {
    SpectateArguments result;
    if (has_flag(argc, argv, "-spectate")) {
        // The managed command accepts the flag without a value as "now".
        result.spectate_at_seconds = 0.0;
        if (const auto parsed = seconds_value(
                value_after(argc, argv, "-spectate")); parsed.has_value()) {
            result.spectate_at_seconds = *parsed;
        }
    }
    if (has_flag(argc, argv, "-rejoin")) {
        if (const auto parsed = seconds_value(
                value_after(argc, argv, "-rejoin")); parsed.has_value()) {
            result.rejoin_at_seconds = *parsed;
        }
    }
    return result;
}

} // namespace fruityprime::mods
