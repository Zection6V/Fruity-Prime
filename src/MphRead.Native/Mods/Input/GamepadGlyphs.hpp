#pragma once

#include "GamepadState.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::Input
{
    enum class GamepadFamily : std::int32_t;

    class GamepadGlyphs final
    {
    public:
        GamepadGlyphs() = delete;

        [[nodiscard]] static GamepadFamily Detect(const std::string& name,
            const std::optional<std::string>& guid = std::nullopt, std::int32_t vendorId = 0);
        [[nodiscard]] static std::string Resolve(GamepadButtons button,
            std::optional<GamepadFamily> family = std::nullopt);
    };
}
