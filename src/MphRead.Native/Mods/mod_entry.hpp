#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mods {

// Shared command-line parsing owned by the Mods/ModEntry boundary. Keeping
// this outside the executable entry point gives the native headless commands
// the same case-insensitive, dash-tolerant matching as the managed parser.
[[nodiscard]] bool has_flag(int argc, char** argv,
                            std::string_view name) noexcept;

[[nodiscard]] std::string value_after(int argc, char** argv,
                                      std::string_view name);

// Positional option helpers used by the command dispatcher. They mirror
// ModEntry.IndexOfFlag and ModEntry.ValuesAfter, including case-insensitive
// and dash-tolerant option names.
[[nodiscard]] int index_of_flag(int argc, char** argv,
                                std::string_view name) noexcept;

[[nodiscard]] std::vector<std::string> values_after(
    int argc, char** argv, std::string_view name);

// Parse the inclusive A-B form used by -hostports without letting malformed
// input reach a uint16_t cast.
[[nodiscard]] bool parse_port_range(std::string_view value, int& first,
                                    int& last) noexcept;

[[nodiscard]] int integer_after(int argc, char** argv, std::string_view name,
                                int fallback) noexcept;

// Resolve the managed ParseHunter form (case-insensitive name or numeric
// enum value) for command paths that announce a hunter to the network.
// Invalid and absent values use the same Samus fallback as ModEntry.
[[nodiscard]] std::uint8_t hunter_after(
    int argc, char** argv, std::string_view name,
    std::uint8_t fallback = 0) noexcept;

// A value beginning with another option is not an optional numeric argument.
// This is the distinction used by -spectate, whose value may be omitted.
[[nodiscard]] std::optional<double> seconds_value(
    std::string_view value) noexcept;

struct SpectateArguments {
    double spectate_at_seconds = -1.0;
    double rejoin_at_seconds = -1.0;
};

[[nodiscard]] SpectateArguments spectate_arguments(int argc,
                                                   char** argv) noexcept;

} // namespace fruityprime::mods
