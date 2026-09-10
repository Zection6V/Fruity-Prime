#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// These helpers belong to the still-porting native utility adapter, not to
// Mods/ModEntry.cs. Keeping them here prevents C# private dispatcher helpers
// from becoming part of the ModEntry public surface.
namespace fruityprime::utility::command_line {

[[nodiscard]] bool has_flag(int argc, char** argv,
                            std::string_view name) noexcept;
[[nodiscard]] std::string value_after(int argc, char** argv,
                                      std::string_view name);
[[nodiscard]] int index_of_flag(int argc, char** argv,
                                std::string_view name) noexcept;
[[nodiscard]] std::vector<std::string> values_after(
    int argc, char** argv, std::string_view name);
[[nodiscard]] bool parse_port_range(std::string_view value, int& first,
                                    int& last) noexcept;
[[nodiscard]] int integer_after(int argc, char** argv, std::string_view name,
                                int fallback) noexcept;
[[nodiscard]] std::uint8_t hunter_after(
    int argc, char** argv, std::string_view name,
    std::uint8_t fallback = 0) noexcept;
[[nodiscard]] std::optional<double> seconds_value(
    std::string_view value) noexcept;

struct SpectateArguments {
    double spectate_at_seconds = -1.0;
    double rejoin_at_seconds = -1.0;
};

[[nodiscard]] SpectateArguments spectate_arguments(
    int argc, char** argv) noexcept;

} // namespace fruityprime::utility::command_line
