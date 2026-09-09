#include "Program.hpp"
#include "Program.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>

namespace {

[[nodiscard]] bool is_space(char value) noexcept {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && is_space(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_space(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

// Int32.TryParse(string, out int) uses the Integer number style: optional
// surrounding whitespace and sign, followed by decimal digits, with no
// partial success or overflow.
[[nodiscard]] bool try_parse_int32(std::string_view text, int& result) noexcept {
    text = trim(text);
    if (text.empty()) {
        return false;
    }

    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) {
        return false;
    }

    unsigned int magnitude = 0;
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), magnitude, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return false;
    }

    constexpr unsigned int max_positive =
        static_cast<unsigned int>(std::numeric_limits<int>::max());
    constexpr unsigned int max_negative = max_positive + 1U;
    if ((!negative && magnitude > max_positive)
        || (negative && magnitude > max_negative)) {
        return false;
    }
    if (negative) {
        result = magnitude == max_negative
            ? std::numeric_limits<int>::min()
            : -static_cast<int>(magnitude);
    } else {
        result = static_cast<int>(magnitude);
    }
    return true;
}

} // namespace

namespace MphReadNative::Program {

std::optional<Version> parse_version(std::string_view value) {
    value = trim(value);
    if (value.empty()) return std::nullopt;
    Version result;
    std::size_t part = 0;
    while (!value.empty() && part < result.parts.size()) {
        const std::size_t dot = value.find('.');
        const std::string_view number = value.substr(0, dot);
        int parsed = 0;
        if (!try_parse_int32(number, parsed) || parsed < 0) {
            return std::nullopt;
        }
        result.parts[part++] = parsed;
        if (dot == std::string_view::npos) {
            value = {};
            break;
        }
        value.remove_prefix(dot + 1);
        if (value.empty()) return std::nullopt;
    }
    return value.empty() && part >= 2 && part <= result.parts.size()
        ? std::optional<Version>(result) : std::nullopt;
}

bool check_version(std::string_view value) noexcept {
    try {
        const auto parsed = parse_version(value);
        return parsed.has_value() && !(*parsed < MinimumExtractVersion);
    } catch (...) {
        return false;
    }
}

bool check_version_file(const std::filesystem::path& path) noexcept {
    try {
        std::ifstream input(path);
        std::string line;
        return static_cast<bool>(std::getline(input, line))
            && check_version(line);
    } catch (...) {
        return false;
    }
}

ArgumentList parse_arguments(std::span<const std::string> args) {
    ArgumentList arguments;
    arguments.reserve(args.size());
    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string& original = args[index];
        if (original.size() <= 1 || original.front() != '-') {
            continue;
        }

        Argument argument;
        argument.Name = original.substr(1);
        if (index + 1 >= args.size()) {
            arguments.push_back(std::move(argument));
            continue;
        }

        const std::string& value_one = args[index + 1];
        if (!value_one.empty() && value_one.front() == '-') {
            arguments.push_back(std::move(argument));
            continue;
        }

        argument.ValueOne = value_one;
        if (index + 2 < args.size()) {
            const std::string& value_two = args[index + 2];
            if (value_two.empty() || value_two.front() != '-') {
                argument.ValueTwo = value_two;
                ++index;
            }
        }
        arguments.push_back(std::move(argument));
        ++index;
    }
    return arguments;
}

const Argument* try_get_argument(std::span<const Argument> arguments,
                                  std::string_view full_name,
                                  std::string_view short_name) noexcept {
    for (const Argument& argument : arguments) {
        if (argument.Name == full_name || argument.Name == short_name) {
            return &argument;
        }
    }
    return nullptr;
}

std::optional<std::string> try_get_string(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name) {
    const Argument* argument =
        try_get_argument(arguments, full_name, short_name);
    if (argument == nullptr || !argument->ValueOne.has_value()) {
        return std::nullopt;
    }
    return argument->ValueOne;
}

std::optional<int> try_get_int(std::span<const Argument> arguments,
                               std::string_view full_name,
                               std::string_view short_name) noexcept {
    const auto string_value =
        try_get_string(arguments, full_name, short_name);
    if (!string_value.has_value()) {
        return std::nullopt;
    }
    int value = 0;
    if (!try_parse_int32(*string_value, value)) {
        return std::nullopt;
    }
    return value;
}

std::vector<std::pair<std::string, int>> get_pairs(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name) noexcept {
    std::vector<std::pair<std::string, int>> pairs;
    for (const Argument& argument : arguments) {
        if ((argument.Name != full_name && argument.Name != short_name)
            || !argument.ValueOne.has_value()) {
            continue;
        }
        int value_two = 0;
        if (argument.ValueTwo.has_value()) {
            (void)try_parse_int32(*argument.ValueTwo, value_two);
        }
        pairs.emplace_back(*argument.ValueOne, value_two);
    }
    return pairs;
}

} // namespace MphReadNative::Program

namespace fruityprime::program {

Arguments::Arguments(int argc, char** argv) {
    if (argc < 0 || argv == nullptr) {
        return;
    }
    values_.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        values_.emplace_back(argv[index] == nullptr ? "" : argv[index]);
    }
}

bool Arguments::has(std::string_view flag) const noexcept {
    return std::any_of(values_.begin(), values_.end(), [flag](const auto& value) {
        return value == flag;
    });
}

std::string Arguments::value_after(std::string_view flag) const {
    for (std::size_t index = 0; index + 1 < values_.size(); ++index) {
        if (values_[index] == flag) {
            return values_[index + 1];
        }
    }
    return {};
}

Action Arguments::action() const noexcept {
    if (has("-launcher")) {
        return Action::Launcher;
    }
    if (has("-server")) {
        return Action::Server;
    }
    if (has("-masterserver")) {
        return Action::MasterServer;
    }
    if (has("-netcheck")) {
        return Action::NetCheck;
    }
    if (has("-connect")) {
        return Action::Connect;
    }
    if (has("-q3convert")) {
        return Action::Q3Convert;
    }
    if (has("-mapgen")) {
        return Action::MapGen;
    }
    if (has("-model-export-obj") || has("-model-export-collada")
        || has("-model-export-textures")) {
        return Action::Export;
    }
    if (has("-soundinfo")) {
        return Action::SoundInfo;
    }
    if (has("-movieinfo") || has("-movieexport")
        || (has("-export") && value_after("-export") == "movie")) {
        return Action::MovieInfo;
    }
    if (has("-rooms")) {
        return Action::Rooms;
    }
    if (has("-help") || has("--help") || has("-h")) {
        return Action::Help;
    }
    return Action::Default;
}

} // namespace fruityprime::program
