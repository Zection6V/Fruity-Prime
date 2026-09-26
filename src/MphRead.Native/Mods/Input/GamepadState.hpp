#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Input
{
    // [Flags]
    enum class GamepadButtons : std::int32_t
    {
        None = 0,
        A = 1 << 0,
        B = 1 << 1,
        X = 1 << 2,
        Y = 1 << 3,
        LeftBumper = 1 << 4,
        RightBumper = 1 << 5,
        Back = 1 << 6,
        Start = 1 << 7,
        LeftThumb = 1 << 8,
        RightThumb = 1 << 9,
        DpadUp = 1 << 10,
        DpadRight = 1 << 11,
        DpadDown = 1 << 12,
        DpadLeft = 1 << 13,
        LeftTrigger = 1 << 14,
        RightTrigger = 1 << 15
    };

    [[nodiscard]] constexpr GamepadButtons operator|(GamepadButtons left, GamepadButtons right) noexcept
    {
        return static_cast<GamepadButtons>(static_cast<std::int32_t>(left) | static_cast<std::int32_t>(right));
    }

    [[nodiscard]] constexpr GamepadButtons operator&(GamepadButtons left, GamepadButtons right) noexcept
    {
        return static_cast<GamepadButtons>(static_cast<std::int32_t>(left) & static_cast<std::int32_t>(right));
    }

    [[nodiscard]] constexpr GamepadButtons operator~(GamepadButtons value) noexcept
    {
        return static_cast<GamepadButtons>(~static_cast<std::int32_t>(value));
    }

    constexpr GamepadButtons& operator|=(GamepadButtons& left, GamepadButtons right) noexcept
    {
        return left = left | right;
    }

    constexpr GamepadButtons& operator&=(GamepadButtons& left, GamepadButtons right) noexcept
    {
        return left = left & right;
    }

    // `(buttons & flag) != 0` in the C#.
    [[nodiscard]] constexpr bool Any(GamepadButtons value) noexcept
    {
        return value != GamepadButtons::None;
    }

    // Enum.GetValues<GamepadButtons>(), in value order.
    inline constexpr std::array<GamepadButtons, 17> GamepadButtonValues{
        GamepadButtons::None, GamepadButtons::A, GamepadButtons::B, GamepadButtons::X, GamepadButtons::Y,
        GamepadButtons::LeftBumper, GamepadButtons::RightBumper, GamepadButtons::Back, GamepadButtons::Start,
        GamepadButtons::LeftThumb, GamepadButtons::RightThumb, GamepadButtons::DpadUp, GamepadButtons::DpadRight,
        GamepadButtons::DpadDown, GamepadButtons::DpadLeft, GamepadButtons::LeftTrigger, GamepadButtons::RightTrigger};

    // GamepadButtons.ToString().
    [[nodiscard]] std::string ToString(GamepadButtons value);
    // Enum.TryParse<GamepadButtons>(text, out value).
    [[nodiscard]] bool TryParse(std::string_view text, GamepadButtons& value);

    struct GamepadState
    {
        bool Connected = false;
        float LeftX = 0.0F;
        float LeftY = 0.0F;
        float RightX = 0.0F;
        float RightY = 0.0F;
        float LeftTrigger = 0.0F;
        float RightTrigger = 0.0F;
        GamepadButtons Buttons = GamepadButtons::None;
        std::optional<std::string> Name{};

        [[nodiscard]] bool Down(GamepadButtons button) const noexcept;

        // ValueType.Equals: every field.
        friend bool operator==(const GamepadState&, const GamepadState&) = default;
    };
}
