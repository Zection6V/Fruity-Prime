#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead
{
    enum class BeamType : std::int8_t;
    enum class GameMode : std::uint8_t;
    enum class Hunter : std::uint8_t;
}

// Typed values read off the command line. These rules are deliberately not
// one rule: each command keeps the parse it was ported with, and merging them
// would change what some command accepts.
//
// - The *Invariant readers use the invariant culture with NumberStyles.Float,
//   plus AllowThousands unless told otherwise. Given no text at all they set
//   the value to 0 and return false.
// - TryParseDoubleCurrent uses the current culture (-gamepad -seconds).
// - The Parse* readers below fall back to a default when the flag is absent
//   or its value does not parse; ParseName falls back to this machine's name,
//   which asks the operating system.
namespace MphRead::Mods::Entry
{
    [[nodiscard]] bool TryParseDoubleInvariant(
        const std::optional<std::string>& text, double& value, bool allowThousands = true);
    [[nodiscard]] bool TryParseFloatInvariant(
        const std::optional<std::string>& text, float& value, bool allowThousands = true);
    [[nodiscard]] bool TryParseDoubleCurrent(std::string_view text, double& value);
    // Enum names, case ignored. False (value untouched) when text is absent
    // or names nothing.
    [[nodiscard]] bool TryParseGameMode(const std::optional<std::string>& text, GameMode& value);
    [[nodiscard]] bool TryParseHunter(const std::optional<std::string>& text, Hunter& value);
    [[nodiscard]] bool TryParseBeam(const std::optional<std::string>& text, BeamType& value);

    // The computer's name (GetComputerNameW / gethostname); throws when the
    // system will not say.
    [[nodiscard]] std::string MachineName();

    // -port, current culture, else NetConfig::DefaultPort.
    [[nodiscard]] int ParsePort(const std::vector<std::string>& args);
    // -masterport, current culture, else NetMasterConfig::DefaultPort.
    [[nodiscard]] int ParseMasterPort(const std::vector<std::string>& args);
    // -name, else MachineName().
    [[nodiscard]] std::string ParseName(const std::vector<std::string>& args);
    // -hunter, else Samus.
    [[nodiscard]] Hunter ParseHunter(const std::vector<std::string>& args);
    // -recolor, current culture, else 0.
    [[nodiscard]] int ParseRecolor(const std::vector<std::string>& args);
}
