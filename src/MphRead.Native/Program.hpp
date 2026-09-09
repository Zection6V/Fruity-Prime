#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphReadNative {

// The managed ProgramException is the only public exception type declared by
// Program.cs.  Keep its native counterpart at the same source boundary; the
// entry adapters may translate it to their platform's exit path.
class ProgramException : public std::runtime_error {
public:
    explicit ProgramException(const std::string& message)
        : std::runtime_error(message) {}
};

// The managed Program.Argument is private to Program.cs.  Keep the native
// counterpart next to its source pair.  The helper surface is under `detail`
// because ParseArguments/TryGetArgument/TryGetString/TryGetInt/GetPairs and
// CheckVersion are private implementation details in the C# source.  Tests
// use this namespace as a native oracle seam; it is not a second CLI API.
namespace Program {

struct Version {
    std::array<int, 4> parts{{0, 0, -1, -1}};
    friend bool operator==(const Version&, const Version&) = default;
    friend bool operator<(const Version& left, const Version& right) noexcept {
        return left.parts < right.parts;
    }
};

inline constexpr Version CurrentVersion{{0, 35, 1, 0}};

namespace detail {

[[nodiscard]] std::optional<Version> parse_version(std::string_view value);
[[nodiscard]] bool check_version(std::string_view value) noexcept;
[[nodiscard]] bool check_version_file(
    const std::filesystem::path& path) noexcept;

struct Argument {
    std::string Name;
    std::optional<std::string> ValueOne;
    std::optional<std::string> ValueTwo;
};

using ArgumentList = std::vector<Argument>;

[[nodiscard]] ArgumentList parse_arguments(
    std::span<const std::string> args);

[[nodiscard]] const Argument* try_get_argument(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name) noexcept;

[[nodiscard]] std::optional<std::string> try_get_string(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name);

[[nodiscard]] std::optional<int> try_get_int(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name) noexcept;

[[nodiscard]] std::vector<std::pair<std::string, int>> get_pairs(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name) noexcept;

} // namespace detail

} // namespace Program
}
