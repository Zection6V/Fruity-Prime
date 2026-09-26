#pragma once

#include "GamepadState.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Input
{
    class GamepadChecks final
    {
    public:
        GamepadChecks() = delete;

        static void Check(bool condition, std::string_view message);
        [[nodiscard]] static std::int32_t Run(const std::optional<std::string>& shots = std::nullopt);

    private:
        static void Near(float actual, float expected, std::string_view message);
        [[nodiscard]] static GamepadState State(GamepadButtons buttons = GamepadButtons::None, float x = 0, float trigger = 0);
        static void CheckFocusLifecycle();
        static void CheckMenuLifecycle();
        static void CheckPersistence();

        inline static std::int32_t _checks = 0;
    };
}
