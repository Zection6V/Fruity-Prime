#include "GamepadLayout.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace MphRead::Mods::Input::GamepadLayoutAdapters
{
    // Equivalent of GLFW.GetJoystickAxes(slot).ToArray(). A missing value is
    // reserved for DllNotFoundException, EntryPointNotFoundException, or
    // BadImageFormatException; every other failure must propagate.
    [[nodiscard]] std::optional<std::vector<float>> GetJoystickAxes(std::int32_t slot);
}

namespace MphRead::Mods::Input
{
    const GamepadLayout GamepadLayout::Triggers(
        0, 1, 3, 4,
        2, 5,
        0, 1, 2, 3,
        4, 5,
        -1, -1,
        6, 7, 9, 10);

    const GamepadLayout GamepadLayout::Buttons(
        0, 1, 2, 3,
        -1, -1,
        0, 1, 2, 3,
        4, 5,
        6, 7,
        8, 9, 10, 11);

    GamepadLayout GamepadLayout::For(std::int32_t slot)
    {
        const std::optional<std::vector<float>> axes =
            GamepadLayoutAdapters::GetJoystickAxes(slot);
        return axes.has_value() && axes->size() >= 6 ? Triggers : Buttons;
    }
}
