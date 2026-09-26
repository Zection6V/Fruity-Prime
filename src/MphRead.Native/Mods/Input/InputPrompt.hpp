#pragma once

#include "GamepadState.hpp"
#include "PadAction.hpp"

#include <string>

namespace MphRead::Mods::Input
{
    enum class UiAction : std::int32_t;

    struct InputPrompt
    {
        GamepadButtons Button = GamepadButtons::None;
        std::string Label{};
        GamepadButtons Modifier = GamepadButtons::None;

        [[nodiscard]] static InputPrompt For(UiAction action);
        [[nodiscard]] static InputPrompt For(PadAction action);
        [[nodiscard]] std::string Glyph() const;
        [[nodiscard]] std::string ToString() const;

        friend bool operator==(const InputPrompt&, const InputPrompt&) = default;
    };
}
