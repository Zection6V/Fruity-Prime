#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// How ModEntry finds a flag on the raw command line. Nothing here parses a
// value: each command reads the token it wants and decides for itself what a
// bad one means, which is why the rules below are looser than a CLI parser's.
//
// - A flag matches with any number of leading hyphens, or none ("-fps",
//   "--FPS" and "fps" are one flag), ignoring case.
// - A repeated flag answers with its *first* occurrence; ValuesAfter is the
//   one lookup that sees them all, in order.
// - ValueAfter returns the next token whatever it is, another flag included.
//   Refusing "-name -server" is the caller's call, not this file's.
// - "--" is not an end of options.
namespace MphRead::Mods::Entry
{
    [[nodiscard]] std::string_view TrimStartHyphen(std::string_view value) noexcept;
    [[nodiscard]] bool IsFlag(std::string_view argument, std::string_view name) noexcept;
    [[nodiscard]] bool HasFlag(const std::vector<std::string>& args, std::string_view name) noexcept;
    // The index of the first occurrence, or -1.
    [[nodiscard]] int IndexOfFlag(const std::vector<std::string>& args, std::string_view name) noexcept;
    [[nodiscard]] std::optional<std::string> ValueAfter(
        const std::vector<std::string>& args, std::string_view name);
    [[nodiscard]] std::vector<std::string> ValuesAfter(
        const std::vector<std::string>& args, std::string_view name);
    [[nodiscard]] bool StartsWithHyphen(const std::optional<std::string>& value) noexcept;
}
