#include "GamepadState.hpp"

#include "../../NativeRuntime/System/Enum.hpp"

namespace MphRead::Mods::Input
{
    namespace
    {
        constexpr ::MphRead::NativeRuntime::EnumNameEntry Names[] = {
            {0, "None"}, {1U << 0, "A"}, {1U << 1, "B"}, {1U << 2, "X"}, {1U << 3, "Y"},
            {1U << 4, "LeftBumper"}, {1U << 5, "RightBumper"}, {1U << 6, "Back"}, {1U << 7, "Start"},
            {1U << 8, "LeftThumb"}, {1U << 9, "RightThumb"}, {1U << 10, "DpadUp"}, {1U << 11, "DpadRight"},
            {1U << 12, "DpadDown"}, {1U << 13, "DpadLeft"}, {1U << 14, "LeftTrigger"}, {1U << 15, "RightTrigger"}};
    }

    std::string ToString(GamepadButtons value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(value, Names, std::size(Names), true);
    }

    bool TryParse(std::string_view text, GamepadButtons& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(text, false, Names, std::size(Names), value);
    }

    bool GamepadState::Down(GamepadButtons button) const noexcept
    {
        return (static_cast<std::int32_t>(Buttons) & static_cast<std::int32_t>(button)) != 0;
    }
}
