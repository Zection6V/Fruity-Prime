#pragma once

#include "GamepadState.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::Mods::Input
{
    class GamepadProbe final
    {
    public:
        GamepadProbe() = delete;

        [[nodiscard]] static std::int32_t Run(double seconds, bool verbose = false);
        [[nodiscard]] static std::string Actions(GamepadButtons buttons);

    private:
        [[nodiscard]] static std::int32_t Watch(double seconds, bool verbose);
        static void ReportPresence();
        [[nodiscard]] static std::string Describe(const GamepadState& state);
        static void Name(std::string& text, GamepadButtons buttons, GamepadButtons match, std::string_view action);
    };
}
