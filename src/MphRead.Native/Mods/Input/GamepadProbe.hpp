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
        ~GamepadProbe() = delete;
        GamepadProbe(const GamepadProbe&) = delete;
        GamepadProbe& operator=(const GamepadProbe&) = delete;
        GamepadProbe(GamepadProbe&&) = delete;
        GamepadProbe& operator=(GamepadProbe&&) = delete;

        [[nodiscard]] static std::int32_t Run(double seconds);

    private:
        [[nodiscard]] static std::int32_t Watch(double seconds);
        static void ReportPresence();
        [[nodiscard]] static std::string Describe(GamepadState state);
        [[nodiscard]] static std::string Actions(GamepadButtons buttons);
        static void Name(std::string& text, GamepadButtons buttons,
            GamepadButtons match, std::string_view action);
    };
}
