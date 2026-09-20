#pragma once

#include "Mods/Update/BuildVersion.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead
{
    class Program final
    {
    public:
        // Native entry seam for executable adapters. The C# entry point itself
        // is private; native startup has no CLR entry-point metadata.
        static void Main(const std::vector<std::string>& args);

        static const Mods::Update::Version Version;

        Program() = delete;
        Program(const Program&) = delete;
        Program& operator=(const Program&) = delete;

    private:
        struct Argument final
        {
            const std::optional<std::string> Name{};
            const std::optional<std::string> ValueOne{};
            const std::optional<std::string> ValueTwo{};

            Argument() = default;
            Argument(std::string name, std::optional<std::string> valueOne,
                std::optional<std::string> valueTwo = std::nullopt);
            Argument(const Argument&) = default;
            Argument(Argument&&) = default;
            Argument& operator=(const Argument& other);
            Argument& operator=(Argument&& other);
        };

        class PairRange;

        static const Mods::Update::Version _minExtractVersion;

        [[nodiscard]] static bool CheckSetup(const std::vector<std::string>& args);
        [[nodiscard]] static bool CheckVersion();
        [[nodiscard]] static PairRange GetPairs(
            const std::vector<Argument>& arguments,
            std::string_view fullName, std::string_view shortName);
        [[nodiscard]] static bool TryGetArgument(
            const std::vector<Argument>& arguments,
            std::string_view fullName, std::string_view shortName,
            std::optional<Argument>& argument);
        [[nodiscard]] static bool TryGetString(
            const std::vector<Argument>& arguments,
            std::string_view fullName, std::string_view shortName,
            std::optional<std::string>& value);
        [[nodiscard]] static bool TryGetInt(
            const std::vector<Argument>& arguments,
            std::string_view fullName, std::string_view shortName,
            std::int32_t& value);
        [[nodiscard]] static std::vector<Argument> ParseArguments(
            const std::vector<std::string>& args);
        [[noreturn]] static void Exit();
        static void Nop();
    };

    class ProgramException : public std::runtime_error
    {
    public:
        explicit ProgramException(const std::string& message);
    };
}
