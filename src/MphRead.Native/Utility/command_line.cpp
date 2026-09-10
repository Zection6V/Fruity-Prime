#include "Utility/command_line.hpp"

#include "Metadata/metadata.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace fruityprime::utility::command_line {
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
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index]))
            != std::tolower(static_cast<unsigned char>(right[index]))) {
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
    for (int index = 1; index < argc; ++index) {
        if (argv[index] != nullptr && equal_name(argv[index], name)) {
            return true;
        }
    }
    return false;
}

int index_of_flag(int argc, char** argv, std::string_view name) noexcept {
    if (argv == nullptr || argc <= 1) {
        return -1;
    }
    for (int index = 1; index < argc; ++index) {
        if (argv[index] != nullptr && equal_name(argv[index], name)) {
            return index;
        }
    }
    return -1;
}

std::string value_after(int argc, char** argv, std::string_view name) {
    if (argv == nullptr || argc <= 2) {
        return {};
    }
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] != nullptr && equal_name(argv[index], name)
            && argv[index + 1] != nullptr) {
            return argv[index + 1];
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
    for (int index = 1; index + 1 < argc; ++index) {
        if (argv[index] != nullptr && equal_name(argv[index], name)
            && argv[index + 1] != nullptr) {
            result.emplace_back(argv[index + 1]);
        }
    }
    return result;
}

bool parse_port_range(std::string_view value, int& first, int& last) noexcept {
    // This is the Split('-', 2) shape used by ModEntry.cs. In particular,
    // there is no artificial 65535 limit at this parsing boundary.
    const std::size_t dash = value.find('-');
    if (dash == std::string_view::npos || dash == 0
        || dash + 1 >= value.size()) {
        return false;
    }
    const auto parse = [](std::string_view part, int& output) noexcept {
        while (!part.empty()
               && std::isspace(static_cast<unsigned char>(part.front())) != 0) {
            part.remove_prefix(1);
        }
        while (!part.empty()
               && std::isspace(static_cast<unsigned char>(part.back())) != 0) {
            part.remove_suffix(1);
        }
        if (part.empty()) {
            return false;
        }
        bool negative = false;
        if (part.front() == '+' || part.front() == '-') {
            negative = part.front() == '-';
            part.remove_prefix(1);
        }
        if (part.empty()) {
            return false;
        }
        std::uint64_t magnitude = 0;
        constexpr std::uint64_t max_positive =
            static_cast<std::uint64_t>(std::numeric_limits<int>::max());
        constexpr std::uint64_t max_negative = max_positive + 1;
        for (const char character : part) {
            if (character < '0' || character > '9') {
                return false;
            }
            magnitude = magnitude * 10U
                + static_cast<std::uint64_t>(character - '0');
            if (magnitude > (negative ? max_negative : max_positive)) {
                return false;
            }
        }
        output = negative
            ? magnitude == max_negative
                ? std::numeric_limits<int>::min()
                : -static_cast<int>(magnitude)
            : static_cast<int>(magnitude);
        return true;
    };
    int parsed_first = 0;
    int parsed_last = 0;
    if (!parse(value.substr(0, dash), parsed_first)
        || !parse(value.substr(dash + 1), parsed_last)
        || parsed_first <= 0 || parsed_last < parsed_first) {
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
        || parsed < static_cast<long>(std::numeric_limits<int>::min())
        || parsed > static_cast<long>(std::numeric_limits<int>::max())) {
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

} // namespace fruityprime::utility::command_line
