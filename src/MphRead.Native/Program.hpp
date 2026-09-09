#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::program {

enum class Action {
    Default,
    Launcher,
    Server,
    MasterServer,
    Connect,
    NetCheck,
    MapGen,
    Q3Convert,
    Export,
    SoundInfo,
    MovieInfo,
    Rooms,
    Help
};

class Arguments {
public:
    Arguments() = default;
    Arguments(int argc, char** argv);
    explicit Arguments(std::vector<std::string> values)
        : values_(std::move(values)) {}

    [[nodiscard]] bool has(std::string_view flag) const noexcept;
    [[nodiscard]] std::string value_after(std::string_view flag) const;
    [[nodiscard]] Action action() const noexcept;
    [[nodiscard]] const std::vector<std::string>& values() const noexcept {
        return values_;
    }

private:
    std::vector<std::string> values_;
};

} // namespace fruityprime::program

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphReadNative {
using ProgramArguments = ::fruityprime::program::Arguments;

// The managed Program.Argument is private to Program.cs.  Keep the native
// counterpart next to its source pair and expose only the operations that the
// managed implementation uses.  In particular, an argument consumes at most
// two following non-option tokens; a token beginning with '-' is always the
// next option, even when it is just '-'.
namespace Program {

using Action = ::fruityprime::program::Action;

struct Version {
    std::array<int, 4> parts{{0, 0, -1, -1}};
    friend bool operator==(const Version&, const Version&) = default;
    friend bool operator<(const Version& left, const Version& right) noexcept {
        return left.parts < right.parts;
    }
};

inline constexpr Version CurrentVersion{{0, 35, 1, 0}};
inline constexpr Version MinimumExtractVersion{{0, 19, 0, 0}};

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

} // namespace Program
}
